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

//Helper that will let us initialize a wiped out version
#define INITIALIZE_BLANK_BUILD_SYSTEM_RESULTS {NULL, BUILD_SYSTEM_STATUS_FAILURE}

//We will maintain an overall module symtab to avoid duplicate searches
module_symtab_t* module_symtab = NULL;

//Static string buffer for any error messages that we print
static char build_system_info[ERROR_SIZE * 5];

//Declare a reusable token stream for file seraching to avoid constant reallocating
static ollie_token_stream_t reusable_file_searching_stream;

//Keep track of the error and warning counts
static u_int32_t num_build_system_errors = 0;
static u_int32_t num_build_system_warnings = 0;


/**
 * A generic printer for any build system errors that we may encounter
 */
static inline void print_build_system_message(error_message_type_t message, char* info, char* file_name, u_int32_t line_number){
	//Different types to print out
	static const char* type[] = {"WARNING", "ERROR", "INFO", "DEBUG"};

	fprintf(stdout, "\n[FILE: %s] --> [LINE %d | OLLIE BUILD SYSTEM %s]: %s\n", file_name, line_number, type[message], info);
}


/**
 * Search the first 2 tokens of the given file to determine if it does or does not match
 * the given module name. If it does, return SUCCESS, otherwise, return FAILURE. This 
 * function makes use of a global reusable file searching stream to avoid excessive
 * allocations/reallocations. On a large project, we may be searching 1000s of files at
 * this stage so being efficient is important
 */
static inline u_int8_t does_file_match_module(char* file_name, dynamic_string_t* module_name, u_int8_t silent_mode){
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
			if(does_file_match_module(path_name, module_name, silent_mode) == TRUE){
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

		printf("SEARCHING DIRECTORY %s\n", path_name);

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
 */
static inline dependency_graph_node_t* find_module(char* initial_directory, dynamic_string_t* module_name, u_int8_t silent_mode){
	/**
	 * First step in our search - hit the module symtab and see if we can find anything in
	 * there. If we can, we save ourselves the trouble of searching the file system
	 */
	symtab_module_record_t* found_module = lookup_module(module_symtab, module_name);

	/**
	 * If we were able to find it in the symbol table give, back the associated dependency
	 * graph node that already exists for this given module
	 */
	if(found_module != NULL){
		return found_module->dependency_graph_node;
	}

	/**
	 * Otherwise we did not find it, so we are going to have to search
	 * for it inside of the given initial directory using a recursive
	 * directory search
	 */
	char dependency_file[FILENAME_MAX];
	u_int8_t found = traverse_and_search_for_module_rec(dependency_file, initial_directory, module_name, silent_mode);

	//Could not find it so get out
	if(found == FALSE){
		return NULL;
	}

	/**
	 * Now that we've found something, we'll need to create a dependency graph node for the 
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
		return NULL;
	}


	dependency_graph_node_t* new_node = dependency_graph_node_alloc(module_name, dependency_file, *stream, dependency_node_type_t node_type)



	symtab_module_record_t* new_module = create_module_record(jk);


	printf("DEPENDENCY %s IS IN FILE %s\n\n", module_name->string, depedency_file);


	return NULL;
}


/**
 * Handle the parsing of an import statement. Note that there are two different things that we
 * can see for an import statement:
 * 	1.) import "value"; <- double quotes tell the compiler to look in the ./value path. This is used
 * 		for local imports
 * 	2.) import <value>; <- angle bracktes tell the compiler to look in the system library "/usr/lib/ollie/" for the
 * 		given module
 *
 * NOTE: by the time that we get here we have already seen the "$import" token
 */
static dependency_graph_node_t* parse_import_statement_and_get_dependency(ollie_token_stream_t* stream, char* main_file_directory, char* current_file_name, int32_t* current_index, u_int8_t silent_mode){
	//Get the next value in the stream
	lexitem_t* lookahead = token_array_get_pointer_at(&(stream->token_stream), *current_index);
	(*current_index)++;

	//The found dependency
	dependency_graph_node_t* found_module_dependency = NULL;

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
			found_module_dependency = find_module(main_file_directory, &(lookahead->lexeme), silent_mode);

			//Fail out if we don't have it
			if(found_module_dependency == NULL){
				sprintf(build_system_info, "Module \"%s\" could not be found anywhere under the local directory", lookahead->lexeme.string);
				print_build_system_message(MESSAGE_TYPE_ERROR, build_system_info, current_file_name, lookahead->line_num);
				num_build_system_errors++;
				return FAILURE;
			}

			//TODO OTHERWISE FOUND IT
			
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
				return FAILURE;
			}

			//Before we waste time searching, let's make sure that the closing > is there
			lookahead = token_array_get_pointer_at(&(stream->token_stream), *current_index);
			(*current_index)++;

			//Didn't find it so we fail out
			if(lookahead->tok != G_THAN){
				sprintf(build_system_info, "Expected > after module name but saw %s instead", lexitem_to_string(lookahead));
				print_build_system_message(MESSAGE_TYPE_ERROR, build_system_info, current_file_name, lookahead->line_num);
				num_build_system_errors++;
				return FAILURE;
			}

			/**
			 * Now let the helper go through and search our local directory for this module. If we can't
			 * find it, then we have an issue and we throw an error
			 */
			found_module_dependency = find_module(OLLIE_LIBRARY_DIRECTORY, &(lookahead->lexeme), silent_mode);

			if(found_module_dependency == NULL){
				sprintf(build_system_info, "Module \"%s\" could not be found anywhere under the local directory", lookahead->lexeme.string);
				print_build_system_message(MESSAGE_TYPE_ERROR, build_system_info, current_file_name, lookahead->line_num);
				num_build_system_errors++;
				return FAILURE;
			}

			//TODO OTHERWISE FOUND IT

			break;

		default:
			sprintf(build_system_info, "Expected \"file_name\" or <file_name> after $import keyword but saw %s instead", lexitem_to_string(lookahead));
			print_build_system_message(MESSAGE_TYPE_ERROR, build_system_info, current_file_name, lookahead->line_num);
			num_build_system_errors++;
			return FAILURE;
	}

	//Let's look for the final semicolon here
	lookahead = token_array_get_pointer_at(&(stream->token_stream), *current_index);
	(*current_index)++;

	//If we don't have it then fail out
	if(lookahead->tok != SEMICOLON){
		print_build_system_message(MESSAGE_TYPE_ERROR, "Semicolon expected after $import statement", current_file_name, lookahead->line_num);
		num_build_system_errors++;
		return FAILURE;
	}



	//DUMMY
	return FAILURE;
}


/**
 * For each file, we must:
 * 	1.) Perform the tokenization for that file only
 * 	2.) Parse the header values and determine if there are dependencies
 * 	3.) Construct a build dependency graph node containing this file name and its token stream
 * 	4.) For each dependency, perform this same process on it
 * TODO VALIDATIONS, ETC
 * 	
 */
static void handle_dependency_file_tokenization(char* file_name, u_int8_t silent_mode){

	//TODO VOID FOR NOW
}


/**
 * The main file in ollie is the file that the user has passed in via the -f option
 * to the ollie compiler
 *
 * The main file is a special case because it may *not* be a module. We will need to
 * validate that the user is not attempting to make this file into a module
 */
static build_system_results_t handle_main_file_tokenization(char* main_file_directory, char* main_file_name, u_int8_t silent_mode){
	//Create and initialize our results
	build_system_results_t results = INITIALIZE_BLANK_BUILD_SYSTEM_RESULTS;

	//Let's first tokenize the main file
	ollie_token_stream_t stream = tokenize(main_file_name, silent_mode);

	/**
	 * If tokenizing failed there's no point in going further.
	 * We fail out here and don't even bother returning anything
	 */
	if(stream.status == STREAM_STATUS_FAILURE){
		print_build_system_message(MESSAGE_TYPE_ERROR, "Tokenzining failed. Please remedy the error and recompile", main_file_name, 0);
		num_build_system_errors++;
		results.status = BUILD_SYSTEM_STATUS_FAILURE;
		return results;
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
		results.status = BUILD_SYSTEM_STATUS_FAILURE; 
		return results;
	}

	/**
	 * Give the main dependency node a name that would never be accepted
	 * by the regular module declaration to avoid collisions
	 */
	dynamic_string_t main_dependency_node_name = dynamic_string_alloc();
	dynamic_string_set(&main_dependency_node_name, "^^MAIN_DEPENDENCY_GRAPH_NODE^^");

	//Otherwise we should be good to package this up into a dependency graph node
	dependency_graph_node_t* main_dependency_node = dependency_graph_node_alloc(&main_dependency_node_name, main_file_name, &stream, DEPENDENCY_GRAPH_NODE_TYPE_MAIN);

	//Insert this into the symtab for completeness
	insert_module(module_symtab, create_module_record(main_dependency_node));

	/**
	 * We will now run through and parse all of the dependencies that this file has. It is of course 
	 * possible that there are no dependencies, but we must do our check here. We will keep
	 * parsing the dependencies until we hit the first non-import token
	 */
	int32_t current_token_index = 0;
	lexitem_t* lookahead;

	//Run through the top of the file and process until we're done seeing imports
	while(TRUE){
		lexitem_t* lookahead = token_array_get_pointer_at(&(stream.token_stream), current_token_index);
		current_token_index++;

		//Terminal case here - we're done looking anymore
		if(lookahead->tok != IMPORT){
			break;
		}

		/**
		 * Let the helper find and possible create the dependency graph node from our
		 * import statement
		 */
		dependency_graph_node_t* dependency = parse_import_statement_and_get_dependency(&stream, main_file_directory, main_file_name, &current_token_index, silent_mode);
		if(dependency == NULL){
			print_build_system_message(MESSAGE_TYPE_ERROR, "Invalid $import directive found in file. Please review and recompile", main_file_name, 0);
			num_build_system_errors++;
			results.status = BUILD_SYSTEM_STATUS_FAILURE; 
			return results;
		}
	}

	//TODO MORE HERE WITH DEPENDENCIES

	//Package up and give back our results
	results.result_node = main_dependency_node;
	results.status = BUILD_SYSTEM_STATUS_SUCCESS;
	return results;
}


/**
 * The main and only entry point to the build system revolves around
 * us parsing dependencies and constructing them into one gigantic, unified token
 * stream. This token stream is what we will use to actually parse and construct
 * the overall CFG
 *
 *
 * TODO RETURN TYPE IS NOT ACCURATE LIKELY
 *
 * NOTE: we can *not* deallocate the dependency graph when we do this because we
 * need all of the info contained within for the rest of compilation
 */
build_system_results_t parse_dependencies_and_construct_token_stream(compiler_options_t* options, u_int8_t silent_mode){
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

	char main_file_directory[FILENAME_MAX];
	get_directory(main_file_directory, main_file_name);

	//Let the helper go out and parse through the main file and its dependencies
	build_system_results_t results = handle_main_file_tokenization(main_file_directory, main_file_name, silent_mode);

	//Destroy the reusable file searcher stream now that we're done
	destroy_token_stream(&reusable_file_searching_stream);	

	return results;
}
