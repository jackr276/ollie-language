/**
 * Author: Jack Robbins
 *
 * This is the implementation file for the dependency graph header file of the same name
*/

#include "data_dependency_graph.h"

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
 * Get the edge weight between the dependent and dependency
*/
u_int32_t get_edge_weight(instruction_t* dependency, instruction_t* dependent){
	//TODO


	return 0;
}
