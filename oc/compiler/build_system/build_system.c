/**
 * Author: Jack Robbins
 * This file defines the Ollie build system used for dependency management in Ollie
 */

#include "build_system.h"
#include <sys/types.h>



/**
 * The main and only entry point to the build system revolves around
 * us parsing dependencies and constructing them into one gigantic, unified token
 * stream. This token stream is what we will use to actually parse and construct
 * the overall CFG
 */
ollie_token_stream_t parse_dependencies_and_construct_token_stream(compiler_options_t* options, u_int8_t silent_mode){

	//TODO NOT AT ALL WORKING YET
	ollie_token_stream_t token_stream = tokenize(options->file_name, silent_mode);

	return token_stream;
}
