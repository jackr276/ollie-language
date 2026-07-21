/**
 * Author: Jack Robbins
 * This header file defines the external APIs for the Ollie build system
 */

#ifndef BUILD_SYSTEM_H
#define BUILD_SYSTEM_H

#include "../lexer/lexer.h"
#include "../utils/utility_structs.h"
#include "../dependency_graph/dependency_graph.h"
#include <sys/types.h>

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
struct build_system_results_t {
	dynamic_array_t compilation_order;
	dependency_graph_node_t* result_node;
	build_system_status_t status;
	int32_t num_errors;
};


/**
 * The main and only entry point to the build system revolves around
 * us parsing dependencies and constructing them into one gigantic, unified token
 * stream. This token stream is what we will use to actually parse and construct
 * the overall CFG
 */
build_system_results_t construct_build_order(compiler_options_t* options, u_int8_t silent_mode);

#endif /* BUILD_SYSTEM_H */
