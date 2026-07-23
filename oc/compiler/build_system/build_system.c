/**
 * Author: Jack Robbins
 * This file defines the Ollie build system used for dependency management in Ollie
 */

#include "build_system.h"
#include "../utils/error_management.h"
#include "../utils/constants.h"
#include "../symtab/symtab.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

//Ollie's general library must always be located here
static char* OLLIE_LIBRARY_DIRECTORY = "/usr/lib/ollie";

/**
 * Different import status types for different
 * errors
 */
typedef enum {
	IMPORT_STATUS_SUCCESS,
	IMPORT_STATUS_NOT_FOUND,
	IMPORT_STATUS_TOKENIZATION_FAILURE,
	IMPORT_STATUS_CIRCULAR_DEPENDENCY,
	IMPORT_STATUS_PASS_THROUGH_FAILURE,
} import_status_t;

/**
 * The import results struct contains the dependency
 * node itself as well as any failure info if we 
 * have it. 
 */
typedef struct import_results_t import_results_t;
struct import_results_t {
	dependency_graph_node_t* result_node;
	import_status_t import_status;
};

//Helper that will let us initialize a wiped out version
#define INITIALIZE_BLANK_BUILD_SYSTEM_RESULTS {{NULL, 0, 0}, NULL, BUILD_SYSTEM_STATUS_FAILURE, 0}

//We will maintain an overall module symtab to avoid duplicate searches
static module_symtab_t* module_symtab = NULL;

//Track the reverse compilation order here
static dynamic_array_t compilation_order;

//Static string buffer for any error messages that we print
static char build_system_info[ERROR_SIZE * 5];

//Declare a reusable token stream for file seraching to avoid constant reallocating
static ollie_token_stream_t reusable_file_searching_stream;

//Keep track of the error and warning counts
static u_int32_t num_build_system_errors = 0;

//Predeclare for recursive calls
static import_results_t find_or_create_module(char* initial_directory, char* current_file_name, dynamic_string_t* module_name, u_int8_t silent_mode);


/**
 * Take a file that may look like: ./oc/test_files/sample.ol and return sample.ol
 */
static inline char* extract_file_name_from_fully_qualified_name(char* fully_qualified_name){
	int32_t length = strlen(fully_qualified_name);

	//Roll this back until we have the index of the first /
	int32_t i = length - 1;
	for(; i >= 0; i--){
		if(fully_qualified_name[i] == '/'){
			break;
		}
	}

	//Offset into this to get it(+ 1 to get past the /)
	return fully_qualified_name + i + 1;
}


/**
 * A generic printer for any build system errors that we may encounter
 */
static void print_build_system_message(error_message_type_t message, char* info, char* file_name, u_int32_t line_number){
	//Different types to print out
	static const char* type[] = {"WARNING", "ERROR", "INFO", "DEBUG"};

	//Get the stripped down file name from here
	char* stripped_file_name = extract_file_name_from_fully_qualified_name(file_name);

	fprintf(stdout, "\n[FILE: %s] --> [LINE %d | OLLIE BUILD SYSTEM %s]: %s\n", stripped_file_name, line_number, type[message], info);
}


/**
 * Debug helper to print the build order
 */
static inline void print_build_order(){
	printf("\n================== BUILD ORDER =================\n");

	for(int32_t i = 0; i < compilation_order.current_index; i++){
		dependency_graph_node_t* node = dynamic_array_get_at(&compilation_order, i);
		printf("%d: %s\n", i + 1, node->file_name);
	}

	printf("================== BUILD ORDER =================\n\n");
}


/**
 * Get the file extension of the given filename string. Returns NULL
 * if no "." can be found
 */
static inline char* get_file_extension(char* file){
	//Get the end to start at
	int32_t length = strlen(file);

	//The end index that we have
	int32_t end_index = length - 1;

	//Work our way backwards until we see a dot
	while(end_index > 0){
		if(file[end_index] == '.'){
			return file + end_index;
		}

		end_index--;
	}

	//If we got to here then we found nothing
	return NULL;
}


/**
 * Parse a fully qualified file name to get the directory. If
 * no '/' is ever found, we return "." as the directory name
 *
 * The directory name is populated into the directory_buffer
 */
static inline void get_directory(char* directory_buffer, char* full_file_name){
	//Get the length to know where the end is
	int32_t length = strlen(full_file_name);
	int32_t end_index = length - 1;

	//Keep going so long as we don't hit the end
	while(end_index > 0){
		/**
		 * Exit case - we have the first instance of a '/' so
		 * we will get the entire name up to but not including the slash
		 */
		if(full_file_name[end_index] == '/'){
			snprintf(directory_buffer, end_index + 1, "%s", full_file_name);
			return;
		}

		end_index--;
	}

	//We didn't find the / - so give back "." as the directory
	sprintf(directory_buffer, "%s", ".");
}


/**
 * Search the first 2 tokens of the given file to determine if it does or does not match
 * the given module name. If it does, return SUCCESS, otherwise, return FAILURE. This 
 * function makes use of a global reusable file searching stream to avoid excessive
 * allocations/reallocations. On a large project, we may be searching 1000s of files at
 * this stage so being efficient is important
 */
static inline u_int8_t does_file_define_module(char* file_name, dynamic_string_t* module_name, u_int8_t silent_mode){
	//We have a reusable token stream - all we need to do to use it is reset it
	reset_token_stream(&reusable_file_searching_stream);

	//Attempt to extract the first 2 tokens
	u_int8_t success = get_first_2_tokens(&reusable_file_searching_stream, file_name, silent_mode);

	/**
	 * We immediately exit the compiler if this happens - means that somehow
	 * somewhere a dependency file is corrupted
	 */
	if(success == FAILURE || reusable_file_searching_stream.status == STREAM_STATUS_FAILURE){
		fprintf(stderr, "Fatal internal compiler error: the file %s could not be tokenized. It is likely that you have a corrupted dependency file", file_name);
		exit(1);
	}

	//Get the first token. If it's not the $module directive, we can leave now
	lexitem_t* cursor = token_array_get_pointer_at(&(reusable_file_searching_stream.token_stream), 0);
	if(cursor->tok != MODULE){
		return FAILURE;
	}

	//Now get the second token. Again if it's not an identifier we can leave
	cursor = token_array_get_pointer_at(&(reusable_file_searching_stream.token_stream), 1);
	if(cursor->tok != IDENT){
		return FAILURE;
	}

	//Now all that's left is to see if these match up
	return (dynamic_strings_equal(&(cursor->lexeme), module_name) == TRUE) ? SUCCESS : FAILURE;
}


/**
 * Traverse a directory and recursively search for a module by tokenizing
 * just the first two tokens in each regular *.ol file that we find. If
 * we come across a directory, we will recursively search the directory
 * as well
 *
 * If we do find the file, we will place it inside of the "dependency_file" string
 * that is passed in preallocated and return success
 */
static u_int8_t traverse_and_search_for_module_rec(char* dependency_file, char* path_name, dynamic_string_t* module_name, u_int8_t silent_mode){
	//Storage for new file paths
	char new_path[FILENAME_MAX];

	//Status struct
	struct stat status;

	//Get the status of the path
	int32_t path_status = stat(path_name, &status);

	//This really should never happen but we'll check anyways
	if(path_status == -1){
		fprintf(stderr, "Fatal internal build system error - invalid path name %s detected", path_name);
		exit(1);
	}

	/**
	 * First option - we have a regular file. We only care about regular files
	 * *if* they are .ol files. If they're not we ignore them
	 */
	if(S_ISREG(status.st_mode)){
		//Get the file extension off of this
		char* file_extension = get_file_extension(path_name);

		/**
		 * If we have a file extension that is *.ol, we will search
		 * this file. Anything else we ignore it and move on
		 */
		if(file_extension != NULL && strcmp(file_extension, ".ol") == 0){
			//If they do match, we need to copy the path name into the dependency_file buffer
			if(does_file_define_module(path_name, module_name, silent_mode) == TRUE){
				strncpy(dependency_file, path_name, FILENAME_MAX);
				return SUCCESS;

			} else {
				return FAILURE;
			}

		//Not a .ol file, we won't even bother searching
		} else {
			return FAILURE;
		}

	/**
	 * Second option - we have a directory(other than . or ..). We'll need to go into 
	 * this directory and traverse it to see if we can find anything in there
	 */
	} else if(S_ISDIR(status.st_mode)) {
		//Open the directory up first
		DIR* directory = opendir(path_name);

		//If we couldn't open it then fail out
		if(directory == NULL){
			fprintf(stderr, "Fatal internal build system error - invalid directory %s detected", path_name);
			exit(1);
		}

		/**
		 * So long as we have directory entries to read, we will 
		 * recursively invoke this function
		 */
		struct dirent* directory_entry;
		while((directory_entry = readdir(directory)) != NULL){
			//Avoid reading ".." or "." - we would infinite loop if we did
			if(directory_entry->d_name[0] == '.'){
				continue;
			}

			//Construct the new filename path here for our directory/file
			snprintf(new_path, FILENAME_MAX, "%s/%s", path_name, directory_entry->d_name);

			//Recursively call into here to do this
			u_int8_t result = traverse_and_search_for_module_rec(dependency_file, new_path, module_name, silent_mode);

			//If we found it - we will not go on any further - just exit out now - we do not need to go farther
			if(result == SUCCESS){
				//Close before leaving
				closedir(directory);
				return SUCCESS;
			}
		}

		//If we do make it here make sure we close the directory
		closedir(directory);
	} 

	//If we've made it all the way to here, then we found nothing
	return FAILURE;
}


/**
 * Handle the parsing of an import statement. Note that there are two different things that we
 * can see for an import statement:
 * 	1.) import "value"; <- double quotes tell the compiler to look in the ./value path. This is used
 * 		for local imports
 * 	2.) import <value>; <- angle bracktes tell the compiler to look in the system library "/usr/lib/ollie/" for the
 * 		given module
 *
 * This helper returns a fully formed, ready-to-use dependency graph node with the file dependency if we were
 * able to find it
 *
 * NOTE: by the time that we get here we have already seen the "$import" token
 */
static inline dependency_graph_node_t* get_dependency_subtree_from_import_statement(ollie_token_stream_t* stream, char* main_file_directory, char* current_file_name, int32_t* current_index, u_int8_t silent_mode){
	//Get the next value in the stream
	lexitem_t* lookahead = token_array_get_pointer_at(&(stream->token_stream), *current_index);
	(*current_index)++;

	//What directory are we searching - this differs based on the type of import status
	char* directory_to_search = NULL;
	
	//Generic result holder
	import_results_t results;

	/**
	 * We can see either "file_name" or <file_name> here. Anything else is
	 * bad and will lead us to fail out
	 */
	switch(lookahead->tok){
		/**
		 * A string constant means that we are going to look for the file
		 * in the local "./" directory and any subdirectory of this current directory
		 */
		case STR_CONST:
			/**
			 * Let the helper go through and search our local directory for this module. If we can't
			 * find it, then we have an issue and we throw an error
			 */
			directory_to_search = main_file_directory;
			break;

		/**
		 * An identifier wrapped in angle brackets means that we are going to look for the file in
		 * the /usr/ollie/lib directory and any subdirectories therein
		 */
		case L_THAN:
			//Refresh the lookahead token
			lookahead = token_array_get_pointer_at(&(stream->token_stream), *current_index);
			(*current_index)++;

			/**
			 * If we don't have an identifier then we fail out
			 */
			if(lookahead->tok != IDENT){
				sprintf(build_system_info, "Expected identifier after $import keyword but saw %s instead", lexitem_to_string(lookahead));
				print_build_system_message(MESSAGE_TYPE_ERROR, build_system_info, current_file_name, lookahead->line_num);
				num_build_system_errors++;
				return NULL;
			}

			//Before we waste time searching, let's make sure that the closing > is there
			lookahead = token_array_get_pointer_at(&(stream->token_stream), *current_index);
			(*current_index)++;

			//Didn't find it so we fail out
			if(lookahead->tok != G_THAN){
				sprintf(build_system_info, "Expected > after module name but saw %s instead", lexitem_to_string(lookahead));
				print_build_system_message(MESSAGE_TYPE_ERROR, build_system_info, current_file_name, lookahead->line_num);
				num_build_system_errors++;
				return NULL;
			}

			//Flag that we want to search in the ollie standard library
			directory_to_search = OLLIE_LIBRARY_DIRECTORY;
			break;

		default:
			sprintf(build_system_info, "Expected \"file_name\" or <file_name> after $import keyword but saw %s instead", lexitem_to_string(lookahead));
			print_build_system_message(MESSAGE_TYPE_ERROR, build_system_info, current_file_name, lookahead->line_num);
			num_build_system_errors++;
			return NULL;
	}

	/**
	 * Now that we know where to look we'll make the actual call. Based on what comes back we'll
	 * have a certain error message to display
	 */
	results = find_or_create_module(directory_to_search, current_file_name, &(lookahead->lexeme), silent_mode);

	switch(results.import_status){
		case IMPORT_STATUS_SUCCESS:
			break;

		case IMPORT_STATUS_NOT_FOUND:
			sprintf(build_system_info, "Module \"%s\" could not be found anywhere under the directory %s", lookahead->lexeme.string, directory_to_search);
			print_build_system_message(MESSAGE_TYPE_ERROR, build_system_info, current_file_name, lookahead->line_num);
			num_build_system_errors++;
			return NULL;

		case IMPORT_STATUS_TOKENIZATION_FAILURE:
			sprintf(build_system_info, "Module \"%s\" was found in but failed to tokenize", lookahead->lexeme.string);
			print_build_system_message(MESSAGE_TYPE_ERROR, build_system_info, current_file_name, lookahead->line_num);
			num_build_system_errors++;
			return NULL;

		case IMPORT_STATUS_CIRCULAR_DEPENDENCY:
			sprintf(build_system_info, "Module \"%s\" was found to have an invalid circular dependency", lookahead->lexeme.string);
			print_build_system_message(MESSAGE_TYPE_ERROR, build_system_info, current_file_name, lookahead->line_num);
			num_build_system_errors++;
			return NULL;

		//Just a pass through failure so don't print anything more
		case IMPORT_STATUS_PASS_THROUGH_FAILURE:
			return NULL;
	}

	//Let's look for the final semicolon here
	lookahead = token_array_get_pointer_at(&(stream->token_stream), *current_index);
	(*current_index)++;

	//If we don't have it then fail out
	if(lookahead->tok != SEMICOLON){
		print_build_system_message(MESSAGE_TYPE_ERROR, "Semicolon expected after $import statement", current_file_name, lookahead->line_num);
		num_build_system_errors++;
		return NULL;
	}

	//Give back the actual node that we found
	return results.result_node;
}


/**
 * Search for a module in the appropriate directory.
 *
 * Before we even open a file, we will first search in the symbol table itself for a module. If we find it 
 * in the symbol table, we will return what the symbol table gave us
 *
 * Otherwise, we will recursively search the intial directory and all subdirectories for any "*.ol" files 
 * whose second token matches the name we are after. We only ever tokenize the first 2 tokens in our search
 * for efficiency's sake. If we do not find it, then we fail out. If we do find it, then we will create and
 * insert the record into the module symtab for future go arounds. We will also fully tokenize the module and give
 * it a proper dependency graph node
 *
 * NOTE: if we are in fact creating a module here for the first time, it is the responsibility of this
 * file itself to parse any further import statements that we have in here
 *
 * NOTE: This helper also handles all cyclical dependency checking and recursively creates the build
 * order
 *
 * algorithm find_or_create_module(module name):
 *  found <- lookup module in symtab
 *  if found is not NULL:
 *  	if found is IN_PROGRESS:
 *  		we have a circular dependency - fail out
 *  	return the found node
 *
 *  traverse for the module in all subdirectories
 *  if not found:
 *  	fail out
 *
 *  tokenzie the entire file
 *  if tokenizing fails:
 *  	fail out
 *
 * 	create a dependency graph node for the file and flag it as IN_PROGRESS
 * 	add dependency graph as a module in the symtab
 *
 * 	for each import statement in the file:
 * 		if find_or_create_module fails(imported module) fails:
 * 			fail out
 * 		add the imported module as a dependency of this one
 *
 * 	flag the node as COMPLETED
 * 	push the node onto the reverse build order list
 * 	return the node
 *
 * This algorithm does 3 things at once: it handles all of our import statements, does all circular dependency detection,
 * and constructs our reverse build order for use by the next step in compilation
 */
static import_results_t find_or_create_module(char* initial_directory, char* current_file_name, dynamic_string_t* module_name, u_int8_t silent_mode){
	//Initialize our import results
	import_results_t results = {NULL, IMPORT_STATUS_NOT_FOUND};

	/**
	 * Step 1: hit the module symtab and see if we can find anything in
	 * there. If we can, we save ourselves the trouble of searching the file system
	 */
	symtab_module_record_t* found_module = lookup_module(module_symtab, module_name);

	/**
	 * If we were able to find it in the symbol table give, back the associated dependency
	 * graph node that already exists for this given module
	 */
	if(found_module != NULL){
		/**
		 * If this is currently in progress, it means that we have a circular dependency and need to fail out. The
		 * node being in progress means that somewhere up the chain, this node is already being processed which makes
		 * this circular
		 */
		if(found_module->dependency_graph_node->visitation_status == DEPENDENCY_NODE_IN_PROGRESS){
			sprintf(build_system_info, "The dependency %s in file %s for file %s has been found to be ciruclar. Please remedy and recompile",
		   								found_module->dependency_graph_node->module_name.string,
		   								found_module->dependency_graph_node->file_name,
										current_file_name);
			print_build_system_message(MESSAGE_TYPE_ERROR, build_system_info, found_module->dependency_graph_node->file_name, 0);
			num_build_system_errors++;
			results.import_status = IMPORT_STATUS_CIRCULAR_DEPENDENCY;
			return results;
		}

		//Otherwise we can pacakge up and return like so
		results.import_status = IMPORT_STATUS_SUCCESS;
		results.result_node = found_module->dependency_graph_node;
		return results;
	}

	/**
	 * Step 2: Otherwise we did not find it, so we are going to have to search
	 * for it inside of the given initial directory using a recursive
	 * directory search. If this fails, we did not find the module so the
	 * entire thing is wrong and we fail out
	 */
	char dependency_file[FILENAME_MAX];
	u_int8_t found = traverse_and_search_for_module_rec(dependency_file, initial_directory, module_name, silent_mode);

	//Could not find it so get out
	if(found == FALSE){
		results.import_status = IMPORT_STATUS_NOT_FOUND;
		return results;
	}

	/**
	 * Step 3: Now that we've found something, we'll need to create a dependency graph node for the 
	 * next go around. In order to do this, we're going to need to fully tokenize the entire
	 * thing. If this tokenizing fails we will return a different failure
	 */
	ollie_token_stream_t new_token_stream = tokenize(dependency_file, silent_mode);

	/**
	 * Remember that our original tokenization pass only did the first 2 tokens, so it's
	 * possible that we could actually fail to tokenize here. If we do then we will
	 * declare that we found the dependency but there's some issue with it
	 */
	if(new_token_stream.status == STREAM_STATUS_FAILURE){
		sprintf(build_system_info, "The dependency %s was found in file %s has failed to tokenize. Please review and recompile.", 
		  							module_name->string,
		  							dependency_file);
		print_build_system_message(MESSAGE_TYPE_ERROR, build_system_info, dependency_file, 0);
		num_build_system_errors++;
		results.import_status = IMPORT_STATUS_TOKENIZATION_FAILURE;
		return results;
	}

	//Create the new dependency node now that we know we've got a good stream
	dependency_graph_node_t* new_node = dependency_graph_node_alloc(module_name, dependency_file, &new_token_stream, DEPENDENCY_GRAPH_NODE_TYPE_DEPENDENCY);

	//Add this onto the symtab for future lookups to find
	symtab_module_record_t* new_module = create_module_record(new_node);
	insert_module(module_symtab, new_module);

	//Flag that it is currently in progress for later circular dependency detection
	new_node->visitation_status = DEPENDENCY_NODE_IN_PROGRESS;

	/**
	 * Step 4: now that we've tokenized the entire thing, we will need to go through
	 * and determine if this file itself has any imports for furhter dependencies. If
	 * it does, we'll have to recursively go through and pull all of those in as well
	 *
	 * REMEMBER: we need to start at the third token index because the first 3
	 * values are going to be $module <ident>;, so we don't want to try and
	 * reprocess those
	 */
	int32_t current_token_index = 3;

	//Run through the top of the file and process until we're done seeing imports
	while(TRUE){
		lexitem_t* lookahead = token_array_get_pointer_at(&(new_token_stream.token_stream), current_token_index);
		current_token_index++;

		//Terminal case here - we're done looking anymore
		if(lookahead->tok != IMPORT){
			break;
		}

		/**
		 * Let the helper find and possible create the dependency graph node from our
		 * import statement. If this succeeds, it means that the direct import
		 * itself *and* all indirect imports worked, so in a sense this not only gives
		 * back a single dependency node but the root of a dependency tree
		 */
		dependency_graph_node_t* dependency = get_dependency_subtree_from_import_statement(&new_token_stream, initial_directory, dependency_file, &current_token_index, silent_mode);
		if(dependency == NULL){
			print_build_system_message(MESSAGE_TYPE_ERROR, "Invalid $import directive found in file. Please review and recompile", dependency_file, lookahead->line_num);
			num_build_system_errors++;
			results.import_status = IMPORT_STATUS_PASS_THROUGH_FAILURE;
			return results;
		}

		/**
		 * Create the relationship in the graph that
		 * the main node(dependant) depends on the dependancy
		 */
		add_dependency(new_node, dependency);
	}

	//Flag that it's now done and add it to the compilation order
	dynamic_array_add(&compilation_order, new_node);
	new_node->visitation_status = DEPENDENCY_NODE_FULLY_PROCESSED;

	//Give this back so that it can be added to the graph
	results.result_node = new_node;
	results.import_status = IMPORT_STATUS_SUCCESS;
	return results; 
}


/**
 * The main file in ollie is the file that the user has passed in via the -f option
 * to the ollie compiler. This is the only file that a user should have to directly
 * reference when compiling
 *
 * The main file is a special case because it may *not* be a module. We will need to
 * validate that the user is not attempting to make this file into a module. If we
 * find that they are attempting that, we will have to fail out
 *
 * As we go through the imports of the main file, we will also be tokenizing and handling
 * the imports of those dependency files themselves
 */
static dependency_graph_node_t* handle_main_file_tokenization(char* main_file_directory, char* main_file_name, u_int8_t silent_mode){
	//Let's first tokenize the main file
	ollie_token_stream_t stream = tokenize(main_file_name, silent_mode);

	/**
	 * If tokenizing failed there's no point in going further.
	 * We fail out here and don't even bother returning anything
	 */
	if(stream.status == STREAM_STATUS_FAILURE){
		print_build_system_message(MESSAGE_TYPE_ERROR, "Tokenzining failed. Please remedy the error and recompile", main_file_name, 0);
		num_build_system_errors++;
		return NULL;
	}

	/**
	 * Let's now verify that there is no $module definition inside of the main file. Remember
	 * that this is strictly forbidden so if we see it we're out. Since the only valid place
	 * to see a $module definition is the very first token, we only need to check that
	 */
	lexitem_t* first_token = token_array_get_pointer_at(&(stream.token_stream), 0);
	if(first_token->tok == MODULE){
		print_build_system_message(MESSAGE_TYPE_ERROR, "The main file may never be defined as a module", main_file_name, 0);
		num_build_system_errors++;
		return NULL;
	}

	/**
	 * Give the main dependency node a name that would never be accepted
	 * by the regular module declaration to avoid collisions
	 */
	dynamic_string_t main_dependency_node_name = dynamic_string_alloc();
	dynamic_string_set(&main_dependency_node_name, "^^MAIN_DEPENDENCY_GRAPH_NODE^^");

	//Otherwise we should be good to package this up into a dependency graph node
	dependency_graph_node_t* main_dependency_node = dependency_graph_node_alloc(&main_dependency_node_name, main_file_name, &stream, DEPENDENCY_GRAPH_NODE_TYPE_MAIN);

	//Flag that this is in progress
	main_dependency_node->visitation_status = DEPENDENCY_NODE_IN_PROGRESS;

	//Insert this into the symtab for completeness
	insert_module(module_symtab, create_module_record(main_dependency_node));

	/**
	 * We will now run through and parse all of the dependencies that this file has. It is of course 
	 * possible that there are no dependencies, but we must do our check here. We will keep
	 * parsing the dependencies until we hit the first non-import token
	 */
	int32_t current_token_index = 0;

	//Run through the top of the file and process until we're done seeing imports
	while(TRUE){
		lexitem_t* lookahead = token_array_get_pointer_at(&(stream.token_stream), current_token_index);
		current_token_index++;

		//Terminal case here - we're done looking anymore
		if(lookahead->tok != IMPORT){
			break;
		}

		/**
		 * Let the helper find and create the dependency graph node from our
		 * import statement. If this succeeds, it means that the direct import
		 * itself *and* all indirect imports worked, so in a sense this not only gives
		 * back a single dependency node but the root of a dependency tree
		 */
		dependency_graph_node_t* dependency = get_dependency_subtree_from_import_statement(&stream, main_file_directory, main_file_name, &current_token_index, silent_mode);
		if(dependency == NULL){
			print_build_system_message(MESSAGE_TYPE_ERROR, "A main file dependency has been found in error. Please review the compiler output to resolve.", main_file_name, 0);
			num_build_system_errors++;
			return NULL;
		}

		/**
		 * Create the relationship in the graph that
		 * the main node(dependant) depends on the dependancy
		 */
		add_dependency(main_dependency_node, dependency);
	}

	//Once we're all the way done, add this onto the reverse compilation order and flag that we're finished
	main_dependency_node->visitation_status = DEPENDENCY_NODE_FULLY_PROCESSED;
	dynamic_array_add(&compilation_order, main_dependency_node);

	//Give back the main dependency node
	return main_dependency_node;
}


/**
 * The main and only entry point to the build system revolves around
 * us parsing dependencies and constructing them into one gigantic, unified token
 * stream. This token stream is what we will use to actually parse and construct
 * the overall CFG
 *
 * NOTE: we can *not* deallocate the dependency graph when we do this because we
 * need all of the info contained within for the rest of compilation
 */
build_system_results_t construct_build_order(compiler_options_t* options, u_int8_t silent_mode){
	//First we'll need some blank results to get started
	build_system_results_t results = INITIALIZE_BLANK_BUILD_SYSTEM_RESULTS;

	//Pre-allocate the reverse compilation order here, we will populate it as we go
	compilation_order = dynamic_array_alloc();

	//Allocate the module symtab first
	module_symtab = module_symtab_alloc();

	//We'll need this for searching files with the first 2 tokens methodology
	reusable_file_searching_stream = token_stream_alloc();

	/**
	 * The actual main file itself is all that the user provides here. The build system will
	 * then crawl through the dependencies in the main file and each of those files recursively
	 * until all dependencies are exhausted. We return *one* unified token stream in the end
	 */
	char* main_file_name = options->file_name;

	//Grab the main file directory that all "" imports are searched for under
	char main_file_directory[FILENAME_MAX];
	get_directory(main_file_directory, main_file_name);

	//Let the helper go out and parse through the main file and its dependencies
	dependency_graph_node_t* main_node = handle_main_file_tokenization(main_file_directory, main_file_name, silent_mode);

	//Destroy the reusable file searcher stream now that we're done
	destroy_token_stream(&reusable_file_searching_stream);	

	//If we have no main node, that means that we've failed here so return a failure
	if(main_node == NULL){
		results.status = BUILD_SYSTEM_STATUS_FAILURE;
		results.num_errors = num_build_system_errors;
		return results;
	}

	/**
	 * If we have debugging enabled display the entire build order
	 */
	if(options->enable_debug_printing == TRUE){
		print_build_order();
	}

	/**
	 * Package up and give back the compilation order
	 * and the main node
	 */
	results.compilation_order = compilation_order;
	results.result_node = main_node;
	results.status = BUILD_SYSTEM_STATUS_SUCCESS;
	return results;
}
