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

//For standardization
#define TRUE 1
#define FALSE 0

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
 * Does a live range for a given variable already exist? If so, we'll need to 
 * coalesce the two live ranges in a union
 *
 * Returns NULL if we found nothing
 */
static live_range_t* find_live_range_with_variable(dynamic_array_t* live_ranges, three_addr_var_t* variable){
	//Run through all of the live ranges that we currently have
	for(u_int16_t _ = 0; _ < live_ranges->current_index; _++){
		//Grab the given live range out
		live_range_t* current = dynamic_array_get_at(live_ranges, _);

		//If the variables are equal(ignoring SSA and dereferencing) then we have a match
		if(variables_equal_no_ssa(variable, dynamic_array_get_at(current->variables, 0), TRUE) == TRUE){
			return current;
		}
	}

	//If we get here we didn't find anything
	return NULL;
}


static void construct_live_ranges_in_block(dynamic_array_t* live_ranges, basic_block_t* basic_block){

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
	//First create the set of live ranges
	dynamic_array_t* live_ranges = dynamic_array_alloc();

	//Since the blocks are already ordered, this is very simple
	basic_block_t* current = cfg->head_block;

	//Run through every single block
	while(current != NULL){
		//Let the helper do this
		construct_live_ranges_in_block(live_ranges, current);

		//Advance to the next
		current = current->direct_successor;
	}

	//Placehold
	return live_ranges;
}


/**
 * Perform our register allocation algorithm on the entire cfg
 */
void allocate_all_registers(cfg_t* cfg){
	//The first thing that we'll do is reconstruct everything in terms of live ranges
	//This should be simplified by our values already being in SSA form
	dynamic_array_t* live_ranges = construct_all_live_ranges(cfg);

	//Print whatever live ranges we did find
	print_all_live_ranges(live_ranges);

}
