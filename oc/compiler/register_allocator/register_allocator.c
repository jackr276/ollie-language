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
#include "../utils/queue/max_priority_queue.h"
#include "../cfg/cfg.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ucontext.h>

//The atomically increasing live range id
u_int32_t live_range_id = 0;

/**
 * Cache all of our register parameters for passing
 */
const general_purpose_register_t gen_purpose_parameter_registers[] = {RDI, RSI, RDX, RCX, R8, R9};
const sse_register_t sse_parameter_registers[] = {XMM0, XMM1, XMM2, XMM3, XMM4, XMM5};

//Spill a live range
static void spill_in_function(basic_block_t* function_entry_block, dynamic_array_t* live_ranges, live_range_t* spill_range);

//Just hold the stack pointer live range
static live_range_t* stack_pointer_lr;
//Holds the instruction pointer LR
static live_range_t* instruction_pointer_lr;
//The stack pointer
static three_addr_var_t* stack_pointer;
//And the type symtab
static type_symtab_t* type_symtab;
//The u64 type for reference
static generic_type_t* u64_type;


/**
 * What is the result of our live range coalescing run?
 * The options represent:
 * 	- nothing was coalesced
 * 	- exclusively general purpose(gp) LRs coalesced
 * 	- exclusively SSE LRs coalesced
 * 	- Both gp and SSE coalesced
 */
typedef enum {
	COALESCENCE_RESULT_NONE,
	COALESCENCE_RESULT_GP_ONLY,
	COALESCENCE_RESULT_SSE_ONLY,
	COALESCENCE_RESULT_BOTH
} coalescence_result_t;


/**
 * Crawl the data area and search for an exact pointer match. If there's not an exact
 * match, then NULL will be returned
 */
static inline stack_region_t* get_stack_region_for_live_range(stack_data_area_t* data_area, live_range_t* lr){
	for(u_int16_t i = 0; i < data_area->stack_regions.current_index; i++){
		//Extract it
		stack_region_t* region = dynamic_array_get_at(&(data_area->stack_regions), i);

		//We need an exact memory address match. Anything short and
		//this does not count
		if(region->variable_referenced == lr){
			return region;
		}
	}

	//If we make it all the way down here, we found nothing so return NULL
	return NULL;
}


/**
 * Does a live range for a given variable already exist? If so, we'll need to 
 * coalesce the two live ranges in a union
 *
 * Returns NULL if we found nothing
 */
static inline live_range_t* find_live_range_with_variable(dynamic_array_t* live_ranges, three_addr_var_t* variable){
	//Run through all of the live ranges that we currently have
	for(u_int16_t _ = 0; _ < live_ranges->current_index; _++){
		//Grab the given live range out
		live_range_t* current = dynamic_array_get_at(live_ranges, _);

		//If the variables are equal(ignoring SSA and dereferencing) then we have a match
		for(u_int16_t i = 0 ; i < current->variables.current_index; i++){
			if(variables_equal_no_ssa(variable, dynamic_array_get_at(&(current->variables), i), TRUE) == TRUE){
				return current;
			}
		}
	}

	//If we get here we didn't find anything
	return NULL;
}


/**
 * Get the live range class for a given variable, whether the variable is general purpose
 * or SSE
 */
static inline live_range_class_t get_live_range_class_for_variable(three_addr_var_t* variable){
	//All determined by the variable size
	switch (variable->variable_size) {
		case BYTE:
		case WORD:
		case DOUBLE_WORD:
		case QUAD_WORD:
			return LIVE_RANGE_CLASS_GEN_PURPOSE;

		case SINGLE_PRECISION:
		case DOUBLE_PRECISION:
			return LIVE_RANGE_CLASS_SSE;

		default:
			printf("Fatal internal compiler error: undefined variable size given for live range class determination\n");
			exit(1);
	}
}


/**
 * Is the given general purpose register caller saved?
 */
static inline u_int8_t is_general_purpose_register_caller_saved(general_purpose_register_t reg){
	switch(reg){
		case RAX:
		case RDI:
		case RSI:
		case RDX:
		case RCX:
		case R8:
		case R9:
		case R10:
		case R11:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Is the given general purpose register callee saved?
 */
static inline u_int8_t is_general_purpose_register_callee_saved(general_purpose_register_t reg){
	//This is all determined based on the register type
	switch(reg){
		case RBX:
		case RBP:
		case R12:
		case R13:
		case R14:
		case R15:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Emit a stack allocation statement
 */
static inline instruction_t* emit_stack_allocation_statement(three_addr_var_t* stack_pointer, type_symtab_t* type_symtab, u_int64_t offset){
	//Allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//This is always a subq statement
	stmt->instruction_type = SUBQ;

	//Store the destination as the stack pointer
	stmt->destination_register = stack_pointer;

	//Emit this directly
	stmt->source_immediate = emit_direct_integer_or_char_constant(offset, lookup_type_name_only(type_symtab, "u64", NOT_MUTABLE)->type);

	//Just give this back
	return stmt;
}


/**
 * Emit a stack deallocation statement
 */
static inline instruction_t* emit_stack_deallocation_statement(three_addr_var_t* stack_pointer, type_symtab_t* type_symtab, u_int64_t offset){
	//Allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//This is always an addq statement
	stmt->instruction_type = ADDQ;

	//Destination is always the stack pointer
	stmt->destination_register = stack_pointer;

	//Emit this directly
	stmt->source_immediate = emit_direct_integer_or_char_constant(offset, lookup_type_name_only(type_symtab, "u64", NOT_MUTABLE)->type);

	//Just give this back
	return stmt;
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
static inline u_int32_t increment_and_get_live_range_id(){
	return live_range_id++;
}


/**
 * Create a live range
 */
static live_range_t* live_range_alloc(symtab_function_record_t* function_defined_in, live_range_class_t live_range_class){
	//Calloc it
	live_range_t* live_range = calloc(1, sizeof(live_range_t));

	//Give it a unique id
	live_range->live_range_id = increment_and_get_live_range_id();

	//And create it's dynamic array
	live_range->variables = dynamic_array_alloc();

	//Store what function this came from
	live_range->function_defined_in = function_defined_in;

	//What category of live range(gen purpose or SSE) is this
	live_range->live_range_class = live_range_class;

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
static inline live_range_t* find_or_create_live_range(dynamic_array_t* live_ranges, basic_block_t* block, three_addr_var_t* variable){
	//Lookup the live range that is associated with this
	live_range_t* live_range = find_live_range_with_variable(live_ranges, variable);

	//If this is not null, then it means that we found it, so we can
	//leave
	if(live_range != NULL){
		return live_range;
	}

	//Otherwise if we get here, we'll need to make it ourselves
	live_range = live_range_alloc(block->function_defined_in, get_live_range_class_for_variable(variable));

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
	dynamic_array_dealloc((&live_range->variables));

	//Destroy the neighbors array as well
	dynamic_array_dealloc((&live_range->neighbors));

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
			//Then the name
			printf("%s:\n", block->function_defined_in->func_name.string);
			print_passed_parameter_stack_data_area(&(block->function_defined_in->stack_passed_parameters));
			print_local_stack_data_area(&(block->function_defined_in->local_stack));
			break;

		//By default just print the name
		default:
			printf(".L%d:\n", block->block_id);
			break;
	}

	//If we have some assigned variables, we will dislay those for debugging
	if(block->assigned_variables.internal_array != NULL){
		printf("Assigned: (");

		for(u_int16_t i = 0; i < block->assigned_variables.current_index; i++){
			print_live_range(stdout, dynamic_array_get_at(&(block->assigned_variables), i));

			//If it isn't the very last one, we need a comma
			if(i != block->assigned_variables.current_index - 1){
				printf(", ");
			}
		}
		printf(")\n");
	}

	//If we have some used variables, we will dislay those for debugging
	if(block->used_variables.internal_array != NULL){
		printf("Used: (");

		for(u_int16_t i = 0; i < block->used_variables.current_index; i++){
			print_live_range(stdout, dynamic_array_get_at(&(block->used_variables), i));

			//If it isn't the very last one, we need a comma
			if(i != block->used_variables.current_index - 1){
				printf(", ");
			}
		}
		printf(")\n");
	}

	//If we have some assigned variables, we will dislay those for debugging
	if(block->live_in.internal_array != NULL){
		printf("LIVE IN: (");

		for(u_int16_t i = 0; i < block->live_in.current_index; i++){
			print_live_range(stdout, dynamic_array_get_at(&(block->live_in), i));

			//If it isn't the very last one, we need a comma
			if(i != block->live_in.current_index - 1){
				printf(", ");
			}
		}
		printf(")\n");
	}

	//If we have some assigned variables, we will dislay those for debugging
	if(block->live_out.internal_array != NULL){
		printf("LIVE OUT: (");

		for(u_int16_t i = 0; i < block->live_out.current_index; i++){
			print_live_range(stdout, dynamic_array_get_at(&(block->live_out), i));

			//If it isn't the very last one, we need a comma
			if(i != block->live_out.current_index - 1){
				printf(", ");
			}
		}
		printf(")\n");
	}

	if(block->predecessors.internal_array != NULL){
		printf("Predecessors: (");
		for(u_int16_t i = 0; i < block->predecessors.current_index; i++){
			basic_block_t* predecessor = dynamic_array_get_at(&(block->predecessors), i);

			printf(".L%d", predecessor->block_id);

			//If it isn't the very last one, we need a comma
			if(i != block->predecessors.current_index - 1){
				printf(", ");
			}
		}

		printf(")\n");
	}

	if(block->successors.internal_array != NULL){
		printf("Successors: (");
		for(u_int16_t i = 0; i < block->successors.current_index; i++){
			basic_block_t* successor = dynamic_array_get_at(&(block->successors), i);

			printf(".L%d", successor->block_id);

			//If it isn't the very last one, we need a comma
			if(i != block->successors.current_index - 1){
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
static void print_function_blocks_with_live_ranges(basic_block_t* function_entry){
	//Extract the given function block
	basic_block_t* current = function_entry;

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
	if(block->jump_table != NULL){
		print_jump_table(stdout, block->jump_table);
	}

	//Switch here based on the type of block that we have
	switch(block->block_type){
		//Function entry blocks need extra printing
		case BLOCK_TYPE_FUNC_ENTRY:
			//Then the name
			printf("%s:\n", block->function_defined_in->func_name.string);
			print_passed_parameter_stack_data_area(&(block->function_defined_in->stack_passed_parameters));
			print_local_stack_data_area(&(block->function_defined_in->local_stack));
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
	//Run through all of the functions individually
	for(u_int16_t i = 0; i < cfg->function_entry_blocks.current_index; i++){
		//Extract the given function block
		basic_block_t* current = dynamic_array_get_at(&(cfg->function_entry_blocks), i);

		//So long as this one isn't NULL
		while(current != NULL){
			//Print it
			print_block_with_registers(current);
			//Advance to the direct successor
			current = current->direct_successor;
		}
	}

	//Now we'll print all global variables
	print_all_global_variables(stdout, &(cfg->global_variables));

	//Print all of the local constants too
	print_local_constants(stdout, &(cfg->local_string_constants), &(cfg->local_f32_constants), &(cfg->local_f64_constants), &(cfg->local_xmm128_constants));
}


/**
 * Print all live ranges that we have. This includes our general purpose
 * live ranges and our SSE live ranges
 */
static void print_all_live_ranges(dynamic_array_t* general_purpose_live_ranges, dynamic_array_t* sse_live_ranges){
	printf("============= All Live Ranges ==============\n");
	printf("=============== GENERAL PURPOSE ============\n");
	printf("There are %d general purpose live ranges\n", general_purpose_live_ranges->current_index);
	//For each live range in the array
	for(u_int16_t i = 0; i < general_purpose_live_ranges->current_index; i++){
		//Grab it out
		live_range_t* current = dynamic_array_get_at(general_purpose_live_ranges, i);

		//We'll print out it's id first
		printf("LR%d: {", current->live_range_id);

		//Now we'll run through and print out all of its variables
		for(u_int16_t j = 0; j < current->variables.current_index; j++){
			//Print the variable name
			print_variable(stdout, dynamic_array_get_at(&(current->variables), j), PRINTING_VAR_BLOCK_HEADER);

			//Print a comma if appropriate
			if(j != current->variables.current_index - 1){
				printf(", ");
			}
		}

		printf("} Neighbors: {");

		//Now we'll print out all of it's neighbors
		for(u_int16_t k = 0; k < current->neighbors.current_index; k++){
			live_range_t* neighbor = dynamic_array_get_at(&(current->neighbors), k);
			printf("LR%d", neighbor->live_range_id);
 
			//Print a comma if appropriate
			if(k != current->neighbors.current_index - 1){
				printf(", ");
			}

		}
		
		//And we'll close it out
		printf("}\tSpill Cost: %d\tDegree: %d\n", current->spill_cost, current->degree);
	}
	printf("=============== GENERAL PURPOSE ============\n");


	//Repeat for SSE
	printf("==================== SSE ===================\n");
	printf("There are %d SSE live ranges\n", sse_live_ranges->current_index);
	//For each live range in the array
	for(u_int16_t i = 0; i < sse_live_ranges->current_index; i++){
		//Grab it out
		live_range_t* current = dynamic_array_get_at(sse_live_ranges, i);

		//We'll print out it's id first
		printf("LR%d: {", current->live_range_id);

		//Now we'll run through and print out all of its variables
		for(u_int16_t j = 0; j < current->variables.current_index; j++){
			//Print the variable name
			print_variable(stdout, dynamic_array_get_at(&(current->variables), j), PRINTING_VAR_BLOCK_HEADER);

			//Print a comma if appropriate
			if(j != current->variables.current_index - 1){
				printf(", ");
			}
		}

		printf("} Neighbors: {");

		//Now we'll print out all of it's neighbors
		for(u_int16_t k = 0; k < current->neighbors.current_index; k++){
			live_range_t* neighbor = dynamic_array_get_at(&(current->neighbors), k);
			printf("LR%d", neighbor->live_range_id);
 
			//Print a comma if appropriate
			if(k != current->neighbors.current_index - 1){
				printf(", ");
			}

		}
		
		//And we'll close it out
		printf("}\tSpill Cost: %d\tDegree: %d\n", current->spill_cost, current->degree);
	}

	printf("==================== SSE ===================\n");
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
	if(dynamic_array_contains(&(block->assigned_variables), live_range) == NOT_FOUND){
		dynamic_array_add(&(block->assigned_variables), live_range);
	}

	//Up the assignment count by adding the estimated execution frequency of the block
	//For example, if the block is in a loop and the loop runs 10 times, this line of
	//code will end up being executed 10 times instead of 1
	live_range->assignment_count += block->estimated_execution_frequency;
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
	if(dynamic_array_contains(&(block->used_variables), live_range) == NOT_FOUND){
		dynamic_array_add(&(block->used_variables), live_range);
	}

	//Up the use count by adding the estimated execution frequency of the block
	//For example, if the block is in a loop and the loop runs 10 times, this line of
	//code will end up being executed 10 times instead of 1
	live_range->use_count += block->estimated_execution_frequency;
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
	if(dynamic_array_contains(&(live_range->variables), variable) != NOT_FOUND){
		return;
	}

	//Most of the time this will just be 0, but when it isn't we'll have it here
	if(variable->linked_var != NULL){
		live_range->class_relative_function_parameter_order = variable->linked_var->class_relative_function_parameter_order;
	}

	//Link this live range to the variable
	variable->associated_live_range = live_range;

	//Otherwise we'll add this in here
	dynamic_array_add(&(live_range->variables), variable);
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
			live_range = live_range_alloc(block->function_defined_in, get_live_range_class_for_variable(variable));

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
static live_range_t* construct_stack_pointer_live_range(three_addr_var_t* stack_pointer){
	//Before we go any further, we'll construct the live
	//range for the stack pointer. Special case here - stack pointer has no block
	live_range_t* stack_pointer_live_range = live_range_alloc(NULL, LIVE_RANGE_CLASS_GEN_PURPOSE);
	//This is guaranteed to be RSP - so it's already been allocated
	stack_pointer_live_range->reg.gen_purpose = RSP;
	//And we absolutely *can not* spill it
	stack_pointer_live_range->spill_cost = UINT32_MAX;

	//This is precolor
	stack_pointer_live_range->is_precolored = TRUE;

	//Add the stack pointer to the dynamic array
	dynamic_array_add(&(stack_pointer_live_range->variables), stack_pointer);
	
	//Store this here as well
	stack_pointer->associated_live_range = stack_pointer_live_range;

	//Store it in the global var for convenience
	stack_pointer_lr = stack_pointer_live_range;

	//Give it back
	return stack_pointer_live_range;
}


/**
 * Create the instruction pointer live range
 */
static live_range_t* construct_instruction_pointer_live_range(three_addr_var_t* instruction_pointer){
	//Before we go any further, we'll construct the live
	//range for the instruction pointer.
	live_range_t* instruction_pointer_live_range = live_range_alloc(NULL, LIVE_RANGE_CLASS_GEN_PURPOSE);
	//This is guaranteed to be RSP - so it's already been allocated
	instruction_pointer_live_range->reg.gen_purpose = RIP;
	//And we absolutely *can not* spill it
	instruction_pointer_live_range->spill_cost = UINT32_MAX;

	//This is precolor
	instruction_pointer_live_range->is_precolored = TRUE;

	//Add the stack pointer to the dynamic array
	dynamic_array_add(&(instruction_pointer_live_range->variables), instruction_pointer);

	//Save this to the global variable
	instruction_pointer_lr = instruction_pointer_live_range;
	
	//Store this here as well
	instruction_pointer->associated_live_range = instruction_pointer_live_range;

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
	} else if(is_move_instruction_destination_assigned(instruction) == FALSE){
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
static inline void assign_live_range_to_destination_variable(dynamic_array_t* general_purpose_live_ranges, dynamic_array_t* sse_live_ranges, basic_block_t* block, instruction_t* instruction){
	//Bail out if this happens
	if(instruction->destination_register == NULL){
		return;
	}

	//Extract for convenience
	three_addr_var_t* destination_register = instruction->destination_register;

	//Get what class we need here
	live_range_class_t target_class = get_live_range_class_for_variable(destination_register);

	//Holder for the live range
	live_range_t* live_range;

	//Use the appropiate dyn array based on the class
	switch(target_class){
		case LIVE_RANGE_CLASS_GEN_PURPOSE:
			live_range = find_or_create_live_range(general_purpose_live_ranges, block, destination_register);
			break;

		case LIVE_RANGE_CLASS_SSE:
			live_range = find_or_create_live_range(sse_live_ranges, block, destination_register);
			break;
	}

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
	
	//Get what class we need here
	target_class = get_live_range_class_for_variable(destination_register2);

	//Use the appropiate dyn array based on the class
	switch(target_class){
		case LIVE_RANGE_CLASS_GEN_PURPOSE:
			live_range = find_or_create_live_range(general_purpose_live_ranges, block, destination_register2);
			break;

		case LIVE_RANGE_CLASS_SSE:
			live_range = find_or_create_live_range(sse_live_ranges, block, destination_register2);
			break;
	}

	//Add this into the live range
	add_variable_to_live_range(live_range, destination_register2);

	//This will *always* be a purely assigned live range
	add_assigned_live_range(live_range, block);
}


/**
 * Handle the live range that comes from the source of an instruction
 */
static inline void assign_live_range_to_source_variable(dynamic_array_t* general_purpose_live_ranges, dynamic_array_t* sse_live_ranges, basic_block_t* block, three_addr_var_t* source_variable){
	//Just leave if it's NULL
	if(source_variable == NULL){
		return;
	}

	//What live range class are we after
	live_range_class_t target_class = get_live_range_class_for_variable(source_variable);

	//Holder for the live range
	live_range_t* live_range;

	switch(target_class){
		case LIVE_RANGE_CLASS_GEN_PURPOSE:
			live_range = assign_live_range_to_variable(general_purpose_live_ranges, block, source_variable);
			break;

		case LIVE_RANGE_CLASS_SSE:
			live_range = assign_live_range_to_variable(sse_live_ranges, block, source_variable);
			break;
	}

	//Add this as a used live range
	add_used_live_range(live_range, block);
}


/**
 * Handle the live range that comes from the source of an instruction
 */
static inline void assign_live_range_to_implicit_source_variable(dynamic_array_t* general_purpose_live_ranges, dynamic_array_t* sse_live_ranges, basic_block_t* block, three_addr_var_t* source_variable){
	//Just leave if it's NULL
	if(source_variable == NULL){
		return;
	}

	//What live range class are we after
	live_range_class_t target_class = get_live_range_class_for_variable(source_variable);

	//Holder for the live range
	live_range_t* live_range;

	switch(target_class){
		case LIVE_RANGE_CLASS_GEN_PURPOSE:
			live_range = assign_live_range_to_variable(general_purpose_live_ranges, block, source_variable);
			break;

		case LIVE_RANGE_CLASS_SSE:
			live_range = assign_live_range_to_variable(sse_live_ranges, block, source_variable);
			break;
	}

	//Bump the use count by using the blocks estimated execution frequency
	live_range->use_count += block->estimated_execution_frequency;
}


/**
 * Construct the live ranges appropriate for a phi function
 *
 * Note that the phi function does not count as an actual assignment, we'll just want
 * to ensure that the live range is ready for us when we need it
 */
static inline void construct_phi_function_live_range(dynamic_array_t* general_purpose_live_ranges, dynamic_array_t* sse_live_ranges, basic_block_t* basic_block, instruction_t* instruction){
	//Get the class here first
	live_range_class_t class = get_live_range_class_for_variable(instruction->assignee);
	
	//Holder for the live range
	live_range_t* live_range;

	//Use the appropriate array based on the type
	switch(class){
		case LIVE_RANGE_CLASS_GEN_PURPOSE:
			live_range = find_or_create_live_range(general_purpose_live_ranges, basic_block, instruction->assignee);
			break;

		case LIVE_RANGE_CLASS_SSE:
			live_range = find_or_create_live_range(sse_live_ranges, basic_block, instruction->assignee);
			break;
	}

	//Add this into the live range
	add_variable_to_live_range(live_range, instruction->assignee);
}


/**
 * Construct the live ranges for the specialized PXOR_CLEAR instruction
 */
static inline void construct_pxor_clear_live_range(dynamic_array_t* general_purpose_live_ranges, dynamic_array_t* sse_live_ranges, basic_block_t* basic_block, instruction_t* instruction){
	//Handle the destination variable
	assign_live_range_to_destination_variable(general_purpose_live_ranges, sse_live_ranges, basic_block, instruction);

	//Extract this LR
	live_range_t* live_range = instruction->destination_register->associated_live_range;

	//Counts as a use as well, the prior function call already handled the assignment
	add_used_live_range(live_range, basic_block);
}


/**
 * An increment/decrement/neg live range is a special case because the invisible "source" needs to be part of the
 * same live range as the destination. We ensure that that happens within this rule
 */
static inline void construct_inc_dec_neg_live_range(dynamic_array_t* general_purpose_live_ranges, dynamic_array_t* sse_live_ranges, basic_block_t* basic_block, instruction_t* instruction){
	//If this is not temporary, we can handle it like any other statement
	if(instruction->destination_register->variable_type != VARIABLE_TYPE_TEMP){
		//Handle the destination variable
		assign_live_range_to_destination_variable(general_purpose_live_ranges, sse_live_ranges, basic_block, instruction);

		//Assign all of the source variable live ranges
		assign_live_range_to_source_variable(general_purpose_live_ranges, sse_live_ranges, basic_block, instruction->source_register);

	//Otherwise, we'll need to take a more specialized approach
	} else {
		//Let's see if we can find this. We only need to consider general purpose here for inc/dec
		live_range_t* live_range = find_or_create_live_range(general_purpose_live_ranges, basic_block, instruction->destination_register);

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
static inline void construct_function_call_live_ranges(dynamic_array_t* general_purpose_live_ranges, dynamic_array_t* sse_live_ranges, basic_block_t* basic_block, instruction_t* instruction){
	//First let's handle the destination register. It is possible that this could 
	//be null
	if(instruction->destination_register != NULL){
		assign_live_range_to_destination_variable(general_purpose_live_ranges, sse_live_ranges, basic_block, instruction);
	}

	/**
	 * NOTE: For indirect function calls, the variable itself is actually stored in the source register.
	 * We'll make this call to account for such a case here
	 */
	assign_live_range_to_source_variable(general_purpose_live_ranges, sse_live_ranges, basic_block, instruction->source_register);

	//Extract for us
	dynamic_array_t function_parameters = instruction->parameters;
				
	//Otherwise we'll run through them all
	for(u_int16_t i = 0; i < function_parameters.current_index; i++){
		//Extract it
		three_addr_var_t* parameter = dynamic_array_get_at(&function_parameters, i);

		/**
		 * Because we are not explicitly reading in this instruction, we need a special rule that takes care to not
		 * add these as used variables. We will instead just assign the appropriate live ranges
		 */
		assign_live_range_to_implicit_source_variable(general_purpose_live_ranges, sse_live_ranges, basic_block, parameter);
	}
}


/**
 * Run through every instruction in a block and construct the live ranges
 *
 * We invoke special processing functions to handle exception cases like phi functions, inc/dec instructions,
 * and function calls
 */
static void construct_live_ranges_in_block(basic_block_t* basic_block, dynamic_array_t* general_purpose_live_ranges, dynamic_array_t* sse_live_ranges){
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
				construct_phi_function_live_range(general_purpose_live_ranges, sse_live_ranges, basic_block, current);
			
				//And we're done - no need to go further
				current = current->next_statement;
				continue;

			case RET:
				//Let the helper deal with it
				assign_live_range_to_implicit_source_variable(general_purpose_live_ranges, sse_live_ranges, basic_block, current->source_register);
				
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
			case NEGB:
			case NEGW:
			case NEGL:
			case NEGQ:
				//These will always be general purpose
				construct_inc_dec_neg_live_range(general_purpose_live_ranges, sse_live_ranges, basic_block, current);
			
				//And we're done - no need to go further
				current = current->next_statement;
				continue;

			//Specialized instruction, let the helper do it
			case PXOR_CLEAR:
				construct_pxor_clear_live_range(general_purpose_live_ranges, sse_live_ranges, basic_block, current);
				
				//Bump it up and move along
				current = current->next_statement;
				continue;

			//Call and indirect call have hidden parameters that need to be accounted for
			case CALL:
			case INDIRECT_CALL:
				//Let the helper rule handle it
				construct_function_call_live_ranges(general_purpose_live_ranges, sse_live_ranges, basic_block, current);

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
		assign_live_range_to_destination_variable(general_purpose_live_ranges, sse_live_ranges, basic_block, current);

		//Assign all of the source variable live ranges
		assign_live_range_to_source_variable(general_purpose_live_ranges, sse_live_ranges, basic_block, current->source_register);
		assign_live_range_to_source_variable(general_purpose_live_ranges, sse_live_ranges, basic_block, current->source_register2);
		assign_live_range_to_source_variable(general_purpose_live_ranges, sse_live_ranges, basic_block, current->address_calc_reg1);
		assign_live_range_to_source_variable(general_purpose_live_ranges, sse_live_ranges, basic_block, current->address_calc_reg2);

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
 *
 * Note: we have 2 distinct sets of live ranges - SSE and non-SSE. These sets are entirely separate so we will build
 * them in tandem, but manipulate them separately to boost efficiency
 */
static inline void construct_live_ranges_in_function(basic_block_t* function_entry, dynamic_array_t* general_purpose_ranges, dynamic_array_t* sse_ranges){
	//Add these both in immediately
	dynamic_array_add(general_purpose_ranges, stack_pointer_lr);
	dynamic_array_add(general_purpose_ranges, instruction_pointer_lr);

	//Grab the entry block
	basic_block_t* current = function_entry;

	//Run through every single block
	while(current != NULL){
		//Let the helper do this
		construct_live_ranges_in_block(current, general_purpose_ranges, sse_ranges);

		//Advance to the next
		current = current->direct_successor;
	}
}


/**
 * Reset the visited status and the liveness arrays for each block
 */
static inline void reset_function_blocks_for_liveness(basic_block_t* function_entry_block){
	//This is our initial current
	basic_block_t* current = function_entry_block;

	//So long as we aren't null
	while(current != NULL){
		//Reset this
		current->visited = FALSE;

		//Also reset the liveness sets
		clear_dynamic_array(&(current->live_in));
		clear_dynamic_array(&(current->live_out));

		//Push it up
		current = current->direct_successor;
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
static void calculate_live_range_liveness_sets(basic_block_t* function_entry_block){
	//Reset the visited status and liveness sets
	reset_function_blocks_for_liveness(function_entry_block);

	//Did we find a difference
	u_int8_t difference_found;

	//The "Prime" blocks are just ways to hold the old dynamic arrays
	dynamic_array_t in_prime;
	dynamic_array_t out_prime;

	//A cursor for the current block
	basic_block_t* current;

	//Wipe these two out
	function_entry_block->function_defined_in->assigned_general_purpose_registers = 0;
	function_entry_block->function_defined_in->assigned_sse_registers = 0;

	//We keep calculating this until we end up with no change in the old and new LIVE_IN/LIVE_OUT sets
	do{
		//Assume that we have not found a difference by default
		difference_found = FALSE;

		//Now we can go through the entire RPO set
		for(u_int16_t _ = 0; _ < function_entry_block->reverse_post_order_reverse_cfg.current_index; _++){
			//The current block is whichever we grab
			current = dynamic_array_get_at(&(function_entry_block->reverse_post_order_reverse_cfg), _);

			//Transfer the pointers over
			in_prime = current->live_in;
			out_prime = current->live_out;

			//Set live out to be a new array
			current->live_out = dynamic_array_alloc();

			//Run through all of the successors
			for(u_int16_t k = 0; k < current->successors.current_index; k++){
				//Grab the successor out
				basic_block_t* successor = dynamic_array_get_at(&(current->successors), k);

				//Add everything in his live_in set into the live_out set
				for(u_int16_t l = 0; l < successor->live_in.current_index; l++){
					//Let's check to make sure we haven't already added this
					live_range_t* successor_live_in_var = dynamic_array_get_at(&(successor->live_in), l);

					//If it doesn't already contain this variable, we'll add it in
					if(dynamic_array_contains(&(current->live_out), successor_live_in_var) == NOT_FOUND){
						dynamic_array_add(&(current->live_out), successor_live_in_var);
					}
				}
			}

			//Since we need all of the used variables, we'll just clone this
			//dynamic array so that we start off with them all
			current->live_in = clone_dynamic_array(&(current->used_variables));

			//Now we need to add every variable that is in LIVE_OUT but NOT in assigned
			for(u_int16_t j = 0; j  < current->live_out.current_index; j++){
				//Grab a reference for our use
				live_range_t* live_out_var = dynamic_array_get_at(&(current->live_out), j);

				//Now we need this block to be not in "assigned" also. If it is in assigned we can't
				//add it. Additionally, we'll want to make sure we aren't adding duplicate live ranges
				if(dynamic_array_contains(&(current->assigned_variables), live_out_var) == NOT_FOUND 
					&& dynamic_array_contains(&(current->live_in), live_out_var) == NOT_FOUND){
					//If this is true we can add
					dynamic_array_add(&(current->live_in), live_out_var);
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
				if(dynamic_arrays_equal(&in_prime, &(current->live_in)) == FALSE
				  || dynamic_arrays_equal(&out_prime, &(current->live_out)) == FALSE){
					//We have in fact found a difference
					difference_found = TRUE;
				}
			}

			//We made it down here, the prime variables are useless. We'll deallocate them
			dynamic_array_dealloc(&in_prime);
			dynamic_array_dealloc(&out_prime);
		}

	//So long as there is a difference
	} while(difference_found == TRUE);
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
		clear_dynamic_array(&(current->neighbors));
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
static dynamic_array_t calculate_live_after_for_block(basic_block_t* block, instruction_t* instruction){
	/**
	 * As you can see in the algorithm, the LIVE_NOW set initially starts
	 * out as LIVE_OUT. For this reason, we will just use the LIVE_OUT
	 * set by a different name for our calculation
	 */
	dynamic_array_t live_after = clone_dynamic_array(&(block->live_out));

	//For later use
	dynamic_array_t operation_function_parameters;
	
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
				add_live_now_live_range(operation->destination_register->associated_live_range, &live_after);

			/**
			 * If the indirection level is more than 0, this means that we're moving into a memory
			 * region. Since this is the case, we're not really assigning to the register here. In
			 * fact, we're using it, so we'll need to add this to LIVE_NOW
			 */
			} else if(is_move_instruction_destination_assigned(operation) == FALSE){
				//Add it to live now and we're done
				add_live_now_live_range(operation->destination_register->associated_live_range, &live_after);

			/**
			 * The final case here is the ideal case in the algorithm, where we have a simple
			 * assignment at the end. To satisfy the algorithm, we'll add all of the interference
			 * between the destination and LIVE_NOW and then delete the destination from live_now
			 */
			} else {
				//And then scrap it from live_now
				dynamic_array_delete(&live_after, operation->destination_register->associated_live_range);
			}
		}

		/**
		 * Some instructions like CXXX and division instructions have 2 destinations. The second destination,
		 * unlike the first, will never have any dual purpose, so we can just add the interference and delete
		 */
		if(operation->destination_register2 != NULL){
			//And then scrap it from live_now
			dynamic_array_delete(&live_after, operation->destination_register2->associated_live_range);
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
			add_live_now_live_range(operation->source_register->associated_live_range, &live_after);
		}

		if(operation->source_register2 != NULL){
			add_live_now_live_range(operation->source_register2->associated_live_range, &live_after);
		}

		if(operation->address_calc_reg1 != NULL){
			add_live_now_live_range(operation->address_calc_reg1->associated_live_range, &live_after);
		}

		if(operation->address_calc_reg2 != NULL){
			add_live_now_live_range(operation->address_calc_reg2->associated_live_range, &live_after);
		}

		/**
		 * SPECIAL CASES:
		 *
		 * Function calls(direct/indirect) have function parameters that are being used
		 */
		switch(operation->instruction_type){
			case CALL:
			case INDIRECT_CALL:
				//Grab it out
				operation_function_parameters = operation->parameters;

				//Let's go through all of these and add them to LIVE_NOW
				for(u_int16_t i = 0; i < operation_function_parameters.current_index; i++){
					//Extract the variable
					three_addr_var_t* variable = dynamic_array_get_at(&operation_function_parameters, i);

					//Add it to live_now
					add_live_now_live_range(variable->associated_live_range, &live_after);
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
 * Scan the given source array and return a result array with only live ranges of the target
 * class inside of the returned array
 */
static inline dynamic_array_t get_live_ranges_from_given_class(dynamic_array_t* source_array, live_range_class_t target_class){
	//Create the array
	dynamic_array_t result = dynamic_array_alloc();

	//Loop through the old array and copy any pointers over
	//that match the target class
	for(u_int16_t i = 0; i < source_array->current_index; i++){
		//Extract our candidate
		live_range_t* candidate = dynamic_array_get_at(source_array, i);

		//Add it into the result array *if* the classes match
		if(candidate->live_range_class == target_class){
			dynamic_array_add(&result, candidate);
		}
	}

	//Give back the distinct result array now
	return result;
}


/**
 * A helper function for interference construction. This adds interference between every
 * value in LIVE_NOW and the given target
 */
static inline void add_interefence_between_target_and_live_now(dynamic_array_t* LIVE_NOW, live_range_t* target){
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
		add_interference(target, range);
	}
}


/**
 * The general purpose interference calculation function works on both SSE and non-SSE targets
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
 * For the distinction between SSE and non-SSE variables, we maintain 2 separate live now sets. This allows
 * us to use one traversal while still maintaining proper separation
 */
static void calculate_all_interference_in_block(basic_block_t* block){
	/**
	 * As you can see in the algorithm, the LIVE_NOW set initially starts
	 * out as LIVE_OUT. For this reason, we will just use the LIVE_OUT
	 * set by a different name for our calculation. We do need to maintain
	 * distinction between float and non float live ranges though, so we
	 * will maintain 2 separate live now buckets
	 */
	dynamic_array_t live_now_general_purpose = get_live_ranges_from_given_class(&(block->live_out), LIVE_RANGE_CLASS_GEN_PURPOSE);
	dynamic_array_t live_now_sse = get_live_ranges_from_given_class(&(block->live_out), LIVE_RANGE_CLASS_SSE);

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
				//Add the interference in the appropriate graph 
				if(operation->destination_register->associated_live_range->live_range_class == LIVE_RANGE_CLASS_GEN_PURPOSE){
					add_interefence_between_target_and_live_now(&live_now_general_purpose, operation->destination_register->associated_live_range);
					add_live_now_live_range(operation->destination_register->associated_live_range, &live_now_general_purpose);

				//If we hit this we're using a float LR 
				} else {
					add_interefence_between_target_and_live_now(&live_now_sse, operation->destination_register->associated_live_range);
					add_live_now_live_range(operation->destination_register->associated_live_range, &live_now_sse);
				}

			/**
			 * If we hit this, this means that the destination register itself is never being assigned. In this
			 * case, we'll just need to add the destination LR to live now
			 */
			} else if(is_move_instruction_destination_assigned(operation) == FALSE){
				if(operation->destination_register->associated_live_range->live_range_class == LIVE_RANGE_CLASS_GEN_PURPOSE){
					add_live_now_live_range(operation->destination_register->associated_live_range, &live_now_general_purpose);
				} else {
					add_live_now_live_range(operation->destination_register->associated_live_range, &live_now_sse);
				}

			/**
			 * The final case here is the ideal case in the algorithm, where we have a simple
			 * assignment at the end. To satisfy the algorithm, we'll add all of the interference
			 * between the destination and LIVE_NOW and then delete the destination from live_now
			 */
			} else {
				if(operation->destination_register->associated_live_range->live_range_class == LIVE_RANGE_CLASS_GEN_PURPOSE){
					//Add the interference
					add_interefence_between_target_and_live_now(&live_now_general_purpose, operation->destination_register->associated_live_range);

					//And then scrap it from live_now
					dynamic_array_delete(&live_now_general_purpose, operation->destination_register->associated_live_range);

				} else {
					//Add the interference
					add_interefence_between_target_and_live_now(&live_now_sse, operation->destination_register->associated_live_range);

					//And then scrap it from live_now
					dynamic_array_delete(&live_now_sse, operation->destination_register->associated_live_range);
				}
			}
		}

		/**
		 * Some instructions like CXXX and division instructions have 2 destinations. The second destination,
		 * unlike the first, will never have any dual purpose, so we can just add the interference and delete
		 */
		if(operation->destination_register2 != NULL){
			if(operation->destination_register2->associated_live_range->live_range_class == LIVE_RANGE_CLASS_GEN_PURPOSE){
				//Add the interference
				add_interefence_between_target_and_live_now(&live_now_general_purpose, operation->destination_register2->associated_live_range);

				//And then scrap it from live_now
				dynamic_array_delete(&live_now_general_purpose, operation->destination_register2->associated_live_range);

			} else {
				//Add the interference
				add_interefence_between_target_and_live_now(&live_now_sse, operation->destination_register2->associated_live_range);

				//And then scrap it from live_now
				dynamic_array_delete(&live_now_sse, operation->destination_register2->associated_live_range);
			}
		}

		/**
		 * STEP:
		 *  Add LA an LB to LIVENOW
		 *
		 *  Remember - in this version of the algorithm we are segregating the general purpose and
		 *  SSE because either is fair game
		 */
		if(operation->source_register != NULL){
			if(operation->source_register->associated_live_range->live_range_class == LIVE_RANGE_CLASS_GEN_PURPOSE){
				add_live_now_live_range(operation->source_register->associated_live_range, &live_now_general_purpose);
			} else {
				add_live_now_live_range(operation->source_register->associated_live_range, &live_now_sse);
			}
		}

		if(operation->source_register2 != NULL){
			if(operation->source_register2->associated_live_range->live_range_class == LIVE_RANGE_CLASS_GEN_PURPOSE){
				add_live_now_live_range(operation->source_register2->associated_live_range, &live_now_general_purpose);
			} else {
				add_live_now_live_range(operation->source_register2->associated_live_range, &live_now_sse);
			}
		}

		if(operation->address_calc_reg1 != NULL){
			if(operation->address_calc_reg1->associated_live_range->live_range_class == LIVE_RANGE_CLASS_GEN_PURPOSE){
				add_live_now_live_range(operation->address_calc_reg1->associated_live_range, &live_now_general_purpose);
			} else {
				add_live_now_live_range(operation->address_calc_reg1->associated_live_range, &live_now_sse);
			}

		}

		if(operation->address_calc_reg2 != NULL){
			if(operation->address_calc_reg2->associated_live_range->live_range_class == LIVE_RANGE_CLASS_GEN_PURPOSE){
				add_live_now_live_range(operation->address_calc_reg2->associated_live_range, &live_now_general_purpose);
			} else {
				add_live_now_live_range(operation->address_calc_reg2->associated_live_range, &live_now_sse);
			}
		}

		/**
		 * SPECIAL CASES:
		 *
		 * Function calls(direct/indirect) have function parameters that are being used
		 */
		switch(operation->instruction_type){
			case CALL:
			case INDIRECT_CALL:
				//Let's go through all of these and add them to LIVE_NOW
				for(u_int16_t i = 0; i < operation->parameters.current_index; i++){
					//Extract the variable
					three_addr_var_t* variable = dynamic_array_get_at(&(operation->parameters), i);

					//Add it to live_now for the appropriate set
					if(variable->associated_live_range->live_range_class == LIVE_RANGE_CLASS_GEN_PURPOSE){
						add_live_now_live_range(variable->associated_live_range, &live_now_general_purpose);
					} else {
						add_live_now_live_range(variable->associated_live_range, &live_now_sse);
					}
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
 * A simple wrapper function that will calculate interferences inside
 * of a given function. This function simply iterates over all blocks
 * and invokes the helper. It *does not* construct the interference
 * graphs. This is the responsibility of the caller
 */
static inline void calculate_all_interferences_in_function(basic_block_t* function_entry_block){
	//We'll first need a pointer
	basic_block_t* current = function_entry_block;

	//Run through every block in the CFG's ordered set
	while(current != NULL){
		//Use the helper. Set stopper to be NULL because we aren't trying to halt
		//anything here
		calculate_all_interference_in_block(current);
		
		//Advance this up
		current = current->direct_successor;
	}
}


/**
 * Calculate interferences *exclusively* for a given class of live range and only that class
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
 * For the distinction between SSE and non-SSE variables, we maintain 2 separate live now sets. This allows
 * us to use one traversal while still maintaining proper separation
 */
static void calculate_target_interference_in_block(basic_block_t* block, live_range_class_t target_class){
	//Get the specific live_now bucket based on the target class
	dynamic_array_t target_live_now = get_live_ranges_from_given_class(&(block->live_out), target_class);

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
				//Add the interference in the appropriate graph 
				if(operation->destination_register->associated_live_range->live_range_class == target_class){
					add_interefence_between_target_and_live_now(&target_live_now, operation->destination_register->associated_live_range);
					add_live_now_live_range(operation->destination_register->associated_live_range, &target_live_now);
				}

			/**
			 * If we hit this, this means that the destination register itself is never being assigned. In this
			 * case, we'll just need to add the destination LR to live now
			 */
			} else if(is_move_instruction_destination_assigned(operation) == FALSE){
				if(operation->destination_register->associated_live_range->live_range_class == target_class){
					add_live_now_live_range(operation->destination_register->associated_live_range, &target_live_now);
				}

			/**
			 * The final case here is the ideal case in the algorithm, where we have a simple
			 * assignment at the end. To satisfy the algorithm, we'll add all of the interference
			 * between the destination and LIVE_NOW and then delete the destination from live_now
			 */
			} else {
				if(operation->destination_register->associated_live_range->live_range_class == target_class){
					//Add the interference
					add_interefence_between_target_and_live_now(&target_live_now, operation->destination_register->associated_live_range);

					//And then scrap it from live_now
					dynamic_array_delete(&target_live_now, operation->destination_register->associated_live_range);
				}
			}
		}

		/**
		 * Some instructions like CXXX and division instructions have 2 destinations. The second destination,
		 * unlike the first, will never have any dual purpose, so we can just add the interference and delete
		 */
		if(operation->destination_register2 != NULL){
			if(operation->destination_register2->associated_live_range->live_range_class == target_class){
				//Add the interference
				add_interefence_between_target_and_live_now(&target_live_now, operation->destination_register2->associated_live_range);

				//And then scrap it from live_now
				dynamic_array_delete(&target_live_now, operation->destination_register2->associated_live_range);
			}
		}

		/**
		 * STEP:
		 *  Add LA an LB to LIVENOW
		 *
		 *  Remember - in this version of the algorithm we are segregating the general purpose and
		 *  SSE because either is fair game
		 */
		if(operation->source_register != NULL
			&& operation->source_register->associated_live_range->live_range_class == target_class){

			add_live_now_live_range(operation->source_register->associated_live_range, &target_live_now);
		}

		if(operation->source_register2 != NULL
			&& operation->source_register2->associated_live_range->live_range_class == target_class){

			add_live_now_live_range(operation->source_register2->associated_live_range, &target_live_now);
		}

		if(operation->address_calc_reg1 != NULL
			&& operation->address_calc_reg1->associated_live_range->live_range_class == target_class){

			add_live_now_live_range(operation->address_calc_reg1->associated_live_range, &target_live_now);
		}

		if(operation->address_calc_reg2 != NULL
			&& operation->address_calc_reg2->associated_live_range->live_range_class == target_class){

			add_live_now_live_range(operation->address_calc_reg2->associated_live_range, &target_live_now);
		}

		/**
		 * SPECIAL CASES:
		 *
		 * Function calls(direct/indirect) have function parameters that are being used
		 */
		switch(operation->instruction_type){
			case CALL:
			case INDIRECT_CALL:
				//Let's go through all of these and add them to LIVE_NOW
				for(u_int16_t i = 0; i < operation->parameters.current_index; i++){
					//Extract the variable
					three_addr_var_t* variable = dynamic_array_get_at(&(operation->parameters), i);

					//Add it to live_now for the appropriate set
					if(variable->associated_live_range->live_range_class == target_class){
						add_live_now_live_range(variable->associated_live_range, &target_live_now);
					}
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
 * Calculate interferences only for the target class of live ranges inside
 * of a given function
 */
static inline void calculate_target_interferences_in_function(basic_block_t* function_entry_block, live_range_class_t target_class){
	//We'll first need a pointer
	basic_block_t* current = function_entry_block;

	//Run through every block in the CFG's ordered set
	while(current != NULL){
		//Use the helper. Set stopper to be NULL because we aren't trying to halt
		//anything here
		calculate_target_interference_in_block(current, target_class);
		
		//Advance this up
		current = current->direct_successor;
	}
}


/**
 * Before we do any other precoloring, we should be crawling the function body to determine what the function parameter
 * live ranges are and to precolor them as need be. This can be done beforehand because the function parameter order
 * is always maintained in the variable itself
 *
 * NOTE: Currently, we do not have the ability to handle more than 6 of each parameter
 */
static inline void precolor_in_body_function_parameters(dynamic_array_t* general_purpose_live_ranges, dynamic_array_t* sse_live_ranges){
	//First we'll run through the general purpose ones
	for(u_int16_t i = 0; i < general_purpose_live_ranges->current_index; i++){
		//Extract it
		live_range_t* general_purpose_lr = dynamic_array_get_at(general_purpose_live_ranges, i);

		//Extract for neatness
		u_int16_t general_purpose_parameter_order = general_purpose_lr->class_relative_function_parameter_order;

		//If it has a function parameter order, we'll color it appropriately
		if(general_purpose_parameter_order > 0){
			general_purpose_lr->reg.gen_purpose = gen_purpose_parameter_registers[general_purpose_parameter_order - 1];
		}
	}

	//Now do the exact same thing for SSE
	for(u_int16_t i = 0; i < sse_live_ranges->current_index; i++){
		//Extract it
		live_range_t* sse_lr = dynamic_array_get_at(sse_live_ranges, i);

		//Extract for neatness
		u_int16_t sse_parameter_order = sse_lr->class_relative_function_parameter_order;

		//If it has a function parameter order, we'll color it appropriately
		if(sse_parameter_order > 0){
			sse_lr->reg.sse_reg = sse_parameter_registers[sse_parameter_order - 1];
		}
	}
}


/**
 * Some variables need to be in special registers at a given time. We can
 * bind them to the right register at this stage and avoid having to worry about it later
 */
static void precolor_instruction(instruction_t* instruction){
	//Pre-color based on what kind of instruction it is
	switch(instruction->instruction_type){
		/**
		 * Ret could be a floating point or non-floating point value, so
		 * we need to determine which kind it is before coloring
		 */
		case RET:
			//If it has one, assign it
			if(instruction->source_register != NULL){
				if(instruction->source_register->associated_live_range->live_range_class == LIVE_RANGE_CLASS_GEN_PURPOSE){
					instruction->source_register->associated_live_range->reg.gen_purpose = RAX;
				} else {
					instruction->source_register->associated_live_range->reg.sse_reg = XMM0;
				}
			}

			break;

		/**
		 * These are exclusively general purpose so we do not need
		 * to handle any kind of floating point coloring here
		 */
		case MULB:
		case MULW:
		case MULL:
		case MULQ:
			//When we do an unsigned multiplication, the implicit source register must be in RAX
			instruction->source_register2->associated_live_range->reg.gen_purpose = RAX;

			//The destination must also be in RAX here
			instruction->destination_register->associated_live_range->reg.gen_purpose = RAX;

			break;

		/**
		 * For all shift instructions, if they have a register source, that
		 * source is required to be in the %cl register
		 *
		 * These are exclusively general purpose so we do not need
		 * to handle any kind of floating point coloring here
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
				instruction->source_register->associated_live_range->reg.gen_purpose = RCX;
			}
		
			break;

		/**
		 * These converter instructions only operate on general purpose registers
		 * so we don't need to do any checking for floating point
		 */
		case CQTO:
		case CLTD:
		case CWTL:
		case CBTW:
			//Source is always %RAX
			instruction->source_register->associated_live_range->reg.gen_purpose = RAX;

			//The results are always RDX and RAX 
			//Lower order bits
			instruction->destination_register->associated_live_range->reg.gen_purpose = RAX;

			//Higher order bits
			instruction->destination_register2->associated_live_range->reg.gen_purpose = RDX;

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
			instruction->source_register2->associated_live_range->reg.gen_purpose = RAX;

			//The first destination register is the quotient, and is in RAX
			instruction->destination_register->associated_live_range->reg.gen_purpose = RAX;

			//The second destination register is the remainder, and is in RDX
			instruction->destination_register2->associated_live_range->reg.gen_purpose = RDX;

			break;

		//Function calls always return through rax
		case CALL:
		case INDIRECT_CALL:
			//We could have a void return, but usually we'll give something
			if(instruction->destination_register != NULL){
				//Go based on the type
				if(instruction->destination_register->associated_live_range->live_range_class == LIVE_RANGE_CLASS_GEN_PURPOSE){
					instruction->destination_register->associated_live_range->reg.gen_purpose = RAX;
				} else {
					instruction->destination_register->associated_live_range->reg.sse_reg = XMM0;
				}
			}

			/**
			 * We also need to allocate the parameters for this function. Conviently,
			 * they are already stored for us so we don't need to do anything like
			 * what we had to do for pre-allocating function parameters
			 */

			//Grab the parameters out
			dynamic_array_t function_params = instruction->parameters;

			u_int16_t general_purpose_parameter_order = 0;
			u_int16_t sse_parameter_order = 0;

			//Run thorugh all of the params and precolor
			for(u_int16_t i = 0; i < function_params.current_index; i++){
				//Grab it out
				three_addr_var_t* param = dynamic_array_get_at(&function_params, i);

				//Now that we have it, we'll grab it's live range
				live_range_t* param_live_range = param->associated_live_range;

				//Precolor and increment the appropriate counter based on the type
				if(param_live_range->live_range_class == LIVE_RANGE_CLASS_GEN_PURPOSE){
					param_live_range->reg.gen_purpose = gen_purpose_parameter_registers[general_purpose_parameter_order];

					general_purpose_parameter_order++;

				} else {
					param_live_range->reg.sse_reg = sse_parameter_registers[sse_parameter_order];

					sse_parameter_order++;
				}
			}

			break;

		//Most of the time we will get here
		default:
			break;
	}
}


/**
 * Crawl the entire CFG and pre-color all registers. This will handle both general
 * purpose and SSE precoloring. If all is going well, we should only need to precolor
 * once per run
 */
static void precolor_function(basic_block_t* function_entry, dynamic_array_t* general_purpose_live_ranges, dynamic_array_t* sse_live_ranges){
	//Before we crawl the instructions, we'll crawl the live range arrays 
	//to precolor any function parameters that are in the function
	//body that we have
	precolor_in_body_function_parameters(general_purpose_live_ranges, sse_live_ranges);

	//Grab a cursor to the head block
	basic_block_t* cursor = function_entry;

	//Crawl the entire CFG
	while(cursor != NULL){
		//Grab a cursor to each statement
		instruction_t* instruction_cursor = cursor->leader_statement;

		//Crawl all statements in the block
		while(instruction_cursor != NULL){
			precolor_instruction(instruction_cursor);

			//Push along to the next statement
			instruction_cursor = instruction_cursor->next_statement;
		}

		//Push onto the next statement
		cursor = cursor->direct_successor;
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
	clear_dynamic_array(&(block->used_variables));
	clear_dynamic_array(&(block->assigned_variables));

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
			case PXOR_CLEAR:
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
 * Recompute the used & assigned sets for a given function. These used
 * and assigned sets also account for the frequencies of the blocks in which they exist.
 * This is a very important step to ensure that our spill cost estimates are accurate and account
 * not only for live range width but also *where* the live range is being used(think inside a of 2 or 3 level
 * loop)
 */
static inline void recompute_used_and_assigned_sets(basic_block_t* function_entry){
	//Grab a cursor block
	basic_block_t* cursor = function_entry;

	//Run through every block
	while(cursor != NULL){
		//Invoke the block-level helper
		compute_block_level_used_and_assigned_sets(cursor);

		//Push it up
		cursor = cursor->direct_successor;
	}
}


/**
 * Do any neighbors of the given live range use the given general purpose register?
 */
static inline u_int8_t do_neighbors_use_general_purpose_register(live_range_t* target, general_purpose_register_t reg){
	//Run through all neighbors
	for(u_int16_t i = 0; i < target->neighbors.current_index; i++){
		//Extract this one out
		live_range_t* neighbor = dynamic_array_get_at(&(target->neighbors), i);

		//Counts as interference
		if(neighbor->reg.gen_purpose == reg){
			return TRUE;
		}
	}

	//By the time we get here it's a no
	return FALSE;
}


/**
 * Do any neighbors of the given live range use the given sse register
 */
static inline u_int8_t do_neighbors_use_sse_register(live_range_t* target, sse_register_t reg){
	//Run through all neighbors
	for(u_int16_t i = 0; i < target->neighbors.current_index; i++){
		//Extract this one out
		live_range_t* neighbor = dynamic_array_get_at(&(target->neighbors), i);

		//Counts as interference
		if(neighbor->reg.sse_reg == reg){
			return TRUE;
		}
	}

	//By the time we get here it's a no
	return FALSE;
}


/**
 * Do we have precoloring interference for these two *general purpose* LRs? If it does, we'll
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
 *
 * Case 1: Source has no reg, and destination has no reg -> No interference 
 * Case 2: Source has no reg, and destination has reg -> No interference (take the destination register)
 * Case 3: Source has reg, and destination has no reg -> No interference (take the source register)
 * Case 4: Source has reg, and destination has reg *and* source->reg == destination->reg -> No Interference
 * Case 5: Source has reg, and destination has reg *and* source->reg != destination->reg -> *Interference*
 */
static u_int8_t does_general_purpose_register_allocation_interference_exist(live_range_t* source, live_range_t* destination){
	switch(source->reg.gen_purpose){
		case NO_REG_GEN_PURPOSE:
			//If the destination has a register, we need
			//to check if any *neighbors* of the source
			//are colored the same. If they are, that would
			//lead to interference
			if(destination->reg.gen_purpose != NO_REG_GEN_PURPOSE){
				//Whatever this is is our answer
				return do_neighbors_use_general_purpose_register(source, destination->reg.gen_purpose);
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
			if(destination->reg.gen_purpose == NO_REG_GEN_PURPOSE){
				return do_neighbors_use_general_purpose_register(destination, source->reg.gen_purpose);
			}

			//If they're the exact same, then this is also fine
			if(destination->reg.gen_purpose == source->reg.gen_purpose){
				return FALSE;
			}

			//Otherwise we have interference
			return TRUE;

		//This means the source has a register already assigned
		default:
			//Even if the destination has no register, it's neighbors 
			//could. We'll use the helper to get our answer
			if(destination->reg.gen_purpose == NO_REG_GEN_PURPOSE){
				return do_neighbors_use_general_purpose_register(destination, source->reg.gen_purpose);
			}

			//If they're the exact same, then this is also fine
			if(destination->reg.gen_purpose == source->reg.gen_purpose){
				//No interference
				return FALSE;
			}

			//Otherwise we have interference
			return TRUE;
	}
}


/**
 * Do we have precoloring interference for these two *SSE* LRs? If it does, we'll
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
 *
 * Case 1: Source has no reg, and destination has no reg -> No interference 
 * Case 2: Source has no reg, and destination has reg -> No interference (take the destination register)
 * Case 3: Source has reg, and destination has no reg -> No interference (take the source register)
 * Case 4: Source has reg, and destination has reg *and* source->reg == destination->reg -> No Interference
 * Case 5: Source has reg, and destination has reg *and* source->reg != destination->reg -> *Interference*
 */
static u_int8_t does_sse_register_allocation_interference_exist(live_range_t* source, live_range_t* destination){
	switch(source->reg.sse_reg){
		case NO_REG_SSE:
			//If the destination has a register, we need
			//to check if any *neighbors* of the source
			//are colored the same. If they are, that would
			//lead to interference
			if(destination->reg.sse_reg != NO_REG_SSE){
				//Whatever this is is our answer
				return do_neighbors_use_sse_register(source, destination->reg.sse_reg);
			}

			//No interference
			return FALSE;

		//This means the source has a register already assigned
		default:
			//Even if the destination has no register, it's neighbors 
			//could. We'll use the helper to get our answer
			if(destination->reg.sse_reg == NO_REG_SSE){
				return do_neighbors_use_sse_register(destination, source->reg.sse_reg);
			}

			//If they're the exact same, then this is also fine
			if(destination->reg.sse_reg == source->reg.sse_reg){
				//No interference
				return FALSE;
			}

			//Otherwise we have interference
			return TRUE;
	}
}


/**
 * Perform coalescence at a block level. Remember that if we do end up coalescing,
 * we need to recompute the used and assigned sets for this block as those are 
 * affected by coalescing
 *
 * This function will modify the "result" parameter to maintain internal consistency
 */
static void perform_block_level_coalescence(basic_block_t* block, interference_graph_t* general_purpose_graph, interference_graph_t* sse_graph, coalescence_result_t* result, u_int8_t debug_printing){
	//Holder for deleting
	instruction_t* holder;

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

		//These cannot possible coalesce since we have a mismatch, we will continue if that is
		//the case
		if(source_live_range->live_range_class != destination_live_range->live_range_class){
			instruction = instruction->next_statement;
			continue;
		}

		/**
		 * Now that we know what the classes match, we will go based on the source's type.
		 *
		 * To truly coalesce, we need to ensure: 
		 * 	1.) Do not interfere with one another(and as such they're in separate webs)
		 *  2.) Do not have any pre-coloring that would prevent them from being merged. For example, if the
		 *	destination register is %rdi because it's a function parameter, we can't just change the register
		 *	it's in
		 */
		switch(source_live_range->live_range_class){
			case LIVE_RANGE_CLASS_GEN_PURPOSE:
				if(do_live_ranges_interfere(general_purpose_graph, destination_live_range, source_live_range) == FALSE
					&& does_general_purpose_register_allocation_interference_exist(source_live_range, destination_live_range) == FALSE){

					//Debug logs for Dev use only
					if(debug_printing == TRUE){
						printf("Can coalesce LR%d and LR%d\n", source_live_range->live_range_id, destination_live_range->live_range_id);
						printf("DELETING LR%d\n", destination_live_range->live_range_id);
						
						printf("Deleting redundant instruction:\n");
						print_instruction(stdout, instruction, PRINTING_VAR_INLINE);
					}

					//Perform the actual coalescence. Remember that the destination is effectively being
					//absorbed into the source
					coalesce_live_ranges(general_purpose_graph, source_live_range, destination_live_range);

					//Update the result based on what we already have
					switch(*result){
						case COALESCENCE_RESULT_NONE:
							*result = COALESCENCE_RESULT_GP_ONLY;
							break;

						case COALESCENCE_RESULT_SSE_ONLY:
							*result = COALESCENCE_RESULT_BOTH;
							break;

						default:
							break;
					}

					//Delete the now useless instruction
					holder = instruction;
					instruction = instruction->next_statement;
					delete_statement(holder);

				//Otherwise we're fine - just bump the instruction up and move along
				} else {
					instruction = instruction->next_statement;
				}

				break;

			case LIVE_RANGE_CLASS_SSE:
				if(do_live_ranges_interfere(sse_graph, destination_live_range, source_live_range) == FALSE
					&& does_sse_register_allocation_interference_exist(source_live_range, destination_live_range) == FALSE){

					//Debug logs for Dev use only
					if(debug_printing == TRUE){
						printf("Can coalesce LR%d and LR%d\n", source_live_range->live_range_id, destination_live_range->live_range_id);
						printf("DELETING LR%d\n", destination_live_range->live_range_id);
						
						printf("Deleting redundant instruction:\n");
						print_instruction(stdout, instruction, PRINTING_VAR_INLINE);
					}

					//Perform the actual coalescence. Remember that the destination is effectively being
					//absorbed into the source
					coalesce_live_ranges(sse_graph, source_live_range, destination_live_range);

					//Update the result based on what we already have
					switch(*result){
						case COALESCENCE_RESULT_NONE:
							*result = COALESCENCE_RESULT_SSE_ONLY;
							break;

						case COALESCENCE_RESULT_GP_ONLY:
							*result = COALESCENCE_RESULT_BOTH;
							break;

						default:
							break;
					}

					//Delete the now useless instruction
					holder = instruction;
					instruction = instruction->next_statement;
					delete_statement(holder);

				//Otherwise we're fine - just bump the instruction up and move along
				} else {
					instruction = instruction->next_statement;
				}

				break;
		}
	}
}


/**
 * Perform live range coalescing on a given instruction. This sees
 * us merge the source and destination operands's webs(live ranges)
 *
 * We coalesce source to destination. When we're done, the *source* should
 * survive, the destination should NOT
 */
static inline coalescence_result_t perform_live_range_coalescence(basic_block_t* function_entry_block, interference_graph_t* general_purpose_graph, interference_graph_t* sse_graph, u_int8_t debug_printing){
	//By default we assume nothing happened
	coalescence_result_t result = COALESCENCE_RESULT_NONE;

	//Run through every single block in here
	basic_block_t* current = function_entry_block;

	//Run through every block
	while(current != NULL){
		//Invoke the helper for the block-level coalescing
		perform_block_level_coalescence(current, general_purpose_graph, sse_graph, &result, debug_printing);

		//Advance to the direct successor
		current = current->direct_successor;
	}

	//Give back the result that's been modified by the rule
	return result;
}


/**
 * Perform coalescence at a block level. Remember that if we do end up coalescing,
 * we need to recompute the used and assigned sets for this block as those are 
 * affected by coalescing. This rule will only look to coalesce for a specific target class
 * of registers. Anything that is not in this target class will be ignored
 *
 * This function will modify the "result" parameter to maintain internal consistency
 */
static void perform_block_level_coalescence_for_target(basic_block_t* block, interference_graph_t* target_graph, live_range_class_t target_class, coalescence_result_t* result, u_int8_t debug_printing){
	//Holder for deleting
	instruction_t* holder;

	//Now we'll run through every instruction in every block
	instruction_t* instruction = block->leader_statement;

	//Now run through all of these
	while(instruction != NULL){
		//If it's not a pure copy *or* it's marked as non-combinable, just move along
		if(is_instruction_pure_copy(instruction) == FALSE){
			instruction = instruction->next_statement;
			continue;
		}

		//Otherwise if we get here, then we know that we have a pure copy instruction
		live_range_t* source_live_range = instruction->source_register->associated_live_range;
		live_range_t* destination_live_range = instruction->destination_register->associated_live_range;

		//On top of the classes matching, we need to ensure that the live range class
		//itself matches what we're looking for. If it doesn't we can skip this and move
		//on
		if(source_live_range->live_range_class != target_class){
			instruction = instruction->next_statement;
			continue;
		}

		//These cannot possible coalesce since we have a mismatch, we will continue if that is
		//the case
		if(source_live_range->live_range_class != destination_live_range->live_range_class){
			instruction = instruction->next_statement;
			continue;
		}

		/**
		 * Now that we know what the classes match, we will go based on the source's type.
		 *
		 * To truly coalesce, we need to ensure: 
		 * 	1.) Do not interfere with one another(and as such they're in separate webs)
		 *  2.) Do not have any pre-coloring that would prevent them from being merged. For example, if the
		 *	destination register is %rdi because it's a function parameter, we can't just change the register
		 *	it's in
		 */
		if(do_live_ranges_interfere(target_graph, destination_live_range, source_live_range) == FALSE){
			//Now let's check for register allocation interference. If we do have interference, we can just
			//skip out of this entirely
			switch(target_class){
				case LIVE_RANGE_CLASS_SSE:
					if(does_sse_register_allocation_interference_exist(source_live_range, destination_live_range) == TRUE){
						instruction = instruction->next_statement;
						continue;
					}

					break;

				case LIVE_RANGE_CLASS_GEN_PURPOSE:
					if(does_general_purpose_register_allocation_interference_exist(source_live_range, destination_live_range) == TRUE){
						instruction = instruction->next_statement;
						continue;
					}

					break;
			}

			//Once we get down here, we know that we have a valid target for coalescing so we are able to go forward
			//Debug logs for Dev use only
			if(debug_printing == TRUE){
				printf("Can coalesce LR%d and LR%d\n", source_live_range->live_range_id, destination_live_range->live_range_id);
				printf("DELETING LR%d\n", destination_live_range->live_range_id);
				
				printf("Deleting redundant instruction:\n");
				print_instruction(stdout, instruction, PRINTING_VAR_INLINE);
			}

			//Invoke the helper to actually coalesce
			coalesce_live_ranges(target_graph, source_live_range, destination_live_range);
			
			//Update accordingly here based on what kind of result that we got
			if(*result == COALESCENCE_RESULT_NONE){
				switch(target_class){
					case LIVE_RANGE_CLASS_SSE:
						*result = COALESCENCE_RESULT_SSE_ONLY;
						break;

					case LIVE_RANGE_CLASS_GEN_PURPOSE:
						*result = COALESCENCE_RESULT_GP_ONLY;
						break;
				}
			}

			//Delete the now useless instruction
			holder = instruction;
			instruction = instruction->next_statement;
			delete_statement(holder);

		//Bump up to the next instruction
		} else {
			instruction = instruction->next_statement;
		}
	}
}


/**
 * Perform all of the coalescing for a given target of live ranges. This will specifically ignore all other
 * classes of live ranges in the internal logic, thus limiting the result types that it can return
 */
static inline coalescence_result_t perform_live_range_coalescence_for_target(basic_block_t* function_entry_block, interference_graph_t* target_graph, live_range_class_t target_class, u_int8_t debug_printing){
	//By default assume nothing happened
	coalescence_result_t result = COALESCENCE_RESULT_NONE;

	//Grab a cursor and run through everything
	basic_block_t* current = function_entry_block;
	while(current != NULL){
		//Invoke the helper for a specific kind of coalescing
		perform_block_level_coalescence_for_target(current, target_graph, target_class, &result, debug_printing);

		//Go to the direct successor
		current = current->direct_successor;
	}

	//Give back the result
	return result;
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
	for(u_int16_t i = 0; i < target->variables.current_index; i++){
		//Grab the variable out
		three_addr_var_t* variable = dynamic_array_get_at(&(target->variables), i);

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
		*currently_spilled = live_range_alloc(target->function, spill_range->live_range_class);

		//Flag that this was once spilled
		(*currently_spilled)->was_spilled = TRUE;

		//Be sure we copy this over too
		(*currently_spilled)->class_relative_function_parameter_order = spill_range->class_relative_function_parameter_order;

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
 * Handle a scenario where we're just handling a source spill for a pure copy instruction
 *
 * Example:
 * movl LR33, LR100
 *
 * Can just become
 * movl (LR0), LR100
 */
static void handle_pure_copy_source_spill(instruction_t* instruction, u_int32_t offset){
	//This will always be a read
	instruction->memory_access_type = READ_FROM_MEMORY;

	//Offset is 0, we just need to do a dereference
	if(offset == 0){
		instruction->source_register = stack_pointer;
		instruction->calculation_mode = ADDRESS_CALCULATION_MODE_DEREF_ONLY_SOURCE;

	//Otherwise, we need to do an offset calculation
	} else {
		instruction->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;
		//Turn this into a load instruction
		instruction->address_calc_reg1 = stack_pointer;

		//IMPORTANT - NULL this out so that future steps don't get confused
		instruction->source_register = NULL;

		//Create the offset using a u64
		instruction->offset = emit_direct_integer_or_char_constant(offset, u64_type);
	}
}


/**
 * Handle a scenario where we have a constant being moved into a destination LR
 * that's been spilled
 *
 * Example:
 * movl $3, LR100
 *
 * Can just become
 * movl $3, 4(LR0)
 */
static void handle_constant_assignment_destination_spill(instruction_t* instruction, u_int32_t offset){
	//This will always be a write
	instruction->memory_access_type = WRITE_TO_MEMORY;

	//Offset is 0, we just need to do a dereference
	if(offset == 0){
		instruction->destination_register = stack_pointer;
		instruction->calculation_mode = ADDRESS_CALCULATION_MODE_DEREF_ONLY_DEST;

	//Otherwise, we need to do an offset calculation
	} else {
		instruction->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;

		//Turn this into a load instruction
		instruction->address_calc_reg1 = stack_pointer;

		//IMPORTANT - NULL this out so that future steps don't get confused
		instruction->destination_register = NULL;

		//Create the offset using a u64
		instruction->offset = emit_direct_integer_or_char_constant(offset, u64_type);
	}
}


/**
 * Handle all spilling for a given instruction. This includes source & destination
 * spilling
 */
static instruction_t* handle_instruction_level_spilling(instruction_t* instruction, dynamic_array_t* live_ranges, live_range_t* spill_range, live_range_t** currently_spilled, stack_region_t* spill_region){
	/**
	 * Optimization: if we have pure copy instructions where we have the source as
	 * our spill range, we do not need to emit any extra instructions. All that we
	 * need to do is turn the move into a memory move
	 */
	if(is_instruction_pure_copy(instruction) == TRUE){
		//Case we want here
		if(instruction->source_register->associated_live_range == spill_range){
			//Let the helper deal with it
			handle_pure_copy_source_spill(instruction, spill_region->function_local_base_address);

			//We're done here
			return instruction;
		}
	}

	/**
	 * Another optimization: if we have an instruction that is
	 * just assigning a constant to our spill range, we can bypass
	 * the spill range entirely and just go right to memory
	 */
	if(is_instruction_constant_assignment(instruction) == TRUE){
		//What we're after here
		if(instruction->destination_register->associated_live_range == spill_range){
			//Let the helper deal with it
			handle_constant_assignment_destination_spill(instruction, spill_region->function_local_base_address);

			//We're done
			return instruction;
		}

	}

	//By default it's the instruction
	instruction_t* latest = instruction;

	//Handle all source spills first
	handle_source_spill(live_ranges, instruction->source_register, spill_range, currently_spilled, instruction, spill_region->function_local_base_address);
	handle_source_spill(live_ranges, instruction->source_register2, spill_range, currently_spilled, instruction, spill_region->function_local_base_address);
	handle_source_spill(live_ranges, instruction->address_calc_reg1, spill_range, currently_spilled, instruction, spill_region->function_local_base_address);
	handle_source_spill(live_ranges, instruction->address_calc_reg2, spill_range, currently_spilled, instruction, spill_region->function_local_base_address);

	//Run through all function parameters
	if(instruction->instruction_type != PHI_FUNCTION){
		//Extract it
		dynamic_array_t parameters = instruction->parameters;

		//Run through and check them all
		for(u_int16_t i = 0; i < parameters.current_index; i++){
			//Extract it
			three_addr_var_t* parameter = dynamic_array_get_at(&parameters, i);

			//Invoke the helper
			handle_source_spill(live_ranges, parameter, spill_range, currently_spilled, instruction, spill_region->function_local_base_address);
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
				handle_source_spill(live_ranges, instruction->destination_register, spill_range, currently_spilled, instruction, spill_region->function_local_base_address);

				//Emit the store instruction for this now
				handle_destination_spill(instruction->destination_register, instruction, spill_region->function_local_base_address);

				//Update latest
				latest = instruction->next_statement;

			//In the case like this, we just need to emit the load
			} else if(is_move_instruction_destination_assigned(instruction) == FALSE){
				//Handle the source spill only
				handle_source_spill(live_ranges, instruction->destination_register, spill_range, currently_spilled, instruction, spill_region->function_local_base_address);

			//In all other cases, we just have the store
			} else {
				//Emit the store instruction for this now
				handle_destination_spill(instruction->destination_register, instruction, spill_region->function_local_base_address);

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
			handle_destination_spill(instruction->destination_register2, instruction, spill_region->function_local_base_address);

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
static void spill_in_function(basic_block_t* function_entry_block, dynamic_array_t* live_ranges, live_range_t* spill_range){
	//Extract the function that we're using
	symtab_function_record_t* function = function_entry_block->function_defined_in;

	//Let's first create the stack region for our spill range
	stack_region_t* spill_region = create_stack_region_for_type(&(function->local_stack), get_largest_type_in_live_range(spill_range));

	//Grab a cursor
	basic_block_t* block_cursor = function_entry_block;

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
 * Bitmap: 011110111110000
 * 					^
 * Index: 5
 *
 * Procedure to grab it:
 * 	Get a mask: 0x01 = 00000000000001
 * 	Shift it over "Index" places: 00000000100000
 * 	Bitwise and the two together: 011110111110000 & 00000000100000 = 00000000100000
 *  Take the result and then shift it back "Index" places: 00000000000001 = 1 = TRUE
 *
 * Function is inlined so in reality this is essentially a macro
 */
static inline u_int32_t get_bitmap_at_index(u_int32_t bitmap, u_int8_t index){
	//Grab a mask
	u_int32_t mask = 0x01;

	//Shift the mask over by the index amount
	mask <<= index;

	//Now take the bitmap and "and" it with the mask that we found. This will knock out
	//anything that is not the index we're after
	u_int32_t result = bitmap & mask;

	//Now we have our result, we need to left shift it over back
	//by the index. This will take our result and put it in the LSB
	//place. In reality what this means is we'll have 0 for false
	//and 1 for true
	result >>= index;

	//Give back the result
	return result;
}


/**
 * Set the bitmap at a given index:
 *
 * Current Bitmap: 011110111010000
 *                          ^
 * Index: 5
 *
 * Procedure to set it:
 * 	Get a mask: 0x01 = 00000000000001
 * 	Shift it over "Index" places: 00000000100000
 * 	Bitwise or the two together: 011110111010000 | 00000000100000 = 011110111110000
 *
 * Function is inlined so in reality this is essentially a macro
 */
static inline void set_bitmap_at_index(u_int32_t* bitmap, u_int8_t index){
	//Grab a mask
	u_int32_t mask = 0x01;

	//Shift it over by the index
	mask <<= index;

	//Bitwise or the two together to set
	*bitmap |= mask;
}


/**
 * Allocate an individual register to a given live range
 *
 * We return TRUE if we were able to color, and we return false if we were not
 */
static u_int8_t allocate_register_general_purpose(live_range_t* live_range){
	//If this is the case, we're already done. This will happen in the event that a register has been pre-colored
	if(live_range->reg.gen_purpose != NO_REG_GEN_PURPOSE){
		//Flag that the function has used this
		set_bitmap_at_index(&(live_range->function_defined_in->assigned_general_purpose_registers), live_range->reg.gen_purpose - 1);

		//All went well
		return TRUE;
	}

	//Allocate an area that holds all the registers that we have available for use. This is offset by 1 from
	//the actual value in the enum. For example, RAX is 1 in the enum, so it's 0 in here. We do not need
	//to use an array here. Instead, we will use a 32 bit integer(more than enough space for us) and store
	//1 or 0 in the physical slot based on if the register is used or not. This avoids the need to touch
	//any memory at all
	u_int32_t register_use_bit_map = 0;

	//Run through every single neighbor
	for(u_int16_t i = 0; i < live_range->neighbors.current_index; i++){
		//Grab the neighbor out
		live_range_t* neighbor = dynamic_array_get_at(&(live_range->neighbors), i);

		//Get whatever register this neighbor has. If it's not the "no_reg" value, 
		//we'll store it in the array
		if(neighbor->reg.gen_purpose != NO_REG_GEN_PURPOSE && neighbor->reg.gen_purpose <= K_COLORS_GEN_USE){
			//Use the helper to do our bitmap setting
			set_bitmap_at_index(&register_use_bit_map, neighbor->reg.gen_purpose - 1);
		}
	}
	
	//Now that the registers array has been populated with interferences, we can scan it and
	//pick the first available register
	u_int16_t i;
	for(i = 0; i < K_COLORS_GEN_USE; i++){
		//If we've found an empty one, that means we're good
		if(get_bitmap_at_index(register_use_bit_map, i) == FALSE){
			break;
		}
	}

	//Now that we've gotten here, i should hold the value of a free register - 1. We'll
	//add 1 back to it to get that free register's name
	if(i < K_COLORS_GEN_USE){
		//Assign the register value to it
		live_range->reg.gen_purpose = i + 1;

		//Flag this as used in the function
		set_bitmap_at_index(&(live_range->function_defined_in->assigned_general_purpose_registers), i);

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
static u_int8_t graph_color_and_allocate_general_purpose(basic_block_t* function_entry, dynamic_array_t* live_ranges){
	//Max priority queue for our live ranges
	max_priority_queue_t priority_live_ranges = max_priority_queue_alloc();

	//Run through and insert everything into the priority live range
	for(u_int16_t i = 0; i < live_ranges->current_index; i++){
		//Extract it
		live_range_t* live_range = dynamic_array_get_at(live_ranges, i);

		//No point in putting this in there, we already know what it will be
		if(live_range == stack_pointer_lr || live_range == instruction_pointer_lr){
			continue;
		}

		//Insert it into the max queue. The higher the spill cost, the higher priority we have
		//here
		max_priority_queue_enqueue(&priority_live_ranges, live_range, live_range->spill_cost);
	}

	//So long as this isn't empty
	while(max_priority_queue_is_empty(&priority_live_ranges) == FALSE){
		//Grab a live range out by deletion
		live_range_t* range = max_priority_queue_dequeue(&priority_live_ranges);

		/**
		 * This degree being less than the number of registers
		 * means we should be able to allocate no issue
		 */
		if(range->degree < K_COLORS_GEN_USE){
			allocate_register_general_purpose(range);

		//Otherwise, we may still be able to allocate here
		} else {
			//We must still attempt to allocate it
			u_int8_t can_allocate = allocate_register_general_purpose(range);
			
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
				spill_in_function(function_entry, live_ranges, range);

				//Destroy the priority queue now that we're done with it
				max_priority_queue_dealloc(&priority_live_ranges);

				//We could not allocate everything here, so we need to return false to
				//trigger the restart by the parent process
				return FALSE;
			}
		}
	}

	//Destroy the priority queue now that we're done with it
	max_priority_queue_dealloc(&priority_live_ranges);

	//Give back true because if we made it here, our graph was N-colorable
	//and we did not have a spill
	return TRUE;
}


/**
 * Allocate an individual SSE live range's register. If we are able to
 * allocate successfully, we return true. If not, we'll return false which
 * will invoke the "spill_in_function" process in the caller
 */
static u_int8_t allocate_register_sse(live_range_t* live_range){
	//This is possible - it means that it's already been allocated. In
	//this case, we'll just fill out the function's assigned list and move
	//along
	if(live_range->reg.sse_reg != NO_REG_SSE){
		//Flag it inside
		set_bitmap_at_index(&(live_range->function_defined_in->assigned_sse_registers), live_range->reg.sse_reg - 1);

		//All went well
		return TRUE;
	}

	//Allocate an area that holds all the registers that we have available for use. This is offset by 1 from
	//the actual value in the enum. For example, RAX is 1 in the enum, so it's 0 in here. We do not need
	//to use an array here. Instead, we will use a 32 bit integer(more than enough space for us) and store
	//1 or 0 in the physical slot based on if the register is used or not. This avoids the need to touch
	//any memory at all
	u_int32_t register_use_bit_map = 0;

	//Run through every single neighbor
	for(u_int16_t i = 0; i < live_range->neighbors.current_index; i++){
		//Grab the neighbor out
		live_range_t* neighbor = dynamic_array_get_at(&(live_range->neighbors), i);

		//Get whatever register this neighbor has. If it's not the "no_reg" value, 
		//we'll store it in the array
		if(neighbor->reg.sse_reg != NO_REG_SSE){
			//Use the helper to do our bitmap setting
			set_bitmap_at_index(&register_use_bit_map, neighbor->reg.sse_reg - 1);
		}
	}
	
	//Now that the registers array has been populated with interferences, we can scan it and
	//pick the first available register
	u_int16_t i;
	for(i = 0; i < K_COLORS_SSE; i++){
		//If we've found an empty one, that means we're good
		if(get_bitmap_at_index(register_use_bit_map, i) == FALSE){
			break;
		}
	}

	//Now that we've gotten here, i should hold the value of a free register - 1. We'll
	//add 1 back to it to get that free register's name
	if(i < K_COLORS_SSE){
		//Assign the register value to it
		live_range->reg.sse_reg = i + 1;

		//Flag this as used in the function
		set_bitmap_at_index(&(live_range->function_defined_in->assigned_sse_registers), i);

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
static u_int8_t graph_color_and_allocate_sse(basic_block_t* function_entry, dynamic_array_t* sse_live_ranges){
	//We need to maintain a priority queue for our live ranges. The ranges with
	//the highest spill cost need to be allocated first
	max_priority_queue_t priority_live_ranges = max_priority_queue_alloc();

	//For each SSE live range only. Unlike the GP live ranges, there's
	//no special LRs here like the stack pointer/instruction pointer ones
	//so we can just add them all
	for(u_int16_t i = 0; i < sse_live_ranges->current_index; i++){
		//Extract it
		live_range_t* live_range = dynamic_array_get_at(sse_live_ranges, i);

		//Add it into the priority queue with it's spill cost as the priority
		max_priority_queue_enqueue(&priority_live_ranges, live_range, live_range->spill_cost);
	}

	//So long as there are more live ranges to allocate
	while(max_priority_queue_is_empty(&priority_live_ranges) == FALSE){
		//Pop it out
		live_range_t* target = max_priority_queue_dequeue(&priority_live_ranges);

		//If the degree is *less* than the number of available registers(K_COLORS_SSE),
		//then this is guaranteed to work and we can invoke the allocator off hte bat
		if(target->degree < K_COLORS_SSE){
			allocate_register_sse(target);

		//Even if it's degree is more than the K colors, there's still a chance
		//that we could allocate it. We will give it a try. If it works, great, if
		//not, we'll need to engage our spilling logic for this 
		} else {
			//Attempt to allocate it
			u_int8_t could_allocate = allocate_register_sse(target);

			//If we were not able to allocate at all, that is where we need
			//to use the spiller
			if(could_allocate == FALSE){
				printf("\n\n\nCould not allocate: LR%d\n", target->live_range_id);

				/**
				 * Now we need to spill this live range. It is important to note that
				 * spilling has the effect of completely rewriting the entire program.
				 * As such, once we spill, we need to redo everything, including the entire 
				 * graph coloring process. This will require a reset. In practice, even
				 * the most extreme programs only require that this be done once or twice
				 */
				spill_in_function(function_entry, sse_live_ranges, target);

				//Destroy the priority queue now that we're done with it
				max_priority_queue_dealloc(&priority_live_ranges);

				//We could not allocate everything here, so we need to return false to
				//trigger the restart by the parent process
				return FALSE;
			}
		}
	}

	//Destroy this before leaving
	max_priority_queue_dealloc(&priority_live_ranges);

	//If we made it all the way down here, then the graph was N-colorable and we're good
	return TRUE;
}


/**
 * Insert caller saved logic for a direct function call. In a direct function call, we'll know what
 * registers are being used by the function being called. As such, we can be precise about what
 * we push/pop onto and off of the stack and have for a more efficient saving regime. This is not
 * possible for indirect function calls, which is the reason for the distinction
 *
 * NOTE: All SSE(xmm) registers are caller saved. The callee is free to clobber these however it
 * sees fit. As such, the burden for saving all of these falls onto the caller here
 */
static instruction_t* insert_caller_saved_logic_for_direct_call(symtab_function_record_t* caller, instruction_t* function_call){
	//If we get here we know that we have a call instruction. Let's
	//grab whatever it's calling out. We're able to do this for a direct call,
	//whereas in an indirect call we are not
	symtab_function_record_t* callee = function_call->called_function;

	//Grab out this LR for reference later on. Remember that this is nullable, so we 
	//need to account for that
	live_range_t* destination_lr = NULL;

	//What type of LR is the destination register
	live_range_class_t destination_lr_class;
	
	//Assign if it's not null
	if(function_call->destination_register != NULL){
		//Extract the destination LR for our uses later
		destination_lr = function_call->destination_register->associated_live_range;

		//Cache the class as well
		destination_lr_class = destination_lr->live_range_class;
	}

	/**
	 * Keep track of what is immediately before and after
	 * our stack param setup. In the event that there
	 * is no stack param setup, these will just be the
	 * function call
	 */
	instruction_t* before_stack_param_setup = function_call;
	instruction_t* after_stack_param_setup = function_call;

	/**
	 * If our function contains stack parameters, then we need to look for the
	 * actual stack allocation/deallocation statements and ensure that we are
	 * putting all of our saving logic *before and after* said statements
	 */
	if(callee->contains_stack_params == TRUE){
		//Let's first find the stack allocation statement. Note that it *has to be here*
		while(before_stack_param_setup->statement_type != THREE_ADDR_CODE_STACK_ALLOCATION_STMT){
			//Go back
			before_stack_param_setup = before_stack_param_setup->previous_statement;
		}

		//Now by a similar token let's find the deallocation statement
		while(after_stack_param_setup->statement_type != THREE_ADDR_CODE_STACK_DEALLOCATION_STMT){
			//Push it up
			after_stack_param_setup = after_stack_param_setup->next_statement;
		}
	}

	/**
	 * Maintain dynamic arrays that are(as of yet) unallocated. We will build
	 * these arrays out with all of the live ranges that need to be saved in 
	 * each class of variable(GP or SSE). When we are done, we will need
	 * to traverse them. GP always comes first to avoid any issues with the stack, which is then
	 * followed by SSE afterwards
	 */
	dynamic_array_t general_purpose_lrs_to_save;
	dynamic_array_t SSE_lrs_to_save;
	
	//Wipe them all out first
	INITIALIZE_NULL_DYNAMIC_ARRAY(general_purpose_lrs_to_save);
	INITIALIZE_NULL_DYNAMIC_ARRAY(SSE_lrs_to_save);

	//Use the helper to calculate LIVE_AFTER up to but not including the actual function call
	dynamic_array_t live_after = calculate_live_after_for_block(function_call->block_contained_in, function_call);
	//We can remove the destination LR from here, there's no point in keeping it in
	dynamic_array_delete(&live_after, destination_lr);

	/**
	 * Run through everything that is alive after this function runs(live_after)
	 * and check if we need to save any registers from that set
	 */
	for(u_int32_t i = 0; i < live_after.current_index; i++){
		//Extract it
		live_range_t* lr = dynamic_array_get_at(&live_after, i);

		//Holders for our different register types
		general_purpose_register_t general_purpose_reg;
		sse_register_t sse_reg;

		//Go based on the class that we've got
		switch(lr->live_range_class){
			case LIVE_RANGE_CLASS_GEN_PURPOSE:
				//Extract the general purpose register
				general_purpose_reg = lr->reg.gen_purpose;

				//If it's not caller-saved, it's irrelevant to us
				if(is_general_purpose_register_caller_saved(general_purpose_reg) == FALSE){
					continue;
				}

				//There's also no point in saving this. This could happen if we have precoloring
				//We'll only check this if the classes match though
				if(destination_lr != NULL && destination_lr_class == LIVE_RANGE_CLASS_GEN_PURPOSE){
					//Skip it
					if(general_purpose_reg == destination_lr->reg.gen_purpose){
						continue;
					}
				}

				/**
				 * Once we get past here, we know that we need to save this
				 * register because the callee will also assign it, so whatever
				 * value it has that we're relying on would not survive the call
				 */
				if(get_bitmap_at_index(callee->assigned_general_purpose_registers, general_purpose_reg - 1) == TRUE){
					//Allocate here if need be
					if(general_purpose_lrs_to_save.internal_array == NULL){
						general_purpose_lrs_to_save = dynamic_array_alloc();
					}

					//Add this into our list of GP LRs to save
					dynamic_array_add(&general_purpose_lrs_to_save, lr);
				}

				break;

			//We know that *all* SSE registers are caller saved. As such, we will
			//need to save anything/everything that is live after for these
			case LIVE_RANGE_CLASS_SSE:
				//Extract the SSE register
				sse_reg = lr->reg.sse_reg;

				//There's also no point in saving this. This could happen if we have precoloring
				//We'll only check this if the classes match though
				if(destination_lr != NULL && destination_lr_class == LIVE_RANGE_CLASS_SSE){
					//Skip it
					if(sse_reg == destination_lr->reg.sse_reg){
						continue;
					}
				}

				/**
				 * Once we get past here, we know that we need to save this
				 * register because the callee will also assign it, so whatever
				 * value it has that we're relying on would not survive the call
				 */
				if(get_bitmap_at_index(callee->assigned_sse_registers, sse_reg - 1) == TRUE){
					//Allocate here if need be
					if(SSE_lrs_to_save.internal_array == NULL){
						SSE_lrs_to_save = dynamic_array_alloc();
					}

					//Add this into our list of GP LRs to save
					dynamic_array_add(&SSE_lrs_to_save, lr);
				}

				break;
		}
	}

	//We'll need to keep track of the last instruction to return it in the end
	instruction_t* first_instruction = before_stack_param_setup;
	instruction_t* last_instruction = after_stack_param_setup;

	/**
	 * Due to the way that we use push/pop for general purpose caller saving, we need
	 * to guarantee that this are inserted first and that any SSE saving happens
	 * afterwards. If we didn't do this, we'd run the risk of clobbering any SSE
	 * values that had been saved on the stack
	 */
	for(u_int32_t i = 0; i < general_purpose_lrs_to_save.current_index; i++){
		//Grab out the LR
		live_range_t* lr_to_save = dynamic_array_get_at(&general_purpose_lrs_to_save, i);

		//Emit a direct push with this live range's register
		instruction_t* push_inst = emit_direct_gp_register_push_instruction(lr_to_save->reg.gen_purpose);

		//Emit the pop instruction for this
		instruction_t* pop_inst = emit_direct_gp_register_pop_instruction(lr_to_save->reg.gen_purpose);

		//Insert the push instruction directly before any stack parameter setup
		insert_instruction_before_given(push_inst, first_instruction);

		//Insert the pop instruction directly after any stack parameter setup
		insert_instruction_after_given(pop_inst, last_instruction);

		/**
		 * This instruction now is the very first instruction in our big block
		 * of instructions
		 */
		first_instruction = push_inst;

		/**
		 * And the pop instruction is now the very last instruction(currently) in
		 * our big block of instructions
		 */
		last_instruction = pop_inst;
	}

	/**
	 * Now let's do the exact same procedure for SSE registers. We're doing this last because again, we
	 * need to guarantee that all of this movement happens *after* any pushing or popping
	 */
	for(u_int32_t i = 0; i < SSE_lrs_to_save.current_index; i++){
		//What is the LR that we want to save
		live_range_t* lr_to_save = dynamic_array_get_at(&SSE_lrs_to_save, i);

		/**
		 * Do we already have a stack region for this exact LR? We will check and if
		 * so, we don't need to make any more room on the stack for it
		 */
		stack_region_t* stack_region = get_stack_region_for_live_range(&(caller->local_stack), lr_to_save);

		//If we didn't have a spill region, then we'll make one
		if(stack_region == NULL){
			stack_region = create_stack_region_for_type(&(caller->local_stack), get_largest_type_in_live_range(lr_to_save));

			//Cache this for later
			stack_region->variable_referenced = lr_to_save;
		}

		//Emit the store instruction and load instruction
		instruction_t* store_instruction = emit_store_instruction(dynamic_array_get_at(&(lr_to_save->variables), 0), stack_pointer, type_symtab, stack_region->function_local_base_address);
		instruction_t* load_instruction = emit_load_instruction(dynamic_array_get_at(&(lr_to_save->variables), 0), stack_pointer, type_symtab, stack_region->function_local_base_address);

		//Insert the push instruction directly before the call instruction
		insert_instruction_before_given(store_instruction, first_instruction);

		//This now is the first instruction
		first_instruction = store_instruction;

		//Insert the pop instruction directly after the last instruction
		insert_instruction_after_given(load_instruction, last_instruction);

		//And this now is the last instruction
		last_instruction = load_instruction;
	}

	//Free it up once done
	dynamic_array_dealloc(&live_after);

	//Let's also free the temporary storage if need be
	if(general_purpose_lrs_to_save.internal_array != NULL){
		dynamic_array_dealloc(&general_purpose_lrs_to_save);
	}
	
	if(SSE_lrs_to_save.internal_array != NULL){
		dynamic_array_dealloc(&SSE_lrs_to_save);
	}

	//Return whatever this ended up being
	return last_instruction;
}


/**
 *
 * For an indirect call, we can not know for certain what registers are and are not used
 * inside of the function. As such, we'll need to save any/all caller saved registers that are in use
 * at the time that the function is called
 *
 * NOTE: All SSE(xmm) registers are caller saved. The callee is free to clobber these however it
 * sees fit. As such, the burden for saving all of these falls onto the caller here
 */
static instruction_t* insert_caller_saved_logic_for_indirect_call(symtab_function_record_t* caller, instruction_t* function_call){
	//Get the destination LR. Remember that this is nullable
	live_range_t* destination_lr = NULL;

	//Also cache what the class of the destination LR is
	live_range_class_t destination_lr_class;

	//Extract the actual function type
	generic_type_t* function_type = function_call->source_register->type;

	//Extract if not null
	if(function_call->destination_register != NULL){
		destination_lr = function_call->destination_register->associated_live_range;

		//What is the class
		destination_lr_class = destination_lr->live_range_class;
	}

	/**
	 * Maintain dynamic arrays that are(as of yet) unallocated. We will build
	 * these arrays out with all of the live ranges that need to be saved in 
	 * each class of variable(GP or SSE). When we are done, we will need
	 * to traverse them. GP always comes first to avoid any issues with the stack, which is then
	 * followed by SSE afterwards
	 */
	dynamic_array_t general_purpose_lrs_to_save;
	dynamic_array_t SSE_lrs_to_save;
	
	//Wipe them all out first
	INITIALIZE_NULL_DYNAMIC_ARRAY(general_purpose_lrs_to_save);
	INITIALIZE_NULL_DYNAMIC_ARRAY(SSE_lrs_to_save);

	//Maintain pointers to instructions that are our landmarks, especially if we have stack param passing
	instruction_t* before_stack_param_setup = function_call;
	instruction_t* after_stack_param_setup = function_call;

	/**
	 * NOTE: if we have any stack passed parameter logic, we need to detect that now and ensure that we 
	 * are inserting our caller saving logic both before and after any of that takes place to ensure we
	 * don't clobber our values inadvertently
	 */
	if(function_type->internal_types.function_type->contains_stack_params == TRUE){
		//Let's first find the stack allocation statement. Note that it *has to be here*
		while(before_stack_param_setup->statement_type != THREE_ADDR_CODE_STACK_ALLOCATION_STMT){
			//Go back
			before_stack_param_setup = before_stack_param_setup->previous_statement;
		}

		//Now by a similar token let's find the deallocation statement
		while(after_stack_param_setup->statement_type != THREE_ADDR_CODE_STACK_DEALLOCATION_STMT){
			//Push it up
			after_stack_param_setup = after_stack_param_setup->next_statement;
		}
	}

	//Grab the live_after array for up to but not including the actual call
	dynamic_array_t live_after = calculate_live_after_for_block(function_call->block_contained_in, function_call);
	//Delete the destination LR from here as it will be assigned by the instruction anyway
	dynamic_array_delete(&live_after, destination_lr);

	/**
	 * Now we will run through every live range in live_after and check if it is caller-saved or not
	 * We are not able to fine-tune things here like we are in the the direct call unfortunately
	 */
	for(u_int32_t i = 0; i < live_after.current_index; i++){
		//Grab it out
		live_range_t* lr = dynamic_array_get_at(&live_after, i);

		//Go based on the given LR class
		switch(lr->live_range_class){
			case LIVE_RANGE_CLASS_GEN_PURPOSE:
				//This register must be caller saved to be relevant
				if(is_general_purpose_register_caller_saved(lr->reg.gen_purpose) == FALSE){
					continue;
				}

				//Let's now check to see if it matches the function destination's register. If it
				//does, we'll bail
				if(destination_lr != NULL && destination_lr_class == LIVE_RANGE_CLASS_GEN_PURPOSE){
					if(destination_lr->reg.gen_purpose == lr->reg.gen_purpose){
						continue;
					}
				}

				//If we make it down here, then we need to save this LR
				if(general_purpose_lrs_to_save.internal_array == NULL){
					general_purpose_lrs_to_save = dynamic_array_alloc();
				}

				//Add it into the array
				dynamic_array_add(&general_purpose_lrs_to_save, lr);

				break;

			//Remember that by default these are all caller saved
			case LIVE_RANGE_CLASS_SSE:
				//Let's now check to see if it matches the function destination's register. If it
				//does, we'll bail
				if(destination_lr != NULL && destination_lr_class == LIVE_RANGE_CLASS_SSE){
					if(destination_lr->reg.sse_reg == lr->reg.sse_reg){
						continue;
					}
				}

				//If we make it down here then this is an LR that we need to save
				if(SSE_lrs_to_save.internal_array == NULL){
					SSE_lrs_to_save = dynamic_array_alloc();
				}

				//Add it into the array
				dynamic_array_add(&SSE_lrs_to_save, lr);

				break;
		}
	}

	//We'll need to keep track of the last instruction to return it in the end
	instruction_t* first_instruction = before_stack_param_setup;
	instruction_t* last_instruction = after_stack_param_setup;

	//Now let's run through all of the general purpose registers first
	for(u_int32_t i = 0; i < general_purpose_lrs_to_save.current_index; i++){
		//Extract what we want to save
		live_range_t* lr_to_save = dynamic_array_get_at(&general_purpose_lrs_to_save, i);

		//Emit a direct push with this live range's register
		instruction_t* push_inst_gp = emit_direct_gp_register_push_instruction(lr_to_save->reg.gen_purpose);

		//Emit the pop instruction for this
		instruction_t* pop_inst_gp = emit_direct_gp_register_pop_instruction(lr_to_save->reg.gen_purpose);

		//The push goes directly before the push instruction
		insert_instruction_before_given(push_inst_gp, first_instruction);
		
		//This now is the first instruction
		first_instruction = push_inst_gp;

		//Insert the pop instruction directly after the last instruction
		insert_instruction_after_given(pop_inst_gp, last_instruction);

		//This now is the last instruction
		last_instruction = pop_inst_gp;
	}

	/**
	 * Once we are entirely done with all of that, we will now handle saving the SSE registers.
	 * These must come before and after the general purpose ones because pushing and popping silently
	 * modifies the stack pointer, meaning that our offsets for this would be inaccurate if we did them
	 * after pushing
	 */
	for(u_int32_t i = 0; i < SSE_lrs_to_save.current_index; i++){
		//Extract the LR
		live_range_t* lr_to_save = dynamic_array_get_at(&SSE_lrs_to_save, i);

		//By the time we get here, we know that we need to save this
		//First check if this has already been saved before
		stack_region_t* stack_region = get_stack_region_for_live_range(&(caller->local_stack), lr_to_save);

		//If it's NULL, we need to make one ourselves
		if(stack_region == NULL){
			stack_region = create_stack_region_for_type(&(caller->local_stack), get_largest_type_in_live_range(lr_to_save));

			//Cache this for later
			stack_region->variable_referenced = lr_to_save;
		}

		//Emit the store instruction and load instruction
		instruction_t* store_instruction = emit_store_instruction(dynamic_array_get_at(&(lr_to_save->variables), 0), stack_pointer, type_symtab, stack_region->function_local_base_address);
		instruction_t* load_instruction = emit_load_instruction(dynamic_array_get_at(&(lr_to_save->variables), 0), stack_pointer, type_symtab, stack_region->function_local_base_address);

		//Insert the push instruction directly before the first instruction
		insert_instruction_before_given(store_instruction, first_instruction);

		//The first instruction now is the store
		first_instruction = store_instruction;

		//Insert the pop instruction directly after the last instruction
		insert_instruction_after_given(load_instruction, last_instruction);

		//And the last one now is the load
		last_instruction = load_instruction;
	}

	//Free it up once done
	dynamic_array_dealloc(&live_after);
	
	//If we had arrays to save, now is the time to deallocate them
	if(general_purpose_lrs_to_save.internal_array != NULL){
		dynamic_array_dealloc(&general_purpose_lrs_to_save);
	}

	if(SSE_lrs_to_save.internal_array != NULL){
		dynamic_array_dealloc(&SSE_lrs_to_save);
	}

	//Return the last instruction to save time when drilling
	return last_instruction;
}


/**
 * Run through the current function and insert all needed save/restore logic
 * for caller-saved registers
 */
static inline void insert_caller_saved_register_logic(basic_block_t* function_entry_block){
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
					instruction = insert_caller_saved_logic_for_direct_call(function, instruction);
					break;
					
				//Use the helper for an indirect call. Indirect calls differ slightly
				//from direct ones because we have less information, so we'll need a different
				//helper here
				case INDIRECT_CALL:
					instruction = insert_caller_saved_logic_for_indirect_call(function, instruction);
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
 * This function handles all callee saving logic for each function that we have
 *
 * NOTE: since all SSE registers are caller-saved, we actually don't need to worry about any SSE registers here because
 * they will all be handled by the caller anyway
 */
static void insert_callee_saving_logic(cfg_t* cfg, basic_block_t* function_entry, basic_block_t* function_exit){
	//Keep a reference to the original entry instruction that we had before
	//we insert any pushes. This will be important for when we need to
	//reassign the function's leader statement
	instruction_t* entry_instruction = function_entry->leader_statement;
	
	//Grab the function record out now too
	symtab_function_record_t* function = function_entry->function_defined_in;

	//We need to see which registers that we use
	for(u_int16_t i = 0; i < K_COLORS_GEN_USE; i++){
		//We don't use this register, so move on
		if(get_bitmap_at_index(function->assigned_general_purpose_registers, i) == FALSE){
			continue;
		}

		//Otherwise if we get here, we know that we use it. Remember
		//the register value is always offset by one
		general_purpose_register_t used_reg = i + 1;

		//If this isn't callee saved, then we know to move on
		if(is_general_purpose_register_callee_saved(used_reg) == FALSE){
			continue;
		}

		//Now we'll need to add an instruction to push this at the entry point of our function
		instruction_t* push_instruction = emit_direct_gp_register_push_instruction(used_reg);

		//Flag that this is a callee saving instruction
		push_instruction->is_callee_saving_instruction = TRUE;

		//Insert this push before the leader instruction
		insert_instruction_before_given(push_instruction, entry_instruction);

		//If the entry instruction is still the function's leader statement, then
		//we'll need to update it. This only happens on the very first push. For
		//everyting subsequent, we won't need to do this
		if(entry_instruction == function_entry->leader_statement){
			//Reassign this to be the very first push
			function_entry->leader_statement = push_instruction;
		}
	}

	//Now that we've added all of the callee saving logic at the function entry, we'll need to
	//go through and add it at the exit(s) as well. Note that we're given the function exit block
	//as an input value here
	
	//For each and every predecessor of the function exit block
	for(u_int16_t i = 0; i < function_exit->predecessors.current_index; i++){
		//Grab the given predecessor out
		basic_block_t* predecessor = dynamic_array_get_at(&(function_exit->predecessors), i);

		//Now we'll go through the registers in the reverse order. This time, when we hit one that
		//is callee-saved and used, we'll emit the push instruction and insert it directly before
		//the "ret". This will ensure that our LIFO structure for pushing/popping is maintained

		//Run through all the registers backwards
		for(int16_t j = K_COLORS_GEN_USE - 1; j >= 0; j--){
			//If we haven't used this register, then skip it
			if(get_bitmap_at_index(function->assigned_general_purpose_registers, j) == FALSE){
				continue;
			}

			//Remember that our positional coding is off by 1(0 is NO_REG value), so we'll
			//add 1 to make the value correct
			general_purpose_register_t used_reg = j + 1;

			//If it's not callee saved then we don't care
			if(is_general_purpose_register_callee_saved(used_reg) == FALSE){
				continue;
			}

			//If we make it here, we know that we'll need to save this register
			instruction_t* pop_instruction = emit_direct_gp_register_pop_instruction(used_reg);

			//Flag that this is a callee-saving instruction
			pop_instruction->is_callee_saving_instruction = TRUE;

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
static inline void insert_saving_logic(cfg_t* cfg, basic_block_t* function_entry_block, basic_block_t* function_exit_block){
	//We'll first insert the caller saved logic. This logic has the potential to
	//generate stack allocations for XMM registers so it needs to come first
	insert_caller_saved_register_logic(function_entry_block);

	//Then we'll do all callee saving
	insert_callee_saving_logic(cfg, function_entry_block, function_exit_block);
}


/**
 * TODO
 */
static inline void update_stack_passed_parameter_offsets(symtab_function_record_t* function){
	for(u_int32_t i = 0; i < function->function_blocks.current_index; i++){
		basic_block_t* block = dynamic_array_get_at(&(function->function_blocks), i);




		//TODO implement

		
	}
}



/**
 * Now that we've done any spilling that we need to do, we'll need to finalize the
 * local stack for this function. In addition to that, we're also going to need to 
 * finalize the stack passed parameters(if there are any) because those values
 * will need to have their stack offsets modified if we did anything to change
 * how large the stack frame of this function is. This helper function will handle
 * all of that
 */
static inline void finalize_local_and_parameter_stack_logic(cfg_t* cfg, basic_block_t* function_entry, basic_block_t* function_exit){
	//Extract what function this is
	symtab_function_record_t* function = function_entry->function_defined_in;

	//The first thing that we'll want to do is align the local stack
	align_stack_data_area(&(function->local_stack));

	//We'll also need it's stack data area
	stack_data_area_t* area = &(function_entry->function_defined_in->local_stack);

	//Grab the total size out
	u_int32_t local_stack_size = area->total_size;

	/**
	 * The total stack frame size is the total footprint that this function will take
	 * up on the stack. Note that this is not always *just* the local stack size. If 
	 * we have callee-saving instructions(pushq instructions) at the top, that also
	 * adds to the total size. We need to keep track of this in case we have stack-passed
	 * parameters who will need their offsets updated
	 */
	u_int32_t total_stack_frame_size = local_stack_size;

	/**
	 * If we have a local stack size that is more than 0, we'll emit it now. Note that
	 * we need to specifically emit this *after* any push instructions at the beginning of the function
	 */
	if(local_stack_size != 0){
		//We need to find where to put this
		instruction_t* after_stack_allocation = function_entry->leader_statement;

		/**
		 * So long as we keep seeing the leading "push" instructions, we need
		 * to keep searching through. Once this loop exits, we know that
		 * we'll have a pointer to the instruction after the last push instruction
		 */
		while(after_stack_allocation->is_callee_saving_instruction == TRUE){
			//This increases the total stack frame size by 8 bytes
			total_stack_frame_size += CALLEE_SAVED_REGISTER_STACK_SIZE_BYTES;

			//Push it down
			after_stack_allocation = after_stack_allocation->next_statement;
		}

		//For each function entry block, we need to emit a stack subtraction that is the size of that given variable
		instruction_t* stack_allocation = emit_stack_allocation_statement(cfg->stack_pointer, cfg->type_symtab, local_stack_size);

		//Now that we have the stack allocation statement, we can add it in to be right before the current leader statement
		insert_instruction_before_given(stack_allocation, after_stack_allocation);

		//Now we need to go through every exit block and emit the stack deallocation
		for(u_int32_t i = 0; i < function_exit->predecessors.current_index; i++){
			//Get the predecessor out
			basic_block_t* predecessor = dynamic_array_get_at(&(function_exit->predecessors), i);

			/**
			 * We need to get this deallocation to happen right after we're done returning/callee-saving
			 *
			 *
			 * ....
			 * <Needs to go here>
			 * ret 
			 *
			 * ....
			 * <Needs to go here>
			 * pop %r9
			 * pop %r11
			 * ret
			 *
			 * So in reality we just need to get a pointer to the last statement that either is a "ret" or is callee-saving
			 * logic
			 */
			instruction_t* last_callee_saving_instruction = predecessor->exit_statement;

			/**
			 * Keep crawling upwards so long as:
			 * 	1.) we're not going to run into a NULL statement
			 * 	2.) the previous statement is still some callee saving logic(pop instructions)
			 */
			while(last_callee_saving_instruction->previous_statement != NULL
					&& last_callee_saving_instruction->previous_statement->is_callee_saving_instruction == TRUE){

				//Bump it up
				last_callee_saving_instruction = last_callee_saving_instruction->previous_statement;
			}

			//We will need this pointer for later
			instruction_t* before_last_callee_saved = last_callee_saving_instruction->previous_statement;

			/**
			 * Do we have an end of a function like this:
			 * ....
			 * addq $16, %rsp
			 * addq $32, %rsp
			 * ret
			 *
			 * In that case, we can save an instruction be combining the two into something like:
			 * ...
			 * addq $48, %rsp
			 * ret
			 */
			if(before_last_callee_saved != NULL
				&& before_last_callee_saved->instruction_type == ADDQ
				&& before_last_callee_saved->destination_register == stack_pointer){

				//Instead of allocating, we are just going to add the local stack size to this given stack size
				sum_constant_with_raw_int64_value(before_last_callee_saved->source_immediate, u64_type, local_stack_size);

			//Otherwise we have no chance to optimize
			} else {
				//Emit the stack deallocation statement
				instruction_t* stack_deallocation = emit_stack_deallocation_statement(cfg->stack_pointer, cfg->type_symtab, local_stack_size);

				//By the time we get down here, we should have a pointer to either the ret statement *or* the very
				//last callee saving statement(pop inst). Our stack deallocator goes before this
				insert_instruction_before_given(stack_deallocation, last_callee_saving_instruction);
			}
		}
	}

	/**
	 * Does this function contain stack params? If so, now is the time
	 * were we need to modify all of their offsets to ensure that we are
	 * grabbing these from the correct stack region when we're inside of 
	 * the function
	 */
	if(function->contains_stack_params == TRUE){
		//Make use of the total stack frame size to recompute all of these offsets
		recompute_stack_passed_parameter_region_offsets(&(function->stack_passed_parameters), total_stack_frame_size);

		//TODO - crawl all of the instructions and rememdiate any of the parameter passed stack addresses
	}
}


/**
 * Perform our function level allocation process
 */
static void allocate_registers_for_function(compiler_options_t* options, cfg_t* cfg, basic_block_t* function_entry, basic_block_t* function_exit){
	//Save whether or not we want to actually print IRs
	u_int8_t print_irs = options->print_irs;
	u_int8_t debug_printing = options->enable_debug_printing;

	/**
	 * STEP 1: Build all live ranges from variables:
	 * 	
	 * 	Invoke the helper rule to crawl the entire CFG, constructing all live
	 * 	ranges *and* doing the needed bookkeeping for used & assigned variables
	 * 	that will be needed for step 2
	 *
	 * 	We only need to do this once for the allocation
	 */
	dynamic_array_t general_purpose_live_ranges = dynamic_array_alloc();
	dynamic_array_t sse_live_ranges = dynamic_array_alloc();

	//We will do a 2-for-1 pass of the entire function level CFG 
	construct_live_ranges_in_function(function_entry, &general_purpose_live_ranges, &sse_live_ranges);

	//If we are printing these now is the time to display
	if(print_irs == TRUE){
		printf("============= Before Liveness ==============\n");
		print_function_blocks_with_live_ranges(function_entry);
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
	compute_spill_costs(&general_purpose_live_ranges);
	compute_spill_costs(&sse_live_ranges);


	//If we are printing these now is the time to display
	if(print_irs == TRUE){
		printf("=============== After Cost Update ============\n");
		print_all_live_ranges(&general_purpose_live_ranges, &sse_live_ranges);
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
	 * We will need to do this every single time we reallocate. This is also a "2-for-1",
	 * meaning that it will count for both general purpose and SSE registers
	*/
	calculate_live_range_liveness_sets(function_entry);


	//Show our IR's here
	if(print_irs == TRUE){
		//Show our live ranges once again
		print_all_live_ranges(&general_purpose_live_ranges, &sse_live_ranges);
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
	//First, calculate all of our interferences
	calculate_all_interferences_in_function(function_entry);

	//Once we have that done, we can construct 2 separate graphs. One graph is for SSE live ranges,
	//and the other is for general purpose live ranges
	interference_graph_t* general_purpose_graph = construct_interference_graph_from_adjacency_lists(&general_purpose_live_ranges);
	interference_graph_t* sse_graph = construct_interference_graph_from_adjacency_lists(&sse_live_ranges);


	//If we are printing these now is the time to display
	if(print_irs == TRUE){
		printf("============= After Live Range Determination ==============\n");
		print_function_blocks_with_live_ranges(function_entry);
		printf("============= After Live Range Determination ==============\n");
	}
	

	/**
	 * STEP 5: Pre-coloring registers
	 *
	 * Now that we have the interference calculated, we will "pre-color" live ranges
	 * whose color is known before allocation. This includes things like:
	 * return values being in %rax, function parameter 1 being in %rdi, etc. This 
	 * precolorer helper will deal with both SSE and general purpose registers, which
	 * is why both are passed through
	 */;
	precolor_function(function_entry, &general_purpose_live_ranges, &sse_live_ranges);


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
	 *
	 * The coalescer will run on all live ranges - both general purpose and SSE. There
	 * are 2 separate flags - general purpose coalesce and sse coalesce that flag if we could 
	 * coalesce at least one range in either one, both or none
	*/
	coalescence_result_t coalescence_result = perform_live_range_coalescence(function_entry, general_purpose_graph, sse_graph, debug_printing);

	/**
	 * If we were in fact able to coalesce, we will have messed up the liveness sets due
	 * to merging live ranges, etc. This may mess up allocation down the line. As such, we
	 * need to come here and recalculate the following:
	 * 	1.) all of the used & assigned sets
	 * 	2.) all of the liveness sets(those rely on used & defined entirely)
	 * 	3.) the interference
	 *
	 * short of this, we will see strange an inaccurate results such as excessive interferenceo
	 *
	 * This coalescence loop is also sensitive 
	 */
	while(coalescence_result != COALESCENCE_RESULT_NONE){
		switch(coalescence_result){
			case COALESCENCE_RESULT_GP_ONLY:
				//Only redo the general purpose live ranges
				reset_all_live_ranges(&general_purpose_live_ranges);

				//Redo all of the used & assigned sets. This will hit everything
				//but since no SSE registers were coalesced, it shouldn't matter
				recompute_used_and_assigned_sets(function_entry);
			
				//Redo the spill costs for the GP registers only
				compute_spill_costs(&general_purpose_live_ranges);

				//Recalculate all liveness sets as well. Again this hits
				//everything but nothing's changed for SSE so it's fine
				calculate_live_range_liveness_sets(function_entry);

				//Calculate only the general purpose interferences
				calculate_target_interferences_in_function(function_entry, LIVE_RANGE_CLASS_GEN_PURPOSE);

				//Now construct the specific interference graph that we need
				general_purpose_graph = construct_interference_graph_from_adjacency_lists(&general_purpose_live_ranges);

				//Now we coalesce for the GP result only. We do this because in theory, we should not
				//be able to do anything else with the other class of registers
				coalescence_result = perform_live_range_coalescence_for_target(function_entry, general_purpose_graph, LIVE_RANGE_CLASS_GEN_PURPOSE, debug_printing);

				break;

			case COALESCENCE_RESULT_SSE_ONLY:
				//Only redo the SSE live ranges
				reset_all_live_ranges(&sse_live_ranges);

				//Redo all of the used & assigned sets. This will hit everything
				//but since no GP registers were coalesced, it shouldn't matter
				recompute_used_and_assigned_sets(function_entry);

				//Redo the spill costs for the GP registers only
				compute_spill_costs(&sse_live_ranges);

				//Recalculate all liveness sets as well. Again this hits
				//everything but nothing's changed for GP so it's fine
				calculate_live_range_liveness_sets(function_entry);

				//Calculate only the SSE interferences
				calculate_target_interferences_in_function(function_entry, LIVE_RANGE_CLASS_SSE);

				//Now construct the specific interference graph that we need
				sse_graph = construct_interference_graph_from_adjacency_lists(&sse_live_ranges);

				//Now we coalesce for the SSE result only. We do this because in theory, we should not
				//be able to do anything else with the other class of registers
				coalescence_result = perform_live_range_coalescence_for_target(function_entry, sse_graph, LIVE_RANGE_CLASS_SSE, debug_printing);

				break;

			//We were able to coalesce *at least one* SSE and *at least one* GP
			//live range, so we need to redo everything
			case COALESCENCE_RESULT_BOTH:
				//Reset all of our live ranges
				reset_all_live_ranges(&general_purpose_live_ranges);
				reset_all_live_ranges(&sse_live_ranges);

				//First step - recalculate all of our used & assigned sets
				recompute_used_and_assigned_sets(function_entry);

				//Redo both of the spill costs
				compute_spill_costs(&general_purpose_live_ranges);
				compute_spill_costs(&sse_live_ranges);

				//Now recompute all liveness sets
				calculate_live_range_liveness_sets(function_entry);

				//Now we build all of our interferences
				calculate_all_interferences_in_function(function_entry);

				//Now rebuild both of the graphs
				general_purpose_graph = construct_interference_graph_from_adjacency_lists(&general_purpose_live_ranges);
				sse_graph = construct_interference_graph_from_adjacency_lists(&sse_live_ranges);

				//And finally - invoke the coalescer again to see what we get
				coalescence_result = perform_live_range_coalescence(function_entry, general_purpose_graph, sse_graph, debug_printing);

				break;

			//Should be unreachable based on the while loop logic
			default:
				break;
		}
	}

	//Show our live ranges once again if requested
	if(print_irs == TRUE){
		print_all_live_ranges(&general_purpose_live_ranges, &sse_live_ranges);
		printf("================= After Coalescing =======================\n");
		print_function_blocks_with_live_ranges(function_entry);
		printf("================= After Coalescing =======================\n");
	}

	/**
	 * STEP 7: Invoke the actual allocator for GP registers
	 *
	 * The allocator will attempt to color the graph. If the graph is not k-colorable, 
	 * then the allocator will spill the least costly LR and return FALSE, and we will go through
	 * this whole process again
	 *
	 * For the actual allocation process, we will do entirely separate allocation loops. We first
	 * look through and attempt to allocate GP registers. Following that, we will then allocate SSE registers
	*/
	u_int8_t colorable_general_purpose = graph_color_and_allocate_general_purpose(function_entry, &general_purpose_live_ranges);

	/**
	 * Our so-called "spill loop" essentially repeats most of the steps
	 * above until we create a colorable graph. Spilling completely disrupts
	 * the interference, use, assignment and liveness properties of the old graph,
	 * so we are required to do most of these steps over again after every spill
	 *
	 * In reality, usually this will only happen once or twice, even in the most extreme 
	 * cases
	 */
	//Keep going so long as we can't color
	while(colorable_general_purpose == FALSE){
		if(print_irs == TRUE){
			printf("============ After Spilling =============== \n");
			print_function_blocks_with_live_ranges(function_entry);
			printf("============ After Spilling =============== \n");
		}

		/**
		 * First reset all of these
		 */
		reset_all_live_ranges(&general_purpose_live_ranges);

		/**
		 * Following this, we need to recompute all of our used and assigned
		 * sets
		 */
		recompute_used_and_assigned_sets(function_entry);

		/**
		 * Now that we've recomputed the used and assigned
		 * sets, we need to go back through and update all of our spill
		 * costs
		 */
		compute_spill_costs(&general_purpose_live_ranges);

		/**
		 * Following that, we need to go through and calculate
		 * all of our liveness sets again
		 */
		calculate_live_range_liveness_sets(function_entry);

		/**
		 * Now we will compute the interferences for our given
		 * set of registers
		 */
		calculate_target_interferences_in_function(function_entry, LIVE_RANGE_CLASS_GEN_PURPOSE);

		/**
		 * Once the liveness sets have been recalculated, we're able
		 * to go through and compute all of the interference again
		 */
		general_purpose_graph = construct_interference_graph_from_adjacency_lists(&general_purpose_live_ranges);

		//Show our live ranges once again if requested
		if(print_irs == TRUE){
			print_all_live_ranges(&general_purpose_live_ranges, &sse_live_ranges);
			printf("================= After Interference =======================\n");
			print_function_blocks_with_live_ranges(function_entry);
			printf("================= After Interference =======================\n");
		}

		/**
		 * And finally - once we have our interference, we are
		 * able to go through and attempt to color once
		 * again. This loop will keep executing until the
		 * graph_color_and_allocate returns a successful result
		 */
		colorable_general_purpose = graph_color_and_allocate_general_purpose(function_entry, &general_purpose_live_ranges);
	}

	
	/**
	 * STEP 8: Invoke the actual allocator for SSE registers
	 *
	 * The allocator will attempt to color the graph. If the graph is not k-colorable, 
	 * then the allocator will spill the least costly LR and return FALSE, and we will go through
	 * this whole process again
	 *
	 * For the actual allocation process, we will do entirely separate allocation loops. We first
	 * look through and attempt to allocate GP registers. Following that, we will then allocate SSE registers
	 * Note that for a lot of functions, having SSE live ranges is not a guarantee. As such, we'll gate
	 * ourselves out here and only do this if it's necessary
	*/
	if(sse_live_ranges.current_index > 0){
		//Initial attempt - this will trigger the spiller if it's not possible
		u_int8_t colorable_sse = graph_color_and_allocate_sse(function_entry, &sse_live_ranges);

		//So long as we could not color the graph
		while(colorable_sse == FALSE){
			//Dev printing only
			if(print_irs == TRUE){
				printf("============ After Spilling =============== \n");
				print_function_blocks_with_live_ranges(function_entry);
				printf("============ After Spilling =============== \n");
			}

			/**
			 * First we need to reset all of the SSE live ranges
			 */
			reset_all_live_ranges(&sse_live_ranges);

			/**
			 * Following that, we can recompute all of our used
			 * and assigned sets
			 */
			recompute_used_and_assigned_sets(function_entry);

			/**
			 * Next we'll recalculate all spill costs for our given
			 * live ranges
			 */
			compute_spill_costs(&sse_live_ranges);

			/**
			 * Following that we'll go through and redo all liveness
			 * for the entire function
			 */
			calculate_live_range_liveness_sets(function_entry);

			/**
			 * Now we'll recalculate all target interferences for the
			 * SSE class of live ranges only
			 */
			calculate_target_interferences_in_function(function_entry, LIVE_RANGE_CLASS_SSE);

			/**
			 * Now we can rebuild our graph from what we had before
			 */
			sse_graph = construct_interference_graph_from_adjacency_lists(&sse_live_ranges);

			//Show our live ranges once again if requested
			if(print_irs == TRUE){
				print_all_live_ranges(&general_purpose_live_ranges, &sse_live_ranges);
				printf("================= After Interference =======================\n");
				print_function_blocks_with_live_ranges(function_entry);
				printf("================= After Interference =======================\n");
			}

			/**
			 * And finally - once we have our interference, we are
			 * able to go through and attempt to color once
			 * again. This loop will keep executing until the
			 * graph_color_and_allocate returns a successful result
			 */
			colorable_sse = graph_color_and_allocate_sse(function_entry, &sse_live_ranges);
		}
	}

	//Destroy both of these now that we're done
	dynamic_array_dealloc(&general_purpose_live_ranges);
	dynamic_array_dealloc(&sse_live_ranges);

	/**
	 * STEP 8: caller/callee saving logic
	 *
	 * Once we make it down here, we have colored the entire graph successfully. But,
	 * we still need to insert any caller/callee saving logic that is needed
	 * when appropriate
	 *
	 * NOTE: We cannot do this at the individual function step because it does require
	 * that we have all functions completely allocated before going forward.
	 */
	insert_saving_logic(cfg, function_entry, function_exit);

	/**
	 * STEP 9: function local stack alignment and stack passed parameter offset updates
	 *
	 * Now that we've done any spilling that we need to do, we'll need to finalize the
	 * local stack for this function. In addition to that, we're also going to need to 
	 * finalize the stack passed parameters(if there are any) because those values
	 * will need to have their stack offsets modified if we did anything to change
	 * how large the stack frame of this function is. This helper function will handle
	 * all of that
	 */
	finalize_local_and_parameter_stack_logic(cfg, function_entry, function_exit);
}


/**
 * Perform our register allocation algorithm on the entire cfg
 */
void allocate_all_registers(compiler_options_t* options, cfg_t* cfg){
	//Save whether or not we want to actually print IRs
	u_int8_t print_irs = options->print_irs;
	u_int8_t print_post_allocation = options->print_post_allocation;

	//Save these in global state
	stack_pointer = cfg->stack_pointer;
	type_symtab = cfg->type_symtab;
	//Load this in for later use
	u64_type = lookup_type_name_only(type_symtab, "u64", NOT_MUTABLE)->type;

	//Construct these two live ranges off the bat - they are evergreen and will be used
	//globally
	stack_pointer_lr = construct_stack_pointer_live_range(stack_pointer);
	instruction_pointer_lr = construct_instruction_pointer_live_range(cfg->instruction_pointer);

	//Run through every function entry block individually and invoke the allocator on
	//all of them separately
	for(u_int32_t i = 0; i < cfg->function_entry_blocks.current_index; i++){
		//Extract the given function entry
		basic_block_t* function_entry = dynamic_array_get_at(&(cfg->function_entry_blocks), i);

		//Also extract the function exit block
		basic_block_t* function_exit = dynamic_array_get_at(&(cfg->function_exit_blocks), i);

		//Invoke the function-level allocator
		allocate_registers_for_function(options, cfg, function_entry, function_exit);
	}

	/**
	 * STEP 10: final cleanup pass
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
