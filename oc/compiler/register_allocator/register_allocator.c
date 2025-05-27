/**
 * Author: Jack Robbins
 *
 * This file contains the implementations of the APIs defined in the header file with the same name
 *
 * The ollie compiler uses a global register allocator with a reduction to the graph-coloring problem.
 * We make use of the interference graph to do this.
*/

#include "register_allocator.h"
//For live rnages
#include "../dynamic_array/dynamic_array.h"
#include "../interference_graph/interference_graph.h"
#include "../cfg/cfg.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

//For standardization
#define TRUE 1
#define FALSE 0

#define K_COLORS 15

//A load and a store generate 2 instructions when we load
//from the stack
#define LOAD_AND_STORE_COST 2

//The atomically increasing live range id
u_int16_t live_range_id = 0;

/**
 * Increment and return the live range ID
 */
static u_int16_t increment_and_get_live_range_id(){
	return live_range_id++;
}


/**
 * Create a live range
 */
static live_range_t* live_range_alloc(u_int16_t starting_line_number){
	//Calloc it
	live_range_t* live_range = calloc(1, sizeof(live_range_t));

	//Give it a unique id
	live_range->live_range_id = increment_and_get_live_range_id();

	//And create it's dynamic array
	live_range->variables = dynamic_array_alloc();

	//Store the starting line number
	live_range->starting_line_num = starting_line_number;

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

	//If we have some assigned variables, we will dislay those for debugging
	if(block->assigned_variables != NULL){
		printf("Assigned: (");

		for(u_int16_t i = 0; i < block->assigned_variables->current_index; i++){
			print_live_range(dynamic_array_get_at(block->assigned_variables, i));

			//If it isn't the very last one, we need a comma
			if(i != block->assigned_variables->current_index - 1){
				printf(", ");
			}
		}
		printf(")\n");
	}

	//If we have some used variables, we will dislay those for debugging
	if(block->used_variables != NULL){
		printf("Used: (");

		for(u_int16_t i = 0; i < block->used_variables->current_index; i++){
			print_live_range(dynamic_array_get_at(block->used_variables, i));

			//If it isn't the very last one, we need a comma
			if(i != block->used_variables->current_index - 1){
				printf(", ");
			}
		}
		printf(")\n");
	}

	//If we have some assigned variables, we will dislay those for debugging
	if(block->live_in != NULL){
		printf("LIVE IN: (");

		for(u_int16_t i = 0; i < block->live_in->current_index; i++){
			print_live_range(dynamic_array_get_at(block->live_in, i));

			//If it isn't the very last one, we need a comma
			if(i != block->live_in->current_index - 1){
				printf(", ");
			}
		}
		printf(")\n");
	}

	//If we have some assigned variables, we will dislay those for debugging
	if(block->live_out != NULL){
		printf("LIVE OUT: (");

		for(u_int16_t i = 0; i < block->live_out->current_index; i++){
			print_live_range(dynamic_array_get_at(block->live_out, i));

			//If it isn't the very last one, we need a comma
			if(i != block->live_out->current_index - 1){
				printf(", ");
			}
		}
		printf(")\n");
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
			print_variable(dynamic_array_get_at(current->variables, j), PRINTING_VAR_BLOCK_HEADER);

			//Print a comma if appropriate
			if(j != current->variables->current_index - 1){
				printf(", ");
			}
		}
		
		//And we'll close it out
		printf("}\tSpill Cost: %d\tStarts at line: %d Ends at line %d\n", current->spill_cost, current->starting_line_num, current->ending_line_num);
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
 * Update the estimate on spilling this variable
 */
static void update_spill_cost(live_range_t* live_range, basic_block_t* block, three_addr_var_t* variable){
	//If we have a temporary variable, the spill cost is essentially
	//infinite because the live range is so short
	if(block->is_global_var_block == TRUE && live_range->spill_cost == 0){
		//Negative spill cost, we want this spilled
		live_range->spill_cost = -10;
	} else if(variable->is_temporary == TRUE){
		live_range->spill_cost = INT16_MAX;
	} else {
		//Otherwise it's not temporary, so we'll need to add the estimated execution frequency
		//of this block times the number of instructions a load/store combo will take
		live_range->spill_cost += LOAD_AND_STORE_COST * block->estimated_execution_frequency; 
	}
}



/**
 * Add a variable to a live range, if it isn't already in there
 */
static void add_variable_to_live_range(live_range_t* live_range, basic_block_t* block, three_addr_var_t* variable, u_int16_t line_number){
	//If it's the stack pointer just get out
	if(variable->is_stack_pointer == TRUE){
		return;
	}

	//Run through the live range
	for(u_int16_t _ = 0; _ < live_range->variables->current_index; _++){
		//We already have it in here, no need to continue
		if(variables_equal(variable, dynamic_array_get_at(live_range->variables, _), TRUE) == TRUE){
			//Update the cost
			update_spill_cost(live_range, block, variable);
			return;
		}
	}

	//Otherwise we'll add this in here
	dynamic_array_add(live_range->variables, variable);

	//Update the cost
	update_spill_cost(live_range, block, variable);

	//Update the ending line number
	if(live_range->ending_line_num < line_number){
		live_range->ending_line_num = line_number;
	}

	//Adding a variable to a live range means that this live range is assigned to in this block
	if(dynamic_array_contains(block->assigned_variables, live_range) == NOT_FOUND){
		dynamic_array_add(block->assigned_variables, live_range);
	}
}


/**
 * Figure out which live range a given variable was associated with
 */
static void assign_live_range_to_variable(dynamic_array_t* live_ranges, basic_block_t* block, three_addr_var_t* variable, u_int32_t line_number){
	//Stack pointer is exempt
	if(variable->is_stack_pointer == TRUE){
		//We'll already have the live range
		dynamic_array_add(block->used_variables, variable->associated_live_range);
		return;
	}

	//Lookup the live range that is associated with this
	live_range_t* live_range = find_live_range_with_variable(live_ranges, variable);

	//For developer flagging
	if(live_range == NULL){
		//This is a function parameter, we need to make it ourselves
		if(variable->linked_var->is_function_paramater == TRUE){
			//Create it. Since this is a function parameter, we start at line 0
			live_range = live_range_alloc(0);
			//Add it in
			dynamic_array_add(live_range->variables, variable);
			//Update the variable too
			variable->associated_live_range = live_range;

			//Finally add this into all of our live ranges
			dynamic_array_add(live_ranges, live_range);

		} else {
			printf("Fatal compiler error: variable found with that has no live range\n");
			print_variable(variable, PRINTING_VAR_INLINE);
			exit(1);
		}
	}

	//Otherwise we just assign it
	variable->associated_live_range = live_range;

	//Update the spill cost
	update_spill_cost(live_range, block, variable);

	//Update the ending line number
	if(live_range->ending_line_num < line_number){
		live_range->ending_line_num = line_number;
	}

	//Assigning a live range to a variable means that this variable was *used* in the block
	if(dynamic_array_contains(block->used_variables, live_range) == NOT_FOUND){
		dynamic_array_add(block->used_variables, live_range);
	}

}


/**
 * Calculate the "live_in" and "live_out" sets for each basic block
 *
 * General algorithm
 *
 * for each block n
 * 	live_out[n] = {}
 * 	live_in[n] = {}
 *
 * for each block n in reverse order
 * 	in'[n] = in[n]
 * 	out'[n] = out[n]
 * 	in[n] = use[n] U (out[n] - def[n])
 * 	out[n] = {}U{x|x is an element of in[S] where S is a successor of n}
 *
 * NOTE: The algorithm converges very fast when the CFG is done in reverse order.
 * As such, we'll go back to front here
 *
 */
static void calculate_liveness_sets(cfg_t* cfg){
	//Reset the visited status
	reset_visited_status(cfg, FALSE);

	//Did we find a difference
	u_int8_t difference_found;

	//The "Prime" blocks are just ways to hold the old dynamic arrays
	dynamic_array_t* in_prime;
	dynamic_array_t* out_prime;

	//A cursor for the current block
	basic_block_t* current;

	do {
		//We'll assume we didn't find a difference each iteration
		difference_found = FALSE;

		//Run through all of the blocks backwards
		for(int16_t i = cfg->function_blocks->current_index - 1; i >= 0; i--){
			//Grab the block out
			basic_block_t* func_entry = dynamic_array_get_at(cfg->function_blocks, i);

			//Now we can go through the entire RPO set
			for(u_int16_t _ = 0; _ < func_entry->reverse_post_order_reverse_cfg->current_index; _++){
				//The current block is whichever we grab
				current = dynamic_array_get_at(func_entry->reverse_post_order_reverse_cfg, _);

				//Transfer the pointers over
				in_prime = current->live_in;
				out_prime = current->live_out;

				//The live in is a combination of the variables used
				//at current and the difference of the LIVE_OUT variables defined
				//ones

				//Since we need all of the used variables, we'll just clone this
				//dynamic array so that we start off with them all
				current->live_in = clone_dynamic_array(current->used_variables);

				//Now we need to add every variable that is in LIVE_OUT but NOT in assigned
				for(u_int16_t j = 0; current->live_out != NULL && j < current->live_out->current_index; j++){
					//Grab a reference for our use
					live_range_t* live_out_var = dynamic_array_get_at(current->live_out, j);

					//Now we need this block to be not in "assigned" also. If it is in assigned we can't
					//add it. Additionally, we'll want to make sure we aren't adding duplicate live ranges
					if(dynamic_array_contains(current->assigned_variables, live_out_var) == NOT_FOUND
						&& dynamic_array_contains(current->live_in, live_out_var) == NOT_FOUND){
						//If this is true we can add
						dynamic_array_add(current->live_in, live_out_var);
					}
				}

				//Now we'll turn our attention to live out. The live out set for any block is the union of the
				//LIVE_IN set for all of it's successors
				
				//Set live out to be a new array
				current->live_out = dynamic_array_alloc();

				//Run through all of the successors
				for(u_int16_t k = 0; current->successors != NULL && k < current->successors->current_index; k++){
					//Grab the successor out
					basic_block_t* successor = dynamic_array_get_at(current->successors, k);

					//Add everything in his live_in set into the live_out set
					for(u_int16_t l = 0; successor->live_in != NULL && l < successor->live_in->current_index; l++){
						//Let's check to make sure we haven't already added this
						live_range_t* successor_live_in_var = dynamic_array_get_at(successor->live_in, l);

						//If it doesn't already contain this variable, we'll add it in
						if(dynamic_array_contains(current->live_out, successor_live_in_var) == NOT_FOUND){
							dynamic_array_add(current->live_out, successor_live_in_var);
						}
					}
				}

				//Now we'll go through and check if the new live in and live out sets are different. If they are different,
				//we'll be doing this whole thing again

				//For efficiency - if there was a difference in one block, it's already done - no use in comparing
				if(difference_found == FALSE){
					//So we haven't found a difference so far - let's see if we can find one now
					if(dynamic_arrays_equal(in_prime, current->live_in) == FALSE
					  || dynamic_arrays_equal(out_prime, current->live_out) == FALSE){
						//We have in fact found a difference
						difference_found = TRUE;
					}
				}

				//We made it down here, the prime variables are useless. We'll deallocate them
				dynamic_array_dealloc(in_prime);
				dynamic_array_dealloc(out_prime);
			}
		}

	//So long as we continue finding differences
	} while(difference_found == TRUE);
}


/**
 * Run through every instruction in a block and construct the live ranges
 */
static void construct_live_ranges_in_block(dynamic_array_t* live_ranges, basic_block_t* basic_block, u_int32_t* current_line_num){
	//Let's first wipe everything regarding this block's used and assigned variables. If they don't exist,
	//we'll allocate them fresh
	if(basic_block->assigned_variables == NULL){
		basic_block->assigned_variables = dynamic_array_alloc();
	} else {
		reset_dynamic_array(basic_block->assigned_variables);
	}

	//Do the same with the used variables
	if(basic_block->used_variables == NULL){
		basic_block->used_variables = dynamic_array_alloc();
	} else {
		reset_dynamic_array(basic_block->used_variables);
	}

	//Reset live in completely
	if(basic_block->live_in != NULL){
		dynamic_array_dealloc(basic_block->live_in);
		basic_block->live_in = NULL;
	}

	//Reset live out completely
	if(basic_block->live_out != NULL){
		dynamic_array_dealloc(basic_block->live_out);
		basic_block->live_out = NULL;
	}

	//Grab a pointer to the head
	instruction_t* current = basic_block->leader_statement;

	//Run through every instruction in the block
	while(current != NULL){
		//Store the line number
		current->instruction_line_number = *current_line_num;

		//If we actually have a destination register
		if(current->destination_register != NULL){
			//Let's see if we can find this
			live_range_t* live_range = find_live_range_with_variable(live_ranges, current->destination_register);

			//If it's null we need to make one
			if(live_range == NULL){
				//Create it
				live_range = live_range_alloc(current->instruction_line_number);

				//Add it into the overall set
				dynamic_array_add(live_ranges, live_range);
			}

			//Add this into the live range
			add_variable_to_live_range(live_range, basic_block, current->destination_register, current->instruction_line_number);

			//Link the variable into this as well
			current->destination_register->associated_live_range = live_range;

		//If we have a phi function, we need to add the assignee to a live range
		} else if(current->instruction_type == PHI_FUNCTION){
			//Let's see if we can find this
			live_range_t* live_range = find_live_range_with_variable(live_ranges, current->assignee);

			//If it's null we need to make one
			if(live_range == NULL){
				//Create it
				live_range = live_range_alloc(current->instruction_line_number);

				//Add it into the overall set
				dynamic_array_add(live_ranges, live_range);
			}

			//Add this into the live range
			add_variable_to_live_range(live_range, basic_block, current->assignee, current->instruction_line_number);
		}

		//Let's also assign all the live ranges that we need to the given variables since we're already 
		//iterating like this
		if(current->source_register != NULL){
			assign_live_range_to_variable(live_ranges, basic_block, current->source_register, current->instruction_line_number);
		}

		if(current->source_register2 != NULL){
			assign_live_range_to_variable(live_ranges, basic_block, current->source_register2, current->instruction_line_number);
		}

		if(current->address_calc_reg1 != NULL){
			assign_live_range_to_variable(live_ranges, basic_block, current->address_calc_reg1, current->instruction_line_number);
		}

		if(current->address_calc_reg2 != NULL){
			assign_live_range_to_variable(live_ranges, basic_block, current->address_calc_reg2, current->instruction_line_number);
		}

		//Increment this
		(*current_line_num)++;

		//Advance it down
		current = current->next_statement;
	}
}


/**
 * Construct the interference graph using LIVENOW sets
 *
 * Algorithm:
 * 	create an interference graph
 * 	for each block b:
 * 		LIVENOW <- LIVEOUT(b)
 * 		for each operation with form op LA, LB -> LC:
 * 			for each LRi in LIVENOW:
 * 				add(LC, LRi) to Interference Graph E 
 * 			remove LC from LIVENOW
 * 			Add LA an LB to LIVENOW
 *
 */
static interference_graph_t construct_interference_graph(cfg_t* cfg){
	//Stack allocate the interference graph
	interference_graph_t graph;

	//Let's construct it. The live range id is the number of live ranges we have
	interference_graph_alloc(&graph, live_range_id);

	//We'll first need a pointer
	basic_block_t* current = cfg->head_block;

	//Run through every block in the CFG's ordered set
	while(current != NULL){
		//Just check for this case first
		if(current->live_out == NULL){
			current = current->direct_successor;
			continue;
		}

		//Even though we use the LIVENOW set in name, in reality it is just LIVEOUT. We'll
		//keep using LIVEOUT for LIVENOW with that in mind
		
		//Grab a pointer to the operations
		instruction_t* operation = current->leader_statement;

		//For every operation that we have
		while(operation != NULL){
			//If we have an exact copy operation, we can
			//skip it as it won't create any interference
			if(operation->instruction_type == PHI_FUNCTION
				|| ((operation->instruction_type == MOVW
				|| operation->instruction_type == MOVL
				|| operation->instruction_type == MOVQ)
				&& operation->instruction_type == 0)
				|| operation->destination_register == NULL
				|| operation->destination_register->associated_live_range == NULL){

				//Skip it
				operation = operation->next_statement;
				continue;
			}

			//Now that we know this operation is valid, we will add interference between this and every
			//other value in live_out
			for(u_int16_t i = 0; current->live_out != NULL && i < current->live_out->current_index; i++){
				//Graph the LR out
				live_range_t* range = dynamic_array_get_at(current->live_out, i);

				//Now we'll add this to the graph
				add_interference(&graph, range, operation->destination_register->associated_live_range);
			}

			//Once we're done with this, we'll delete the destination's live range from the LIVE_NOW set
			dynamic_array_delete(current->live_out, operation->destination_register->associated_live_range);

			//Now we'll add any other registers to LIVEOUT
			//iterating like this
			if(operation->source_register != NULL
				&& dynamic_array_contains(current->live_out, operation->source_register->associated_live_range) == NOT_FOUND){
				dynamic_array_add(current->live_out, operation->source_register->associated_live_range);
			}

			if(operation->source_register2 != NULL
				&& dynamic_array_contains(current->live_out, operation->source_register2->associated_live_range) == NOT_FOUND){
				dynamic_array_add(current->live_out, operation->source_register2->associated_live_range);
			}

			if(operation->address_calc_reg1 != NULL
				&& dynamic_array_contains(current->live_out, operation->address_calc_reg1->associated_live_range) == NOT_FOUND){
				dynamic_array_add(current->live_out, operation->address_calc_reg1->associated_live_range);
			}

			if(operation->address_calc_reg2 != NULL
				&& dynamic_array_contains(current->live_out, operation->address_calc_reg2->associated_live_range) == NOT_FOUND){
				dynamic_array_add(current->live_out, operation->address_calc_reg2->associated_live_range);
			}

			//Advance it
			operation = operation->next_statement;
		}

		//Advance this up
		current = current->direct_successor;
	}

	return graph;
}

/**
 * Create the stack pointer live range
 */
static live_range_t* construct_stack_pointer_live_range(three_addr_var_t* stack_pointer){
	//Before we go any further, we'll construct the live
	//range for the stack pointer
	live_range_t* stack_pointer_live_range = live_range_alloc(0);
	//This is guaranteed to be RSP - so it's already been allocated
	stack_pointer_live_range->reg = RSP;
	//And we absolutely *can not* spill it
	stack_pointer_live_range->spill_cost = INT16_MAX;

	//This never ends
	stack_pointer_live_range->ending_line_num = INT16_MAX;

	//Add the stack pointer to the dynamic array
	dynamic_array_add(stack_pointer_live_range->variables, stack_pointer);
	
	//Store this here as well
	stack_pointer->associated_live_range = stack_pointer_live_range;

	//Give it back
	return stack_pointer_live_range;
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

	//Add it into the dynamic array
	dynamic_array_add(live_ranges, construct_stack_pointer_live_range(cfg->stack_pointer));

	//Since the blocks are already ordered, this is very simple
	basic_block_t* current = cfg->head_block;

	//The current line number
	u_int32_t current_line_num = 0;

	//Run through every single block
	while(current != NULL){
		//If we hit the start of a new function, add this in
		if(dynamic_array_contains(cfg->function_blocks, current) != NOT_FOUND){
			current_line_num = 0;
		}

		//Let the helper do this
		construct_live_ranges_in_block(live_ranges, current, &current_line_num);

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

	//We now need to compute all of the LIVE OUT values
	calculate_liveness_sets(cfg);

	printf("============= After Live Range Determination ==============\n");
	print_blocks_with_live_ranges(cfg->head_block);
	printf("============= After Live Range Determination ==============\n");

	//Now let's determine the interference graph
	interference_graph_t graph = construct_interference_graph(cfg);

	printf("================ Interference Graph =======================\n");
	print_interference_graph(&graph);
	printf("================ Interference Graph =======================\n");
}
