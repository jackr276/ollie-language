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
data_depency_graph_t dependency_graph_alloc(){
	//Stack allocate
	data_depency_graph_t graph;

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
*/
void add_dependence(instruction_t* depends_on, instruction_t* target){
	//Allocate this if we don't have it
	if(depends_on->successor_instructions == NULL){
		depends_on->successor_instructions = dynamic_array_alloc();
	}

	//Again allocate if we don't have
	if(target->predecessor_instructions == NULL){
		target->predecessor_instructions = dynamic_array_alloc();
	}

	//The dependent is a successor of the dependency
	dynamic_array_add(depends_on->successor_instructions, target);

	//The dependency is a predecessor of the dependent
	dynamic_array_add(target->predecessor_instructions, depends_on);
}


/**
 * Remove a dependence between the dependent and the dependency
*/
void remove_dependence(instruction_t* depends_on, instruction_t* target){
	//Remove the dependent from our successors here
	dynamic_array_delete(depends_on->successor_instructions, target);

	//Remove the dependency from the dependent's predecessors
	dynamic_array_delete(target->predecessor_instructions, depends_on);
}


/**
 * Print out the entirety of the data dependence graph
 */
void print_data_dependence_graph(FILE* output, instruction_t** graph, u_int32_t num_nodes){
	//Run through all of the nodes
	for(u_int32_t i = 0; i < num_nodes; i++){
		//Extract it
		instruction_t* instruction = graph[i];

		fprintf(output, "================================================\n");

		//Print the instruction
		fprintf(output, "Instruction:\n");
		print_instruction(stdout, instruction, PRINTING_VAR_IN_INSTRUCTION);
		//Now show what it depends on
		fprintf(output, "Depends on:\n");

		//Run through all of what we depend on
		for(u_int16_t j = 0; instruction->predecessor_instructions != NULL && j < instruction->predecessor_instructions->current_index; j++){
			//Print out the predecessor
			instruction_t* predecessor = dynamic_array_get_at(instruction->predecessor_instructions, j);
			print_instruction(stdout, predecessor, PRINTING_VAR_IN_INSTRUCTION);
		}

		fprintf(output, "================================================\n");
	}
}


/**
 * Free a data dependency graph. This also includes freeing all 
 * of the nodes and resultant lists
 */
void dependency_graph_dealloc(data_depency_graph_t* graph){
	//Run through all of the nodes
	for(u_int16_t i = 0; i < graph->current_index; i++){
		//Free the dynamic array
		dynamic_array_dealloc(graph->nodes[i].neighbors);
	}

	//Now free the overall array
	free(graph->nodes);
}
