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
 * Is the given node the main node or is it a dependency
 */
typedef enum {
	DEPENDENCY_GRAPH_NODE_TYPE_MAIN,
	DEPENDENCY_GRAPH_NODE_TYPE_DEPENDENCY,
} dependency_node_type_t;

/**
 * Define a node for our build graph. Every file will get
 * a build graph node that will contain it token stream
 * and record its dependencies
 */
struct dependency_graph_node_t {
	//The token stream for the file in question
	ollie_token_stream_t token_stream;
	//The module name that we have
	dynamic_string_t module_name;
	//Unique node ID
	int32_t node_id;
	//The type of node this is
	dependency_node_type_t type;
	//Less important - the name of the actaul file
	char file_name[FILENAME_MAX];
};

/**
 * Allocate a dependency graph node on the heap. All dependency
 * graph nodes will be heap allocated
 */
dependency_graph_node_t* dependency_graph_node_alloc(dynamic_string_t* module_name, char* file_name, ollie_token_stream_t* stream, dependency_node_type_t node_type);

/**
 * Deallocate the given dependency graph node
 */
void dependency_graph_node_dealloc(dependency_graph_node_t* node);

#endif /* DEPENDENCY_GRAPH_H */
