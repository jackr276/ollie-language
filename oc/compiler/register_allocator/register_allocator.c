/**
 * Author: Jack Robbins
 *
 * This file contains the implementations of the APIs defined in the header file with the same name
 *
 * The ollie compiler uses a global register allocator with a reduction to the graph-coloring problem.
 * We make use of the interference graph to do this.
*/

#include "register_allocator.h"
//For live ranges
#include "../dynamic_array/dynamic_array.h"
#include "../interference_graph/interference_graph.h"
#include "../cfg/cfg.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

//For standardization
#define TRUE 1
#define FALSE 0

//The number of colors that we have for general use registers
#define K_COLORS_GEN_USE 15

//A load and a store generate 2 instructions when we load
//from the stack
#define LOAD_AND_STORE_COST 2

//The atomically increasing live range id
u_int16_t live_range_id = 0;

//The array that holds all of our parameter passing
const register_holder_t parameter_registers[] = {RDI, RSI, RDX, RCX, R8, R9};

//Avoid need to rearrange
static interference_graph_t* construct_interference_graph(cfg_t* cfg, dynamic_array_t* live_ranges);


/**
 * Priority queue insert a live range in here
 *
 * Highest spill cost = highest priority. We want to guarantee that the values which get into registers first
 * are high spill cost items. We don't want to end up having to spill these. If we need to spill a lower
 * priority item, we won't feel it nearly as much
 * Higher priority items go to the back to make removal O(1)(using dynamic_array_delete_from_back())
 */
static void dynamic_array_priority_insert_live_range(dynamic_array_t* array, live_range_t* live_range){
	//Now we'll see if we need to reallocate this. We use current index + 1 because we could be inserting
	//at the back
	if(array->current_index + 1 == array->current_max_size){
		//We'll double the current max size
		array->current_max_size *= 2;

		//And we'll reallocate the array
		array->internal_array = realloc(array->internal_array, sizeof(void*) * array->current_max_size);
	}

	//We'll need this out of the scope
	u_int16_t i = 0;

	//Run through the array and figure out where to put this
	for(; i < array->current_index; i++){
		//Grab the current one out
		live_range_t* current = dynamic_array_get_at(array, i);

		//If this one is lower priority than the given one, we'll stop
		if(current->spill_cost > live_range->spill_cost){
			break;
		}
	}

	//Shift to the right by 1
	for(int16_t j = array->current_index; j >= i; j--){
		array->internal_array[j+1] = array->internal_array[j];
	}

	//Now we can insert the array at i
	array->internal_array[i] = live_range;

	//Bump this up by 1
	array->current_index++;

	//And we're all set
}


/**
 * Developer utility function to validate the priority queue implementation
 */
static void print_live_range_array(dynamic_array_t* live_ranges){
	printf("{");

	//Print everything out
	for(u_int16_t i = 0; i < live_ranges->current_index; i++){
		live_range_t* range = dynamic_array_get_at(live_ranges, i);

		printf("LR%d(%d)", range->live_range_id, range->spill_cost);

		if(i != live_ranges->current_index - 1){
			printf(", ");
		}
	}

	printf("}\n");
}


/**
 * Increment and return the live range ID
 */
static u_int16_t increment_and_get_live_range_id(){
	return live_range_id++;
}


/**
 * Create a live range
 */
static live_range_t* live_range_alloc(symtab_function_record_t* function_defined_in, variable_size_t size){
	//Calloc it
	live_range_t* live_range = calloc(1, sizeof(live_range_t));

	//Give it a unique id
	live_range->live_range_id = increment_and_get_live_range_id();

	//And create it's dynamic array
	live_range->variables = dynamic_array_alloc();


	//Store what function this came from
	live_range->function_defined_in = function_defined_in;

	//Create the neighbors array as well
	live_range->neighbors = dynamic_array_alloc();

	//Store the size as well
	live_range->size = size;

	//Finally we'll return it
	return live_range;
}


/**
 * Free all the memory that's reserved by a live range
 */
static void live_range_dealloc(live_range_t* live_range){
	//First we'll destroy the array that it has
	dynamic_array_dealloc(live_range->variables);

	//Destroy the neighbors array as well
	dynamic_array_dealloc(live_range->neighbors);

	//Then we can destroy the live range itself
	free(live_range);
}


/**
 * Print out the live ranges in a block
*/
static void print_block_with_live_ranges(basic_block_t* block){
	//If this is some kind of switch block, we first print the jump table
	if(block->block_type == BLOCK_TYPE_SWITCH || block->jump_table.nodes != NULL){
		print_jump_table(stdout, &(block->jump_table));
	}

	//If it's a function entry block, we need to print this out
	if(block->block_type == BLOCK_TYPE_FUNC_ENTRY){
		printf("%s:\n", block->function_defined_in->func_name);
		print_stack_data_area(&(block->function_defined_in->data_area));
	} else {
		printf(".L%d:\n", block->block_id);
	}

	//If we have some assigned variables, we will dislay those for debugging
	if(block->assigned_variables != NULL){
		printf("Assigned: (");

		for(u_int16_t i = 0; i < block->assigned_variables->current_index; i++){
			print_live_range(stdout, dynamic_array_get_at(block->assigned_variables, i));

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
			print_live_range(stdout, dynamic_array_get_at(block->used_variables, i));

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
			print_live_range(stdout, dynamic_array_get_at(block->live_in, i));

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
			print_live_range(stdout, dynamic_array_get_at(block->live_out, i));

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
			print_instruction(stdout, cursor, PRINTING_LIVE_RANGES);
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
 * Print instructions with registers
*/
static void print_block_with_registers(basic_block_t* block){
	//If this is some kind of switch block, we first print the jump table
	if(block->block_type == BLOCK_TYPE_SWITCH || block->jump_table.nodes != NULL){
		print_jump_table(stdout, &(block->jump_table));
	}

	//If it's a function entry block, we need to print this out
	if(block->block_type == BLOCK_TYPE_FUNC_ENTRY){
		printf("%s:\n", block->function_defined_in->func_name);

		//We'd only want to print the stack if this is not the final run
		print_stack_data_area(&(block->function_defined_in->data_area));

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
			print_instruction(stdout, cursor, PRINTING_REGISTERS);
		}

		//Move along to the next one
		cursor = cursor->next_statement;
	}

	//For spacing
	printf("\n");
}


/**
 * Run through using the direct successor strategy and print all
 * ordered blocks with their registers after allocation
 */
static void print_blocks_with_registers(basic_block_t* head_block){
	//Run through the direct successors so long as the block is not null
	basic_block_t* current = head_block;

	//So long as this one isn't NULL
	while(current != NULL){
		//Print it
		print_block_with_registers(current);
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
			print_variable(stdout, dynamic_array_get_at(current->variables, j), PRINTING_VAR_BLOCK_HEADER);

			//Print a comma if appropriate
			if(j != current->variables->current_index - 1){
				printf(", ");
			}
		}

		printf("} Neighbors: {");

		//Now we'll print out all of it's neighbors
		for(u_int16_t k = 0; k < current->neighbors->current_index; k++){
			live_range_t* neighbor = dynamic_array_get_at(current->neighbors, k);
			printf("LR%d", neighbor->live_range_id);
 
			//Print a comma if appropriate
			if(k != current->neighbors->current_index - 1){
				printf(", ");
			}

		}
		
		//And we'll close it out
		printf("}\tSpill Cost: %d\tDegree: %d\n", current->spill_cost, current->degree);
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
		for(u_int16_t i = 0 ; i < current->variables->current_index; i++){
			if(variables_equal_no_ssa(variable, dynamic_array_get_at(current->variables, i), TRUE) == TRUE){
				return current;
			}
		}
	}

	//If we get here we didn't find anything
	return NULL;
}


/**
 * Update the estimate on spilling this variable
 */
static void update_spill_cost(live_range_t* live_range, basic_block_t* block, three_addr_var_t* variable){
	if(variable->is_temporary == TRUE){
		if(live_range->spill_cost == 0){
			live_range->spill_cost = 1;
		}

		//Let's try just doubling for now
		live_range->spill_cost *= 2;

	} else {
		//Add this 
		live_range->spill_cost += LOAD_AND_STORE_COST * block->estimated_execution_frequency; 
	}
}



/**
 * Add a variable to a live range, if it isn't already in there
 */
static void add_variable_to_live_range(live_range_t* live_range, basic_block_t* block, three_addr_var_t* variable){
	//If it's the stack pointer just get out
	if(variable->is_stack_pointer == TRUE){
		return;
	}

	//If the literal memory address is already in here, then all we need to do is update
	//the cost
	if(dynamic_array_contains(live_range->variables, variable) != NOT_FOUND){
		//Update the cost
		update_spill_cost(live_range, block, variable);
		return;
	}

	//If this needs to be spilled, then the whole live range needs to be spilled
	if(variable->linked_var != NULL && variable->linked_var->must_be_spilled == TRUE){
		live_range->must_be_spilled = TRUE;
	}

	//Otherwise we'll add this in here
	dynamic_array_add(live_range->variables, variable);

	//Update the cost
	update_spill_cost(live_range, block, variable);

	//Adding a variable to a live range means that this live range is assigned to in this block
	if(dynamic_array_contains(block->assigned_variables, live_range) == NOT_FOUND){
		dynamic_array_add(block->assigned_variables, live_range);
	}
}


/**
 * Figure out which live range a given variable was associated with
 */
static void assign_live_range_to_variable(dynamic_array_t* live_ranges, basic_block_t* block, three_addr_var_t* variable){
	//Stack pointer is exempt
	if(variable->is_stack_pointer == TRUE){
		//We'll already have the live range
		dynamic_array_add(block->used_variables, variable->associated_live_range);
		return;
	}

	//If this is the case it already has one
	if(variable->associated_live_range != NULL){
		return;
	}

	//Lookup the live range that is associated with this
	live_range_t* live_range = find_live_range_with_variable(live_ranges, variable);

	//For developer flagging
	if(live_range == NULL){
		//This is a function parameter, we need to make it ourselves
		if(variable->linked_var != NULL && variable->linked_var->is_function_paramater == TRUE){
			print_variable(stdout, variable, PRINTING_VAR_INLINE);
			//Create it. Since this is a function parameter, we start at line 0
			live_range = live_range_alloc(block->function_defined_in, variable->variable_size);
			//Add it in
			dynamic_array_add(live_range->variables, variable);
			//Update the variable too
			variable->associated_live_range = live_range;

			//Finally add this into all of our live ranges
			dynamic_array_add(live_ranges, live_range);

		} else {
			printf("Fatal compiler error: variable found with that has no live range\n");
			print_variable(stdout, variable, PRINTING_VAR_INLINE);
			//TODO THIS MUST BE FIXED
			exit(0);
		}
	}

	//We now add this variable back into the live range
	add_variable_to_live_range(live_range, block, variable);

	//Otherwise we just assign it
	variable->associated_live_range = live_range;

	//Update the spill cost
	update_spill_cost(live_range, block, variable);

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

			//Reset the registers in here while we're at it
			memset(func_entry->function_defined_in->used_registers, 0, sizeof(u_int8_t) * 17);

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
 * Do we have precoloring interference for these two registers? If we do, we'll
 * return true and this will prevent the coalescing algorithm from combining them
 *
 * Precoloring is important to work around. On the surface for some move instructions,
 * it may seem like the move is a pointless copy. However, this is not the case when precoloring
 * is involved because moving into those exact registers is very important. Since we cannot guarantee
 * that we're going to move into those exact registers long in advance, we need to keep the movements
 * for precoloring around
 */
static u_int8_t does_precoloring_interference_exist(live_range_t* a, live_range_t* b){
	/**
	 * The logic here: if they *both* don't equal no reg *and* their registers
	 * are not equal, then we have precoloring interference
	 */
	if(a->reg != NO_REG || b->reg != NO_REG){
		return TRUE;
	} else {
		return FALSE;
	}
}


/**
 * Perform live range coalescing on a given instruction. This sees
 * us merge the source and destination operands's webs(live ranges)
 *
 * We coalesce source to destination. When we're done, the *source* should
 * survive, the destination should NOT
 */
static void perform_live_range_coalescence(cfg_t* cfg, dynamic_array_t* live_ranges, interference_graph_t* graph){
	//Run through every single block in here
	basic_block_t* current = cfg->head_block;
	while(current != NULL){
		//Now we'll run through every instruction in every block
		instruction_t* instruction = current->leader_statement;

		//Now run through all of these
		while(instruction != NULL){
			//If we have a pure copy instruction(movX with no indirection), we can coalesce
			if(is_instruction_pure_copy(instruction) == TRUE){
				//If our live ranges interfere, we can perform the coalescing
				if(do_live_ranges_interfere(graph, instruction->source_register->associated_live_range, instruction->destination_register->associated_live_range) == FALSE
					//Also check for precoloring interference
					&& does_precoloring_interference_exist(instruction->source_register->associated_live_range, instruction->destination_register->associated_live_range) == FALSE){
					printf("Can coalesce LR%d and LR%d\n", instruction->source_register->associated_live_range->live_range_id, instruction->destination_register->associated_live_range->live_range_id);

					printf("DELETING LR%d\n", instruction->destination_register->associated_live_range->live_range_id);
					//Delete this live range from our list as it no longer exists
					dynamic_array_delete(live_ranges, instruction->destination_register->associated_live_range);

					//We will coalesce the destination register's live range and the source register's live range
					coalesce_live_ranges(graph, instruction->source_register->associated_live_range, instruction->destination_register->associated_live_range);

					//Once we're done, this instruction is now useless, so we'll delete it
					instruction_t* temp = instruction;
					//Push this up
					instruction = instruction->next_statement;

					printf("Deleting:\n");
					print_instruction(stdout, temp, PRINTING_VAR_INLINE);

					//Delete the old one from the graph
					delete_statement(cfg, current, temp);

				//This is a theoretical possibility, wehere we could have already performed some coalescence that ends us up here. If this
				//is the case, we'll just delete the instruction
				} else if(instruction->source_register->associated_live_range == instruction->destination_register->associated_live_range){
					instruction_t* temp = instruction;
					//Push this up
					instruction = instruction->next_statement;

					printf("Deleting DUPLICATE:\n");
					print_instruction(stdout, temp, PRINTING_LIVE_RANGES);

					//Delete the old one from the block
					delete_statement(cfg, current, temp);

				//Just advance it
				} else {
					instruction = instruction->next_statement;
				}

			} else {
				//Advance it
				instruction = instruction->next_statement;
			}
		}
		//Advance to the direct successor
		current = current->direct_successor;
	}
}


/**
 * Run through every instruction in a block and construct the live ranges
 */
static void construct_live_ranges_in_block(cfg_t* cfg, dynamic_array_t* live_ranges, basic_block_t* basic_block){
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
		//Special case - we only need to add the assignee here 
		if(current->instruction_type == PHI_FUNCTION){
			//Let's see if we can find this
			live_range_t* live_range = find_live_range_with_variable(live_ranges, current->assignee);

			//If it's null we need to make one
			if(live_range == NULL){
				//Create it
				live_range = live_range_alloc(basic_block->function_defined_in, current->assignee->variable_size);

				//Add it into the overall set
				dynamic_array_add(live_ranges, live_range);
			}

			//Add this into the live range
			add_variable_to_live_range(live_range, basic_block, current->assignee);

			//And we're done - no need to go further
			current = current->next_statement;
			continue;
		}

		/**
		 * If we make it here, we know that we have a normal instruction that exists on
		 * the target architecture. Here we can construct our live ranges and exploit any opportunities
		 * for live range coalescing
		 */

		//If we actually have a destination register
		if(current->destination_register != NULL){
			//Let's see if we can find this
			live_range_t* live_range = find_live_range_with_variable(live_ranges, current->destination_register);

			//If it's null we need to make one
			if(live_range == NULL){
				//Create it
				live_range = live_range_alloc(basic_block->function_defined_in, current->destination_register->variable_size);

				//Add it into the overall set
				dynamic_array_add(live_ranges, live_range);
			}

			//Add this into the live range
			add_variable_to_live_range(live_range, basic_block, current->destination_register);

			//Link the variable into this as well
			current->destination_register->associated_live_range = live_range;
		}

		//Let's also assign all the live ranges that we need to the given variables since we're already 
		//iterating like this
		if(current->source_register != NULL){
			assign_live_range_to_variable(live_ranges, basic_block, current->source_register);
		}

		if(current->source_register2 != NULL){
			assign_live_range_to_variable(live_ranges, basic_block, current->source_register2);
		}

		if(current->address_calc_reg1 != NULL){
			assign_live_range_to_variable(live_ranges, basic_block, current->address_calc_reg1);
		}

		if(current->address_calc_reg2 != NULL){
			assign_live_range_to_variable(live_ranges, basic_block, current->address_calc_reg2);
		}

		//Advance it down
		current = current->next_statement;
	}
}


/**
 * Some variables need to be in special registers at a given time. We can
 * bind them to the right register at this stage and avoid having to worry about it later
 */
static void pre_color(instruction_t* instruction){
	/**
	 * The first thing will check for here is after-call function parameters. These
	 * need to be allocated appropriately
	 */

	//One thing to check for - function parameter passing
	if(instruction->destination_register != NULL && instruction->destination_register->linked_var != NULL
		&& instruction->destination_register->linked_var->function_parameter_order > 0){
		//Allocate accordingly
		instruction->destination_register->associated_live_range->reg = parameter_registers[instruction->destination_register->linked_var->function_parameter_order - 1];
		instruction->destination_register->associated_live_range->is_precolored = TRUE;
	}

	//One thing to check for - function parameter passing
	if(instruction->source_register != NULL && instruction->source_register->linked_var != NULL
		&& instruction->source_register->linked_var->function_parameter_order > 0){
		//Allocate accordingly
		instruction->source_register->associated_live_range->reg = parameter_registers[instruction->source_register->linked_var->function_parameter_order - 1];
		instruction->source_register->associated_live_range->is_precolored = TRUE;
	}

	//Check source 2 as well
	if(instruction->source_register2 != NULL && instruction->source_register2->linked_var != NULL
		&& instruction->source_register2->linked_var->function_parameter_order > 0){
		//Allocate accordingly
		instruction->source_register2->associated_live_range->reg = parameter_registers[instruction->source_register2->linked_var->function_parameter_order - 1];
		instruction->source_register2->associated_live_range->is_precolored = TRUE;
	}

	//Check address calc 1 as well
	if(instruction->address_calc_reg1 != NULL && instruction->address_calc_reg1->linked_var != NULL
		&& instruction->address_calc_reg1->linked_var->function_parameter_order > 0){
		//Allocate accordingly
		instruction->address_calc_reg1->associated_live_range->reg = parameter_registers[instruction->address_calc_reg1->linked_var->function_parameter_order - 1];
		instruction->address_calc_reg1->associated_live_range->is_precolored = TRUE;
	}

	//Check address calc 2 as well
	if(instruction->address_calc_reg2 != NULL && instruction->address_calc_reg2->linked_var != NULL
		&& instruction->address_calc_reg2->linked_var->function_parameter_order > 0){
		//Allocate accordingly
		instruction->address_calc_reg2->associated_live_range->reg = parameter_registers[instruction->address_calc_reg2->linked_var->function_parameter_order - 1];
		instruction->address_calc_reg2->associated_live_range->is_precolored = TRUE;
	}

	//Pre-color based on what kind of instruction it is
	switch(instruction->instruction_type){
		//If a return instruction has a
		//value, it must be in %RAX so we can assign
		//that entire live range to %RAX
		case RET:
			//If it has one, assign it
			if(instruction->source_register != NULL){
				instruction->source_register->associated_live_range->reg = RAX;
				instruction->source_register->associated_live_range->is_precolored = TRUE;
			}
			break;
		case MOVB:
		case MOVL:
		case MOVQ:
		case MOVW:
			//If we're moving into something preparing for division, this needs
			//to be in RAX
			if(instruction->next_statement != NULL &&
				(instruction->next_statement->instruction_type == CLTD || instruction->next_statement->instruction_type == CQTO)
				&& instruction->next_statement->next_statement != NULL
				&& (is_division_instruction(instruction->next_statement->next_statement) == TRUE
				|| is_modulus_instruction(instruction->next_statement->next_statement) == TRUE)){
				//This needs to be in RAX
				instruction->destination_register->associated_live_range->reg = RAX;
				instruction->destination_register->associated_live_range->is_precolored = TRUE;

			//We also need to check for all kinds of paremeter passing
			} else if(instruction->destination_register->parameter_number > 0){
				instruction->destination_register->associated_live_range->reg = parameter_registers[instruction->destination_register->parameter_number - 1];
				instruction->destination_register->associated_live_range->carries_function_param = TRUE;
				instruction->destination_register->associated_live_range->is_precolored = TRUE;
			}

			break;

		case MULB:
		case MULW:
		case MULL:
		case MULQ:
			//The destination must be in RAX here
			instruction->destination_register->associated_live_range->reg = RAX;
			instruction->destination_register->associated_live_range->is_precolored = TRUE;
			break;


		case DIVB:
		case DIVW:
		case DIVL:
		case DIVQ:
		case IDIVB:
		case IDIVW:
		case IDIVL:
		case IDIVQ:
			//The destination must be in RAX here
			instruction->destination_register->associated_live_range->reg = RAX;
			instruction->destination_register->associated_live_range->is_precolored = TRUE;
			break;

		case DIVB_FOR_MOD:
		case DIVW_FOR_MOD:
		case IDIVB_FOR_MOD:
		case IDIVW_FOR_MOD:
		case DIVL_FOR_MOD:
		case DIVQ_FOR_MOD:
		case IDIVL_FOR_MOD:
		case IDIVQ_FOR_MOD:
			//The destination for all division remainders is RDX
			instruction->destination_register->associated_live_range->reg = RDX;
			instruction->destination_register->associated_live_range->is_precolored = TRUE;
			break;

		//Function calls always return through rax
		case CALL:
			//We could have a void return, but usually we'll give something
			if(instruction->destination_register != NULL){
				instruction->destination_register->associated_live_range->reg = RAX;
				instruction->destination_register->associated_live_range->is_precolored = TRUE;
			}

			/**
			 * We also need to allocate the parameters for this function. Conviently,
			 * they are already stored for us so we don't need to do anything like
			 * what we had to do for pre-allocating function parameters
			 */

			//Grab the parameters out
			dynamic_array_t* function_params = instruction->function_parameters;

			//If we actually have function parameters
			if(function_params != NULL){
				for(u_int16_t i = 0; i < function_params->current_index; i++){
					//Grab it out
					three_addr_var_t* param = dynamic_array_get_at(function_params, i);

					//Now that we have it, we'll grab it's live range
 					live_range_t* param_live_range = param->associated_live_range;

					//This is precolored
					param_live_range->is_precolored = TRUE;

					//And we'll use the function param list to precolor appropriately
					param_live_range->reg = parameter_registers[i];
				}
			}

			break;

		//Most of the time we will get here
		default:
			break;
	}
}


/**
 * Reset all live ranges in the given array
 */
static void reset_all_live_ranges(dynamic_array_t* live_ranges){
	//Run through every live range in the array
	for(u_int16_t i = 0; i < live_ranges->current_index; i++){
		//Grab the live range out
		live_range_t* current = dynamic_array_get_at(live_ranges, i);

		//We'll reset the register if it's not pre-colored
		if(current->is_precolored == FALSE){
			current->reg = NO_REG;
		}

		//And we'll also reset all of the neighbors
		reset_dynamic_array(current->neighbors);
	}
}


/**
 * Construct the interference graph using LIVENOW sets
 *
 * NOTE: We must walk the block from bottom to top
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
static interference_graph_t* construct_interference_graph(cfg_t* cfg, dynamic_array_t* live_ranges){
	//First thing that we'll do is reset all live ranges
	reset_all_live_ranges(live_ranges);

	//It starts off as null
	interference_graph_t* graph = NULL;

	//We'll first need a pointer
	basic_block_t* current = cfg->head_block;

	//Run through every block in the CFG's ordered set
	while(current != NULL){
		//Just check for this case first
		if(current->live_out == NULL){
			current = current->direct_successor;
			continue;
		}

		//live now is initially live out. Just settigg this pointer
		//for naming congruety
		dynamic_array_t* live_now = current->live_out;

		//Even though we use the LIVENOW set in name, in reality it is just LIVEOUT. We'll
		//keep using LIVEOUT for LIVENOW with that in mind
		
		//Grab a pointer to the operations
		instruction_t* operation = current->exit_statement;

		//For every operation that we have
		while(operation != NULL){
			//Hitch a ride on this traversal to do pre-coloring
			pre_color(operation);

			//Let's check to see if any pre-spilling has affected us here
			if(operation->CLASS == THREE_ADDR_CODE_MEM_ADDR_ASSIGNMENT){
				printf("\n\n\n\n\n\n\nMust be spilled\n");

				//We'll just need to use op1's stack offset here
				operation->offset->long_const = operation->op1->stack_offset;

				printf("stack offset is %d\n", operation->op1->stack_offset);

				//Make sure that we unset this flag
				if(operation->offset->long_const != 0){
					operation->offset->is_value_0 = FALSE;
				}
			}

			//If we have an exact copy operation, we can
			//skip it as it won't create any interference
			if(operation->instruction_type == PHI_FUNCTION){
				//Skip it
				operation = operation->previous_statement;
				continue;
			}

			//If the destination register here is NULL, we
			//may actually be dealing with a comparison operation of some kind.
			//In doing this, we'll want to still mark the operands as LIVE
			if(operation->destination_register == NULL){
				//Now we'll add all interferences like this
				if(operation->source_register != NULL
					&& dynamic_array_contains(live_now, operation->source_register->associated_live_range) == NOT_FOUND){
					dynamic_array_add(live_now, operation->source_register->associated_live_range);
				}

				//Check the second source register(could be a comparison op)
				if(operation->source_register2 != NULL
					&& dynamic_array_contains(live_now, operation->source_register2->associated_live_range) == NOT_FOUND){
					dynamic_array_add(live_now, operation->source_register2->associated_live_range);
				}

				//Check the address calc registers
				if(operation->address_calc_reg1 != NULL
					&& dynamic_array_contains(live_now, operation->address_calc_reg1->associated_live_range) == NOT_FOUND){
					dynamic_array_add(live_now, operation->address_calc_reg1->associated_live_range);
				}
				
				//Check the address calc registers
				if(operation->address_calc_reg2 != NULL
					&& dynamic_array_contains(live_now, operation->address_calc_reg2->associated_live_range) == NOT_FOUND){
					dynamic_array_add(live_now, operation->address_calc_reg2->associated_live_range);
				}


				operation = operation->previous_statement;
				continue;
			}

			//Now that we know this operation is valid, we will add interference between this and every
			//other value in live_out
			for(u_int16_t i = 0; i < live_now->current_index; i++){
				//Graph the LR out
				live_range_t* range = dynamic_array_get_at(live_now, i);

				//Now we'll add this to the graph
				add_interference(graph, operation->destination_register->associated_live_range, range);
			}

			//Once we're done with this, we'll delete the destination's live range from the LIVE_NOW set
			//HOWEVER: we must account for the fact that x86 instructions often use the second operand
			//as a destination. If this is not the case, then we can't remove this because we aren't done
			if(is_destination_also_operand(operation) == TRUE){
				//Even beyond this, since this is a source, we'll need to add it
				if(dynamic_array_contains(live_now, operation->destination_register->associated_live_range) == NOT_FOUND){
					dynamic_array_add(live_now, operation->destination_register->associated_live_range);
				}
			//Otherwise we can delete
			} else {
				dynamic_array_delete(live_now, operation->destination_register->associated_live_range);
			}

			//Now we'll add all interferences like this
			if(operation->source_register != NULL
				&& dynamic_array_contains(live_now, operation->source_register->associated_live_range) == NOT_FOUND){
				dynamic_array_add(live_now, operation->source_register->associated_live_range);
			}

			if(operation->source_register2 != NULL
				&& dynamic_array_contains(live_now, operation->source_register2->associated_live_range) == NOT_FOUND){
				dynamic_array_add(live_now, operation->source_register2->associated_live_range);
			}

			if(operation->address_calc_reg1 != NULL
				&& dynamic_array_contains(live_now, operation->address_calc_reg1->associated_live_range) == NOT_FOUND){
				dynamic_array_add(live_now, operation->address_calc_reg1->associated_live_range);
			}

			if(operation->address_calc_reg2 != NULL
				&& dynamic_array_contains(live_now, operation->address_calc_reg2->associated_live_range) == NOT_FOUND){
				dynamic_array_add(live_now, operation->address_calc_reg2->associated_live_range);
			}

			//Crawl back up by 1
			operation = operation->previous_statement;
		}

		//Advance this up
		current = current->direct_successor;
	}

	//Now at the very end, we'll construct the matrix
	graph = construct_interference_graph_from_adjacency_lists(live_ranges);
	return graph;
}


/**
 * Create the stack pointer live range
 */
static live_range_t* construct_stack_pointer_live_range(three_addr_var_t* stack_pointer){
	//Before we go any further, we'll construct the live
	//range for the stack pointer. Special case here - stack pointer has no block
	live_range_t* stack_pointer_live_range = live_range_alloc(NULL, QUAD_WORD);
	//This is guaranteed to be RSP - so it's already been allocated
	stack_pointer_live_range->reg = RSP;
	//And we absolutely *can not* spill it
	stack_pointer_live_range->spill_cost = INT16_MAX;

	//This is precolor
	stack_pointer_live_range->is_precolored = TRUE;

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


	//Run through every single block
	while(current != NULL){
		//Let the helper do this
		construct_live_ranges_in_block(cfg, live_ranges, current);

		//Advance to the next
		current = current->direct_successor;
	}

	//Placehold
	return live_ranges;
}


/**
 * Spill an assignment instruction by emitting a store statement to add this into
 * memory. This is easier than a use spill because all we need to do
 * is insert a store instruction right after the use
 */
static void handle_assignment_spill(cfg_t* cfg, three_addr_var_t* var, live_range_t* spill_range, instruction_t* instruction, u_int64_t offset){
	//Let the helper do this for us
	instruction_t* store = emit_store_instruction(var, cfg->stack_pointer, cfg->type_symtab, offset);

	//Grab the block out too
	basic_block_t* block = instruction->block_contained_in;

	//Link this in too
	store->block_contained_in = block;

	//Now all that's left to do is insert this after the instruction where it's the destination
	//Grab out what used to be the next statement
	instruction_t* old_next = instruction->next_statement;

	//The store is now the next statement
	instruction->next_statement = store;

	//Link this one in
	store->next_statement = old_next;
	store->previous_statement = instruction;

	//It can either have a next one that we need to link in, or
	//it's at the end of a block
	if(old_next != NULL){
		old_next->previous_statement = store;
	} else {
		block->exit_statement = store;
	}
}


/**
 * Spill a use into memory and replace the live ranges appropriately
 *
 * We will be emitting a new live range here, so we should give it back as a pointer
 */
static three_addr_var_t* handle_use_spill(cfg_t* cfg, dynamic_array_t* live_ranges, three_addr_var_t* affected_var, live_range_t* spill_range, instruction_t* instruction, u_int64_t offset){
	//Copy the old variable
	three_addr_var_t* new_var = emit_var_copy(affected_var);

	//Grab the block out too
	basic_block_t* block = instruction->block_contained_in;

	//Create a new live range just for this variable
	new_var->associated_live_range = live_range_alloc(block->function_defined_in, affected_var->variable_size);

	//Add this variable to this live range
	add_variable_to_live_range(new_var->associated_live_range, block, new_var);

	//Add this in to our current list of live ranges
	dynamic_array_add(live_ranges, new_var->associated_live_range);

	//Now we'll want to load from memory
	instruction_t* load = emit_load_instruction(new_var, cfg->stack_pointer, cfg->type_symtab, offset);

	//Link the load instruction with what block it's in
	load->block_contained_in = block;

	//Grab the previous one out
	instruction_t* previous = instruction->previous_statement;

	//Link load and the prior instruction
	load->previous_statement = previous;
	if(previous != NULL){
		previous->next_statement = load;
	} else {
		block->leader_statement = previous;
	}

	//And link the instruction to the load
	load->next_statement = instruction;
	instruction->previous_statement = load;

	//Give back this live range now
	return new_var;
}


/**
 * Spill a live range to memory to make a graph N-colorable
 *
 * After a live range is spilled, all definitions go to memory, and all uses
 * come from memory(stack memory)
 *
 * RULES:
 * Every assignment must now be followed by a store 
 * Every use must be preceeded by a load
 */
static void spill(cfg_t* cfg, dynamic_array_t* live_ranges, live_range_t* spill_range){
	//Since we are spilling to the stack, we'll need to get the stack_data_area structure
	//out from whichever function this is from

	//We'll need this for the stack offset
	three_addr_var_t* var = dynamic_array_get_at(spill_range->variables, 0);

	//Now that we have the data area, we'll need to add enough space for the new variable
	//in the stack data area
	add_spilled_variable_to_stack(&(spill_range->function_defined_in->data_area), var);

	//Now we'll grab out this one's offset
	u_int32_t stack_offset = var->stack_offset;

	//Now that we've added this in, we'll need to go through and add 
	//the loads and stores
	
	//Optimization - live-ranges are function level, so we'll just go through the function
	//blocks until we find one that matches this function
	basic_block_t* function_block;
	for(u_int16_t i = 0; i < cfg->function_blocks->current_index; i++){
		//Grab the block out
		function_block = dynamic_array_get_at(cfg->function_blocks, i);

		//We've got our match
		if(function_block->function_defined_in == spill_range->function_defined_in){
			break;
		}
	}

	/**
	 * Keep track of what is currently spilled. If something is currently spilled
	 * and we have not yet modified the value(written to destination), then we
	 * don't need to keep loading at every use
	 */
	live_range_t* currently_spilled = NULL;

	//Now we have our function block, and we'll crawl it until we reach the end
	while(function_block != NULL && function_block->function_defined_in == spill_range->function_defined_in){
		//Now we'll crawl this block and find every place where this live range is used/defined
		instruction_t* current = function_block->leader_statement;

		//Crawl through every block
		while(current != NULL){
			//We'll also need a use spill here
			if(is_destination_also_operand(current) == TRUE
				&& current->destination_register->associated_live_range == spill_range){
				if(currently_spilled == NULL){
					//We'll need to handle it like this
					current->destination_register = handle_use_spill(cfg, live_ranges, current->destination_register, spill_range, current, stack_offset);
					currently_spilled = current->destination_register->associated_live_range;
				}
			}

			//We'll need a use spill if this is the case
			//Handle the case for the source register
			if(current->source_register != NULL
				&& current->source_register->associated_live_range == spill_range){
				if(currently_spilled == NULL){
					//Let the helper deal with it
					current->source_register = handle_use_spill(cfg, live_ranges, current->source_register, spill_range, current, stack_offset);
					currently_spilled = current->source_register->associated_live_range;
				}
			}

			//Check this register as well
			if(current->source_register2 != NULL
				&& current->source_register2->associated_live_range == spill_range){
				if(currently_spilled == NULL){
					//Let the helper deal with it
					current->source_register2 = handle_use_spill(cfg, live_ranges, current->source_register2, spill_range, current, stack_offset);
					currently_spilled = current->source_register2->associated_live_range;
				}
			}

			//Check this register as well
			if(current->address_calc_reg1 != NULL
				&& current->address_calc_reg1->associated_live_range == spill_range){
				if(currently_spilled == NULL){
					//Let the helper deal with it
					handle_use_spill(cfg, live_ranges, current->address_calc_reg1, spill_range, current, stack_offset);
				}
			}

			//Check this register as well
			if(current->address_calc_reg2 != NULL
				&& current->address_calc_reg2->associated_live_range == spill_range){
				if(currently_spilled == NULL){
					//Let the helper deal with it
					handle_use_spill(cfg, live_ranges, current->address_calc_reg2, spill_range, current, stack_offset);
				}
			}

			/**
			 * We'll check for destination spills at the very end because this requires
			 * us to reset the currently spilled var after we load to memory
			 */
			if(current->destination_register != NULL
				&& current->destination_register->associated_live_range == spill_range){
				//Let the helper deal with it
				handle_assignment_spill(cfg, current->destination_register, spill_range, current, stack_offset);

				//Reset currenlty spilled
				currently_spilled = NULL;

				//Advance this up by 1 to get past the statement we just added in
				current = current->next_statement;

			/**
			 * We could also have a case where this is what was currently spilled. If so,
			 * we'll also need to store that
			 */
			} else if(current->destination_register != NULL 
						&& current->destination_register->associated_live_range == currently_spilled){
				//Let the helper deal with it
				handle_assignment_spill(cfg, current->destination_register, currently_spilled, current, stack_offset);

				//Reset currenlty spilled
				currently_spilled = NULL;

				//Advance this up by 1 to get past the statement we just added in
				current = current->next_statement;
			}

			//Advance the pointer
			current = current->next_statement;
		}

		//Advance it up
		function_block = function_block->direct_successor;
	}
	
	//Once we're done spilling, this live range is now completely useless to us
}


/**
 * Pre-spill any live ranges that must be spilled
 */
static void pre_spill(cfg_t* cfg, dynamic_array_t* live_ranges) {
	u_int16_t max_index = live_ranges->current_index;

	//Run through and see what must be spilled
	for(u_int16_t i = 0; i < max_index; i++){
		//Do we need to spill this one
		live_range_t* range = dynamic_array_get_at(live_ranges, i);

		//Does this need to be spilled?
		if(range->must_be_spilled == TRUE){
			spill(cfg, live_ranges, range);

			//Set it to false now so we don't respill it
			range->must_be_spilled = FALSE;
		}
	}
}


/**
 *
 * Allocate an individual register to a given live range
 *
 * We return TRUE if we were able to color, and we return false if we were not
 */
static u_int8_t allocate_register(interference_graph_t* graph, dynamic_array_t* live_ranges, live_range_t* live_range){
	//If this is the case, we're already done. This will happen in the event that a register has been pre-colored
	if(live_range->reg != NO_REG){
		return TRUE;
	}

	//Allocate an area that holds all the registers that we have available for use. This is offset by 1 from
	//the actual value in the enum. For example, RAX is 1 in the enum, so it's 0 in here
	u_int8_t registers[K_COLORS_GEN_USE];

	//Wipe this entire thing out
	memset(registers, 0, sizeof(u_int8_t) * K_COLORS_GEN_USE);

	//Run through every single neighbor
	for(u_int16_t i = 0; i < live_range->neighbors->current_index; i++){
		//Grab the neighbor out
		live_range_t* neighbor = dynamic_array_get_at(live_range->neighbors, i);

		//Get whatever register this neighbor has. If it's not the "no_reg" value, 
		//we'll store it in the array
		if(neighbor->reg != NO_REG && neighbor->reg <= K_COLORS_GEN_USE){
			//Flag it as used
			registers[neighbor->reg - 1] = TRUE;
		}
	}
	
	//Now that the registers array has been populated with interferences, we can scan it and
	//pick the first available register
	u_int16_t i;
	for(i = 0; i < K_COLORS_GEN_USE; i++){
		//If we've found an empty one, that means we're good
		if(registers[i] == FALSE){
			break;
		}
	}

	//Now that we've gotten here, i should hold the value of a free register - 1. We'll
	//add 1 back to it to get that free register's name
	if(i < K_COLORS_GEN_USE){
		//Assign the register value to it
		live_range->reg = i + 1;

		//Flag this as used in the function
		live_range->function_defined_in->used_registers[i] = TRUE;

		//Return true here
		return TRUE;

	//This means that our neighbors allocated all of the registers available
	} else {
		return FALSE;
	}
}


/**
 * Perform graph coloring to allocate all registers in the interference graph
 *
 * Graph coloring is used as a way to model this problem. For us, no two interfering
 * live ranges may have the same register. In graph coloring, no two adjacent nodes
 * may have the same color. It is easy to see how these problems resemble eachother.
 *
 * Algorithm graphcolor:
 * 	for all live ranges in interference graph:
 * 		if live_range's degree is less than N:
 * 			remove it
 * 			color it
 * 		else:
 * 			if live_range can still be colored:
 * 				remove it and color it
 * 			else:
 * 				spill and rewrite the whole program, and 
 * 				redo all allocation
 *
 *
 * Return TRUE if the graph was colorable, FALSE if not
 */
static u_int8_t graph_color_and_allocate(cfg_t* cfg, dynamic_array_t* live_ranges, interference_graph_t* graph){
	//We first need to construct the priority version of the live range arrays
	//Create a new one
	dynamic_array_t* priority_live_ranges = dynamic_array_alloc();

	//Run through and insert everything into the priority live range
	for(u_int16_t i = 0; i < live_ranges->current_index; i++){
		dynamic_array_priority_insert_live_range(priority_live_ranges, dynamic_array_get_at(live_ranges, i));
	}

	//So long as this isn't empty
	while(dynamic_array_is_empty(priority_live_ranges) == FALSE){
		//Grab a live range out by deletion
		live_range_t* range = dynamic_array_delete_from_back(priority_live_ranges);

		//Now that we have it, we'll color it
		if(range->degree < K_COLORS_GEN_USE){
			allocate_register(graph, live_ranges, range);
		//Otherwise, we may still be able to allocate here
		} else {
			//We must still attempt to allocate it
			u_int8_t can_allocate = allocate_register(graph, live_ranges, range);
			
			//However if this is false, we need to perform a spill
			if(can_allocate == FALSE){
				printf("\n\n\nCould not allocate: LR%d\n", range->live_range_id);

				/**
				 * Now we need to spill this live range. It is important to note that
				 * spilling has the effect of completely rewriting the entire program.
				 * As such, once we spill, we need to redo everything, including the entire 
				 * graph coloring process. This will require a reset. In practice, even
				 * the most extreme programs only require that this be done once or twice
				 */
				spill(cfg, live_ranges, range);

				//We could not allocate everything here, so we need to return false to
				//trigger the restart by the process
				return FALSE;
			}
		}
	}

	//Destroy the dynamic array
	dynamic_array_dealloc(priority_live_ranges);

	//Give back true because if we made it here, our graph was N-colorable
	//and we did not have a spill
	return TRUE;
}


/**
 * Repeate the process of register allocation until the sub-function
 * returns TRUE. TRUE means that we were able to allocate all registers without a need for a spill
 */
static void allocate_registers(cfg_t* cfg, dynamic_array_t* live_ranges, interference_graph_t* graph){
	//Let the helper function color everything. If the graph was not colorable, then
	//we need to redo everything after a spill
	u_int8_t colorable = graph_color_and_allocate(cfg, live_ranges, graph);

	//So long as this wasn't colorable, we need to keep doing this
	
	while(colorable == FALSE){
		printf("============= Retrying with ====================\n");

		//Show our live ranges once again
		print_all_live_ranges(live_ranges);
		print_blocks_with_live_ranges(cfg->head_block);
		//We now need to compute all of the LIVE OUT values
		calculate_liveness_sets(cfg);

		//Now let's determine the interference graph
		graph = construct_interference_graph(cfg, live_ranges);

		//Now we retry
		colorable = graph_color_and_allocate(cfg, live_ranges, graph);
	}
}


/**
 * Run through the current function and insert all needed save/restore logic
 * for caller-saved registers
 */
static void insert_caller_saved_register_logic(basic_block_t* current_function, heap_stack_t* stack){
	//We'll grab out everything we need from this function
	//Extract this for convenience
	symtab_function_record_t* function = current_function->function_defined_in;

	//Define a cursor for crawling
	basic_block_t* cursor = current_function;

	//So long as we're in this current function, keep going
	while(cursor != NULL && cursor->function_defined_in == function){
		//Now we'll grab a hook to the first statement
		instruction_t* instruction = cursor->leader_statement;

		//Now we'll run through every single instruction in here
		while(instruction != NULL){
			//If this is not a function call, we don't really care, so
			//just advance(most likely case)
			if(instruction->instruction_type != CALL){
				instruction = instruction->next_statement;
				continue;
			}

			//Define a dynamic array for saving the live ranges that will
			//need to be saved
			dynamic_array_t* saving_array = dynamic_array_alloc();

			//If we get here we know that we have a call instruction. Let's
			//grab whatever it's calling out
			symtab_function_record_t* callee = instruction->called_function;

			//Every function is guaranteed to have a return value/result
			live_range_t* result_lr = instruction->destination_register->associated_live_range; 

			//We can crawl this Live Range's neighbors to see what is interefering with it. Once
			//we know what is interfering, we can see which registers they use and compare that 
			//with the register array
			for(u_int16_t i = 0; result_lr->neighbors != NULL && i < result_lr->neighbors->current_index; i++){
				//Grab the neighbor out
				live_range_t* lr = dynamic_array_get_at(result_lr->neighbors, i);

				//And grab it's register out
				register_holder_t reg = lr->reg;

				//If it isn't caller saved, we don't care
				if(is_register_caller_saved(reg) == FALSE){
					continue;
				}

				//If the interference array is true *and* it's a neighbor's
				//register, we'll save it
				if(callee->used_registers[reg - 1] == TRUE){
					//Flag this LR in here
					dynamic_array_add(saving_array, lr);
				}
			}

			//If we don't have at least one, just skip on to the next one
			if(saving_array->current_index == 0){
				//Destroy the saving array
				dynamic_array_dealloc(saving_array);

				//Go onto the next one
				instruction = instruction->next_statement;
				continue;
			}
			
			//We'll need a heap stack to do this
			heap_stack_t* stack = heap_stack_alloc();

			//These variables will be convenient for dealing with the addition of instructions
			instruction_t* call_instruction = instruction;
			instruction_t* before_push = call_instruction->previous_statement;

			//Once we make it all the way down here, we know exactly which registers that we need to save before this function call.
			//We can now run through the saving array to do this
			for(u_int16_t i = 0; i < saving_array->current_index; i++){
				//Grab the live-range out
				live_range_t* lr = dynamic_array_get_at(saving_array, i);
				
				//Save this LR onto the stack for later
				push(stack, lr);

				//Emit a push instruction with the live range as the source
				instruction_t* push_inst = emit_push_instruction(dynamic_array_get_at(lr->variables, 0));

				//We always put this push instruction above the function call, in between it and whatever else was last there
				before_push->next_statement = push_inst;
				push_inst->previous_statement = before_push;

				//Link it in with the call instruction
				push_inst->next_statement = call_instruction;
				call_instruction->previous_statement = push_inst;

				//Update what this is
				before_push = push_inst;
			}

			//And now, we'll need to go through the stack and add these all back in reverse-order. We'll
			//always be adding after what we most recently added
			
			//The last instruction is always the call instruction first
			instruction_t* last_instruction = instruction;

			//So long as the stack isn't empty
			while(heap_stack_is_empty(stack) == HEAP_STACK_NOT_EMPTY){
				//Grab the live range off of it
				live_range_t* current = pop(stack);

				//Emit the pop instruction for this
				instruction_t* pop_inst = emit_pop_instruction(dynamic_array_get_at(current->variables, 0));

				//Tie this in to what comes after it
				pop_inst->next_statement = last_instruction->next_statement;

				//Tie it in with the previous as well
				if(pop_inst->next_statement != NULL){
					pop_inst->next_statement->previous_statement = pop_inst;
				}

				//And now we'll tie in the last instruction
				last_instruction->next_statement = pop_inst;
				pop_inst->previous_statement = last_instruction;

				//The pop inst now is the last instruction we've seen
				last_instruction = pop_inst;
			}
			
			//Destroy the heapstack
			heap_stack_dealloc(stack);
			//Destroy the dynamic array
			dynamic_array_dealloc(saving_array);

			//Onto the next instruction
			instruction = instruction->next_statement;
		}

		//Advance down to the direct successor
		cursor = cursor->direct_successor;
	}
}


/**
 * Now that we are done spilling, we need to insert all of the stack logic,
 * including additions and subtractions, into the functions. We also need
 * to insert pushing of any/all callee saved and caller saved registers to maintain
 * our calling convention
 */
static void insert_all_stack_and_saving_logic(cfg_t* cfg){
	//We need to run through all of the used registers and make a note of all callee-saved registers
	//in here we'll use a lightstack to keep track of the pushing/popping logic in here as well
	heap_stack_t* heap_stack = heap_stack_alloc();

	//Run through every function entry point in the CFG
	for(u_int16_t i = 0; i < cfg->function_blocks->current_index; i++){
		//Reset the heap stack every time
		reset_heap_stack(heap_stack);

		//Important to maintain this, it's initially null
		instruction_t* last_push_instruction = NULL;

		//Grab it out
		basic_block_t* current_function_entry = dynamic_array_get_at(cfg->function_blocks, i);

		//Grab the function defined in as well
		symtab_function_record_t* function = current_function_entry->function_defined_in;

		//We need to see which registers that we use
		for(u_int16_t i = 0; i < 15; i++){
			//We don't use this register, so move on
			if(function->used_registers[i] == FALSE){
				continue;
			}

			//Otherwise if we get here, we know that we use it. Remember
			//the register value is always offset by one
			register_holder_t used_reg = i + 1;

			//If this isn't callee saved, then we know to move on
			if(is_register_callee_saved(used_reg) == FALSE){
				continue;
			}

			//Create the current live range here
			live_range_t* range = live_range_alloc(function, QUAD_WORD);
			
			//Give it the appropriate register
			range->reg = used_reg;

			//Now we'll need a variable to carry this live range in it
			three_addr_var_t* var = emit_temp_var_from_live_range(range);

			//Add this onto the stack
			push(heap_stack, var);

			//Now we'll need to add an instruction to push this at the entry point of our function
			instruction_t* push = emit_push_instruction(var);

			//Now we'll add this right after the last push instruction. This is important because we need
			//to maintain the stack structure for popping
			if(last_push_instruction == NULL){
				//Forward link to the head
				push->next_statement = current_function_entry->leader_statement;

				//Link this backwards too
				if(current_function_entry->leader_statement != NULL){
					current_function_entry->leader_statement->previous_statement = push;
				}

				//This now is the head
				current_function_entry->leader_statement = push;

				//This now is the last push instruction
				last_push_instruction = push;

			//Otherwise it wasn't null, so we'll need to add it after the last push instruction
			} else {
				//This one's next now points to the last one's next
				push->next_statement = last_push_instruction->next_statement;

				//Backwards link it too
				if(push->next_statement != NULL){
					push->next_statement->previous_statement = push;
				}

				//Link these in as well
				last_push_instruction->next_statement = push;
				push->previous_statement = last_push_instruction;

				//Save for the next go around
				last_push_instruction = push;
			}
		}

		//We'll also need it's stack data area
		stack_data_area_t area = current_function_entry->function_defined_in->data_area;

		//Align it
		align_stack_data_area(&area);

		//Grab the total size out
		u_int32_t total_size = area.total_size;

		if(total_size != 0){
			//For each function entry block, we need to emit a stack subtraction that is the size of that given variable
			instruction_t* stack_allocation = emit_stack_allocation_statement(cfg->stack_pointer, cfg->type_symtab, total_size);

			//Stack allocation always goes after the last push instruction, or it becomes the head
			if(last_push_instruction == NULL){
				current_function_entry->leader_statement->previous_statement = stack_allocation;
				stack_allocation->next_statement = current_function_entry->leader_statement;

				//This is now the head
				current_function_entry->leader_statement = stack_allocation;

			//If we get here, we know that we need to go after the last push instruction
			} else {
				//Link this one's next in here
				stack_allocation->next_statement = last_push_instruction->next_statement;
				last_push_instruction->next_statement->previous_statement = stack_allocation;

				last_push_instruction->next_statement = stack_allocation;
				stack_allocation->previous_statement = last_push_instruction;
			}
		}
		
		//Now we need to go through this entire function and find any/all return statements. They would always
		//be exit statements, so this is how we will hunt for them
		basic_block_t* cursor = current_function_entry;

		//Crawl through the whole thing until we hit the end of the function
		while((total_size != 0 || heap_stack_is_empty(heap_stack) == HEAP_STACK_NOT_EMPTY)
				&& cursor != NULL && cursor->function_defined_in == current_function_entry->function_defined_in){
			//If we have an empty block for some reason or we don't have a return we're done, so move on
			if(cursor->exit_statement == NULL || cursor->exit_statement->instruction_type != RET){
				cursor = cursor->direct_successor;
				continue;
			}

			//Now that we got here, we know that the exit statement is a ret statement
			instruction_t* ret_stmt = cursor->exit_statement;
			//Grab the previous statement too
			instruction_t* previous = ret_stmt->previous_statement;

			//We need to add all of the popping of our callee-saved registers here. We will crawl
			//through the stack without damaging it, because there may be more than one return statement
			//that is in need of this popping logic
			stack_node_t* current = heap_stack->top;

			//Run through and add them all in
			while(current != NULL){
				//Emit the pop instruction
				instruction_t* pop = emit_pop_instruction(current->data);

				//We'll now tie this in here
				pop->previous_statement = previous;

				//Most common case
				if(previous != NULL){
					previous->next_statement = pop;
				} else {
					//This is the leader statement(exceptionally rare)
					cursor->leader_statement = pop;
				}

				//Add this one in too
				pop->next_statement = ret_stmt;
				ret_stmt->previous_statement = pop;

				//IMPORTANT - update "previous" so that the next iteration
				//knows what to do
				previous = pop;

				//Advance the stack node
				current = current->next;
			}

			//Here is our deallocation statement
			if(total_size != 0){
				instruction_t* stack_deallocation = emit_stack_deallocation_statement(cfg->stack_pointer, cfg->type_symtab, total_size);

				//Tie these too together
				ret_stmt->previous_statement = stack_deallocation;
				stack_deallocation->next_statement = ret_stmt;

				//Tie this one into previous
				stack_deallocation->previous_statement = previous;

				//Most common case
				if(previous != NULL){
					previous->next_statement = stack_deallocation;
				} else {
					//This is the leader statement(exceptionally rare)
					cursor->leader_statement = stack_deallocation;
				}
			}

			//Advance the cursor up
			cursor = cursor->direct_successor;
		}
		
		//Reset the heapstack once more
		reset_heap_stack(heap_stack);

		//And now we'll let the helper insert all of the caller-saved register logic
		insert_caller_saved_register_logic(current_function_entry, heap_stack);
	}

	//Destroy the heapstack
	heap_stack_dealloc(heap_stack);
}


/**
 * Perform our register allocation algorithm on the entire cfg
 */
void allocate_all_registers(compiler_options_t* options, cfg_t* cfg){
	//Save whether or not we want to actually print IRs
	u_int8_t print_irs = options->print_irs;

	//The first thing that we'll do is reconstruct everything in terms of live ranges
	//This should be simplified by our values already being in SSA form
	dynamic_array_t* live_ranges = construct_all_live_ranges(cfg);

	//If we want to print, we'll show all live ranges
	if(print_irs == TRUE){
		//Print whatever live ranges we did find
		print_all_live_ranges(live_ranges);
	}

	//Pre-spill if we need to
	pre_spill(cfg, live_ranges);

	//We now need to compute all of the LIVE OUT values
	calculate_liveness_sets(cfg);

	//Now let's determine the interference graph
	interference_graph_t* graph = construct_interference_graph(cfg, live_ranges);

	//Again if we want to print, now is the time
	if(print_irs == TRUE){
		printf("============= After Live Range Determination ==============\n");
		print_blocks_with_live_ranges(cfg->head_block);
		printf("============= After Live Range Determination ==============\n");
	}

	//Now let's perform our live range coalescence to reduce the overall size of our
	//graph
	perform_live_range_coalescence(cfg, live_ranges, graph);

	//Show our live ranges once again if requested
	if(print_irs == TRUE){
		print_all_live_ranges(live_ranges);
		printf("================= After Coalescing =======================\n");
		print_blocks_with_live_ranges(cfg->head_block);
		printf("================= After Coalescing =======================\n");
	}
	
	//Let the allocator method take care of everything
	allocate_registers(cfg, live_ranges, graph);

	//Once registers are allocated, we need to crawl and insert all stack allocations/subtractions
	insert_all_stack_and_saving_logic(cfg);

	//One final print post allocation
	if(print_irs == TRUE){
		printf("================= After Allocation =======================\n");
		print_blocks_with_registers(cfg->head_block);
		printf("================= After Allocation =======================\n");
	}
}
