/**
 * Author: Jack Robbins
 *
 * This file contains the implementations of the APIs
 * defined in the interference graph header file
*/

#include "interference_graph.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "../utils/constants.h"


/**
 * Allocate an interference graph. The graph itself should be stack allocated,
 * this will only serve to allocate the internal nodes
*/
static interference_graph_t* interference_graph_alloc(u_int16_t live_range_count){
	//Allocate it
	interference_graph_t* graph = calloc(1, sizeof(interference_graph_t));

	//Mark this first
	graph->live_range_count = live_range_count;

	//Since this is a 2-way graph, we'll need to allocate enough space for a 2-way table
	graph->nodes = calloc(live_range_count * live_range_count, sizeof(u_int8_t));

	//and we're all set
	return graph;
}


/**
 * Mark that live ranges a and b interfere. This function does not impact the graph at all
 */
void add_interference(live_range_t* a, live_range_t* b){
	//If these are the exact same live range, they can't interfere with eachother 
	//so we'll skip this
	if(a == b){
		return;
	}

	//Stack pointer - this never interferes with anything
	if(a->reg.gen_purpose == RSP || b->reg.gen_purpose == RSP){
		return;
	}

	//Add b to a's neighbors if it's not already there
	if(dynamic_array_contains(&(a->neighbors), b) == NOT_FOUND){
		dynamic_array_add(&(a->neighbors), b);
	}

	//Add a to b's neighbors if it's not already there
	if(dynamic_array_contains(&(b->neighbors), a) == NOT_FOUND){
		dynamic_array_add(&(b->neighbors), a);
	}

	//Reset their degree values
	a->degree = a->neighbors.current_index;
	b->degree = b->neighbors.current_index;
}


/**
 * Mark that live ranges a and b do not interfere. This can be used if an interference
 * relation is removed
 */
void remove_interference(interference_graph_t* graph, live_range_t* a, live_range_t* b){
	//To add the interference we'll first need to calculate the offsets for both
	//b's and a's version
	u_int16_t offset_a_b = a->interference_graph_index * graph->live_range_count + b->interference_graph_index;
	u_int16_t offset_b_a = b->interference_graph_index * graph->live_range_count + a->interference_graph_index;

	//Now we'll go to the adjacency matrix and add this in
	graph->nodes[offset_a_b] = FALSE;
	graph->nodes[offset_b_a] = FALSE;

	//And we'll remove them from eachother's adjacency lists
	dynamic_array_delete(&(a->neighbors), b);
	dynamic_array_delete(&(b->neighbors), a);

	//Reset their degree values
	a->degree = a->neighbors.current_index;
	b->degree = b->neighbors.current_index;
}


/**
 * Coalesce a live range with another one. This will have the effect of everything in
 * said live range becoming as one. The only live range that will survive following this 
 * is the target. Once this is done, there should be *no* variables that point to the
 * caolescee at all
 */
void coalesce_live_ranges(interference_graph_t* graph, live_range_t* target, live_range_t* coalescee){
	//All of these variables now belong to the target
	for(u_int16_t i = 0; i < coalescee->variables.current_index; i++){
		//Grab it out
		three_addr_var_t* new_var = dynamic_array_get_at(&(coalescee->variables), i);

		//Add it into the target's variables
		dynamic_array_add(&(target->variables), new_var);

		//Update the associated live range to be the target
		new_var->associated_live_range = target;
	}

	//Clone this because we will be messing with it
	dynamic_array_t clone = clone_dynamic_array(&(coalescee->neighbors));

	//Go through all neighbors of the coalescee
	for(u_int16_t i = 0; i < clone.current_index; i++){
		//Extract the neighbor out
		live_range_t* neighbor = dynamic_array_get_at(&clone, i);

		//The neighbor and the coalescee no longer count
		remove_interference(graph, neighbor, coalescee);

		//The target and the neighbor are now interfering
		add_interference(graph, target, neighbor);
	}

	/**
	 * If the target has no register, we will be taking the coalescee's register
	 *
	 * If the target has a register and the source has no/the same register, no
	 * action is needed
	 */
	if(target->reg.gen_purpose == NO_REG_GEN_PURPOSE){
		target->reg = coalescee->reg;
	}

	//If the target already has no function parameter order,
	//we can copy over the coalescee's
	if(target->function_parameter_order == 0){
		target->function_parameter_order = coalescee->function_parameter_order;
	}

	//We now add the spill cost of the one that was coalesced to the target
	target->spill_cost += coalescee->spill_cost;

	//Increment the assignment count here
	target->assignment_count += coalescee->assignment_count;
}


/**
 * Build the interference graph from the adjacency lists
 */
interference_graph_t* construct_interference_graph_from_adjacency_lists(dynamic_array_t* live_ranges){
	//Run through and give everything an index
	for(u_int16_t i = 0; i < live_ranges->current_index; i++){
		live_range_t* range = dynamic_array_get_at(live_ranges, i);
		range->interference_graph_index = i;
	}
	
	//Now we'll create the actual graph itself
	interference_graph_t* graph = interference_graph_alloc(live_ranges->current_index);

	//Once we have that, we're ready to add all of our interferences
	for(u_int16_t i = 0; i < live_ranges->current_index; i++){
		//Grab it out
		live_range_t* range = dynamic_array_get_at(live_ranges, i);

		//Now we iterate through it's adjacency list
		for(u_int16_t j = 0; j < range->neighbors.current_index; j++){
			//Grab it out
			live_range_t* neighbor = dynamic_array_get_at(&(range->neighbors), j);

			//Calculate the offsets
			u_int16_t offset_a_b = range->interference_graph_index * graph->live_range_count + neighbor->interference_graph_index;
			u_int16_t offset_b_a = neighbor->interference_graph_index * graph->live_range_count + range->interference_graph_index;

			//Now we'll go to the adjacency matrix and add this in
			graph->nodes[offset_a_b] = TRUE;
			graph->nodes[offset_b_a] = TRUE;
		}
	}

	//Give the graph back
	return graph;
}


/**
 * Check whether or not two live ranges interfere
 *
 * Returns true if yes, false if no
 */
u_int8_t do_live_ranges_interfere(interference_graph_t* graph, live_range_t* a, live_range_t* b){
	//By default, everything interferes with itself
	if(a == b){
		return TRUE;
	}

	//To determine this, we'll first need the offset
	u_int16_t offset_a_b = a->interference_graph_index * graph->live_range_count + b->interference_graph_index;

	//Now we'll need to return the graph at said value
	return graph->nodes[offset_a_b];
}


/**
 * Print out a visual representation of the interference graph
 */
void print_interference_graph(interference_graph_t* graph){
	char name[50];
	//Print out every column first
	printf("%4s ", "#");
	
	//Column headers
	for(u_int16_t i = 0; i < graph->live_range_count; i++){
		sprintf(name, "LR%d", i);
		printf(" %4s", name);
	}

	printf("\n");

	//Now we'll print every single row
	for(u_int16_t i = 0; i < graph->live_range_count; i++){
		//Print the name first
		sprintf(name, "LR%d", i);
		printf(" %4s ", name);

		//Then for each column, we'll print out X for true or _ for false
		for(u_int16_t j = 0; j < graph->live_range_count; j++){
			if(graph->nodes[i * graph->live_range_count + j] == TRUE){
				printf(" %3s ", "X");
			} else {
				printf(" %3s ", "_");
			}
		}

	//Newline to end it out
	printf("\n");
	}
}


/**
 * Print out the adjacency lists of every single live range
 */
void print_adjacency_lists(dynamic_array_t* live_ranges){
	//For each live range in the live ranges array
	for(u_int16_t i = 0; i < live_ranges->current_index; i++){
		//Grab it out
		live_range_t* live_range = dynamic_array_get_at(live_ranges, i);

		//We'll print it
		printf("LR%d: {", live_range->live_range_id);

		//Now we'll run through all of its interferees
		for(u_int16_t j = 0; j < live_range->neighbors.current_index; j++){
			live_range_t* neighbor = dynamic_array_get_at(&(live_range->neighbors), j);
			printf("LR%d", neighbor->live_range_id);

			if(j != live_range->neighbors.current_index - 1){
				printf(", ");
			}
		}

		printf("}\n");
	}

}


/**
 * Get the "degree" for a certain live range. The degree is the number of
 * nodes that interfere with it. We also call these neighbors
 */
u_int16_t get_live_range_degree(live_range_t* a){
	return a->neighbors.current_index;
}


/**
 * Destroy the interference graph
*/
void interference_graph_dealloc(interference_graph_t* graph){
	//All that we need to do here is deallocate the internal list
	free(graph->nodes);

	//And then deallocate this
	free(graph);
}
