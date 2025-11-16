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
#include "../utils/dynamic_array/dynamic_array.h"
#include "../utils/constants.h"
#include "../interference_graph/interference_graph.h"
#include "../postprocessor/postprocessor.h"
#include "../cfg/cfg.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

//The atomically increasing live range id
u_int32_t live_range_id = 0;

//The array that holds all of our parameter passing
const general_purpose_register_t parameter_registers[] = {RDI, RSI, RDX, RCX, R8, R9};

//Avoid need to rearrange
static interference_graph_t* construct_interference_graph(cfg_t* cfg, dynamic_array_t* live_ranges);
static void spill(cfg_t* cfg, dynamic_array_t* live_ranges, live_range_t* spill_range);

//Just hold the stack pointer live range
static live_range_t* stack_pointer_lr;
//Holds the instruction pointer LR
static live_range_t* instruction_pointer_lr;
//The stack pointer
static three_addr_var_t* stack_pointer;
//And the type symtab
static type_symtab_t* type_symtab;

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
static u_int32_t increment_and_get_live_range_id(){
	return live_range_id++;
}


/**
 * Create a live range
 */
static live_range_t* live_range_alloc(symtab_function_record_t* function_defined_in){
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

	//Finally we'll return it
	return live_range;
}


/**
 * Either find a live range with the given variable or create
 * one if it does not exist
 *
 * NOTE that this function does *not* add anything to the live range
 */
static live_range_t* find_or_create_live_range(dynamic_array_t* live_ranges, basic_block_t* block, three_addr_var_t* variable){
	//Lookup the live range that is associated with this
	live_range_t* live_range = find_live_range_with_variable(live_ranges, variable);

	//If this is not null, then it means that we found it, so we can
	//leave
	if(live_range != NULL){
		return live_range;
	}

	//Otherwise if we get here, we'll need to make it ourselves
	live_range = live_range_alloc(block->function_defined_in);

	//Add it into the live range
	dynamic_array_add(live_ranges, live_range);

	//Give it back
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
	if(block->jump_table != NULL){
		print_jump_table(stdout, block->jump_table);
	}

	//Switch here based on the type of block that we have
	switch(block->block_type){
		//Function entry blocks need extra printing
		case BLOCK_TYPE_FUNC_ENTRY:
			//First print out the function's local constants
			print_local_constants(stdout, block->function_defined_in);

			//Then the name
			printf("%s:\n", block->function_defined_in->func_name.string);
			print_stack_data_area(&(block->function_defined_in->data_area));
			break;

		//By default just print the name
		default:
			printf(".L%d:\n", block->block_id);
			break;
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

	if(block->predecessors != NULL){
		printf("Predecessors: (");
		for(u_int16_t i = 0; i < block->predecessors->current_index; i++){
			basic_block_t* predecessor = dynamic_array_get_at(block->predecessors, i);

			printf(".L%d", predecessor->block_id);

			//If it isn't the very last one, we need a comma
			if(i != block->predecessors->current_index - 1){
				printf(", ");
			}
		}

		printf(")\n");
	}

	if(block->successors != NULL){
		printf("Successors: (");
		for(u_int16_t i = 0; i < block->successors->current_index; i++){
			basic_block_t* successor = dynamic_array_get_at(block->successors, i);

			printf(".L%d", successor->block_id);

			//If it isn't the very last one, we need a comma
			if(i != block->successors->current_index - 1){
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
static void print_blocks_with_live_ranges(cfg_t* cfg){
	//Run through the direct successors so long as the block is not null
	basic_block_t* current = cfg->head_block;

	//So long as this one isn't NULL
	while(current != NULL){
		//Print it
		print_block_with_live_ranges(current);
		//Advance to the direct successor
		current = current->direct_successor;
	}

	//Print all global variables after the blocks
	print_all_global_variables(stdout, cfg->global_variables);
}


/**
 * Print instructions with registers
*/
static void print_block_with_registers(basic_block_t* block){
	//If this is some kind of switch block, we first print the jump table
	if(block->jump_table != NULL){
		print_jump_table(stdout, block->jump_table);
	}

	//Switch here based on the type of block that we have
	switch(block->block_type){
		//Function entry blocks need extra printing
		case BLOCK_TYPE_FUNC_ENTRY:
			//First print out the function's local constants
			print_local_constants(stdout, block->function_defined_in);

			//Then the name
			printf("%s:\n", block->function_defined_in->func_name.string);
			print_stack_data_area(&(block->function_defined_in->data_area));
			break;

		//By default just print the name
		default:
			printf(".L%d:\n", block->block_id);
			break;
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
static void print_blocks_with_registers(cfg_t* cfg){
	//Run through the direct successors so long as the block is not null
	basic_block_t* current = cfg->head_block;

	//So long as this one isn't NULL
	while(current != NULL){
		//Print it
		print_block_with_registers(current);
		//Advance to the direct successor
		current = current->direct_successor;
	}

	//Print all global variables after the blocks
	print_all_global_variables(stdout, cfg->global_variables);
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
 * Update the cost estimate for all live ranges in the event that we need to spill
 * them. This step is crucial in determining the ordering of live range values during register
 * allocation
 */
static void compute_spill_costs(dynamic_array_t* live_ranges){
	//Run through every single live range
	for(u_int16_t i = 0; i < live_ranges->current_index; i++){
		//Extract the given LR
		live_range_t* live_range = dynamic_array_get_at(live_ranges, i);

		//Theres no point in updating either of these because they will never be spilled
		if(live_range == stack_pointer_lr || live_range == instruction_pointer_lr){
			continue;
		}

		//If this was already spilled, we can't spill it again
		if(live_range->was_spilled == TRUE){
			live_range->spill_cost = INT32_MAX;
			continue;
		}

		//The cost of spilling a live range is always the assignment count times the cost to store plus
		//the use count times the cost to use
		u_int32_t spill_cost = live_range->assignment_count * STORE_COST + live_range->use_count * LOAD_COST;

		//Add it into the live range's existing spill cost
		live_range->spill_cost = spill_cost;
	}
}


/**
 * Add an assigned live range to a block
 */
static void add_assigned_live_range(live_range_t* live_range, basic_block_t* block){
	//Assigning a live range to a variable means that this variable was *assigned* in the block
	//Do note that it may very well have also been used, but we do not handle that here
	if(dynamic_array_contains(block->assigned_variables, live_range) == NOT_FOUND){
		dynamic_array_add(block->assigned_variables, live_range);
	}

	//This counts as an assigned live range - save for tracking
	live_range->assignment_count++;
}


/**
 * Add a used live range to a block
 */
static void add_used_live_range(live_range_t* live_range, basic_block_t* block){
	//There is no point in tracking uses here - these live ranges are not impacted
	//by interference
	if(live_range == stack_pointer_lr || live_range == instruction_pointer_lr){
		return;
	} 

	//Assigning a live range to a variable means that this variable was *used* in the block
	if(dynamic_array_contains(block->used_variables, live_range) == NOT_FOUND){
		dynamic_array_add(block->used_variables, live_range);
	}

	//No matter what, this increases
	live_range->use_count++;
}


/**
 * Add a LIVE_NOW live range
 */
static void add_live_now_live_range(live_range_t* live_range, dynamic_array_t* LIVE_NOW){
	//Don't bother adding these
	if(live_range == instruction_pointer_lr || live_range == stack_pointer_lr){
		return;
	}

	//Avoid duplicate addition
	if(dynamic_array_contains(LIVE_NOW, live_range) == NOT_FOUND){
		dynamic_array_add(LIVE_NOW, live_range);
	}
}


/**
 * Add a variable to a live range, if it isn't already in there
 */
static void add_variable_to_live_range(live_range_t* live_range, three_addr_var_t* variable){
	//If the literal memory address is already in here we leave
	if(dynamic_array_contains(live_range->variables, variable) != NOT_FOUND){
		return;
	}

	//Most of the time this will just be 0, but when it isn't we'll have it here
	if(variable->linked_var != NULL){
		live_range->function_parameter_order = variable->parameter_number;
	}

	//Link this live range to the variable
	variable->associated_live_range = live_range;

	//Otherwise we'll add this in here
	dynamic_array_add(live_range->variables, variable);
}


/**
 * Remove a variable from a given live range
 */
static void remove_variable_from_live_range(live_range_t* live_range, three_addr_var_t* variable){
	//If the literal memory address is already in here we leave
	if(dynamic_array_contains(live_range->variables, variable) == NOT_FOUND){
		return;
	}

}


/**
 * Figure out which live range a given variable was associated with. 
 *
 * NOTE: We *only* get here if we have used variables. This means that assigned variables do not count
 */
static live_range_t* assign_live_range_to_variable(dynamic_array_t* live_ranges, basic_block_t* block, three_addr_var_t* variable){
	//If this is the case it already has one
	if(variable->associated_live_range != NULL){
		return variable->associated_live_range;
	}

	//Lookup the live range that is associated with this
	live_range_t* live_range = find_live_range_with_variable(live_ranges, variable);

	//For developer flagging
	if(live_range == NULL){
		//This is a function parameter, we need to make it ourselves
		if(variable->membership == FUNCTION_PARAMETER){
			//Create it. Since this is a function parameter, we start at line 0
			live_range = live_range_alloc(block->function_defined_in);

			//Finally add this into all of our live ranges
			dynamic_array_add(live_ranges, live_range);

		} else {
			printf("Fatal compiler error: variable found with that has no live range\n");
			print_variable(stdout, variable, PRINTING_VAR_INLINE);
			print_function_name(variable->linked_var->function_declared_in);
			printf("\n\n");
			exit(0);
		}
	}

	//We now add this variable back into the live range
	add_variable_to_live_range(live_range, variable);

	//Give it back
	return live_range;
}


/**
 * Create the stack pointer live range
 */
static live_range_t* construct_and_add_stack_pointer_live_range(dynamic_array_t* live_ranges, three_addr_var_t* stack_pointer){
	//Before we go any further, we'll construct the live
	//range for the stack pointer. Special case here - stack pointer has no block
	live_range_t* stack_pointer_live_range = live_range_alloc(NULL);
	//This is guaranteed to be RSP - so it's already been allocated
	stack_pointer_live_range->reg = RSP;
	//And we absolutely *can not* spill it
	stack_pointer_live_range->spill_cost = UINT32_MAX;

	//This is precolor
	stack_pointer_live_range->is_precolored = TRUE;

	//Add the stack pointer to the dynamic array
	dynamic_array_add(stack_pointer_live_range->variables, stack_pointer);
	
	//Store this here as well
	stack_pointer->associated_live_range = stack_pointer_live_range;

	//Store it in the global var for convenience
	stack_pointer_lr = stack_pointer_live_range;

	//Add it into the list of all live ranges
	dynamic_array_add(live_ranges, stack_pointer_live_range);

	//Give it back
	return stack_pointer_live_range;
}


/**
 * Create the instruction pointer live range
 */
static live_range_t* construct_and_add_instruction_pointer_live_range(dynamic_array_t* live_ranges, three_addr_var_t* instruction_pointer){
	//Before we go any further, we'll construct the live
	//range for the instruction pointer.
	live_range_t* instruction_pointer_live_range = live_range_alloc(NULL);
	//This is guaranteed to be RSP - so it's already been allocated
	instruction_pointer_live_range->reg = RIP;
	//And we absolutely *can not* spill it
	instruction_pointer_live_range->spill_cost = UINT32_MAX;

	//This is precolor
	instruction_pointer_live_range->is_precolored = TRUE;

	//Add the stack pointer to the dynamic array
	dynamic_array_add(instruction_pointer_live_range->variables, instruction_pointer);

	//Save this to the global variable
	instruction_pointer_lr = instruction_pointer_live_range;
	
	//Store this here as well
	instruction_pointer->associated_live_range = instruction_pointer_live_range;

	//Add the instruction pointer LR in
	dynamic_array_add(live_ranges, instruction_pointer_lr);

	//Give it back
	return instruction_pointer_live_range;
}


/**
 * Handle all of the special cases that a destination variable can have, depending on
 * whether it is a source & destination both or not
 */
static void update_use_assignment_for_destination_variable(instruction_t* instruction, basic_block_t* block){
	//Extract the LR
	live_range_t* live_range = instruction->destination_register->associated_live_range;

	//There are a few things that could happen here in terms of a variable use:
	//If this is the case, then we need to set this new LR as both used and assigned
	if(is_destination_also_operand(instruction) == TRUE){
		//Counts as both
		add_assigned_live_range(live_range, block);
		add_used_live_range(live_range, block);

	//If this is being derefenced, then it's not a true assignment, just a use
	} else if(is_destination_assigned(instruction) == FALSE){
		add_used_live_range(live_range, block);

	//If we get all the way to here, then it was truly assigned
	} else {
		add_assigned_live_range(live_range, block);
	}
}


/**
 * Handle a live range being assigned to a destination variable,
 * and all of the bookkeeping that comes with it
 */
static void assign_live_range_to_destination_variable(dynamic_array_t* live_ranges, basic_block_t* block, instruction_t* instruction){
	//Bail out if this happens
	if(instruction->destination_register == NULL){
		return;
	}

	//Extract for convenience
	three_addr_var_t* destination_register = instruction->destination_register;

	//Let's see if we can find this
	live_range_t* live_range = find_or_create_live_range(live_ranges, block, destination_register);

	//Add this into the live range
	add_variable_to_live_range(live_range, destination_register);

	//Invoke the helper for this part
	update_use_assignment_for_destination_variable(instruction, block);

	//All done
	if(instruction->destination_register2 == NULL){
		return;
	}

	/**
	 * For certain instructions like conversion & division instructions, we have 2
	 * destination registers. These registers will always be strict assignees so we
	 * don't need to do any of the manipulation like before.
	 */
	//Extract for convenience
	three_addr_var_t* destination_register2 = instruction->destination_register2;

	//Let's see if we can find this
	live_range = find_or_create_live_range(live_ranges, block, destination_register2);

	//Add this into the live range
	add_variable_to_live_range(live_range, destination_register2);

	//This will *always* be a purely assigned live range
	add_assigned_live_range(live_range, block);
}


/**
 * Handle the live range that comes from the source of an instruction
 */
static void assign_live_range_to_source_variable(dynamic_array_t* live_ranges, basic_block_t* block, three_addr_var_t* source_variable){
	//Just leave if it's NULL
	if(source_variable == NULL){
		return;
	}

	//Let the helper deal with this
	live_range_t* live_range = assign_live_range_to_variable(live_ranges, block, source_variable);

	//Add this as a used live range
	add_used_live_range(live_range, block);
}


/**
 * Assign a live range to a function parameter variable
 *
 * Since function parameters are not explicitly being read from, we do not want to
 * count this as a use
 */
static void assign_live_range_to_function_parameter(dynamic_array_t* live_ranges, basic_block_t* block, three_addr_var_t* function_parameter_var){
	//Just leave if we're null
	if(function_parameter_var == NULL){
		return;
	}

	//Just a wrapper
	live_range_t* live_range = assign_live_range_to_variable(live_ranges, block, function_parameter_var);

	//We will bump the use count, but this is not officially a read
	live_range->use_count++;
}


/**
 * Handle the live range that comes from the source of an instruction
 */
static void assign_live_range_to_ret_variable(dynamic_array_t* live_ranges, basic_block_t* block, three_addr_var_t* source_variable){
	//Just leave if it's NULL
	if(source_variable == NULL){
		return;
	}

	//Let the helper deal with this
	live_range_t* live_range = assign_live_range_to_variable(live_ranges, block, source_variable);

	//We will bump the use count, but this is not officially a read
	live_range->use_count++;
}


/**
 * Construct the live ranges appropriate for a phi function
 *
 * Note that the phi function does not count as an actual assignment, we'll just want
 * to ensure that the live range is ready for us when we need it
 */
static void construct_phi_function_live_range(dynamic_array_t* live_ranges, basic_block_t* basic_block, instruction_t* instruction){
	//Let's see if we can find this
	live_range_t* live_range = find_or_create_live_range(live_ranges, basic_block, instruction->assignee);

	//Add this into the live range
	add_variable_to_live_range(live_range, instruction->assignee);
}


/**
 * An increment/decrement live range is a special case because the invisible "source" needs to be part of the
 * same live range as the destination. We ensure that that happens within this rule
 */
static void construct_inc_dec_live_range(dynamic_array_t* live_ranges, basic_block_t* basic_block, instruction_t* instruction){
	//If this is not temporary, we can handle it like any other statement
	if(instruction->destination_register->is_temporary == FALSE){
		//Handle the destination variable
		assign_live_range_to_destination_variable(live_ranges, basic_block, instruction);

		//Assign all of the source variable live ranges
		assign_live_range_to_source_variable(live_ranges, basic_block, instruction->source_register);

	//Otherwise, we'll need to take a more specialized approach
	} else {
		//Let's see if we can find this
		live_range_t* live_range = find_or_create_live_range(live_ranges, basic_block, instruction->destination_register);

		//Add this into the live range
		add_variable_to_live_range(live_range, instruction->destination_register);

		//This does count as an assigned live range
		add_assigned_live_range(live_range, basic_block);

		//Assign the live range to op1 in here as well
		add_variable_to_live_range(live_range, instruction->source_register);

		//Since we rely on this value being live for the instruction, this also counts
		//as a use
		add_used_live_range(live_range, basic_block);
	}
}


/**
 * A function call statement keeps track of the parameters that it uses. In doing this,
 * it is using those parameters. We need to keep track of this by recording it as a use
 */
static void construct_function_call_live_ranges(dynamic_array_t* live_ranges, basic_block_t* basic_block, instruction_t* instruction){
	//First let's handle the destination register. It is possible that this could 
	//be null
	if(instruction->destination_register != NULL){
		assign_live_range_to_destination_variable(live_ranges, basic_block, instruction);
	}

	/**
	 * NOTE: For indirect function calls, the variable itself is actually stored in the source register.
	 * We'll make this call to account for such a case here
	 */
	assign_live_range_to_source_variable(live_ranges, basic_block, instruction->source_register);

	//Extract for us
	dynamic_array_t* function_parameters = instruction->parameters;

	//If these are NULL then there's nothing else for us here
	if(function_parameters == NULL){
		return;
	}
				
	//Otherwise we'll run through them all
	for(u_int16_t i = 0; i < function_parameters->current_index; i++){
		//Extract it
		three_addr_var_t* parameter = dynamic_array_get_at(function_parameters, i);

		/**
		 * Because we are not explicitly reading in this instruction, we need a special rule that takes care to not
		 * add these as used variables. We will instead just assign the appropriate live ranges
		 */
		assign_live_range_to_function_parameter(live_ranges, basic_block, parameter);
	}
}


/**
 * Run through every instruction in a block and construct the live ranges
 *
 * We invoke special processing functions to handle exception cases like phi functions, inc/dec instructions,
 * and function calls
 */
static void construct_live_ranges_in_block(dynamic_array_t* live_ranges, basic_block_t* basic_block){
	//Call the helper API to wipe out any old variable tracking in here
	reset_block_variable_tracking(basic_block);

	//Grab a pointer to the head
	instruction_t* current = basic_block->leader_statement;

	//Run through every instruction in the block
	while(current != NULL){
		//Handle special cases
		switch(current->instruction_type){
			/**
			 * For phi functions, we will simply mark that the variable has been assigned and create
			 * the appropriate live range. We do not
			 * need to mark anything else here
			 */
			case PHI_FUNCTION:
				//Invoke the helper rule for this
				construct_phi_function_live_range(live_ranges, basic_block, current);
			
				//And we're done - no need to go further
				current = current->next_statement;
				continue;

			case RET:
				assign_live_range_to_ret_variable(live_ranges, basic_block, current->source_register);
				
				current = current->next_statement;
				continue;

			/**
			 * For increment/decrement instructions - the only variable that we have just so happens to also
			 * be the source. As such, we need to ensure that both of these end up in the same live range
			 */
			case INCB:
			case INCL:
			case INCQ:
			case INCW:
			case DECQ:
			case DECL:
			case DECW:
			case DECB:
				//Let the helper rule do the actual construction
				construct_inc_dec_live_range(live_ranges, basic_block, current);
			
				//And we're done - no need to go further
				current = current->next_statement;
				continue;

			//Call and indirect call have hidden parameters that need to be accounted for
			case CALL:
			case INDIRECT_CALL:
				//Let the helper rule handle it
				construct_function_call_live_ranges(live_ranges, basic_block, current);

				//And we're done - no need to go further
				current = current->next_statement;
				continue;

			//Just head out
			default:
				break;
		}

		/**
		 * If we make it here, we know that we have a normal instruction that exists on
		 * the target architecture. Here we can construct our live ranges and exploit any opportunities
		 * for live range coalescing
		 */

		//Handle the destination variable
		assign_live_range_to_destination_variable(live_ranges, basic_block, current);

		//Assign all of the source variable live ranges
		assign_live_range_to_source_variable(live_ranges, basic_block, current->source_register);
		assign_live_range_to_source_variable(live_ranges, basic_block, current->source_register2);
		assign_live_range_to_source_variable(live_ranges, basic_block, current->address_calc_reg1);
		assign_live_range_to_source_variable(live_ranges, basic_block, current->address_calc_reg2);

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

	//Construct and add the stack pointer's LR
	construct_and_add_stack_pointer_live_range(live_ranges, cfg->stack_pointer);

	//Construct and add the instruction pointer's LR
	construct_and_add_instruction_pointer_live_range(live_ranges, cfg->instruction_pointer);

	//Since the blocks are already ordered, this is very simple
	basic_block_t* current = cfg->head_block;

	//Run through every single block
	while(current != NULL){
		//Let the helper do this
		construct_live_ranges_in_block(live_ranges, current);

		//Advance to the next
		current = current->direct_successor;
	}

	//Give back the array
	return live_ranges;
}


/**
 * Reset the visited status and the liveness arrays for each block
 */
static void reset_blocks_for_liveness(cfg_t* cfg){
	//Run through all of our blocks
	for(u_int16_t i = 0; i < cfg->created_blocks->current_index; i++){
		//Grab it out
		basic_block_t* current = dynamic_array_get_at(cfg->created_blocks, i);

		//Reset this
		current->visited = FALSE;

		//Also reset the liveness sets
		reset_dynamic_array(current->live_in);
		reset_dynamic_array(current->live_out);
	}
}


/**
 * Calculate the "live_in" and "live_out" sets for each basic block. More broadly, we can do this for 
 * every single function
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
 * 	out[n] = {}U{x|x is an element of in[S] where S is a successor of n}
 * 	in[n] = use[n] U (out[n] - def[n])
 *
 * NOTE: The algorithm converges very fast when the CFG is done in reverse order.
 * As such, we'll go back to front here
 *
 */
static void calculate_live_range_liveness_sets(cfg_t* cfg){
	//Reset the visited status and liveness sets
	reset_blocks_for_liveness(cfg);

	//Did we find a difference
	u_int8_t difference_found;

	//The "Prime" blocks are just ways to hold the old dynamic arrays
	dynamic_array_t* in_prime;
	dynamic_array_t* out_prime;

	//A cursor for the current block
	basic_block_t* current;

	/**
	 * We will go through all of the function blocks separately first. Since
	 * the functions are distinct, we do not need to do them all together.
	 * We can focus on a single function at a time. This way, if certain functions
	 * are "hot spots" and require multiple iterations, they will not drag the rest of the 
	 * blocks with them for said recalculation
	 */
	//Run through all functions
	for(u_int16_t i = 0; i < cfg->function_entry_blocks->current_index; i++){
		//Grab the entry block
		basic_block_t* function_entry = dynamic_array_get_at(cfg->function_entry_blocks, i);

		//We'll reset the assigned registers array here because we have not assigned any registers at this
		//point
		memset(function_entry->function_defined_in->assigned_registers, 0, sizeof(u_int8_t) * K_COLORS_GEN_USE);

		//We keep calculating this until we end up with no change in the old and new LIVE_IN/LIVE_OUT sets
		do{
			//Assume that we have not found a difference by default
			difference_found = FALSE;

			//Now we can go through the entire RPO set
			for(u_int16_t _ = 0; _ < function_entry->reverse_post_order_reverse_cfg->current_index; _++){
				//The current block is whichever we grab
				current = dynamic_array_get_at(function_entry->reverse_post_order_reverse_cfg, _);

				//Transfer the pointers over
				in_prime = current->live_in;
				out_prime = current->live_out;

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
				
			
				/**
				 * Now for the final portion of the algorithm. We need to see if the LIVE_IN and LIVE_OUT
				 * sets that we've computed on this iteration are equal. If they're not equal, then we have
				 * not yet found the full solution, and we need to go again
				 */
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

		//So long as there is a difference
		} while(difference_found == TRUE);
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

		//Set the degree to be 0 as well
		current->degree = 0;

		//These are both used and assigned to not at all
		current->use_count = 0;
		current->assignment_count = 0;

		//The spill cost is also wiped out now
		current->spill_cost = 0;

		//And we'll also reset all of the neighbors
		reset_dynamic_array(current->neighbors);
	}
}


/**
 * A helper function for interference construction. This adds interference between every
 * value in LIVE_NOW and the given destination_lr
 */
static void add_destination_interference(interference_graph_t* graph, dynamic_array_t* LIVE_NOW, live_range_t* destination_lr){
	for(u_int16_t i = 0; i < LIVE_NOW->current_index; i++){
		//Graph the LR out
		live_range_t* range = dynamic_array_get_at(LIVE_NOW, i);

		//If the live range is the stack pointer or instruction pointer, it does
		//not really count as interference because those registers are never alive
		//at the same time. As such, we'll skip if that's the case
		if(range == stack_pointer_lr || range == instruction_pointer_lr){
			continue;
		}

		//Now we'll add this to the graph
		add_interference(graph, destination_lr, range);
	}
}


/**
 * Calculate "live_after" in a given block. "live_after" represents all of the live ranges
 * that will survive "after" a function runs
 *
 * Algorithm:
 * 	create an interference graph
 * 	for each block b:
 * 		LIVEAFTER <- LIVEOUT(b)
 * 		for each operation with form op LA, LB -> LC:
* 			if operation = stopper:
 * 				exit
 * 			for each LRi in LIVEAFTER:
 * 				add(LC, LRi) to Interference Graph E 
 * 			remove LC from LIVEAFTER 
 * 			Add LA an LB to LIVEAFTER 
 */
static dynamic_array_t* calculate_live_after_for_block(basic_block_t* block, instruction_t* instruction){
	/**
	 * As you can see in the algorithm, the LIVE_NOW set initially starts
	 * out as LIVE_OUT. For this reason, we will just use the LIVE_OUT
	 * set by a different name for our calculation
	 */
	dynamic_array_t* live_after = clone_dynamic_array(block->live_out);
	
	//We will crawl our way up backwards through the CFG
	instruction_t* operation = block->exit_statement;

	//Run through backwards until we reach the instruction that
	//will stop us
	while(operation != NULL && operation != instruction){
		//If we have an exact copy operation, we can
		//skip it as it won't create any interference
		if(operation->instruction_type == PHI_FUNCTION){
			//Skip it
			operation = operation->previous_statement;
			continue;
		}

		/**
		 * Step from algorithm:
		 *
		 * for each LRi in LIVENOW:
		 * 	add(DEST, LRi) to Interference Graph E 
		 * remove LC from LIVENOW
		 *
		 * 	Mark that the destination interferes with every LIVE_NOW range
		 *
		 * 	This is straightforward in the algorithm, but for us it's not so
		 * 	straightforward. There are several cases where the destination
		 * 	register may be present but this step in the algorithm will not 
		 * 	apply or may apply in a different way
		 */
		if(operation->destination_register != NULL){
			/**
			 * This is if we have something like add LRa, LRb. LRb
			 * is a destination but it's also a value. As such, we will
			 * *not* delete this after we add our interference
			 */
			if(is_destination_also_operand(operation) == TRUE){
				//Since this is *also* an operand, it needs to be added to the LIVE_NOW array. It would not be picked up any
				//other way
				add_live_now_live_range(operation->destination_register->associated_live_range, live_after);

			/**
			 * If the indirection level is more than 0, this means that we're moving into a memory
			 * region. Since this is the case, we're not really assigning to the register here. In
			 * fact, we're using it, so we'll need to add this to LIVE_NOW
			 */
			} else if(is_destination_assigned(operation) == FALSE){
				//Add it to live now and we're done
				add_live_now_live_range(operation->destination_register->associated_live_range, live_after);

			/**
			 * The final case here is the ideal case in the algorithm, where we have a simple
			 * assignment at the end. To satisfy the algorithm, we'll add all of the interference
			 * between the destination and LIVE_NOW and then delete the destination from live_now
			 */
			} else {
				//And then scrap it from live_now
				dynamic_array_delete(live_after, operation->destination_register->associated_live_range);
			}
		}

		/**
		 * Some instructions like CXXX and division instructions have 2 destinations. The second destination,
		 * unlike the first, will never have any dual purpose, so we can just add the interference and delete
		 */
		if(operation->destination_register2 != NULL){
			//And then scrap it from live_now
			dynamic_array_delete(live_after, operation->destination_register2->associated_live_range);
		}

		/**
		 * STEP:
		 *  Add LA an LB to LIVENOW
		 *
		 * This really means add any non-destination variables to LIVENOW. We will take
		 * into account every special case here, including function calls and INC/DEC instructions
		 *
		 * These first few are the obvious cases
		 */
		if(operation->source_register != NULL){
			add_live_now_live_range(operation->source_register->associated_live_range, live_after);
		}

		if(operation->source_register2 != NULL){
			add_live_now_live_range(operation->source_register2->associated_live_range, live_after);
		}

		if(operation->address_calc_reg1 != NULL){
			add_live_now_live_range(operation->address_calc_reg1->associated_live_range, live_after);
		}

		if(operation->address_calc_reg2 != NULL){
			add_live_now_live_range(operation->address_calc_reg2->associated_live_range, live_after);
		}

		/**
		 * SPECIAL CASES:
		 *
		 * Function calls(direct/indirect) have function parameters that are being used
		 */
		switch(operation->instruction_type){
			case CALL:
			case INDIRECT_CALL:
				//No point here
				if(operation->parameters == NULL){
					break;
				}
				
				//Grab it out
				dynamic_array_t* operation_function_parameters = operation->parameters;

				//Let's go through all of these and add them to LIVE_NOW
				for(u_int16_t i = 0; i < operation_function_parameters->current_index; i++){
					//Extract the variable
					three_addr_var_t* variable = dynamic_array_get_at(operation_function_parameters, i);

					//Add it to live_now
					add_live_now_live_range(variable->associated_live_range, live_after);
				}

				break;

			//By default do nothing
			default:
				break;
		}

		//Crawl back up by 1
		operation = operation->previous_statement;
	}

	//And give it back
	return live_after;
}


/**
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
 * This algorithm operates on the basic(and obvious) principle that two values cannot occupy the same
 * register at the same time. So, we can determine what values are "live"(currently in use) by keeping
 * track of what values have been used but not written. We crawl up the block, starting with the "live_out"
 * values as our initial set. Remember that live out values are just values that are live-in at one of the
 * successors of the current block. In other words, we require that these values survive after the block is done
 * executing.
 *
 * We work by starting at the bottom of the block and crawling our way up. A value is considered "live_now" until
 * we find the instruction where it was a destination. Once we find the instruction where the value was written
 * to, we can safely remove it from LIVE_NOW because everything before that cannot possibly rely on the register
 * because it hadn't been written to yet.
 *
 * We have a stopper parameter that can be used to halt the execution at a certain point. This is used for
 * function call caller/callee saving calculations
 */
static void calculate_interference_in_block(interference_graph_t* graph, basic_block_t* block){
	/**
	 * As you can see in the algorithm, the LIVE_NOW set initially starts
	 * out as LIVE_OUT. For this reason, we will just use the LIVE_OUT
	 * set by a different name for our calculation
	 */
	dynamic_array_t* live_now = clone_dynamic_array(block->live_out);
	
	//We will crawl our way up backwards through the CFG
	instruction_t* operation = block->exit_statement;

	//Run through backwards
	while(operation != NULL){
		//If we have an exact copy operation, we can
		//skip it as it won't create any interference
		if(operation->instruction_type == PHI_FUNCTION){
			//Skip it
			operation = operation->previous_statement;
			continue;
		}

		/**
		 * Step from algorithm:
		 *
		 * for each LRi in LIVENOW:
		 * 	add(DEST, LRi) to Interference Graph E 
		 * remove LC from LIVENOW
		 *
		 * 	Mark that the destination interferes with every LIVE_NOW range
		 *
		 * 	This is straightforward in the algorithm, but for us it's not so
		 * 	straightforward. There are several cases where the destination
		 * 	register may be present but this step in the algorithm will not 
		 * 	apply or may apply in a different way
		 */
		if(operation->destination_register != NULL){
			/**
			 * This is if we have something like add LRa, LRb. LRb
			 * is a destination but it's also a value. As such, we will
			 * *not* delete this after we add our interference
			 */
			if(is_destination_also_operand(operation) == TRUE){
				//This counts as interference
				add_destination_interference(graph, live_now, operation->destination_register->associated_live_range);
				//Since this is *also* an operand, it needs to be added to the LIVE_NOW array. It would not be picked up any
				//other way
				add_live_now_live_range(operation->destination_register->associated_live_range, live_now);

			/**
			 * If the indirection level is more than 0, this means that we're moving into a memory
			 * region. Since this is the case, we're not really assigning to the register here. In
			 * fact, we're using it, so we'll need to add this to LIVE_NOW
			 */
			} else if(is_destination_assigned(operation) == FALSE){
				//Add it to live now and we're done
				add_live_now_live_range(operation->destination_register->associated_live_range, live_now);

			/**
			 * The final case here is the ideal case in the algorithm, where we have a simple
			 * assignment at the end. To satisfy the algorithm, we'll add all of the interference
			 * between the destination and LIVE_NOW and then delete the destination from live_now
			 */
			} else {
				//Add the interference
				add_destination_interference(graph, live_now, operation->destination_register->associated_live_range);

				//And then scrap it from live_now
				dynamic_array_delete(live_now, operation->destination_register->associated_live_range);
			}
		}

		/**
		 * Some instructions like CXXX and division instructions have 2 destinations. The second destination,
		 * unlike the first, will never have any dual purpose, so we can just add the interference and delete
		 */
		if(operation->destination_register2 != NULL){
			//Add the interference
			add_destination_interference(graph, live_now, operation->destination_register2->associated_live_range);

			//And then scrap it from live_now
			dynamic_array_delete(live_now, operation->destination_register2->associated_live_range);
		}

		/**
		 * STEP:
		 *  Add LA an LB to LIVENOW
		 *
		 * This really means add any non-destination variables to LIVENOW. We will take
		 * into account every special case here, including function calls and INC/DEC instructions
		 *
		 * These first few are the obvious cases
		 */
		if(operation->source_register != NULL){
			add_live_now_live_range(operation->source_register->associated_live_range, live_now);
		}

		if(operation->source_register2 != NULL){
			add_live_now_live_range(operation->source_register2->associated_live_range, live_now);
		}

		if(operation->address_calc_reg1 != NULL){
			add_live_now_live_range(operation->address_calc_reg1->associated_live_range, live_now);
		}

		if(operation->address_calc_reg2 != NULL){
			add_live_now_live_range(operation->address_calc_reg2->associated_live_range, live_now);
		}

		/**
		 * SPECIAL CASES:
		 *
		 * Function calls(direct/indirect) have function parameters that are being used
		 */
		switch(operation->instruction_type){
			case CALL:
			case INDIRECT_CALL:
				//No point here
				if(operation->parameters == NULL){
					break;
				}
				
				//Grab it out
				dynamic_array_t* operation_function_parameters = operation->parameters;

				//Let's go through all of these and add them to LIVE_NOW
				for(u_int16_t i = 0; i < operation_function_parameters->current_index; i++){
					//Extract the variable
					three_addr_var_t* variable = dynamic_array_get_at(operation_function_parameters, i);

					//Add it to live_now
					add_live_now_live_range(variable->associated_live_range, live_now);
				}

				break;

			//By default do nothing
			default:
				break;
		}

		//Crawl back up by 1
		operation = operation->previous_statement;
	}
}


/**
 * Construct the interference graph using LIVENOW sets
 *
 * This function will always invoke a helper that does it for a specific block
 */
static interference_graph_t* construct_interference_graph(cfg_t* cfg, dynamic_array_t* live_ranges){
	//It starts off as null
	interference_graph_t* graph = NULL;

	//We'll first need a pointer
	basic_block_t* current = cfg->head_block;

	//Run through every block in the CFG's ordered set
	while(current != NULL){
		//Use the helper. Set stopper to be NULL because we aren't trying to halt
		//anything here
		calculate_interference_in_block(graph, current);
		
		//Advance this up
		current = current->direct_successor;
	}

	//Now at the very end, we'll construct the matrix
	graph = construct_interference_graph_from_adjacency_lists(live_ranges);

	//And finally give the graph back
	return graph;
}


/**
 * Does precoloring interference exist?
 *
 * Precoloring interference exists if:
 *  An LR wants to be colored with Register R
 *  But one of it's neighbors *is already* colored with Register R
 *
 * Takes in the register that we want to color(coloree) and the register we want to color it with
 *
 * This function will return the value of the neighbor that we interfere with. This will be needed
 * for spilling
 */
static live_range_t* does_precoloring_interference_exist(live_range_t* coloree, general_purpose_register_t reg){
	//Extract for convenience
	dynamic_array_t* neighbors = coloree->neighbors;

	//Run through all of the neighbors
	for(u_int16_t i = 0; i < neighbors->current_index; i++){
		//Grab the given neighbor out
		live_range_t* neighbor = dynamic_array_get_at(neighbors, i);

		//This collision means we do have interference
		if(neighbor->reg == reg){
			return NULL;
		}
	}

	//If we get here then no
	return NULL;
}


/**
 * Perform the precoloring. Return TRUE if we can precolor, FALSE if we cannot and had to spill
 */
static u_int8_t precolor_live_range(cfg_t* cfg, dynamic_array_t* live_ranges, live_range_t* coloree, general_purpose_register_t reg){
	//Returns NULL if there's no interference, the interferee if there is one
	live_range_t* interferee = does_precoloring_interference_exist(coloree, reg);

	//Does nothing for now
	//This is turned off - until we have a better
	//scheme for it
	if(interferee != NULL){
		if(coloree->spill_cost < interferee->spill_cost){
			spill(cfg, live_ranges, coloree);
		} else {
			spill(cfg, live_ranges, interferee);
		}

		//Whatever we spill the return value here is false
		return FALSE;
	}

	//Assign the register over
	coloree->reg = reg;

	//And mark that it's pre-colored
	coloree->is_precolored = TRUE;

	return TRUE;
}



/**
 * Some variables need to be in special registers at a given time. We can
 * bind them to the right register at this stage and avoid having to worry about it later
 *
 * Returns TRUE if we could color, false if not
 */
static u_int8_t precolor_instruction(cfg_t* cfg, dynamic_array_t* live_ranges, instruction_t* instruction){
	u_int8_t colorable;

	/**
	 * The first thing will check for here is after-call function parameters. These
	 * need to be allocated appropriately
	 */

	//One thing to check for - function parameter passing
	if(instruction->destination_register != NULL
		&& instruction->destination_register->associated_live_range->function_parameter_order > 0){
		//Extract the register
		general_purpose_register_t reg = parameter_registers[instruction->destination_register->associated_live_range->function_parameter_order - 1];

		//Let the helper deal with it
		colorable = precolor_live_range(cfg, live_ranges, instruction->destination_register->associated_live_range, reg);

		//We had a spill - so we'll need to jump out immediately
		if(colorable == FALSE){
			return FALSE;
		}
	}

	//One thing to check for - function parameter passing
	if(instruction->source_register != NULL
		&& instruction->source_register->associated_live_range->function_parameter_order > 0){
		//Extract the register
		general_purpose_register_t reg = parameter_registers[instruction->source_register->associated_live_range->function_parameter_order - 1];

		//Let the helper deal with it
		colorable = precolor_live_range(cfg, live_ranges, instruction->source_register->associated_live_range, reg);

		//We had a spill - so we'll need to jump out immediately
		if(colorable == FALSE){
			return FALSE;
		}
	}

	//Check source 2 as well
	if(instruction->source_register2 != NULL
		&& instruction->source_register2->associated_live_range->function_parameter_order > 0){
		//Extract the register
		general_purpose_register_t reg = parameter_registers[instruction->source_register2->associated_live_range->function_parameter_order - 1];

		//Let the helper deal with it
		colorable = precolor_live_range(cfg, live_ranges, instruction->source_register2->associated_live_range, reg);

		//We had a spill - so we'll need to jump out immediately
		if(colorable == FALSE){
			return FALSE;
		}
	}

	//Check address calc 1 as well
	if(instruction->address_calc_reg1 != NULL
		&& instruction->address_calc_reg1->associated_live_range->function_parameter_order > 0){
		//Extract the register
		general_purpose_register_t reg = parameter_registers[instruction->address_calc_reg1->associated_live_range->function_parameter_order - 1];

		//Let the helper deal with it
		colorable = precolor_live_range(cfg, live_ranges, instruction->address_calc_reg1->associated_live_range, reg);

		//We had a spill - so we'll need to jump out immediately
		if(colorable == FALSE){
			return FALSE;
		}
	}

	//Check address calc 2 as well
	if(instruction->address_calc_reg2 != NULL
		&& instruction->address_calc_reg2->associated_live_range->function_parameter_order > 0){
		//Extract the register
		general_purpose_register_t reg = parameter_registers[instruction->address_calc_reg2->associated_live_range->function_parameter_order - 1];
		
		//Let the helper deal with it
		colorable = precolor_live_range(cfg, live_ranges, instruction->address_calc_reg2->associated_live_range, reg);

		//We had a spill - so we'll need to jump out immediately
		if(colorable == FALSE){
			return FALSE;
		}
	}

	//Pre-color based on what kind of instruction it is
	switch(instruction->instruction_type){
		//If a return instruction has a
		//value, it must be in %RAX so we can assign
		//that entire live range to %RAX
		case RET:
			//If it has one, assign it
			if(instruction->source_register != NULL){
				//Let the helper do it
				colorable = precolor_live_range(cfg, live_ranges, instruction->source_register->associated_live_range, RAX);

				//We had to spill here - jump out
				if(colorable == FALSE){
					return FALSE;
				}
			}
			break;

		case MULB:
		case MULW:
		case MULL:
		case MULQ:
			//When we do an unsigned multiplication, the implicit source register must be in RAX
			colorable = precolor_live_range(cfg, live_ranges, instruction->source_register2->associated_live_range, RAX);

			//We had to spill here - jump out
			if(colorable == FALSE){
				return FALSE;
			}

			//The destination must also be in RAX here
			colorable = precolor_live_range(cfg, live_ranges, instruction->destination_register->associated_live_range, RAX);

			//We had to spill here - jump out
			if(colorable == FALSE){
				return FALSE;
			}

			break;

		/**
		 * For all shift instructions, if they have a register source, that
		 * source is required to be in the %cl register
		 */
		case SALB:
		case SALW:
		case SALL:
		case SALQ:
		case SHLB:
		case SHLW:
		case SHLL:
		case SHLQ:
		case SARB:
		case SARW:
		case SARL:
		case SARQ:
		case SHRB:
		case SHRW:
		case SHRL:
		case SHRQ:
			//Do we have a register source?
			if(instruction->source_register != NULL){
				//Due to a quirk in old x86, shift instructions must have their source in RCX
				colorable = precolor_live_range(cfg, live_ranges, instruction->source_register->associated_live_range, RCX);

				//We had to spill here - jump out
				if(colorable == FALSE){
					return FALSE;
				}
			}
		
			break;

		case CQTO:
		case CLTD:
		case CWTL:
		case CBTW:
			//Source is always %RAX
			colorable =  precolor_live_range(cfg, live_ranges, instruction->source_register->associated_live_range, RAX);

			//We had to spill here - jump out
			if(colorable == FALSE){
				return FALSE;
			}

			//The results are always RDX and RAX 
			//Lower order bits
			colorable = precolor_live_range(cfg, live_ranges, instruction->destination_register->associated_live_range, RAX);

			//We had to spill here - jump out
			if(colorable == FALSE){
				return FALSE;
			}

			//Higher order bits
			colorable = precolor_live_range(cfg, live_ranges, instruction->destination_register2->associated_live_range, RDX);

			//We had to spill here - jump out
			if(colorable == FALSE){
				return FALSE;
			}

			break;

		case DIVB:
		case DIVW:
		case DIVL:
		case DIVQ:
		case IDIVB:
		case IDIVW:
		case IDIVL:
		case IDIVQ:
			//The source register for a division must be in RAX
			colorable = precolor_live_range(cfg, live_ranges, instruction->source_register2->associated_live_range, RAX);

			//We had to spill here - jump out
			if(colorable == FALSE){
				return FALSE;
			}

			//The first destination register is the quotient, and is in RAX
			colorable = precolor_live_range(cfg, live_ranges, instruction->destination_register->associated_live_range, RAX);

			//We had to spill here - jump out
			if(colorable == FALSE){
				return FALSE;
			}

			//The second destination register is the remainder, and is in RDX
			colorable = precolor_live_range(cfg, live_ranges, instruction->destination_register2->associated_live_range, RDX);

			//We had to spill here - jump out
			if(colorable == FALSE){
				return FALSE;
			}

			break;

		//Function calls always return through rax
		case CALL:
		case INDIRECT_CALL:
			//We could have a void return, but usually we'll give something
			if(instruction->destination_register != NULL){
				colorable = precolor_live_range(cfg, live_ranges, instruction->destination_register->associated_live_range, RAX);

				//We had to spill here - jump out
				if(colorable == FALSE){
					return FALSE;
				}
			}

			/**
			 * We also need to allocate the parameters for this function. Conviently,
			 * they are already stored for us so we don't need to do anything like
			 * what we had to do for pre-allocating function parameters
			 */

			//Grab the parameters out
			dynamic_array_t* function_params = instruction->parameters;

			//If we actually have function parameters
			if(function_params != NULL){
				for(u_int16_t i = 0; i < function_params->current_index; i++){
					//Grab it out
					three_addr_var_t* param = dynamic_array_get_at(function_params, i);

					//Now that we have it, we'll grab it's live range
 					live_range_t* param_live_range = param->associated_live_range;

					//And we'll use the function param list to precolor appropriately
					colorable = precolor_live_range(cfg, live_ranges, param_live_range, parameter_registers[i]);

					//We had to spill here - jump out
					if(colorable == FALSE){
						return FALSE;
					}
				}
			}

			break;

		//Most of the time we will get here
		default:
			break;
	}

	//If we make it down here, then it worked
	return TRUE;
}


/**
 * Crawl the entire CFG and pre-color all registers.
 *
 * If we encounter a case where 2 registers are attempting to be pre-colored
 * with the same register, then we have a case where we must spill
 *
 * This function returns TRUE if pre-coloring worked, FALSE if not
 */
static u_int8_t pre_color(cfg_t* cfg, dynamic_array_t* live_ranges){
	//By default assume that we can precolor it
	u_int8_t could_be_precolored = TRUE;

	//Grab a cursor to the head block
	basic_block_t* cursor = cfg->head_block;

	//Crawl the entire CFG
	while(cursor != NULL){
		//Grab a cursor to each statement
		instruction_t* instruction_cursor = cursor->leader_statement;

		//Crawl all statements in the block
		while(instruction_cursor != NULL){
			//TODO LINK INTO SPILLER
			//Invoke the helper to pre-color it
			could_be_precolored = precolor_instruction(cfg, live_ranges, instruction_cursor);

			//Push along to the next statement
			instruction_cursor = instruction_cursor->next_statement;
		}

		//Push onto the next statement
		cursor = cursor->direct_successor;
	}

	return could_be_precolored;
}


/**
 * Does any neighbor of the target already use "reg"?
 */
static u_int8_t does_neighbor_precoloring_interference_exist(live_range_t* target, general_purpose_register_t reg){
	//Run through all neighbors
	for(u_int16_t i = 0; i < target->neighbors->current_index; i++){
		//Extract this one out
		live_range_t* neighbor = dynamic_array_get_at(target->neighbors, i);

		//Counts as interference
		if(neighbor->reg == reg){
			return TRUE;
		}
	}

	//By the time we get here it's a no
	return FALSE;
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
 *
 * Another criteria for register allocation interference - do any of the new source's neighbor's
 * have themselves precolored the same as the source/destination register? If so that does count
 * as interference
 */
static u_int8_t does_register_allocation_interference_exist(live_range_t* source, live_range_t* destination){
	/**
	 * Cases here:
	 *
	 * Case 1: Source has no reg, and destination has no reg -> TRUE
	 * Case 2: Source has no reg, and destination has reg -> TRUE (take the destination register)
	 * Case 3: Source has reg, and destination has no reg -> TRUE (take the source register)
	 * Case 4: Source has reg, and destination has reg *and* source->reg == destination->reg -> TRUE
	 * Case 5: Source has reg, and destination has reg *and* source->reg != destination->reg -> FALSE
	 */
	switch(source->reg){
		//If the source has no reg, this will work
		case NO_REG:
			//If the destination has a register, we need
			//to check if any *neighbors* of the source
			//are colored the same. If they are, that would
			//lead to interference
			if(destination->reg != NO_REG){
				//Whatever this is is our answer
				return does_neighbor_precoloring_interference_exist(source, destination->reg);
			}

			//No interference
			return FALSE;

		/**
		 * Special case - if the source register is RSP, we need to ensure
		 * that the destination that we're moving to is only ever
		 * assigned to one(that would be the assignment between it and %rsp)
		 *
		 * If it is, that means that we'd be overwriting the stack pointer which
		 * is a big issue
		 */
		case RSP:
			//We *cannot* combine these two
			if(destination->assignment_count > 1){
				return TRUE;
			}

			//Even if the destination has no register, it's neighbors 
			//could. We'll use the helper to get our answer
			if(destination->reg == NO_REG){
				return does_neighbor_precoloring_interference_exist(destination, source->reg);
			}

			//If they're the exact same, then this is also fine
			if(destination->reg == source->reg){
				//No interference
				return FALSE;
			}

			//Otherwise we have interference
			return TRUE;

		//This means the source has a register already assigned
		default:
			//Even if the destination has no register, it's neighbors 
			//could. We'll use the helper to get our answer
			if(destination->reg == NO_REG){
				return does_neighbor_precoloring_interference_exist(destination, source->reg);
			}

			//If they're the exact same, then this is also fine
			if(destination->reg == source->reg){
				//No interference
				return FALSE;
			}

			//Otherwise we have interference
			return TRUE;
	}
}


/**
 * Compute the used and assignment sets for a given block
 *
 * We will tie this into the coalesce block-level function. We will recompute used/assigned
 * at the block level if we coalesce at the block level
 *
 * Actually, we may need to do this for the whole CFG due to the way the live ranges are not tied to their
 * variables. We will likely need to recompute at CFG level
 */
static void compute_block_level_used_and_assigned_sets(basic_block_t* block){
	//Wipe these two values out
	reset_dynamic_array(block->used_variables);
	reset_dynamic_array(block->assigned_variables);

	//Instruction cursor
	instruction_t* cursor = block->leader_statement;

	//Now we run through the block to recompute
	while(cursor != NULL){
		switch(cursor->instruction_type){
			//These two have no use associated with them
			case PHI_FUNCTION:
			case RET:
				break;

			case INCB:
			case INCW:
			case INCL:
			case INCQ:
			case DECB:
			case DECW:
			case DECL:
			case DECQ:
				//This counts as both an assignment and a use
				add_assigned_live_range(cursor->destination_register->associated_live_range, block);
				add_used_live_range(cursor->destination_register->associated_live_range, block);
				break;

			default:
				//Handle destination 1
				if(cursor->destination_register != NULL){
					update_use_assignment_for_destination_variable(cursor, block);
				}

				//Handle destination 2(this is rare but we have it sometimes)
				if(cursor->destination_register2 != NULL){
					add_assigned_live_range(cursor->destination_register2->associated_live_range, block);
				}

				//And then the usual procedure for source1 
				if(cursor->source_register != NULL){
					add_used_live_range(cursor->source_register->associated_live_range, block);
				}

				//And then the usual procedure for source2
				if(cursor->source_register2 != NULL){
					add_used_live_range(cursor->source_register2->associated_live_range, block);
				}

				//And then the usual procedure for the address calc reg
				if(cursor->address_calc_reg1 != NULL){
					add_used_live_range(cursor->address_calc_reg1->associated_live_range, block);
				}

				//And then the usual procedure for the address calc reg
				if(cursor->address_calc_reg2 != NULL){
					add_used_live_range(cursor->address_calc_reg2->associated_live_range, block);
				}
					
				break;
		}

		//Advance it up
		cursor = cursor->next_statement;
	}
}


/**
 * Recompute the used & assigned sets for the whole CFG
 * This is done after we coalesce at least one live range
 */
static void recompute_used_and_assigned_sets(cfg_t* cfg){
	//Grab a cursor block
	basic_block_t* cursor = cfg->head_block;

	//Run through every block
	while(cursor != NULL){
		//Invoke the block-level helper
		compute_block_level_used_and_assigned_sets(cursor);

		//Push it up
		cursor = cursor->direct_successor;
	}
}


/**
 * Perform coalescence at a block level. Remember that if we do end up coalescing, we need to recompute the used and assigned sets for
 * this block as those are affected by coalescing
 */
static u_int8_t perform_block_level_coalescence(basic_block_t* block, interference_graph_t* graph, u_int8_t debug_printing){
	//By default assume nothing happened
	u_int8_t coalescence_occured = FALSE;

	//Now we'll run through every instruction in every block
	instruction_t* instruction = block->leader_statement;

	//Now run through all of these
	while(instruction != NULL){
		//If it's not a pure copy *or* it's marked as non-combinable, just move along
		if(is_instruction_pure_copy(instruction) == FALSE
			|| instruction->cannot_be_combined == TRUE){
			instruction = instruction->next_statement;
			continue;
		}

		//Otherwise if we get here, then we know that we have a pure copy instruction
		live_range_t* source_live_range = instruction->source_register->associated_live_range;
		live_range_t* destination_live_range = instruction->destination_register->associated_live_range;

		//We need to ensure that the two live ranges:
		//	1.) Do not interfere with one another(and as such they're in separate webs)
		//	2.) Do not have any pre-coloring that would prevent them from being merged. For example, if the
		//	destination register is %rdi because it's a function parameter, we can't just change the register
		//	it's in
		if(do_live_ranges_interfere(graph, destination_live_range, source_live_range) == FALSE
			&& does_register_allocation_interference_exist(source_live_range, destination_live_range) == FALSE){

			//DEBUG LOGS
			if(debug_printing == TRUE){
				printf("Can coalesce LR%d and LR%d\n", source_live_range->live_range_id, destination_live_range->live_range_id);
				printf("DELETING LR%d\n", destination_live_range->live_range_id);
			}

			//Perform the actual coalescence
			coalesce_live_ranges(graph, source_live_range, destination_live_range);

			//Be sure to now set this flag - we have coalesced overall here
			coalescence_occured = TRUE;

			//Grab a holder to this 
			instruction_t* holder = instruction;

			//Push this up
			instruction = instruction->next_statement;

			//DEBUG
			if(debug_printing == TRUE){
				printf("Deleting:\n");
				print_instruction(stdout, holder, PRINTING_VAR_INLINE);
			}

			//Delete the old one from the graph
			delete_statement(holder);
		
		//All we need do here is advance it up
		} else {
			//Push this up
			instruction = instruction->next_statement;
		}
	}

	//Return whether or not we did or did not coalesce
	return coalescence_occured;
}


/**
 * Perform live range coalescing on a given instruction. This sees
 * us merge the source and destination operands's webs(live ranges)
 *
 * We coalesce source to destination. When we're done, the *source* should
 * survive, the destination should NOT
 */
static u_int8_t perform_live_range_coalescence(cfg_t* cfg, interference_graph_t* graph, u_int8_t debug_printing){
	//By default, assume we did not coalesce anything
	u_int8_t coalescence_occured = FALSE;

	//Run through every single block in here
	basic_block_t* current = cfg->head_block;

	//Run through every block
	while(current != NULL){
		//Invoke the helper for the block-level coalescing
		u_int8_t block_coalesced = perform_block_level_coalescence(current, graph, debug_printing);

		//If it's false - then the new value is what we got. If it's already true, don't bother
		//setting it again
		if(coalescence_occured == FALSE){
			coalescence_occured = block_coalesced;
		}

		//Advance to the direct successor
		current = current->direct_successor;
	}

	//Give back whether or not we did coalesce
	return coalescence_occured;
}


/**
 *
 * Allocate an individual register to a given live range
 *
 * We return TRUE if we were able to color, and we return false if we were not
 */
static u_int8_t allocate_register(live_range_t* live_range){
	//If this is the case, we're already done. This will happen in the event that a register has been pre-colored
	if(live_range->reg != NO_REG){
		//Flag this as used in the function
		if(live_range->assignment_count > 0){
			live_range->function_defined_in->assigned_registers[live_range->reg - 1] = TRUE;
		}

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
		if(live_range->assignment_count > 0){
			live_range->function_defined_in->assigned_registers[i] = TRUE;
		}

		//Return true here
		return TRUE;

	//This means that our neighbors allocated all of the registers available
	} else {
		return FALSE;
	}
}


/**
 * Get the largest type in a given Live range. This is used for determining stack allocation
 * size. We need to do this due to how type coercion can work in OC
 */
static generic_type_t* get_largest_type_in_live_range(live_range_t* target){
	//Seed with 0
	u_int32_t largest_type_size = 0;

	//Starts off as null
	generic_type_t* largest_type = NULL;

	//Run through all of the variables
	for(u_int16_t i = 0; i < target->variables->current_index; i++){
		//Grab the variable out
		three_addr_var_t* variable = dynamic_array_get_at(target->variables, i);

		//If we're bigger, reassign
		if(variable->type->type_size > largest_type_size){
			//This is now the largest type
			largest_type = variable->type;

			//And our largest type size is this one's size
			largest_type_size = largest_type->type_size;
		}
	}

	//Give back the largest type
	return largest_type;
}


/**
 * Handle all source spilling that we need to do
 */
static void handle_source_spill(dynamic_array_t* live_ranges, three_addr_var_t* target_source, live_range_t* spill_range, live_range_t** currently_spilled, instruction_t* target, u_int32_t offset){
	//Do we even need to bother here? If not, just leave
	if(target_source == NULL || target_source->associated_live_range != spill_range){
		return;
	}

	//Otherwise, we have a match. Let's first check if we already have this register out as something that is "currently_spilled". If we
	//don't then we need to generate a load instruction and put it before the target
	if(*currently_spilled == NULL){
		//We'll need a dummy var for this
		three_addr_var_t* dummy = emit_temp_var(target_source->type);

		//Once we have the dummy, we can create the new LR
		*currently_spilled = live_range_alloc(target->function);

		//Flag that this was once spilled
		(*currently_spilled)->was_spilled = TRUE;

		//Be sure we copy this over too
		(*currently_spilled)->function_parameter_order = spill_range->function_parameter_order;

		//Add it in
		dynamic_array_add(live_ranges, *currently_spilled);

		//We can put the dummy in now
		add_variable_to_live_range(*currently_spilled, dummy);

		//Handle the load instruction
		instruction_t* load_instruction = emit_load_instruction(dummy, stack_pointer, type_symtab, offset);

		//Insert it before the target
		insert_instruction_before_given(load_instruction, target);
	}

	//No matter what happened there, the target source now points to this new currently spilled LR
	add_variable_to_live_range(*currently_spilled, target_source);
}


/**
 * Handle spilling a destination to memory
 */
static void handle_destination_spill(three_addr_var_t* var, instruction_t* instruction, u_int32_t offset){
	//Let the helper do this for us
	instruction_t* store = emit_store_instruction(var, stack_pointer, type_symtab, offset);

	//Insert the store after the assignment
	insert_instruction_after_given(store, instruction);
}


/**
 * Handle all spilling for a given instruction. This includes source & destination
 * spilling
 */
static instruction_t* handle_instruction_level_spilling(instruction_t* instruction, dynamic_array_t* live_ranges, live_range_t* spill_range, live_range_t** currently_spilled, stack_region_t* spill_region){
	//By default it's the instruction
	instruction_t* latest = instruction;

	//Handle all source spills first
	handle_source_spill(live_ranges, instruction->source_register, spill_range, currently_spilled, instruction, spill_region->base_address);
	handle_source_spill(live_ranges, instruction->source_register2, spill_range, currently_spilled, instruction, spill_region->base_address);
	handle_source_spill(live_ranges, instruction->address_calc_reg1, spill_range, currently_spilled, instruction, spill_region->base_address);
	handle_source_spill(live_ranges, instruction->address_calc_reg2, spill_range, currently_spilled, instruction, spill_region->base_address);

	//Run through all function parameters
	if(instruction->parameters != NULL && instruction->instruction_type != PHI_FUNCTION){
		//Extract it
		dynamic_array_t* parameters = instruction->parameters;

		//Run through and check them all
		for(u_int16_t i = 0; i < parameters->current_index; i++){
			//Extract it
			three_addr_var_t* parameter = dynamic_array_get_at(parameters, i);

			//Invoke the helper
			handle_source_spill(live_ranges, parameter, spill_range, currently_spilled, instruction, spill_region->base_address);
		}
	}

	/**
	 * Let's now handle the destination register. There are 2 things that we need to account for
	 * in the destination register due to the way that we've been reassigning things. The destination
	 * register may itself be the spill range, *or* it may also be the "currently spilled" value
	 * that we've been working with. We will handle both cases
	 */
	if(instruction->destination_register != NULL){
		//First case - the destination register is the spill range
		if(instruction->destination_register->associated_live_range == spill_range
			|| instruction->destination_register->associated_live_range == *currently_spilled){

			//In this case, we need to handle a source spill and a destination store
			if(is_destination_also_operand(instruction) == TRUE){
				//Handle the source first
				handle_source_spill(live_ranges, instruction->destination_register, spill_range, currently_spilled, instruction, spill_region->base_address);

				//Emit the store instruction for this now
				handle_destination_spill(instruction->destination_register, instruction, spill_region->base_address);

				//Update latest
				latest = instruction->next_statement;

			//In the case like this, we just need to emit the load
			} else if(is_destination_assigned(instruction) == FALSE){
				//Handle the source spill only
				handle_source_spill(live_ranges, instruction->destination_register, spill_range, currently_spilled, instruction, spill_region->base_address);

			//In all other cases, we just have the store
			} else {
				//Emit the store instruction for this now
				handle_destination_spill(instruction->destination_register, instruction, spill_region->base_address);

				//Update latest
				latest = instruction->next_statement;
			}
		}
	}

	//Same - but rarer - for destination register 2
	if(instruction->destination_register2 != NULL){
		//First case - the destination register is the spill range
		if(instruction->destination_register2->associated_live_range == spill_range
			|| instruction->destination_register2->associated_live_range == *currently_spilled){

			//Emit the store instruction for this now
			handle_destination_spill(instruction->destination_register2, instruction, spill_region->base_address);

			//Update latest
			latest = instruction->next_statement;
		}
	}

	//No matter what, currently spilled gets nulled out here
	*currently_spilled = NULL;

	//Give back the last instruction
	return latest;
}



/**
 * Spill a given live range across the entire CFG. Remember that when we spill,
 * we replace every use of the old live range with a load and every assignment
 * with a store.
 *
 * For instance:
 *
 * spill_range = LR33
 *
 * addb LR10, LR33
 *
 * Should become
 *
 * movb (LR0), LR35(new)
 * addb LR10, LR35
 * movb LR35, (LR0)
 *
 * And LR33 is nowhere to be seen anymore.
 *
 * By the time that we are done, LR33 should *never* be seen in the CFG again. It should
 * have no neighbors and nobody should need to contend with it, because it will have been replaced
 * by a bunch of smaller, shorter live ranges
 *
 *
 * Spilling principles: Every spill will generate its own brand new live range. This live range
 * is our "currently spilled" live range. For the example above, once we have spilled into the new
 * LR35, we need to keep that as active until we load it back up
 */
static void spill(cfg_t* cfg, dynamic_array_t* live_ranges, live_range_t* spill_range){
	//Extract the function that we're using
	symtab_function_record_t* function = spill_range->function_defined_in;

	//Let's first create the stack region for our spill range
	stack_region_t* spill_region = create_stack_region_for_type(&(function->data_area), get_largest_type_in_live_range(spill_range));

	//Now that we have our spill region, we need to start crawling through the entire CFG and replacing
	//all uses/assignments appropriately
	
	//Grab a cursor
	basic_block_t* block_cursor = cfg->head_block;

	//Initially nothing is spilled
	live_range_t* currently_spilled = NULL;

	//So long as it's not null
	while(block_cursor != NULL){
		//Now we need an instruction cursor
		instruction_t* cursor = block_cursor->leader_statement;

		//So long as this is not NULL, keep going
		while(cursor != NULL){
			//Let the helper deal with it
			cursor = handle_instruction_level_spilling(cursor, live_ranges, spill_range, &currently_spilled, spill_region);

			//Push it up to the next instruction
			cursor = cursor->next_statement;
		}

		//Push it up
		block_cursor = block_cursor->direct_successor;
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
static u_int8_t graph_color_and_allocate(cfg_t* cfg, dynamic_array_t* live_ranges){
	//We first need to construct the priority version of the live range arrays
	//Create a new one
	dynamic_array_t* priority_live_ranges = dynamic_array_alloc();

	//Run through and insert everything into the priority live range
	for(u_int16_t i = 0; i < live_ranges->current_index; i++){
		//Extract it
		live_range_t* live_range = dynamic_array_get_at(live_ranges, i);

		//No point in putting this in there, we already know what it will be
		if(live_range == stack_pointer_lr || live_range == instruction_pointer_lr){
			continue;
		}

		//Otherwise put it into our priority queue
		dynamic_array_priority_insert_live_range(priority_live_ranges, live_range);
	}

	//So long as this isn't empty
	while(dynamic_array_is_empty(priority_live_ranges) == FALSE){
		//Grab a live range out by deletion
		live_range_t* range = dynamic_array_delete_from_back(priority_live_ranges);

		/**
		 * This degree being less than the number of registers
		 * means we should be able to allocate no issue
		 */
		if(range->degree < K_COLORS_GEN_USE){
			allocate_register(range);

		//Otherwise, we may still be able to allocate here
		} else {
			//We must still attempt to allocate it
			u_int8_t can_allocate = allocate_register(range);
			
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
				//trigger the restart by the parent process
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
 * Insert caller saved logic for a direct function call. In a direct function call, we'll know what
 * registers are being used by the function being called. As such, we can be precise about what
 * we push/pop onto and off of the stack and have for a more efficient saving regime. This is not
 * possible for indirect function calls, which is the reason for the distinction
 */
static instruction_t* insert_caller_saved_logic_for_direct_call(instruction_t* instruction){
	//If we get here we know that we have a call instruction. Let's
	//grab whatever it's calling out. We're able to do this for a direct call,
	//whereas in an indirect call we are not
	symtab_function_record_t* callee = instruction->called_function;

	//Grab out this LR for reference later on. Remember that this is nullable, so we 
	//need to account for that
	live_range_t* destination_lr = NULL;
	//By default it's NO_REG, we will assign if it exists below
	general_purpose_register_t destination_lr_reg = NO_REG;
	
	//Assign if it's not null
	if(instruction->destination_register != NULL){
		destination_lr = instruction->destination_register->associated_live_range;
		//Save the register as well now that we know it exists
		destination_lr_reg = destination_lr->reg;
	}

	//Start off with this as the last instruction
	instruction_t* last_instruction = instruction;

	//Use the helper to calculate LIVE_AFTER up to but not including the actual function call
	dynamic_array_t* live_after = calculate_live_after_for_block(instruction->block_contained_in, instruction);
	//We can remove the destination LR from here, there's no point in keeping it in
	dynamic_array_delete(live_after, destination_lr);

	/**
	 * Run through everything that is alive after this function runs(live_after)
	 * and check if we need to save any registers from that set
	 */
	for(u_int16_t i = 0; i < live_after->current_index; i++){
		//Extract it
		live_range_t* lr = dynamic_array_get_at(live_after, i);

		//And remove its register
		general_purpose_register_t reg = lr->reg;

		//If it's not caller-saved, it's irrelevant to us
		if(is_register_caller_saved(reg) == FALSE){
			continue;
		}

		//There's also no point in saving this. This could happen
		//if we have precoloring
		if(reg == destination_lr_reg){
			continue;
		}

		/**
		 * Once we get past here, we know that we need to save this
		 * register because the callee will also assign it, so whatever
		 * value it has that we're relying on would not survive the call
		 */
		if(callee->assigned_registers[reg - 1] == TRUE){
			//Emit a direct push with this live range's register
			instruction_t* push_inst = emit_direct_register_push_instruction(reg);

			//Emit the pop instruction for this
			instruction_t* pop_inst = emit_direct_register_pop_instruction(reg);

			//Insert the push instruction directly before the call instruction
			insert_instruction_before_given(push_inst, instruction);

			//Insert the pop instruction directly after the last instruction
			insert_instruction_after_given(pop_inst, instruction);

			//If the last instruction still is the original instruction. That
			//means that this is the first pop instruction that we're inserting.
			//As such, we'll set the last instruction to be this pop instruction
			//to save ourselves time down the line
			if(last_instruction == instruction){
				last_instruction = pop_inst;
			}
		}
	}

	//Free it up once done
	dynamic_array_dealloc(live_after);

	//Return whatever this ended up being
	return last_instruction;
}


/**
 * For an indirect call, we can not know for certain what registers are and are not used
 * inside of the function. As such, we'll need to save any/all caller saved registers that are in use
 * at the time that the function is called
 */
static instruction_t* insert_caller_saved_logic_for_indirect_call(instruction_t* instruction){
	//Get the destination LR. Remember that this is nullable
	live_range_t* destination_lr = NULL;
	//We'll reassign if it exists
	general_purpose_register_t destination_reg = NO_REG;

	//Extract if not null
	if(instruction->destination_register != NULL){
		destination_lr = instruction->destination_register->associated_live_range;
		//Extract this too if not null
		destination_reg = destination_lr->reg;
	}

	//We'll maintain a pointer to the last instruction. This initially is the instruction that we
	//have, but will change to be the first pop instruction that we make 
	instruction_t* last_instruction = instruction;

	//Grab the live_after array for up to but not including the actual call
	dynamic_array_t* live_after = calculate_live_after_for_block(instruction->block_contained_in, instruction);
	//Delete the destination LR from here as it will be assigned by the instruction anyway
	dynamic_array_delete(live_after, destination_lr);

	/**
	 * Now we will run through every live range in live_after and check if it is caller-saved or not
	 * We are not able to fine-tune things here like we are in the the direct call unfortunately
	 */
	for(u_int16_t i = 0; i < live_after->current_index; i++){
		//Grab it out
		live_range_t* lr = dynamic_array_get_at(live_after, i);

		//Extract its register too
		general_purpose_register_t reg = lr->reg;

		//It's not caller saved, so it's irrelevant
		if(is_register_caller_saved(reg) == FALSE){
			continue;
		}

		//We'll also skip over this too
		if(reg == destination_reg){
			continue;
		}

		//Emit a direct push with this live range's register
		instruction_t* push_inst = emit_direct_register_push_instruction(reg);

		//Emit the pop instruction for this
		instruction_t* pop_inst = emit_direct_register_pop_instruction(reg);

		//Insert the push instruction directly before the call instruction
		insert_instruction_before_given(push_inst, instruction);

		//Insert the pop instruction directly after the last instruction
		insert_instruction_after_given(pop_inst, instruction);

		//If the last instruction still is the original instruction. That
		//means that this is the first pop instruction that we're inserting.
		//As such, we'll set the last instruction to be this pop instruction
		//to save ourselves time down the line
		if(last_instruction == instruction){
			last_instruction = pop_inst;
		}
	}

	//Free it up once done
	dynamic_array_dealloc(live_after);

	//Return the last instruction to save time when drilling
	return last_instruction;
}


/**
 * Run through the current function and insert all needed save/restore logic
 * for caller-saved registers
 */
static void insert_caller_saved_register_logic(basic_block_t* function_entry_block){
	//We'll grab out everything we need from this function
	//Extract this for convenience
	symtab_function_record_t* function = function_entry_block->function_defined_in;

	//Define a cursor for crawling
	basic_block_t* cursor = function_entry_block;

	//So long as we're in this current function, keep going
	while(cursor != NULL && cursor->function_defined_in == function){
		//Now we'll grab a hook to the first statement
		instruction_t* instruction = cursor->leader_statement;

		//Now we'll run through every single instruction in here
		while(instruction != NULL){
			switch(instruction->instruction_type){
				//Use the helper for a direct call
				case CALL:
					instruction = insert_caller_saved_logic_for_direct_call(instruction);
					break;
					
				//Use the helper for an indirect call. Indirect calls differ slightly
				//from direct ones because we have less information, so we'll need a different
				//helper here
				case INDIRECT_CALL:
					instruction = insert_caller_saved_logic_for_indirect_call(instruction);
					break;

				//By default we leave and just advance
				default:
					break;
			}

			//Onto the next instruction
			instruction = instruction->next_statement;
		}

		//Advance down to the direct successor
		cursor = cursor->direct_successor;
	}
}


/**
 * This function handles all callee saving logic for each function that we have on top off emitting
 * the stack allocation and deallocation statements that we need for each function
 */
static void insert_stack_and_callee_saving_logic(cfg_t* cfg, basic_block_t* function_entry, basic_block_t* function_exit){
	//Keep a reference to the original entry instruction that we had before
	//we insert any pushes. This will be important for when we need to
	//reassign the function's leader statement
	instruction_t* entry_instruction = function_entry->leader_statement;
	
	//Grab the function record out now too
	symtab_function_record_t* function = function_entry->function_defined_in;

	//We'll also need it's stack data area
	stack_data_area_t area = function_entry->function_defined_in->data_area;

	//Align it
	align_stack_data_area(&area);

	//Grab the total size out
	u_int32_t total_size = area.total_size;

	//We need to see which registers that we use
	for(u_int16_t i = 0; i < K_COLORS_GEN_USE; i++){
		//We don't use this register, so move on
		if(function->assigned_registers[i] == FALSE){
			continue;
		}

		//Otherwise if we get here, we know that we use it. Remember
		//the register value is always offset by one
		general_purpose_register_t used_reg = i + 1;

		//If this isn't callee saved, then we know to move on
		if(is_register_callee_saved(used_reg) == FALSE){
			continue;
		}

		//Now we'll need to add an instruction to push this at the entry point of our function
		instruction_t* push = emit_direct_register_push_instruction(used_reg);

		//Insert this push before the leader instruction
		insert_instruction_before_given(push, entry_instruction);

		//If the entry instruction is still the function's leader statement, then
		//we'll need to update it. This only happens on the very first push. For
		//everyting subsequent, we won't need to do this
		if(entry_instruction == function_entry->leader_statement){
			//Reassign this to be the very first push
			function_entry->leader_statement = push;
		}
	}

	//If we have a total size to emit, we'll add it in here
	if(total_size != 0){
		//For each function entry block, we need to emit a stack subtraction that is the size of that given variable
		instruction_t* stack_allocation = emit_stack_allocation_statement(cfg->stack_pointer, cfg->type_symtab, total_size);

		//Now that we have the stack allocation statement, we can add it in to be right before the current leader statement
		insert_instruction_before_given(stack_allocation, entry_instruction);

		//If the entry instruction was the function's leader statement, then this now will be the leader statement
		if(entry_instruction == function_entry->leader_statement){
			function_entry->leader_statement = entry_instruction;
		}
	}


	//Now that we've added all of the callee saving logic at the function entry, we'll need to
	//go through and add it at the exit(s) as well. Note that we're given the function exit block
	//as an input value here
	
	//For each and every predecessor of the function exit block
	for(u_int16_t i = 0; i < function_exit->predecessors->current_index; i++){
		//Grab the given predecessor out
		basic_block_t* predecessor = dynamic_array_get_at(function_exit->predecessors, i);

		//If the area has a larger total size than 0, we'll need to add in the deallocation
		//before every return statement
		if(total_size > 0){
			//Emit the stack deallocation statement
			instruction_t* stack_deallocation = emit_stack_deallocation_statement(cfg->stack_pointer, cfg->type_symtab, total_size);

			//We will insert this right before the very last statement in each predecessor
			insert_instruction_before_given(stack_deallocation, predecessor->exit_statement);
		}

		//Now we'll go through the registers in the reverse order. This time, when we hit one that
		//is callee-saved and used, we'll emit the push instruction and insert it directly before
		//the "ret". This will ensure that our LIFO structure for pushing/popping is maintained

		//Run through all the registers backwards
		for(int16_t j = K_COLORS_GEN_USE - 1; j >= 0; j--){
			//If we haven't used this register, then skip it
			if(function->assigned_registers[j] == FALSE){
				continue;
			}

			//Remember that our positional coding is off by 1(0 is NO_REG value), so we'll
			//add 1 to make the value correct
			general_purpose_register_t used_reg = j + 1;

			//If it's not callee saved then we don't care
			if(is_register_callee_saved(used_reg) == FALSE){
				continue;
			}

			//If we make it here, we know that we'll need to save this register
			instruction_t* pop_instruction = emit_direct_register_pop_instruction(used_reg);

			//Insert it before the ret
			insert_instruction_before_given(pop_instruction, predecessor->exit_statement);
		}
	}
}


/**
 * Now that we are done spilling, we need to insert all of the stack logic,
 * including additions and subtractions, into the functions. We also need
 * to insert pushing of any/all callee saved and caller saved registers to maintain
 * our calling convention
 */
static void insert_saving_logic(cfg_t* cfg){
	//Run through every function entry point in the CFG
	for(u_int16_t i = 0; i < cfg->function_entry_blocks->current_index; i++){
		//Grab out the function exit and entry blocks
		basic_block_t* current_function_entry = dynamic_array_get_at(cfg->function_entry_blocks, i);
		basic_block_t* current_function_exit = dynamic_array_get_at(cfg->function_exit_blocks, i);

		//We'll first insert the callee-saved stack and register logic
		insert_stack_and_callee_saving_logic(cfg, current_function_entry, current_function_exit);

		//And now we'll let the helper insert all of the caller-saved register logic
		insert_caller_saved_register_logic(current_function_entry);
	}
}


/**
 * Perform our register allocation algorithm on the entire cfg
 */
void allocate_all_registers(compiler_options_t* options, cfg_t* cfg){
	//Save whether or not we want to actually print IRs
	u_int8_t print_irs = options->print_irs;
	u_int8_t print_post_allocation = options->print_post_allocation;
	u_int8_t debug_printing = options->enable_debug_printing;

	//Save these in global state
	stack_pointer = cfg->stack_pointer;
	type_symtab = cfg->type_symtab;

	//Save the flag that tells us whether or not the graph that we constructed was colorable
	u_int8_t colorable = FALSE;

	/**
	 * STEP 1: Build all live ranges from variables:
	 * 	
	 * 	Invoke the helper rule to crawl the entire CFG, constructing all live
	 * 	ranges *and* doing the needed bookkeeping for used & assigned variables
	 * 	that will be needed for step 2
	 *
	 * 	We only need to do this once for the allocation
	 */
	dynamic_array_t* live_ranges = construct_all_live_ranges(cfg);

	//If we are printing these now is the time to display
	if(print_irs == TRUE){
		printf("============= Before Liveness ==============\n");
		print_blocks_with_live_ranges(cfg);
		printf("============= Before Liveness ==============\n");
	}

	/**
	 * STEP 2: Compute spill costs for live ranges
	 *
	 * Once we've constructed all of our live ranges, we need to go through and
	 * update the spill cost for each one according to its use and assignment
	 * counts. Since it is not possible to get an accurate use/assignment count
	 * until the entire cfg is combed over, we need to do this in a separate step
	 */
	compute_spill_costs(live_ranges);

	//If we are printing these now is the time to display
	if(print_irs == TRUE){
		printf("=============== After Cost Update ============\n");
		print_all_live_ranges(live_ranges);
		printf("=============== After Cost Update ============\n");
	}

	/**
	 * STEP 3: Construct LIVE_IN and LIVE_OUT sets
	 *
	 * Before we can properly determine interference, we need
	 * to construct the LIVE_IN and LIVE_OUT sets in each block.
	 * We cannot simply reuse these from before because they've been so heavily
	 * modified by this point in the compilation process that starting over
	 * is easier
	 *
	 * We will need to do this every single time we reallocate
	*/
	calculate_live_range_liveness_sets(cfg);

	//Show our IR's here
	if(print_irs == TRUE){
		//Show our live ranges once again
		print_all_live_ranges(live_ranges);
	}

	/**
	 * STEP 4: Construct the interference graph
	 *
	 * Now that we have the LIVE_IN and LIVE_OUT sets constructed, we're able
	 * to determine the interference that exists between Live Ranges. This is
	 * a necessary step in being able to allocate registers in any way at all
	 * The algorithm is detailed more in the function
	 *
	 * Again, this is required every single time we need to retry after a spill
	*/
	interference_graph_t* graph = construct_interference_graph(cfg, live_ranges);

	//If we are printing these now is the time to display
	if(print_irs == TRUE){
		printf("============= After Live Range Determination ==============\n");
		print_blocks_with_live_ranges(cfg);
		printf("============= After Live Range Determination ==============\n");
	}

	/**
	 * STEP 5: Pre-coloring registers
	 *
	 * Now that we have the interference calculated, we will "pre-color" live ranges
	 * whose color is known before allocation. This includes things like:
	 * return values being in %rax, function parameter 1 being in %rdi, etc.
	 *
	 * This has the potential to cause spills
	 */
	 colorable = pre_color(cfg, live_ranges);

	/**
	 * If we couldn't precolor, we'll have spilled a live range and as such must go
	 * to the "spill loop"
	 */
	if(colorable == FALSE){
		goto spill_loop;
	}

	/**
	 * STEP 6: Live range coalescence optimization
	 *
	 * One small optimization that we can make is to perform live-range coalescence
	 * on our given live ranges. We are able to coalesce live ranges if they do
	 * not interfere and we have a pure copy like movq LR0, LR1. More detail
	 * is given in the function
	 *
	 * Since spilling breaks up large live ranges, it has the opportunity to
	 * allow for even more coalescence. We will use this to our advantage
	 * by letting this rule run every time
	*/
	u_int8_t could_coalesce = perform_live_range_coalescence(cfg, graph, debug_printing);

	/**
	 * If we were in fact able to coalesce, we will have messed up the liveness sets due
	 * to merging live ranges, etc. This may mess up allocation down the line. As such, we
	 * need to come here and recalculate the following:
	 * 	1.) all of the used & assigned sets
	 * 	2.) all of the liveness sets(those rely on used & defined entirely)
	 * 	3.) the interference
	 *
	 * short of this, we will see strange an inaccurate results such as excessive interference
	 */
	if(could_coalesce == TRUE){
		//We need to *reset* all of our live ranges here
		reset_all_live_ranges(live_ranges);

		//First step - recalculate all of our used & assigned sets
		recompute_used_and_assigned_sets(cfg);

		//After we coalesce, we need to recompute all of the
		//spill costs
		compute_spill_costs(live_ranges);

		//Then - recalculate all liveness sets
		calculate_live_range_liveness_sets(cfg);

		//Finally, recalculate all of the interference now that all of the
		//prerequisites have been met
		graph = construct_interference_graph(cfg, live_ranges);
	}

	//Show our live ranges once again if requested
	if(print_irs == TRUE){
		print_all_live_ranges(live_ranges);
		printf("================= After Coalescing =======================\n");
		print_blocks_with_live_ranges(cfg);
		printf("================= After Coalescing =======================\n");
	}

	/**
	 * STEP 7: Invoke the actual allocator
	 *
	 * The allocator will attempt to color the graph. If the graph is not k-colorable, 
	 * then the allocator will spill the least costly LR and return FALSE, and we will go through
	 * this whol process again
	*/
	colorable = graph_color_and_allocate(cfg, live_ranges);

	/**
	 * Our so-called "spill loop" essentially repeats most of the steps
	 * above until we create a colorable graph. Spilling completely disrupts
	 * the interference, use, assignment and liveness properties of the old graph,
	 * so we are required to do most of these steps over again after every spill
	 *
	 * In reality, usually this will only happen once or twice, even in the most extreme 
	 * cases
	 */
	u_int16_t count = 0;
spill_loop:
	//Keep going so long as we can't color
	while(colorable == FALSE){
		if(print_irs == TRUE){
			printf("============ After Spilling =============== \n");
			print_blocks_with_live_ranges(cfg);
			printf("============ After Spilling =============== \n");
		}
		count++;

		/**
		 * First reset all of these
		 */
		reset_all_live_ranges(live_ranges);

		/**
		 * Following this, we need to recompute all of our used and assigned
		 * sets
		 */
		recompute_used_and_assigned_sets(cfg);

		/**
		 * Now that we've recomputed the used and assigned
		 * sets, we need to go back through and update all of our spill
		 * costs
		 */
		compute_spill_costs(live_ranges);

		/**
		 * Following that, we need to go through and calculate
		 * all of our liveness sets again
		 */
		calculate_live_range_liveness_sets(cfg);

		/**
		 * Once the liveness sets have been recalculated, we're able
		 * to go through and compute all of the interference again
		 */
		graph = construct_interference_graph(cfg, live_ranges);

		//Show our live ranges once again if requested
		if(print_irs == TRUE){
			print_all_live_ranges(live_ranges);
			printf("================= After Interference =======================\n");
			print_blocks_with_live_ranges(cfg);
			printf("================= After Interference =======================\n");
		}


		u_int8_t could_coalesce = perform_live_range_coalescence(cfg, graph, debug_printing);

		/**
		 * If we were in fact able to coalesce, we will have messed up the liveness sets due
		 * to merging live ranges, etc. This may mess up allocation down the line. As such, we
		 * need to come here and recalculate the following:
		 * 	1.) all of the used & assigned sets
		 * 	2.) all of the liveness sets(those rely on used & defined entirely)
		 * 	3.) the interference
		 *
		 * short of this, we will see strange an inaccurate results such as excessive interference
		 */
		if(could_coalesce == TRUE){
			//We need to *reset* all of our live ranges here
			reset_all_live_ranges(live_ranges);

			//First step - recalculate all of our used & assigned sets
			recompute_used_and_assigned_sets(cfg);

			//After we coalesce, we need to recompute all of the
			//spill costs
			compute_spill_costs(live_ranges);

			//Then - recalculate all liveness sets
			calculate_live_range_liveness_sets(cfg);

			//Finally, recalculate all of the interference now that all of the
			//prerequisites have been met
			graph = construct_interference_graph(cfg, live_ranges);
		}

		/**
		 * And finally - once we have our interference, we are
		 * able to go through and attempt to color once
		 * again. This loop will keep executing until the
		 * graph_color_and_allocate returns a successful result
		 */
		colorable = graph_color_and_allocate(cfg, live_ranges);

		if(count > 2) exit(1);
	}

	/**
	 * STEP 8: caller/callee saving logic
	 *
	 * Once we make it down here, we have colored the entire graph successfully. But,
	 * we still need to insert any caller/callee saving logic that is needed
	 * when appropriate
	*/
	insert_saving_logic(cfg);

	/**
	 * STEP 9: final cleanup pass
	 *
	 * This is detailed more in the postprocessor.c file that we're invoking
	 * here
	*/
	postprocess(cfg);

	//One final print post allocation
	if(print_irs == TRUE || print_post_allocation == TRUE){
		printf("================= After Allocation =======================\n");
		print_blocks_with_registers(cfg);
		printf("================= After Allocation =======================\n");
	}
}
