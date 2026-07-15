/**
 * Author: Jack Robbins
 * This header file exposes the needed APIs for other files to use the dependency graph
 */

#ifndef DEPENDENCY_GRAPH_H
#define DEPENDENCY_GRAPH_H

#include "../utils/dynamic_array/dynamic_array.h"
#include "../lexer/lexer.h"

typedef struct dependency_graph_node_t dependency_graph_node_t;

/**
 * Define a node for our build graph. Every file will get
 * a build graph node that will contain it token stream
 * and record its dependencies
 */
struct dependency_graph_node_t {
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
 * Allocate a dependency graph node on the heap. All dependency
 * graph nodes will be heap allocated
 */
dependency_graph_node_t* dependency_graph_node_alloc();

/**
 * Deallocate the given dependency graph node
 */
void dependency_graph_node_dealloc(dependency_graph_node_t* node);

#endif /* DEPENDENCY_GRAPH_H */
