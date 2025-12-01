/**
 * Author: Jack Robbins
 *
 * This is the implementation file for the dependency graph header file of the same name
*/

#include "data_dependency_graph.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

/**
 * Create a data dependency graph. Parent struct
 * is stack allocated
 */
data_dependency_graph_t dependency_graph_alloc(u_int32_t num_nodes){
	//Stack allocate
	data_dependency_graph_t graph;

	//Allocate the nodes
	graph.nodes = calloc(num_nodes, sizeof(data_dependency_graph_node_t*));

	//Initially we have the default amount
	graph.node_count = num_nodes;

	//And the index
	graph.current_index = 0;

	//And give it back
	return graph;
}


/**
 * Reset the visited status for the graph
 */
static void reset_dependency_graph_visited_status(data_dependency_graph_t* graph){
	//Run through them all
	for(u_int32_t i = 0; i < graph->node_count; i++){
		//Clear it out
		graph->nodes[i]->visited = FALSE;
	}
}


/**
 * Helper method that visits a node in the topological sort
 *
 * Algorithm:
 * 	if node is visited then
 * 		return
 *
 * 	for each dependency of node "m" do:
 * 		visit(m)
 *
 * 	mark node as visited
 * 	add node to *head* of list
 */
static void topological_sort_visit_node(data_dependency_graph_node_t* node, data_dependency_graph_node_t** sorted, u_int32_t* sorted_index){
	//Base case, we're visited already
	if(node->visited == TRUE){
		return;
	}

	//Run through all of the dependencies
	for(u_int16_t i = 0; i < node->neighbors->current_index; i++){
		//Recursive visit call
		topological_sort_visit_node(dynamic_array_get_at(node->neighbors, i), sorted, sorted_index);
	}

	//Now that we're here, the node is truly visited
	node->visited = TRUE;

	//Insert at the head
	sorted[*sorted_index] = node;
	
	//Push it up
	(*sorted_index)++;
}


/**
 * Perform an inplace topological sort on the graph. This is a necessary
 * step before we attempt to find any priorities. This sort happens *inplace*,
 * meaning that it will modify the internal array of the graph
 *
 * Basic algorithm:
 * 	for each node in the node list n
 * 		if node was visited:
 * 			continue
 * 		else:
 * 			visit(n)
 */
void inplace_topological_sort(data_dependency_graph_t* graph){
	//Let's first create a new memory area that we can use to store
	//the topologically sorted version
	//NOTE: for efficiency sake, this list will actually be in reverse order(head is at the last index). We
	//will reverse it at the end
	data_dependency_graph_node_t** sorted_in_reverse = calloc(graph->node_count, sizeof(data_dependency_graph_node_t*));
	//We need to track this too
	u_int32_t sorted_current_index = 0;

	//Let's first wipe the visited status just in case something else ran before us
	reset_dependency_graph_visited_status(graph);

	//Run through all of the nodes
	for(u_int16_t i = 0; i < graph->node_count; i++){
		//It's already visited, so skip it
		if(graph->nodes[i]->visited == TRUE){
			continue;
		}

		//Otherwise, we visit the node
		topological_sort_visit_node(graph->nodes[i], sorted_in_reverse, &sorted_current_index);
	}

	//Once we've done that, we'll actually reverse the list in sorted by going from back-to-front
	//and replacing the graph's list
	for(int16_t i = sorted_current_index - 1; i >= 0; i--){
		//Grab the node out
		data_dependency_graph_node_t* node = sorted_in_reverse[i];

		//Grab the in-order index
		u_int16_t index = graph->node_count - i - 1;

		//Store the index for later on
		node->index = index;

		//Loading in backwards here, so the graph nodes
		//will be at their node count minus i
		graph->nodes[index] = node;
	}

	//Once we're here, we can scrap the reverse sorted list
	free(sorted_in_reverse);
}


/**
 * Create and add a node for a given instruction
 */
void add_data_dependency_node_for_instruction(data_dependency_graph_t* graph, instruction_t* instruction){
	//Allocate it
	data_dependency_graph_node_t* node = calloc(1, sizeof(data_dependency_graph_node_t));

	//Store the instruction pointer
	node->instruction = instruction;

	//Allocate the dynamic array
	node->neighbors = dynamic_array_alloc();

	//Populate the cycle count
	node->cycles_to_complete = get_estimated_cycle_count(instruction);

	//Add it into the list
	graph->nodes[graph->current_index] = node;

	//And now all we need to do is push the index value up
	graph->current_index++;
}


/**
 * Get the leaves of the data DAG. The leaves are simply instructions that have no dependencies
 */
dynamic_array_t* get_data_dependency_graph_leaf_nodes(data_dependency_graph_t* graph){
	//Create the dynamic array first
	dynamic_array_t* leaves = dynamic_array_alloc();

	//Done via a simple linear scan
	for(u_int32_t i = 0; i < graph->current_index; i++){
		//We want nothing that we rely on here
		if(graph->nodes[i]->relies_on_count == 0){
			dynamic_array_add(leaves, graph->nodes[i]);
		}
	}

	return leaves;
}


/**
 * Get the roots of the data DAG. The roots are simply instructions that have nothing
 * else depends on. There will often be more than one root
 */
dynamic_array_t* get_data_dependency_graph_root_nodes(data_dependency_graph_t* graph){
	//Create the dynamic array first
	dynamic_array_t* roots = dynamic_array_alloc();

	//Done via a simple linear scan
	for(u_int32_t i = 0; i < graph->current_index; i++){
		//For a root, we want to be relied on by nothing
		if(graph->nodes[i]->relied_on_by_count == 0){
			dynamic_array_add(roots, graph->nodes[i]);
		}
	}

	return roots;
}


/**
 * Find the dependency graph node for a given instruction
 *
 * To do this, we can perform a simple linear lookup and compare the memory addresses
 *
 * Returns NULL if the instruction is not in there. However, if we're using this rule, the instruction
 * should always be in there
 */
data_dependency_graph_node_t* get_dependency_node_for_given_instruction(data_dependency_graph_t* graph, instruction_t* instruction){
	//Run through all of the nodes
	for(u_int32_t i = 0; i < graph->node_count; i++){
		if(graph->nodes[i]->instruction == instruction){
			return graph->nodes[i];
		}
	}

	//Otherwise if we get to here, we didn't find it so we return NULL
	return NULL;
}


/**
 * Compute the longest path on a topologically sorted graph between the node and the root
 *
 * Pseudocde(Source S, Root R, DAG D(in topological order))
 *
 * if root has no depencies:
 * 	return 0
 *
 * for each vertex V in D:
 * 	dist[V] = -INF
 *
 * dist[S] = 0
 *
 * for each node U in D
 * 	if dist[U] == -INF:
 * 		continue //unreachable
 * 	for each edge U -> V with weight w:
 * 		if dist[U] + W < dist[V]:
 * 			dist[V] = dist[U] + W
 *
 * 	return dist[R] //dist[R] holds the longest path to R
 *
 */
static int32_t compute_longest_path_to_root_node(data_dependency_graph_t* graph, data_dependency_graph_node_t* start, data_dependency_graph_node_t* root){
	//If this root has no dependencies, then we can just get out
	//instead of going through the trouble. This can potentially happen
	//for jmps that end a block
	if(root->relies_on_count == 0){
		return 0;
	}

	//We will have a distances array for all of our distances. These are indexed
	//by the graph's index itself
	int32_t* vertices = calloc(graph->current_index, sizeof(int32_t));
	
	//Make them all INT_MIN except for our source
	for(u_int16_t i = 0; i < graph->current_index; i++){
		vertices[i] = INT_MIN;
	}

	//With the exception of our given vertex. This one we know the
	//distance is 0
	vertices[start->index] = 0;

	//Initialize our longest path
	int32_t longest_path = 0;


	//Scrap the vertices array now that we're done
	free(vertices);

	return longest_path;
}



/**
 * Find the priority for a given node in the dependency graph D
 *
 * The priority is found by finding the longest weighted path from the node to any root in D.
 *
 * Pseudocode:
 * 	longest_path = 0
 *
 * 	for every root in graph D
 * 		candidate = compute_longest_path(node, root_node)
 *
 * 		if candidate > longest_path:
 * 			longest_path = candidate
 * 		
 * By the end, we will have found the longest path
 */
static int32_t compute_longest_weighted_path_heuristic_for_node(data_dependency_graph_t* graph, data_dependency_graph_node_t* node, dynamic_array_t* roots){
	//If this node is already a root, we can just return 0, there is no path to speak of
	if(node->relied_on_by_count == 0){
		return 0;
	}

	//Initialize this to have the longest path be at 0
	int32_t longest_path = 0;
	//Potential longest path
	int32_t candidate;

	//Run through every single node
	for(u_int16_t i = 0; i < roots->current_index; i++){
		//Extract the root node
		data_dependency_graph_node_t* root_node = dynamic_array_get_at(roots, i);

		//Let the helper compute it
		candidate = compute_longest_path_to_root_node(graph, node, root_node);

		//Did we beat the current longest path? If so then
		//we are the longest path
		if(candidate > longest_path){
			longest_path = candidate;
		}
	}

	//Give back whatever our longest path was
	return longest_path;
}


/**
 * Find the priority for all nodes in the dependency graph D. This is done
 * internally using the longest path between a given node and a root
 */
void compute_priorities_for_all_nodes(data_dependency_graph_t* graph){
	//Extract the graph's roots
	dynamic_array_t* roots = get_data_dependency_graph_root_nodes(graph);

	//We need to have the graph topologically sorted
	inplace_topological_sort(graph);

	//Run through every single node in the graph
	for(u_int16_t i = 0; i < graph->current_index; i++){
		//Compute the priority for the given node
		graph->nodes[i]->priority = compute_longest_weighted_path_heuristic_for_node(graph, graph->nodes[i], roots);
	}

	//We're done with the roots so scrap them
	dynamic_array_dealloc(roots);
}


/**
 * Add a dependence between the dependent and the dependency
 *
 * NOTE: This assumes that the graph *already* has nodes for the target and depends_on instructions
 * created and at the ready
*/
void add_dependence(data_dependency_graph_t* graph, instruction_t* target, instruction_t* depends_on){
	//Extract both of their nodes from the graph's lists
	data_dependency_graph_node_t* target_node = get_dependency_node_for_given_instruction(graph, target);
	data_dependency_graph_node_t* depends_on_node = get_dependency_node_for_given_instruction(graph, depends_on);

	//This is a hard failure if we ever get here
	if(target_node == NULL || depends_on_node == NULL){
		printf("Fatal internal compiler error: attempt to add dependence for nonexistent instruction DAG nodes\n");
		exit(1);
	}

	//The target now depends on one more thing
	target_node->relies_on_count++;

	//The dependency node now has one more thing relying on it
	depends_on_node->relied_on_by_count++;

	//Now we link them together in the list. This will be added to the "depends_on" node's list because the list
	//is a "from->to" type list. We have a dependency connection from the depends_on node to the target
	dynamic_array_add(depends_on_node->neighbors, target_node);
}


/**
 * Print out the entirety of the data dependence graph
 */
void print_data_dependence_graph(FILE* output, data_dependency_graph_t* graph){
	//Run through all of the nodes
	for(u_int32_t i = 0; i < graph->current_index; i++){
		//Extract the address
		data_dependency_graph_node_t* node = graph->nodes[i];

		fprintf(output, "================================================\n");

		//Print the instruction
		fprintf(output, "Instruction: ");
		print_instruction(stdout, node->instruction, PRINTING_VAR_IN_INSTRUCTION);
		//Now show what it depends on
		fprintf(output, "Depended on by: [\n");

		//Run through all of what we depend on
		for(u_int16_t j = 0; j < node->neighbors->current_index; j++){
			//Print out the successor 
			data_dependency_graph_node_t* successor = dynamic_array_get_at(node->neighbors, j);
			print_instruction(stdout, successor->instruction, PRINTING_VAR_IN_INSTRUCTION);
		}

		printf("]\n");

		fprintf(output, "================================================\n");
	}
}


/**
 * Free a data dependency graph. This also includes freeing all 
 * of the nodes and resultant lists
 */
void dependency_graph_dealloc(data_dependency_graph_t* graph){
	//Run through all of the nodes
	for(u_int16_t i = 0; i < graph->node_count; i++){
		//Grab a pointer to this
		data_dependency_graph_node_t* node = graph->nodes[i];

		//Free the dynamic array
		dynamic_array_dealloc(node->neighbors);

		//Now deallocate the node itself
		free(node);
	}

	//Now free the overall array of nodes
	free(graph->nodes);
}
