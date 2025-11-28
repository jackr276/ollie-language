/**
 * Author: Jack Robbins
 *
 * This is the implementation file for the dependency graph header file of the same name
*/

#include "data_dependency_graph.h"

/**
 * Add a dependence between the dependent and the dependency
*/
void add_dependence(instruction_t* dependency, instruction_t* dependent){
	//Allocate this if we don't have it
	if(dependency->successor_instructions == NULL){
		dependency->successor_instructions = dynamic_array_alloc();
	}

	//Again allocate if we don't have
	if(dependent->predecessor_instructions == NULL){
		dependency->predecessor_instructions = dynamic_array_alloc();
	}

	//The dependent is a successor of the dependency
	dynamic_array_add(dependency->successor_instructions, dependent);

	//The dependency is a predecessor of the dependent
	dynamic_array_add(dependent->predecessor_instructions, dependency);
}


/**
 * Remove a dependence between the dependent and the dependency
*/
void remove_dependence(instruction_t* dependency, instruction_t* dependent){
	//Remove the dependent from our successors here
	dynamic_array_delete(dependency->successor_instructions, dependent);

	//Remove the dependency from the dependent's predecessors
	dynamic_array_delete(dependent->predecessor_instructions, dependency);
}


/**
 * Get the edge weight between the dependent and dependency
*/
u_int32_t get_edge_weight(instruction_t* dependency, instruction_t* dependent){
	//TODO


	return 0;
}
