/**
 * Author: Jack Robbins
 * This file defines the Ollie build system used for dependency management in Ollie
 */

#include "build_system.h"
#include "../utils/error_management.h"
#include "../utils/constants.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

//Helper that will let us initialize a wiped out version
#define INITIALIZE_BLANK_BUILD_SYSTEM_RESULTS {NULL, BUILD_SYSTEM_STATUS_FAILURE}

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
 * Handle the parsing of an import statement. Note that there are two different things that we
 * can see for an import statement:
 * 	1.) import "value"; <- double quotes tell the compiler to look in the ./value path. This is used
 * 		for local imports
 * 	2.) import <value>; <- angle bracktes tell the compiler to look in the system library "/usr/lib/ollie/" for the
 * 		given module
 *
 * Returns SUCCESS if this worked, FAILURE if not. We will also be populating the pre-allocated "file_to_import" buffer
 * with the full path of our filename for the caller to process
 */
static u_int8_t parse_import_statement(ollie_token_stream_t* stream, char* file_to_import, int32_t* current_index){

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
static build_system_results_t handle_main_file_tokenization(char* main_file_name, u_int8_t silent_mode){
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

	//Otherwise we should be good to package this up into a dependency graph node
	dependency_graph_node_t* main_dependency_node = dependency_graph_node_alloc(&stream, DEPENDENCY_GRAPH_NODE_TYPE_MAIN);

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
		 * Otherwise we have seen an import statement. We will need to parse
		 * it and determine, through a file search, what the module is that we
		 * are after here
		 */
		char file_name[FILENAME_MAX];
		memset(file_name, 0, FILENAME_MAX);

		//Let the helper parse through this
		u_int8_t result = parse_import_statement(&stream, file_name, &current_token_index);
		if(result == FAILURE){
			print_build_system_message(MESSAGE_TYPE_ERROR, "Invalid $import directive found in file. Please review and recompile", main_file_name, 0);
			num_build_system_errors++;
			results.status = BUILD_SYSTEM_STATUS_FAILURE; 
			return results;
		}
	}



	//TODO MORE HERE WITH DEPENDENCIES
	//
	

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
 */
build_system_results_t parse_dependencies_and_construct_token_stream(compiler_options_t* options, u_int8_t silent_mode){
	/**
	 * The actual main file itself is all that the user provides here. The build system will
	 * then crawl through the dependencies in the main file and each of those files recursively
	 * until all dependencies are exhausted. We return *one* unified token stream in the end
	 */
	char* main_file_name = options->file_name;

	//Let the helper go out and parse through the main file and its dependencies
	build_system_results_t results = handle_main_file_tokenization(main_file_name, silent_mode);

	return results;
}
