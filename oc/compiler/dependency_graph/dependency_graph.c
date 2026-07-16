/**
 * Author: Jack Robbins
 * This C file contains the definitions for the APIs defined in the header file of the same name
 *
 * NOTE: there is a 1-to-1 relationship between dependency graph nodes and modules. Every module
 * has its own dependency graph node
 */

#include "dependency_graph.h"
#include <stdlib.h>

//Maintain a unique atomically increasing node it
static int32_t current_node_id = 0;

/**
 * Return the current node id and increment it for the next go around
 */
static inline int32_t get_next_node_id(){
	return current_node_id++;
}


/**
 * Allocate a dependency graph node on the heap. All dependency
 * graph nodes will be heap allocated
 */
dependency_graph_node_t* dependency_graph_node_alloc(ollie_token_stream_t* stream, dependency_node_type_t type){
	dependency_graph_node_t* node = calloc(1, sizeof(dependency_graph_node_t));

	//Populate the unique identifier
	node->node_id = get_next_node_id();
	node->type = type;

	//Copy the stream over entirely through dereference
	node->token_stream = *stream;

	//TODO DEPENDS ON AND DEPENDED ON BY

	//Give this back once done
	return node;
}


/**
 * Deallocate the given dependency graph node
 */
void dependency_graph_node_dealloc(dependency_graph_node_t* node){

	//
	//
	//TODO
	//
	//
}
