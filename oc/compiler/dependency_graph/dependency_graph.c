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
 * Create the overall control structure for a dependency graph
 */
dependency_graph_t dependency_graph_alloc(){
	dependency_graph_t graph;

	//Allocate the nodes themselves
	graph.nodes = dynamic_array_alloc();

	//These will be created later on
	graph.adjacency_matrix = NULL;
	graph.transitive_closure = NULL;

	//Initially we've got nothing in here
	graph.num_nodes = 0;

	//Give back the stack-allocated graph
	return graph;
}


/**
 * Allocate a dependency graph node on the heap. All dependency
 * graph nodes will be heap allocated
 */
dependency_graph_node_t* dependency_graph_node_alloc(dependency_graph_t* graph, dynamic_string_t* module_name, char* file_name, ollie_token_stream_t* stream, dependency_node_type_t node_type){
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

	//Add this into the dependency graph as a whole
	dynamic_array_add(&(graph->nodes), node);

	//Bump up the node count
	(graph->num_nodes)++;

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

	/**
	 * Our depends_on will have a new depended_on_by, so allocate
	 * the array if we don't have it already
	 */
	if(depends_on->depended_on_by.internal_array == NULL){
		depends_on->depended_on_by = dynamic_array_alloc();
	}

	//Add the relationship in now
	dynamic_array_add(&(depends_on->depended_on_by), dependant);
	dynamic_array_add(&(dependant->depends_on), depends_on);
}


/**
 * Create an adjacency matrix from the dependency graph. This is going to make it easier
 * for us to compute the transitive closure to check for circular dependencies
 */
u_int8_t* get_adjacency_matrix_from_dependency_graph(dependency_graph_node_t* root){
	//TODO

}



/**
 * Deallocate the given dependency graph node
 */
void dependency_graph_node_dealloc(dependency_graph_node_t* node){
	//Deallocate the token stream
	token_array_dealloc(&(node->token_stream.token_stream));

	//Deallocate these two if they do in fact exist(the rule will check)
	dynamic_array_dealloc(&(node->depends_on));
	dynamic_array_dealloc(&(node->depended_on_by));

	//Now destroy the file & module names
	dynamic_string_dealloc(&(node->module_name));

	//Finally we can free the overall node itself(all nodes are heap allocated)
	free(node);
}


/**
 * Deallocate the overall dependency graph
 */
void dependency_graph_dealloc(dependency_graph_t* graph){
	//Free the node array
	dynamic_array_dealloc(&(graph->nodes));

	//And destory these now that they're not needed
	free(graph->transitive_closure);
	free(graph->adjacency_matrix);
}
