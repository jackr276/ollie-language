/**
 * Author: Jack Robbins
 * This file defines the Ollie build system used for dependency management in Ollie
 */

#include "build_system.h"
#include "../utils/dynamic_array/dynamic_array.h"
#include <sys/types.h>

//Maintain a unique atomic node it
static u_int32_t current_node_id = 0;

/**
 * Basic enum for the build system status
 * That we'll pass around
 */
typedef enum {
	BUILD_SYSTEM_STATUS_FAILURE,
	BUILD_SYSTEM_STATUS_SUCCESS
} build_system_status_t;

/**
 * Similar to how CFG results work, we will pass around a struct
 * that contains our status and our created build graph nodes
 * as a return value
 */
typedef struct build_system_results_t build_system_results_t;

/**
 * For each file, we must:
 * 	1.) Perform the tokenization for that file only
 * 	2.) Parse the header values and determine if there are dependencies
 * 	3.) Construct a build dependency graph node containing this file name and its token stream
 * 	4.) For each dependency, perform this same process on it
 * TODO VALIDATIONS, ETC
 * 	
 */
static void handle_file_dependencies_and_tokenization(char* file_name, u_int8_t silent_mode){

	//TODO VOID FOR NOW
}


/**
 * The main file in ollie is the file that the user has passed in via the -f option
 * to the ollie compiler
 *
 * The main file is a special case because it may *not* be a module. We will need to
 * validate that the user is not attempting to make this file into a module
 */
static void handle_main_file_tokenization(char* main_file_name, u_int8_t silent_mode){
	//Let's first tokenize the main file
	ollie_token_stream_t stream = tokenize(main_file_name, silent_mode);

	/**
	 * If tokenizing failed there's no point in going further.
	 * We fail out here and don't even bother returning anything
	 */
	if(stream.status == STREAM_STATUS_FAILURE){

	}



}


/**
 * The main and only entry point to the build system revolves around
 * us parsing dependencies and constructing them into one gigantic, unified token
 * stream. This token stream is what we will use to actually parse and construct
 * the overall CFG
 */
ollie_token_stream_t parse_dependencies_and_construct_token_stream(compiler_options_t* options, u_int8_t silent_mode){
	/**
	 * The actual main file itself is all that the user provides here. The build system will
	 * then crawl through the dependencies in the main file and each of those files recursively
	 * until all dependencies are exhausted. We return *one* unified token stream in the end
	 */
	char* main_file_name = options->file_name;

	//TODO NOT AT ALL WORKING YET
	ollie_token_stream_t token_stream = tokenize(options->file_name, silent_mode);

	return token_stream;
}
