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
 * Define a node for our build graph. Every file will get
 * a build graph node that will contain it token stream
 * and record its dependencies
 */
typedef struct build_graph_node_t build_graph_node_t;
struct build_graph_node_t {
	//The token stream for the file in question
	ollie_token_stream_t token_stream;
	//TODO MAY UPDATE AS NEEDS ARISE
	dynamic_array_t depends_on;
	dynamic_array_t depended_on_by;
	//Unique node ID
	u_int32_t current_node_id;
	//Name of the file that this node came from
	char* file_name;
};


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
 * The main and only entry point to the build system revolves around
 * us parsing dependencies and constructing them into one gigantic, unified token
 * stream. This token stream is what we will use to actually parse and construct
 * the overall CFG
 *
 *
 * TODO $module $endmodule IDEA like we've done before with macros
 *
 * Example:
 *
 * $module "system.printer"
 *
 * //dependencies as need be
 *
 * //Actual code
 *
 * $endmodule
 *
 * Can be as big or as small as you want
 *
 * Each module should generate its own unique token stream - each module must be independently operable
 *
 * Many things to check with global vars, functions etc that need testing
 *
 * LATER IDEA NOT RIGHT NOW - submodules maybe
 *
 * BUT THEN HOW DO WE HANDLE FILES? How are we going to know which files to pull in and
 * compile? If one file can have multiple modules then how do we find the modules on the first
 * go around?
 *
 *
 * Common pattern:
 *
 * search in two areas:
 * 	The directory that we are being run in and all subdirectories under it
 * 	The common library (/usr/lib/ollie/)
 *
 * We will search through here in an attempt to find our module
 *
 * NOTE: OLLIE WILL NEVER SUPPORT INCREMENTAL BUILDS OR CACHING PRECOMP'D FILES
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
