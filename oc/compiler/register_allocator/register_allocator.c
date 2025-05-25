/**
 * Author: Jack Robbins
 *
 * This file contains the implementations of the APIs defined in the header file with the same name
*/

#include "register_allocator.h"
//For live rnages
#include "../dynamic_array/dynamic_array.h"
#include <stdio.h>
#include <stdlib.h>
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
 * Print out the live ranges in a block
*/
static void print_block_with_live_ranges(basic_block_t* block){
	//If this is some kind of switch block, we first print the jump table
	if(block->block_type == BLOCK_TYPE_SWITCH || block->jump_table.nodes != NULL){
		print_jump_table(&(block->jump_table));
	}

	//If it's a function entry block, we need to print this out
	if(block->block_type == BLOCK_TYPE_FUNC_ENTRY){
		printf("%s:\n", block->func_record->func_name);
		print_stack_data_area(&(block->func_record->data_area));
	} else {
		printf(".L%d:\n", block->block_id);
	}

	//Now grab a cursor and print out every statement that we 
	//have
	instruction_t* cursor = block->leader_statement;

	//So long as it isn't null
	while(cursor != NULL){
		//We actually no longer need these
		if(cursor->instruction_type != PHI_FUNCTION){
			print_instruction(cursor, PRINTING_LIVE_RANGES);
		}

		//Move along to the next one
		cursor = cursor->next_statement;
	}

	//For spacing
	printf("\n");
}


/**
 * Run through using the direct successor strategy and print all ordered blocks.
 * We print much less here than the debug printer in the CFG, because all dominance
 * relations are now useless
 */
static void print_blocks_with_live_ranges(basic_block_t* head_block){
	//Run through the direct successors so long as the block is not null
	basic_block_t* current = head_block;

	//So long as this one isn't NULL
	while(current != NULL){
		//Print it
		print_block_with_live_ranges(current);
		//Advance to the direct successor
		current = current->direct_successor;
	}
}


/**
 * Print all live ranges that we have
 */
static void print_all_live_ranges(dynamic_array_t* live_ranges){
	printf("============= All Live Ranges ==============\n");
	//For each live range in the array
	for(u_int16_t i = 0; i < live_ranges->current_index; i++){
		//Grab it out
		live_range_t* current = dynamic_array_get_at(live_ranges, i);

		//We'll print out it's id first
		printf("LR%d: {", current->live_range_id);

		//Now we'll run through and print out all of its variables
		for(u_int16_t j = 0; j < current->variables->current_index; j++){
			//Print the variable name
			print_variable(dynamic_array_get_at(current->variables, j), PRINTING_VAR_INLINE);

			//Print a comma if appropriate
			if(j != current->variables->current_index - 1){
				printf(", ");
			}
		}
		
		//And we'll close it out
		printf("}\n");
	}
	printf("============= All Live Ranges ==============\n");
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


/**
 * Add a variable to a live range, if it isn't already in there
 */
static void add_variable_to_live_range(live_range_t* live_range, three_addr_var_t* variable){
	//Run through the live range
	for(u_int16_t _ = 0; _ < live_range->variables->current_index; _++){
		//We already have it in here, no need to continue
		if(variables_equal(variable, dynamic_array_get_at(live_range->variables, _), TRUE) == TRUE){
			return;
		}
	}

	//Otherwise we'll add this in here
	dynamic_array_add(live_range->variables, variable);
}


/**
 * Figure out which live range a given variable was associated with
 */
static void assign_live_range_to_variable(dynamic_array_t* live_ranges, three_addr_var_t* variable){
	//Stack pointer is exempt
	if(variable->is_stack_pointer == TRUE || (variable->linked_var != NULL && variable->linked_var->is_function_paramater == TRUE)){
		return;
	}

	//Lookup the live range that is associated with this
	live_range_t* live_range = find_live_range_with_variable(live_ranges, variable);

	//For developer flagging
	if(live_range == NULL){
		printf("Fatal compiler error: variable found with that has no live range\n");
		print_variable(variable, PRINTING_VAR_INLINE);
		exit(1);
	}

	//Otherwise we just assign it
	variable->associated_live_range = live_range;
}


/**
 * Run through every instruction in a block and construct the live ranges
 */
static void construct_live_ranges_in_block(dynamic_array_t* live_ranges, basic_block_t* basic_block){
	//Grab a pointer to the head
	instruction_t* current = basic_block->leader_statement;

	//Run through every instruction in the block
	while(current != NULL){
		//If we actually have a destination register
		if(current->destination_register != NULL){
			//Let's see if we can find this
			live_range_t* live_range = find_live_range_with_variable(live_ranges, current->destination_register);

			//If it's null we need to make one
			if(live_range == NULL){
				//Create it
				live_range = live_range_alloc();

				//Add it into the overall set
				dynamic_array_add(live_ranges, live_range);
			}

			//Add this into the live range
			add_variable_to_live_range(live_range, current->destination_register);

			//Link the variable into this as well
			current->destination_register->associated_live_range = live_range;

		//If we have a phi function, we need to add the assignee to a live range
		} else if(current->instruction_type == PHI_FUNCTION){
			//Let's see if we can find this
			live_range_t* live_range = find_live_range_with_variable(live_ranges, current->assignee);

			//If it's null we need to make one
			if(live_range == NULL){
				//Create it
				live_range = live_range_alloc();

				//Add it into the overall set
				dynamic_array_add(live_ranges, live_range);
			}

			//Add this into the live range
			add_variable_to_live_range(live_range, current->assignee);
		}

		//Let's also assign all the live ranges that we need to the given variables since we're already 
		//iterating like this
		if(current->source_register != NULL){
			assign_live_range_to_variable(live_ranges, current->source_register);
		}

		if(current->source_register2 != NULL){
			assign_live_range_to_variable(live_ranges, current->source_register2);
		}

		if(current->address_calc_reg1 != NULL){
			assign_live_range_to_variable(live_ranges, current->address_calc_reg1);
		}

		if(current->address_calc_reg2 != NULL){
			assign_live_range_to_variable(live_ranges, current->address_calc_reg2);
		}

		//Advance it down
		current = current->next_statement;
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

	printf("============= After Live Range Determination ==============\n");
	print_blocks_with_live_ranges(cfg->head_block);
	printf("============= After Live Range Determination ==============\n");
}
