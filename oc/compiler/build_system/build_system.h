/**
 * Author: Jack Robbins
 * This header file defines the external APIs for the Ollie build system
 */

#ifndef BUILD_SYSTEM_H
#define BUILD_SYSTEM_H

#include "../lexer/lexer.h"
#include "../utils/utility_structs.h"
#include "../utils/dynamic_array/dynamic_array.h"
#include <sys/types.h>

typedef struct build_graph_node_t build_graph_node_t;

/**
 * Define a node for our build graph. Every file will get
 * a build graph node that will contain it token stream
 * and record its dependencies
 */
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
 * The main and only entry point to the build system revolves around
 * us parsing dependencies and constructing them into one gigantic, unified token
 * stream. This token stream is what we will use to actually parse and construct
 * the overall CFG
 */
ollie_token_stream_t parse_dependencies_and_construct_token_stream(compiler_options_t* options, u_int8_t silent_mode);

#endif /* BUILD_SYSTEM_H */
