/**
 * Author: Jack Robbins
 *
 * This is the implementation file for the dependency graph header file of the same name
*/

#include "data_dependency_graph.h"
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


	//TODO
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
		fprintf(output, "Depends on: [\n");

		//Run through all of what we depend on
		for(u_int16_t j = 0; j < node->neighbors->current_index; j++){
			//Print out the predecessor
			data_dependency_graph_node_t* predecessor = dynamic_array_get_at(node->neighbors, j);
			print_instruction(stdout, predecessor->instruction, PRINTING_VAR_IN_INSTRUCTION);
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
