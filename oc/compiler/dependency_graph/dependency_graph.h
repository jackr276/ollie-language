/**
 * Author: Jack Robbins
 * This header file exposes the needed APIs for other files to use the dependency graph
 */

#ifndef DEPENDENCY_GRAPH_H
#define DEPENDENCY_GRAPH_H

#include "../utils/dynamic_array/dynamic_array.h"
#include "../lexer/lexer.h"
#include <sys/types.h>

typedef struct dependency_graph_node_t dependency_graph_node_t;
typedef struct dependency_results_t dedendency_results_t;

/**
 * Is the given node the main node or is it a dependency
 */
typedef enum {
	DEPENDENCY_GRAPH_NODE_TYPE_MAIN,
	DEPENDENCY_GRAPH_NODE_TYPE_DEPENDENCY,
} dependency_node_type_t;


/**
 * For our visitation of these nodes - we'll
 * maintain states like this
 */
typedef enum {
	DEPENDENCY_NODE_UNVISITED,
	DEPENDENCY_NODE_IN_PROGRESS,
	DEPENDENCY_NODE_FULLY_PROCESSED
} dependency_node_visitation_status_t;


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
	//What does this node itself depend on
	dynamic_array_t depends_on;
	//Who are we depended on by
	dynamic_array_t depended_on_by;
	//Unique node ID
	int32_t node_id;
	//The type of node this is
	dependency_node_type_t type;
	//What is our visitation status
	dependency_node_visitation_status_t visitation_status;
	//Less important - the name of the actaul file
	char file_name[FILENAME_MAX];
};

/**
 * Allocate a dependency graph node on the heap. All dependency
 * graph nodes will be heap allocated
 */
dependency_graph_node_t* dependency_graph_node_alloc(dynamic_string_t* module_name, char* file_name, ollie_token_stream_t* stream, dependency_node_type_t node_type);

/**
 * Add a dependency relationship between dependant and depends_on
 */
void add_dependency(dependency_graph_node_t* dependant, dependency_graph_node_t* depends_on);

/**
 * Deallocate the given dependency graph node
 */
void dependency_graph_node_dealloc(dependency_graph_node_t* node);

#endif /* DEPENDENCY_GRAPH_H */
