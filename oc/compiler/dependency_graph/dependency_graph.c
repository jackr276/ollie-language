/**
 * Author: Jack Robbins
 * This C file contains the definitions for the APIs defined in the header file of the same name
 *
 * NOTE: there is a 1-to-1 relationship between dependency graph nodes and modules. Every module
 * has its own dependency graph node
 */

#include "dependency_graph.h"
#include <stdio.h>
#include <string.h>
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
dependency_graph_node_t* dependency_graph_node_alloc(dynamic_string_t* module_name, char* file_name, ollie_token_stream_t* stream, dependency_node_type_t node_type){
	dependency_graph_node_t* node = calloc(1, sizeof(dependency_graph_node_t));

	//Populate the unique identifier
	node->node_id = get_next_node_id();
	node->type = node_type;

	//Copy the stream over entirely through dereference
	node->token_stream = *stream;

	//Copy over the file name and the module name
	node->module_name = clone_dynamic_string(module_name);

	//Copy the filename over here 
	strncpy(node->file_name, file_name, FILENAME_MAX);

	//By default we're all unvisited
	node->visitation_status = DEPENDENCY_NODE_UNVISITED;

	//Give this back once done
	return node;
}


/**
 * Add a dependency relationship between dependant and depends_on
 */
void add_dependency(dependency_graph_node_t* dependant, dependency_graph_node_t* depends_on){
	/**
	 * Our dependant will have a new dependency, so allocate
	 * the array if we don't have it already
	 */
	if(dependant->depends_on.internal_array == NULL){
		dependant->depends_on = dynamic_array_alloc();
	}

	//Add the relationship in now
	dynamic_array_add(&(dependant->depends_on), depends_on);
}


/**
 * Deallocate the given dependency graph node
 */
void dependency_graph_node_dealloc(dependency_graph_node_t* node){
	//Deallocate the token stream
	token_array_dealloc(&(node->token_stream.token_stream));

	//Deallocate these two if they do in fact exist(the rule will check)
	dynamic_array_dealloc(&(node->depends_on));

	//Now destroy the file & module names
	dynamic_string_dealloc(&(node->module_name));

	//Finally we can free the overall node itself(all nodes are heap allocated)
	free(node);
}
