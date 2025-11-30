/**
 * Author: Jack Robbins
 *
 * This is the implementation file for the dependency graph header file of the same name
*/

#include "data_dependency_graph.h"
#include <stdio.h>
#include <sys/types.h>

//We find that basic blocks in many cases usually have less than 15 nodes
#define DEFAULT_DATA_DEPENDENCY_GRAPH_NODE_COUNT 15


/**
 * Create a data dependency graph. Parent struct
 * is stack allocated
 */
data_dependency_graph_t dependency_graph_alloc(){
	//Stack allocate
	data_dependency_graph_t graph;

	//Allocate the nodes
	graph.nodes = calloc(DEFAULT_DATA_DEPENDENCY_GRAPH_NODE_COUNT, sizeof(data_dependency_graph_node_t));

	//Initially we have the default amount
	graph.current_max_index = DEFAULT_DATA_DEPENDENCY_GRAPH_NODE_COUNT;

	//And the index
	graph.current_index = 0;

	//And give it back
	return graph;
}


/**
 * Add a dependence between the dependent and the dependency
 *
 * NOTE: This assumes that the graph *already* has nodes for the target and depends_on instructions
 * created and at the ready
*/
void add_dependence(data_dependency_graph_t* graph, instruction_t* target, instruction_t* depends_on){
}


/**
 * Print out the entirety of the data dependence graph
 */
void print_data_dependence_graph(FILE* output, data_dependency_graph_t* graph){
	//Run through all of the nodes
	for(u_int32_t i = 0; i < graph->current_index; i++){
		//Extract the address
		data_dependency_graph_node_t* node = &(graph->nodes[i]);

		fprintf(output, "================================================\n");

		//Print the instruction
		fprintf(output, "Instruction:\n");
		print_instruction(stdout, node->instruction, PRINTING_VAR_IN_INSTRUCTION);
		//Now show what it depends on
		fprintf(output, "Depends on:\n");

		//Run through all of what we depend on
		for(u_int16_t j = 0; j < node->neighbors->current_index; j++){
			//Print out the predecessor
			data_dependency_graph_node_t* predecessor = dynamic_array_get_at(node->neighbors, j);
			print_instruction(stdout, predecessor->instruction, PRINTING_VAR_IN_INSTRUCTION);
		}

		fprintf(output, "================================================\n");
	}
}


/**
 * Free a data dependency graph. This also includes freeing all 
 * of the nodes and resultant lists
 */
void dependency_graph_dealloc(data_dependency_graph_t* graph){
	//Run through all of the nodes
	for(u_int16_t i = 0; i < graph->current_index; i++){
		//Free the dynamic array
		dynamic_array_dealloc(graph->nodes[i].neighbors);
	}

	//Now free the overall array
	free(graph->nodes);
}
