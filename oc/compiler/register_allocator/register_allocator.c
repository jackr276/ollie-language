/**
 * Author: Jack Robbins
 *
 * This file contains the implementations of the APIs defined in the header file with the same name
*/

#include "register_allocator.h"
//For live rnages
#include "../dynamic_array/dynamic_array.h"
#include <stdio.h>
#include <sys/types.h>

//The atomically increasing live range id
u_int16_t live_range_id = 0;

/**
 * Increment and return the live range ID
 */
static u_int16_t increment_and_get_live_range_id(){
	live_range_id++;
	return live_range_id;
}


/**
 * Create a live range
 */
static live_range_t* live_range_alloc(){
	//Calloc it
	live_range_t* live_range = calloc(1, sizeof(live_range_t));

	//Give it a unique id
	live_range->live_range_id = increment_and_get_live_range_id();

	//And create it's dynamic array
	live_range->variables = dynamic_array_alloc();

	//Finally we'll return it
	return live_range;
}


/**
 * Free all the memory that's reserved by a live range
 */
static void live_range_dealloc(live_range_t* live_range){
	//First we'll destroy the array that it has
	dynamic_array_dealloc(live_range->variables);

	//Then we can destroy the live range itself
	free(live_range);
}


/**
 * Print all live ranges that we have
 */
static void print_all_live_ranges(dynamic_array_t* live_ranges){
	//For each live range in the array
	for(u_int16_t i = 0; i < live_ranges->current_index; i++){
		//Grab it out
		live_range_t* current = dynamic_array_get_at(live_ranges, i);

		//We'll print out it's id first
		printf("LR%d: {", current->live_range_id);

		//Now we'll run through and print out all of its variables
		for(u_int16_t j = 0; j < current->variables->current_index; j++){
			//Print the variable name
			print_variable_name(dynamic_array_get_at(current->variables, j));

			//Print a comma if appropriate
			if(j != current->variables->current_index - 1){
				printf(", ");
			}
		}
		
		//And we'll close it out
		printf("}\n");
	}
}


/**
 * Construct the live ranges for all variables that we'll need to concern ourselves with
 *
 * Conveniently, all code in OIR is translated into SSA form by the front end. In doing this, we're able
 * to find live ranges in one pass of the code
 *
 * We will run through the entirety of the straight-line code. We will use the disjoint-set union-find algorithm
 * to do this.
 *
 * For each instruction with an assignee:
 * 	If assignee is not in a live range set:
 * 		make a new live range set and add the variable to it
 * 	else:
 * 		add the variable to the corresponding live range set
 * 		mark said variable 
 */
static dynamic_array_t* construct_all_live_ranges(cfg_t* cfg){

	//Placehold
	return NULL;
}


/**
 * Perform our register allocation algorithm on the entire cfg
 */
void allocate_all_registers(cfg_t* cfg){

}
