/**
 * Author: Jack Robbins
 *
 * This file contains the implementation for the APIs defined in the header file of the same name
 *
 * The instruction selector for Ollie is what's known as a peephole selector. We crawl the entirety
 * of the generated LLIR(OIR) that we're given. We then simplify various known patterns and finally we convert the resultant 
 * simplified OIR into assembly using a variety of patten matching
*/

#include "instruction_selector.h"
#include "../utils/queue/heap_queue.h"
#include "../utils/constants.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

//We'll need this a lot, so we may as well have it here
static generic_type_t* u64;
static generic_type_t* u32;
static generic_type_t* i32;
static generic_type_t* u8;

//The window for our "sliding window" optimizer
typedef struct instruction_window_t instruction_window_t;


/**
 * Will we be printing these out as instructions or as three address code
 * statements?
 */
typedef enum {
	PRINT_THREE_ADDRESS_CODE,
	PRINT_INSTRUCTION
} instruction_printing_mode_t;


/**
 * The widow that we have here will store three instructions at once. This allows
 * us to look at three instruction patterns at any given time.
 */
struct instruction_window_t{
	//We store three instructions and a status
	instruction_t* instruction1;
	instruction_t* instruction2;
	instruction_t* instruction3;
};


/**
 * Does the block that we're passing in end in a direct(jmp) jump to
 * the very next block. If so, we'll return what block the jump goes to.
 * If not, we'll return null.
 */
static basic_block_t* does_block_end_in_jump(basic_block_t* block){
	//If it's null then leave
	if(block->exit_statement == NULL){
		return NULL;
	}

	//Go based on our type here
	switch(block->exit_statement->statement_type){
		//Direct jump, just use the if block
		case THREE_ADDR_CODE_JUMP_STMT:
			return block->exit_statement->if_block;

		//In a branch statement, the else block is
		//the direct jump
		case THREE_ADDR_CODE_BRANCH_STMT:
			return block->exit_statement->else_block;

		//By default no
		default:
			return NULL;
	}
}


/**
 * The first step in our instruction selector is to get the instructions stored in
 * a straight line in the exact way that we want them to be. This is done with a breadth-first
 * search traversal of the simplified CFG that has been optimized. 
 *
 * One special consideration we'll take is ordering nodes with a given jump next to eachother.
 * For example, if block .L15 ends in a direct jump to .L16, we'll endeavor to have .L16 right
 * after .L15 so that in a later stage, we can eliminate that jump.
 */
static basic_block_t* order_blocks(cfg_t* cfg){
	//We'll first wipe the visited status on this CFG
	reset_visited_status(cfg, TRUE);
	
	//We will perform a breadth first search and use the "direct successor" area
	//of the blocks to store them all in one chain
	
	//The current block
	basic_block_t* previous;
	//The starting point that all traversals will use
	basic_block_t* head_block;

	//Initialize these to null first
	previous = head_block = NULL;
	
	//We'll need to use a queue every time, we may as well just have one big one
	heap_queue_t* queue = heap_queue_alloc();

	//For each function
	for(u_int16_t _ = 0; _ < cfg->function_entry_blocks->current_index; _++){
		//Grab the function block out
		basic_block_t* func_block = dynamic_array_get_at(cfg->function_entry_blocks, _);

		//This function start block is the begging of our BFS	
		enqueue(queue, func_block);
		
		//So long as the queue is not empty
		while(queue_is_empty(queue) == HEAP_QUEUE_NOT_EMPTY){
			//Grab this block off of the queue
			basic_block_t* current = dequeue(queue);

			//If previous is NULL, this is the first block
			if(previous == NULL){
				previous = current;
				//This is also the head block then
				head_block = previous;
			//We need to handle the rare case where we reach two of the same blocks(maybe the block points
			//to itself) but neither have been visited. We make sure that, in this event, we do not set the
			//block to be it's own direct successor
			} else if(previous != current && current->visited == FALSE){
				//We'll add this in as a direct successor
				previous->direct_successor = current;

				//Do we end in a jump?
				basic_block_t* end_jumps_to = does_block_end_in_jump(previous);

				//If we do AND what we're jumping to is the direct successor, then we'll
				//delete the jump statement as it is now unnecessary
				if(end_jumps_to == previous->direct_successor){
					//Get rid of this jump as it's no longer needed
					//delete_statement(previous->exit_statement);
				}

				//Add this in as well
				previous = current;
			}

			//Make sure that we flag this as visited
			current->visited = TRUE;

			//Let's first check for our special case - us jumping to a given block as the very last statement. If
			//this turns back something that isn't null, it'll be the first thing we add in
			basic_block_t* direct_end_jump = does_block_end_in_jump(current);

			//If this is the case, we'll add it in first
			if(direct_end_jump != NULL && direct_end_jump->visited == FALSE){
				//Add it into the queue
				enqueue(queue, direct_end_jump);
			}

			//Now we'll go through each of the successors in this node
			for(u_int16_t idx = 0; current->successors != NULL && idx < current->successors->current_index; idx++){
				//Now as we go through here, if the direct end jump wasn't NULL, we'll have already added it in. We don't
				//want to have that happen again, so we'll make sure that if it's not NULL we don't double add it

				//Grab the successor
				basic_block_t* successor = dynamic_array_get_at(current->successors, idx);

				//If we had that jumping to block case happen, make sure we skip over it to avoid double adding
				if(successor == direct_end_jump){
					continue;
				}

				//If the block is completely empty(function end block), we'll also skip
				if(successor->leader_statement == NULL){
					successor->visited = TRUE;
					continue;
				}

				//Otherwise it's not, so we'll add it in
				if(successor->visited == FALSE){
					enqueue(queue, successor);
				}
			}
		}
	}

	//Destroy the queue when done
	heap_queue_dealloc(queue);

	//Set this for later on
	cfg->head_block = head_block;

	//Give back the head block
	return head_block;
}


/**
 * Print a block our for reading
*/
static void print_ordered_block(basic_block_t* block, instruction_printing_mode_t mode){
	//If this is some kind of switch block, we first print the jump table
	if(block->jump_table != NULL){
		print_jump_table(stdout, block->jump_table);
	}

	//Switch here based on the type of block that we have
	switch(block->block_type){
		//Function entry blocks need extra printing
		case BLOCK_TYPE_FUNC_ENTRY:
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
		//Based on what mode we're given here, we call the appropriate
		//print statement
		if(mode == PRINT_THREE_ADDRESS_CODE){
			//Hand off to printing method
			print_three_addr_code_stmt(stdout, cursor);
		} else {
			print_instruction(stdout, cursor, PRINTING_VAR_IN_INSTRUCTION);
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
static void print_ordered_blocks(cfg_t* cfg, basic_block_t* head_block, instruction_printing_mode_t mode){
	//Run through the direct successors so long as the block is not null
	basic_block_t* current = head_block;

	//So long as this one isn't NULL
	while(current != NULL){
		//Print it
		print_ordered_block(current, mode);
		//Advance to the direct successor
		current = current->direct_successor;
	}

	//Print all global variables after the blocks
	print_all_global_variables(stdout, cfg->global_variables);
}


/**
 * This is used to manage when we need to swap a variable out and handle all use case
 * modifications
 */
static void replace_variable(three_addr_var_t* old, three_addr_var_t* new){
	//Decrement the old one's use count
	old->use_count--;

	//And update the new one's use count
	new->use_count++;
}


/**
 * Take the binary logarithm of something that we already know
 * is a power of 2. 
 *
 * IMPORTANT: This function will *only* work with values that are already
 * known to be powers of 2. If you pass in something that isn't a power of 2,
 * the answer *will* be wrong
 *
 * Here's how this works:
 * Take 8: 1000, which is 2^3
 *
 * 1000 >> 1 = 0100 != 1, current power: 1
 * 0100 >> 1 = 0010 != 1, current power: 2 
 * 0010 >> 1 = 0001 != 1, current power: 3 
 *
 */
static u_int32_t log2_of_known_power_of_2(u_int64_t value){
	//Store a counter, initialize to 0
	u_int32_t counter = 0;

	//So long as we can shift to the left
	while(value != 1){
		//One more power here
		counter++;
		//Go back by 1
		value = value >> 1;
	}

	return counter;
}


/**
 * Take in a constant and update it with its binary log value
 */
static void update_constant_with_log2_value(three_addr_const_t* constant){
	//Switch based on the type
	switch(constant->const_type){
		case INT_CONST:
		case INT_CONST_FORCE_U:
			constant->constant_value.integer_constant = log2_of_known_power_of_2(constant->constant_value.integer_constant);
			break;

		case LONG_CONST:
		case LONG_CONST_FORCE_U:
			constant->constant_value.long_constant = log2_of_known_power_of_2(constant->constant_value.long_constant);
			break;

		case CHAR_CONST:
			constant->constant_value.char_constant = log2_of_known_power_of_2(constant->constant_value.char_constant);
			break;

		//We should never get here
		default:
			break;
	}
}


/**
 * Initialize the instruction window by taking in the first 3 values in the head block
 */
static instruction_window_t initialize_instruction_window(basic_block_t* head){
	//Grab the window
	instruction_window_t window;
	window.instruction1 = NULL;
	window.instruction2 = NULL;
	window.instruction3 = NULL;

	//The first instruction is the leader statement
	window.instruction1 = head->leader_statement;

	//If this is null(possible but rare), just give it back
	if(window.instruction1 == NULL){
		return window;
	}

	//Instruction 2 is next to the head
	window.instruction2 = window.instruction1->next_statement;

	//If this isn't null, 3 is this guy's next one
	if(window.instruction2 != NULL){
		window.instruction3 = window.instruction2->next_statement;
	}
	
	//And now we give back the window
	return window;
}


/**
 * Reconstruct the instruction window after some kind of deletion/reordering
 *
 * The "seed" is always the first instruction, it's what we use to set the rest up
 */
static void reconstruct_window(instruction_window_t* window, instruction_t* seed){
	//Instruction 1 is always the seed
	window->instruction1 = seed;

	//The second one always comes after here
	instruction_t* second = seed->next_statement;

	//This is always the next statement
	window->instruction2 = second;

	//If second is not null, third is just what is after second
	if(second != NULL){
		window->instruction3 = second->next_statement;
	} else {
		window->instruction3 = NULL;
	}
}


/**
 * Advance the window up by 1 instruction. This means that the lowest instruction slides
 * out of our window, and the one next to the highest instruction slides into it
 */
static instruction_window_t* slide_window(instruction_window_t* window){
	//Instruction1 becomes instruction 2
	window->instruction1 = window->instruction2;
	//Instruction2 becomes instruction3
	window->instruction2 = window->instruction3;

	//Instruction3 becomes what's next to instruction 2
	if(window->instruction2 != NULL){
		window->instruction3 = window->instruction2->next_statement;
	} else {
		window->instruction3 = NULL;
	}

	//Give it back
	return window;
}


/**
 * Is an operation valid for token folding? If it is, we'll return true
 * The invalid operations are &&, ||, / and %, and * *when* it is unsigned
 */
static u_int8_t is_operation_valid_for_constant_folding(instruction_t* instruction, three_addr_const_t* constant){
	switch(instruction->op){
		//Division will work for one and a power of 2
		case F_SLASH:
			//If it's 1, then yes we can do this
			if(is_constant_value_one(constant) == TRUE){
				return TRUE;
			}

			//If this is the case, then we are also able to constant fold
			if(is_constant_power_of_2(constant) == TRUE){
				return TRUE;
			}
			
			//Otherwise it won't work
			return FALSE;

		//For modulus, we can only do this when the constant is one. Anything
		//modulo'd by 1 is just 0
		case MOD:
			//If it's 1, then yes we can do this
			if(is_constant_value_one(constant) == TRUE){
				return TRUE;
			}

			//Otherwise it won't work
			return FALSE;

		case STAR:
			//If it's 0, then yes we can do this
			if(is_constant_value_zero(constant) == TRUE){
				return TRUE;
			}

			//If it's 1, then yes we can do this
			if(is_constant_value_one(constant) == TRUE){
				return TRUE;
			}

			//If this is the case, then we are also able to constant fold
			if(is_constant_power_of_2(constant) == TRUE){
				return TRUE;
			}

			/**
			 * Once we make it all the way down here, we no longer have
			 * the chance to do any clever optimizations or use shifting.
			 * If this is an unsigned operation, we'll have to use the
			 * MULL opcode, which only takes one operand. As such, we'll
			 * reject anything that is unsigned for folding
			 */

			//If this is unsigned, we cannot do this
			if(is_type_signed(instruction->assignee->type) == FALSE){
				return FALSE;
			}

			//But if it is signed, we can
			return TRUE;

		default:
			return TRUE;
	}

}


/**
 * Can an assignment statement be optimized away? If the assignment statement
 * involves converting between types, or it involves memory indirection, then
 * we cannot simply remove it
 */
static u_int8_t can_assignment_instruction_be_removed(instruction_t* assignment_instruction){
	//If this is a constant assignment, then yes we can
	if(assignment_instruction->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT){
		return TRUE;
	}

	//Otherwise, we know that we have a regular assignment statement
	//This cannot be optimized away
	if(is_expanding_move_required(assignment_instruction->assignee->type, assignment_instruction->op1->type) == TRUE){
		return FALSE;
	}

	//Otherwise if we get here, then we can
	return TRUE;
}


/**
 * A helper function that selects and returns the appopriately sized move for 
 * a logical and, or or not statement
 */
static instruction_t* emit_appropriate_move_statement(three_addr_var_t* destination, three_addr_var_t* source){
	//We will first compare the sizes and see if a conversion is needed
	if(is_expanding_move_required(destination->type, source->type) == TRUE){
		//Go based on whether or not the type is signed
		if(is_type_signed(destination->type) == TRUE){
			return emit_movsx_instruction(destination, source);
		} else {
			return emit_movzx_instruction(destination, source);
		}
	
	//Otherwise return a regular move instruction
	} else {
		return emit_movX_instruction(destination, source);
	}
}


/**
 * Can we do an inplace constant operation? Currently we only
 * do these for *, + and -
 */
static u_int8_t binary_operator_valid_for_inplace_constant_match(ollie_token_t op){
	switch(op){
		case PLUS:
		case MINUS:
		case STAR:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Remediate a memory address instruction. Note that this will not convert a statement to
 * assembly, it will simply take the memory address instruction and put it into the
 * a different OIR form
 *
 * This needs to handle both stack variables and global variables
 */
static void remediate_memory_address_instruction(cfg_t* cfg, instruction_t* instruction){
	//Grab this out
	symtab_variable_record_t* var = instruction->op1->linked_var;

	//Our most common case - global variables for obvious reasons do not have a stack address
	if(var->membership != GLOBAL_VARIABLE){
		//Extract the stack offset for our use
		u_int32_t stack_offset = var->stack_region->base_address;

		//If this offset is not 0, then we have an operation in the form of
		//"stack_pointer" + stack offset
		if(stack_offset != 0){
			instruction->statement_type = THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT;

			//Addition statement
			instruction->op = PLUS;

			//Emit the offset value
			instruction->op1_const = emit_direct_integer_or_char_constant(stack_offset, u64);

			//And we're offsetting from the stack pointer
			instruction->op1 = cfg->stack_pointer;

		//Otherwise if this is 0, then all we're doing is assigning the stack pointer
		} else {
			//This is an assign statement
			instruction->statement_type = THREE_ADDR_CODE_ASSN_STMT;

			//And the op1 is the stack pointer
			instruction->op1 = cfg->stack_pointer;
		}
	
	//Otherwise it is a global variable, and we will treat it as such
	} else {
		//These will always be lea's
		instruction->instruction_type = LEAQ;

		//Signify that we have a global variable
		instruction->calculation_mode = ADDRESS_CALCULATION_MODE_GLOBAL_VAR;

		//The first address calc register is the instruction pointer
		instruction->address_calc_reg1 = cfg->instruction_pointer;

		//We'll use the at this point ignored op2 slot to hold the value of the offset
		instruction->op2 = instruction->op1;
	}
}


/**
 * The pattern optimizer takes in a window and performs hyperlocal optimzations
 * on passing instructions. If we do end up deleting instructions, we'll need
 * to take care with how that affects the window that we take in
 */
static u_int8_t simplify_window(cfg_t* cfg, instruction_window_t* window){
	//By default, we didn't change anything
	u_int8_t changed = FALSE;

	//Let's perform some quick checks. If we see a window where the first instruction
	//is NULL or the second one is NULL, there's nothing we can do. We'll just leave in this
	//case
	if(window->instruction1 == NULL || window->instruction2 == NULL){
		return changed;
	}

	//Right off the bat, if we see any memory address instructions, now is our time to
	//convert out of them
	if(window->instruction1->statement_type == THREE_ADDR_CODE_MEM_ADDRESS_STMT){
		remediate_memory_address_instruction(cfg, window->instruction1);
	}

	//Same for instruction 2 if we see it
	if(window->instruction2->statement_type == THREE_ADDR_CODE_MEM_ADDRESS_STMT){
		remediate_memory_address_instruction(cfg, window->instruction2);
	}

	//Now we'll match based off of a series of patterns. Depending on the pattern that we
	//see, we perform one small optimization
	
	/**
	 * ================== CONSTANT ASSINGNMENT FOLDING ==========================
	 *
	 * If we see something like this
	 * t2 <- 0x8
	 * x0 <- t2
	 *
	 * We can "fold" to result in:
	 * x0 <- 0x8
	 * 
	 * This will also result in the deletion of the first statement
	 *
	 * This also works with store statements
	 */

	//If we see a constant assingment first and then we see a an assignment
	if(window->instruction1->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT
		&& window->instruction1->assignee->is_temporary == TRUE
		&& window->instruction1->assignee->use_count <= 1
		&& window->instruction2 != NULL
		&& window->instruction2->statement_type == THREE_ADDR_CODE_ASSN_STMT 
		&& variables_equal(window->instruction1->assignee, window->instruction2->op1, FALSE) == TRUE){

		//Grab this out for convenience
		instruction_t* assign_operation = window->instruction2;

		//Now we'll modify this to be an assignment const statement
		assign_operation->op1_const = window->instruction1->op1_const;

		//Modify the type of the assignment
		assign_operation->statement_type = THREE_ADDR_CODE_ASSN_CONST_STMT;

		//The use count here now goes down by one
		assign_operation->op1->use_count--;

		//Make sure that we now null out op1
		assign_operation->op1 = NULL;

		//Once we've done this, the first statement is entirely useless
		delete_statement(window->instruction1);

		//Once we've deleted the statement, we'll need to completely rewire the block
		//The binary operation is now the start
		reconstruct_window(window, assign_operation);
	
		//Whatever happened here, we did change something
		changed = TRUE;
	}

	/**
	 * --------------------- Folding constant assignments in arithmetic expressions ----------------
	 *  In cases where we have a binary operation that is not a BIN_OP_WITH_CONST, but after simplification
	 *  could be, we want to eliminate unnecessary register pressure by having consts directly in the arithmetic expression 
	 *
	 * NOTE: This does not work for division or modulus instructions
	 */
	//Check first with 1 and 2
	if(window->instruction2 != NULL && window->instruction2->statement_type == THREE_ADDR_CODE_BIN_OP_STMT
		&& window->instruction1->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT){
		//Is the variable in instruction 1 temporary *and* the same one that we're using in instrution2? Let's check.
		if(window->instruction1->assignee->is_temporary == TRUE
			//Validate that the use count is less than 1
			&& window->instruction1->assignee->use_count <= 1
			&& is_operation_valid_for_constant_folding(window->instruction2, window->instruction1->op1_const) == TRUE //And it's valid for constant folding
			&& variables_equal(window->instruction1->assignee, window->instruction2->op2, FALSE) == TRUE){
			//If we make it in here, we know that we may have an opportunity to optimize. We simply 
			//Grab this out for convenience
			instruction_t* const_assignment = window->instruction1;

			//Let's mark that this is now a binary op with const statement
			window->instruction2->statement_type = THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT;

			//op2 is now used one less time
			window->instruction2->op2->use_count--;

			//We'll want to NULL out the secondary variable in the operation
			window->instruction2->op2 = NULL;
			
			//We'll replace it with the op1 const that we've gotten from the prior instruction
			window->instruction2->op1_const = const_assignment->op1_const;

			//We can now delete the very first statement
			delete_statement(window->instruction1);

			//Reconstruct the window with instruction2 as the start
			reconstruct_window(window, window->instruction1);

			//This does count as a change
			changed = TRUE;
		}
	}

	//Now check with 1 and 3. The prior compression may have made this more worthwhile
	if(window->instruction3 != NULL && window->instruction3->statement_type == THREE_ADDR_CODE_BIN_OP_STMT
		&& window->instruction1->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT){
		//Is the variable in instruction 1 temporary *and* the same one that we're using in instrution2? Let's check.
		if(window->instruction1->assignee->is_temporary == TRUE
			//Validate that this is not being used more than once
			&& window->instruction1->assignee->use_count <= 1
			&& is_operation_valid_for_constant_folding(window->instruction3, window->instruction1->op1_const) == TRUE //And it's valid for constant folding
			&& variables_equal(window->instruction2->assignee, window->instruction3->op2, FALSE) == FALSE
			&& variables_equal(window->instruction1->assignee, window->instruction3->op2, FALSE) == TRUE){
			//If we make it in here, we know that we may have an opportunity to optimize. We simply 
			//Grab this out for convenience
			instruction_t* const_assignment = window->instruction1;

			//Let's mark that this is now a binary op with const statement
			window->instruction3->statement_type = THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT;

			//Op2 for instruction3 is now used one less time
			window->instruction3->op2->use_count--;

			//We'll want to NULL out the secondary variable in the operation
			window->instruction3->op2 = NULL;
			
			//We'll replace it with the op1 const that we've gotten from the prior instruction
			window->instruction3->op1_const = const_assignment->op1_const;

			//We can now delete the very first statement
			delete_statement(window->instruction1);

			//Reconstruct the window with instruction2 as the seed
			reconstruct_window(window, window->instruction2);

			//This does count as a change
			changed = TRUE;
		}
	}


	/**
	 * ================= Handling pure constant operations ========================
	 * t27 <- 5
	 * t27 <- t27 (+/-/star(*)) 68
	 *
	 * Can become: t27 <- 340
	 */
	if(window->instruction1->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT 
		&& window->instruction2 != NULL
		&& window->instruction2->statement_type == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT
		&& binary_operator_valid_for_inplace_constant_match(window->instruction2->op) == TRUE
		&& window->instruction1->assignee->is_temporary == TRUE
		&& variables_equal(window->instruction2->op1, window->instruction1->assignee, FALSE) == TRUE){

		//Go based on the op. We already know that we can do this by the time 
		//we get here
		switch(window->instruction2->op){
			case STAR:
				//We can multiply the constants now. The result will be stored in op1 const
				multiply_constants(window->instruction2->op1_const, window->instruction1->op1_const);
				break;

			case PLUS:
				//We can add the constants now. The result will be stored in op1 const
				add_constants(window->instruction2->op1_const, window->instruction1->op1_const);
				break;
			
			case MINUS:
				//Important caveat here. The constant above is the first one that 
				subtract_constants(window->instruction1->op1_const, window->instruction2->op1_const);
				break;

				//Overwrite with op1
				window->instruction2->op1_const = window->instruction1->op1_const;

			//Unreachable - just so the compiler won't complain
			default:
				break;
		}


		//Instruction 2 is now simply an assign const statement
		window->instruction2->statement_type = THREE_ADDR_CODE_ASSN_CONST_STMT;

		//Op1 is now used one less time
		window->instruction2->op1->use_count--;

		//Null out where the old value was
		window->instruction2->op1 = NULL;

		//Instruction 1 is now completely useless *if* that was the only time that
		//his assignee was used. Otherwise, we need to keep it in
		if(window->instruction1->assignee->use_count == 0){
			delete_statement(window->instruction1);
		}

		//Reconstruct the window with instruction 2 as the start
		reconstruct_window(window, window->instruction2);

		//This counts as a change
		changed = TRUE;
	}


	/**
	 * ================= Handling pure constant operations ========================
	 * t27 <- 5
	 * t27 <- t27 (+/-/star(*)) 68
	 *
	 * Can become: t27 <- 340
	 *
	 * This is the same as above, but for 2 & 3
	 */
	if(window->instruction2->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT 
		&& window->instruction3 != NULL
		&& window->instruction3->statement_type == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT
		&& binary_operator_valid_for_inplace_constant_match(window->instruction3->op) == TRUE
		&& window->instruction2->assignee->is_temporary == TRUE
		&& variables_equal(window->instruction3->op1, window->instruction2->assignee, FALSE) == TRUE){

		//Go based on the op. We already know that we can do this by the time 
		//we get here
		switch(window->instruction3->op){
			case STAR:
				//We can multiply the constants now. The result will be stored in op1 const
				multiply_constants(window->instruction3->op1_const, window->instruction2->op1_const);
				break;

			case PLUS:
				//We can add the constants now. The result will be stored in op1 const
				add_constants(window->instruction3->op1_const, window->instruction2->op1_const);
				break;
			
			case MINUS:
				//Important caveat here. The constant above is the first one that 
				subtract_constants(window->instruction2->op1_const, window->instruction3->op1_const);
				break;

				//Overwrite with op1
				window->instruction3->op1_const = window->instruction2->op1_const;

			//Unreachable - just so the compiler won't complain
			default:
				break;
		}


		//Instruction 2 is now simply an assign const statement
		window->instruction3->statement_type = THREE_ADDR_CODE_ASSN_CONST_STMT;

		//Op1 is now used one less time
		window->instruction3->op1->use_count--;

		//Null out where the old value was
		window->instruction3->op1 = NULL;

		//Instruction 1 is now completely useless *if* that was the only time that
		//his assignee was used. Otherwise, we need to keep it in
		if(window->instruction2->assignee->use_count == 0){
			delete_statement(window->instruction2);
		}

		//Reconstruct the window with instruction 2 as the start
		reconstruct_window(window, window->instruction3);

		//This counts as a change
		changed = TRUE;
	}


	/**
	 * --------------------- Redundnant copying elimination ------------------------------------
	 *  Let's now fold redundant copies. Here is an example of a redundant copy
	 * 	t10 <- x_2
	 * 	t11 <- t10
	 *
	 * This can be folded into simply:
	 * 	t11 <- x_2
	 */
	//If we have two consecutive assignment statements
	if(window->instruction2 != NULL 
		&& window->instruction2->statement_type == THREE_ADDR_CODE_ASSN_STMT 
		&& window->instruction1->statement_type == THREE_ADDR_CODE_ASSN_STMT
		&& can_assignment_instruction_be_removed(window->instruction1) == TRUE
		&& can_assignment_instruction_be_removed(window->instruction2) == TRUE){
		//Grab these out for convenience
		instruction_t* first = window->instruction1;
		instruction_t* second = window->instruction2;
		
		//If the variables are temp and the first one's assignee is the same as the second's op1, we can fold
		if(first->assignee->is_temporary == TRUE && variables_equal(first->assignee, second->op1, TRUE) == TRUE
			//And the assignee of the first statement is only ever used once
			&& first->assignee->use_count <= 1){

			//Manage our use state here
			replace_variable(second->op1, first->op1);

			//Reorder the op1's
			second->op1 = first->op1;

			//We can now delete the first statement
			delete_statement(first);

			//Reconstruct the window with second as the start
			reconstruct_window(window, second);
				
			//Regardless of what happened, we did see a change here
			changed = TRUE;
		}
	}

	/**
	 * --------------------- Redundnant copying elimination with loads ------------------------------------
	 *  Let's now fold redundant copies. Here is an example of a redundant copy
	 * 	load t10 <- x_2
	 * 	t11 <- t10
	 *
	 * This can be folded into simply:
	 * load t11 <- x_2
	 */
	//If we have two consecutive assignment statements
	if(window->instruction2 != NULL 
		&& window->instruction2->statement_type == THREE_ADDR_CODE_ASSN_STMT 
		&& window->instruction1->statement_type == THREE_ADDR_CODE_LOAD_STATEMENT
		&& can_assignment_instruction_be_removed(window->instruction2) == TRUE){
		//Grab these out for convenience
		instruction_t* load = window->instruction1;
		instruction_t* move = window->instruction2;
		
		//If the variables are temp and the first one's assignee is the same as the second's op1, we can fold
		if(load->assignee->is_temporary == TRUE && variables_equal(load->assignee, move->op1, TRUE) == TRUE
			//And the load's assignee is only ever used once
			&& load->assignee->use_count <= 1){

			//The load's assignee now is the move's assignee
			load->assignee = move->assignee;

			//The second move is now useless
			delete_statement(move);

			//Reconstruct our window based around the load
			reconstruct_window(window, load);
				
			//Regardless of what happened, we did see a change here
			changed = TRUE;
		}
	}

	/**
	 * ==================== Op1 Assignment Folding for expressions ======================
	 * If we have expressions like:
	 *   t3 <- x_0
	 *   t4 <- y_0
	 *   t5 <- t3 && t4
	 *
	 *  We need to recognize opportunities for assignment folding. And ideal optimization would transform
	 *  this into:
	 *   
	 *   t5 <- x_0 && y_0
	 *
	 *   This rule will do the first part of that
	 *
	 *  We will seek to do that in this optimization
	 *
	 *
	 *  Note that this is just for op1 assignment folding. It is generall much more restrictive
	 *  because so many operations overwrite their op1(think add, subtract), and those would therefore
	 *  be *invalid* for this
	 */
	//Check first with 1 and 2. We need a binary operation that has a comparison operator in it
	if(is_instruction_binary_operation(window->instruction2) == TRUE
		&& window->instruction1->statement_type == THREE_ADDR_CODE_ASSN_STMT
		&& is_operation_valid_for_op1_assignment_folding(window->instruction2->op) == TRUE){

		//Is the variable in instruction 1 temporary *and* the same one that we're using in instruction1? Let's check.
		if(window->instruction1->assignee->is_temporary == TRUE 
			//Make sure that this is the only use
			&& window->instruction1->assignee->use_count <= 1
			&& window->instruction1->op1->is_temporary == FALSE
			&& variables_equal(window->instruction1->assignee, window->instruction2->op1, FALSE) == TRUE){

			//Set these two to be equal
			window->instruction2->op1 = window->instruction1->op1;

			//We can now delete the very first statement
			delete_statement(window->instruction1);

			//Reconstruct the window with instruction2 as the seed
			reconstruct_window(window, window->instruction2);

			//This does count as a change
			changed = TRUE;
		}
	}


	/**
	 * -------------------- Arithmetic expressions with assignee the same as op1 ---------------------
	 *  There will be times where we generate arithmetic expressions like this:
	 *
	 * 	t19 <- a_3
	 * 	t20 <- t19 + y_0
	 * 	a_4 <- t20
	 *
	 *  Since a_4 and a_3 are the same variable(register), this is an ideal candidate to be compressed
	 *  like
	 *
	 *  a_4 <- a_3 + y_0
	 *
	 * This 3-instruction large optimizaion will look for this
	 */
	//If the first statement is an assignmehnt statement, and the second statement is a binary operation,
	//and the third statement is an assignment statement, we have our chance to optimize
	if(window->instruction1->statement_type == THREE_ADDR_CODE_ASSN_STMT
		&& is_instruction_binary_operation(window->instruction2) == TRUE
		&& window->instruction3 != NULL
		&& window->instruction3->statement_type == THREE_ADDR_CODE_ASSN_STMT){

		//Grab these out for convenience
		instruction_t* first = window->instruction1;
		instruction_t* second = window->instruction2;
		instruction_t* third = window->instruction3;

		//We still need further checks to see if this is indeed the pattern above. If
		//we survive all of these checks, we know that we're set to optimize
		if(first->assignee->is_temporary == TRUE && third->assignee->is_temporary == FALSE &&
			first->assignee->use_count <= 2 &&
			variables_equal_no_ssa(first->op1, third->assignee, FALSE) == TRUE &&
	 		variables_equal(first->assignee, second->op1, FALSE) == TRUE &&
	 		variables_equal(second->assignee, third->op1, FALSE) == TRUE){

			//Manage our use state here
			replace_variable(second->op1, first->op1);

			//The second op1 will now become the first op1
			second->op1 = first->op1;

			//And the second's assignee will now be the third's assignee
			second->assignee = third->assignee;

			//Following this, all we need to do is delete and rearrange
			delete_statement(first);
			delete_statement(third);

			//Reconstruct the window with second as the new instruction1
			reconstruct_window(window, second);

			//Regardless of what happened, we did change the window, so we'll
			//update this
			changed = TRUE;
		}
	}
	

	/**
	 * --------------------- Folding constant assingments in LEA statements ----------------------
	 *  In cases where we have a lea statement that uses a constant which is assigned to a temporary
	 *  variable right before it, we should eliminate that unnecessary assingment by folding that constant
	 *  into the lea statement.
	 *
	 *  We can also go one further by performing the said multiplication to get out the value that we want
	 *
	 * NOTE: This will actually produce invalid binary operation instructions in the short run. However, 
	 * when the instruction selector gets to them, we will turn them into memory move operations
	 */
	if(window->instruction2 != NULL && window->instruction2->statement_type == THREE_ADDR_CODE_LEA_STMT
		&& window->instruction1->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT){
		//If the first instruction's assignee is temporary and it matches the lea statement, then we have a match
		if(window->instruction1->assignee->is_temporary == TRUE &&
	 		variables_equal(window->instruction1->assignee, window->instruction2->op2, FALSE) == TRUE){

			//What we can do is rewrite the LEA statement all together as a simple addition statement. We'll
			//evaluate the multiplication of the constant and lea multiplicator at comptime
			u_int64_t address_offset = window->instruction2->lea_multiplicator;

			//Let's now grab what the constant is
			three_addr_const_t* constant = window->instruction1->op1_const;
			
			//What kind of constant do we have?
			if(constant->const_type == INT_CONST || constant->const_type == INT_CONST_FORCE_U){
				//If this is a the case, we'll multiply the address const by the int value
				address_offset *= constant->constant_value.integer_constant;
			//Otherwise, this has to be a long const
			} else {
				address_offset *= constant->constant_value.long_constant;
			}

			//Once we've done this, the address offset is now properly multiplied. We'll reuse
			//the constant from operation one, and convert the lea statement into a BIN_OP_WITH_CONST
			//statement. This saves a lot of loading and arithmetic operations
		
			//This is now a long const
			constant->const_type = LONG_CONST;

			//Set this to be the address offset
			constant->constant_value.long_constant = address_offset;

			//Add it into instruction 2
			window->instruction2->op1_const = constant;

			//We'll now transfrom instruction 2 into a bin op with const
			window->instruction2->op2 = NULL;
			window->instruction2->op = PLUS;
			window->instruction2->statement_type = THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT;

			//We can now scrap the first instruction entirely
			delete_statement(window->instruction1);

			//Reconstruct the window. Instruction 2 is the new start
			reconstruct_window(window, window->instruction2);

			//This counts as a change 
			changed = TRUE;
		}
	}


	/**
	 * =================== Adjacent assignment statement folding ====================
	 * If we have a binary operation or a bin op with const statement followed by an
	 * assignment of that result to a non-temporary variable, we have an opportunity
	 * to simplify. This would lead nicely into the following optimizations below
	 *
	 * Example: 
	 * t12 <- a_2 + 0x1
	 * a_3 <- t12
	 * could become
	 * a_3 <- a_2 + 0x1
	 */

	//If the first instruction is a binary operation and the immediately following instruction is an assignment
	//operation, this is a potential match
	if(is_instruction_binary_operation(window->instruction1) == TRUE
		&& window->instruction2 != NULL
		&& window->instruction2->statement_type == THREE_ADDR_CODE_ASSN_STMT){
		//For convenience/memory ease
		instruction_t* first = window->instruction1;
		instruction_t* second = window->instruction2;

		//If we have a temporary start variable, a non temp end variable, and the variables
		//match in the corresponding spots, we have our opportunity
		if(first->assignee->is_temporary == TRUE && second->assignee->is_temporary == FALSE
			&& variables_equal(first->assignee, second->op1, FALSE) == TRUE
			//Special no-ssa comparison, we expect that the ssa would be different due to assignment levels
			&& variables_equal_no_ssa(second->assignee, first->op1, FALSE) == TRUE){

			//We will now take the second variables assignee to be the first statements assignee
			first->assignee = second->assignee;

			//That's really all we need to do for the first one. Now, we need to delete the second
			//statement entirely
			delete_statement(second);

			//We'll reconstruct the window here, the first is still the first
			reconstruct_window(window, first);

			//Regardless of what happened here, we did change the window so we'll set the flag
			changed = TRUE;

		//We could also have a scenario like this that will apply only to logical combination(&& and ||) operators.
		/**
		 * t33 <- t34 && t35
		 * x_0 <- t33
		 *
		 * Because of the way that we handle logical and, we can actuall eliminate the second assignment
		 * with no issue
		 * x_0 <- t34 && t35
		 *
		 * NOTE: This does not work for logical or, due to the way we handle logical OR
		 */
		} else if(first->op == DOUBLE_AND
				&& first->assignee->is_temporary == TRUE 
				&& variables_equal(first->assignee, second->op1, FALSE) == TRUE){

			//Set these to be equal
			first->assignee = second->assignee;

			//We can now scrap the second statement
			delete_statement(second);

			//We'll reconstruct the window here, the first is still the first
			reconstruct_window(window, first);

			//This counts as a change
			changed = TRUE;
		}
	}


	/**
	 * ==================== On-the-fly logical and/or ========================
	 * t27 <- 5
	 * t27 <- t27 && 68
	 *
	 * t27 <- 1
	 *
	 * Or
	 * t27 <- 5
	 * t27 <- t27 || 68
	 *
	 * t27 <- 1
	 */
	if(window->instruction1->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT 
		&& window->instruction2 != NULL
		&& window->instruction2->statement_type == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT
		&& (window->instruction2->op == DOUBLE_AND || window->instruction2->op == DOUBLE_OR)
		&& variables_equal(window->instruction2->op1, window->instruction1->assignee, FALSE) == TRUE){

		//We will handle the constants accordingly
		if(window->instruction2->op == DOUBLE_OR) {
			logical_or_constants(window->instruction2->op1_const, window->instruction1->op1_const);
		} else {
			logical_and_constants(window->instruction2->op1_const, window->instruction1->op1_const);
		}

		//Instruction 2 is now simply an assign const statement
		window->instruction2->statement_type = THREE_ADDR_CODE_ASSN_CONST_STMT;

		//Op1 is now used one less time
		window->instruction2->op1->use_count--;

		//Null out where the old value was
		window->instruction2->op1 = NULL;

		//Instruction 1 is now completely useless *if* that was the only time that
		//his assignee was used. Otherwise, we need to keep it in
		if(window->instruction1->assignee->use_count == 0){
			delete_statement(window->instruction1);
		}

		//Reconstruct the window with instruction 2 as the start
		reconstruct_window(window, window->instruction2);

		//This counts as a change
		changed = TRUE;
	}


	/**
	 * ======================= Logical And operation simplifying ==========================
	 * t2 <- t4 && 0 === set t2 to be 0
	 * t2 <- t4 && (non-zero) === test t4,t4 and setne t2 if t4 isn't 0
	 *
	 * It is safe to assume that a logical and binary operation with constant is always
	 * simplifiable
	 */
	if(window->instruction1->statement_type == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT
		&& window->instruction1->op == DOUBLE_AND){
		//For convenience extract this
		instruction_t* current_instruction = window->instruction1;

		//First option - the value is 0
		if(is_constant_value_zero(current_instruction->op1_const) == TRUE){
			//It's now just an assign statement
			current_instruction->statement_type = THREE_ADDR_CODE_ASSN_CONST_STMT;

			//Wipe out op1
			if(current_instruction->op1 != NULL){
				current_instruction->op1->use_count--;
				current_instruction->op1 = NULL;
			}

		//Otherwise, the value is not 0
		} else {
			//First we add a test instruction
			instruction_t* test_instruction = test_instruction = emit_test_statement(emit_temp_var(u8), current_instruction->op1, current_instruction->op1);
						
			//The result of this will be used for our set instruction
			instruction_t* setne_instruction = emit_setne_code(emit_temp_var(u8));
			//Subtly hook this in, even though it's not needed
			setne_instruction->op1 = test_instruction->assignee;

			//Assign the two over
			instruction_t* assignment = emit_assignment_instruction(current_instruction->assignee, setne_instruction->assignee);

			//Insert these both in beforehand
			insert_instruction_before_given(test_instruction, current_instruction);
			insert_instruction_before_given(setne_instruction, current_instruction);
			insert_instruction_before_given(assignment, current_instruction);

			//And then remove this now useless current instruction
			delete_statement(current_instruction);

			//Reconstruct the window based on the set instruction
			reconstruct_window(window, assignment);
		}

		//We changed something
		changed = TRUE;
	}


	/**
	 * ======================= Logical OR operation simplifying ==========================
	 * t2 <- t4 || 0 === test t4, t4 and setne t2 if t4 isn't 0
	 * t2 <- t4 || (non-zero) === t2 <- 1
	 *
	 * It is safe to assume that a logical and binary operation with constant is always
	 * simplifiable
	 */
	if(window->instruction1->statement_type == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT
		&& window->instruction1->op == DOUBLE_OR){
		//For convenience extract this
		instruction_t* current_instruction = window->instruction1;

		//First option - the value is 0. If it is, then anything else is irrelevant
		if(is_constant_value_zero(current_instruction->op1_const) == TRUE){
			//First we add a test instruction
			instruction_t* test_instruction = test_instruction = emit_test_statement(emit_temp_var(u8), current_instruction->op1, current_instruction->op1);
						
			//The result of this will be used for our set instruction
			instruction_t* setne_instruction = emit_setne_code(emit_temp_var(u8));
			//Subtly hook this in, even though it's not needed
			setne_instruction->op1 = test_instruction->assignee;

			//Assign the two over
			instruction_t* assignment = emit_assignment_instruction(current_instruction->assignee, setne_instruction->assignee);

			//Insert these both in beforehand
			insert_instruction_before_given(test_instruction, current_instruction);
			insert_instruction_before_given(setne_instruction, current_instruction);
			insert_instruction_before_given(assignment, current_instruction);

			//And then remove this now useless current instruction
			delete_statement(current_instruction);

			//Reconstruct the window based on the set instruction
			reconstruct_window(window, assignment);

		//Otherwise, the value is not 0
		} else {
			//It's now just an assign statement
			current_instruction->statement_type = THREE_ADDR_CODE_ASSN_CONST_STMT;

			//Wipe out op1
			if(current_instruction->op1 != NULL){
				current_instruction->op1->use_count--;
				current_instruction->op1 = NULL;
			}

			//Set the constant's value to 1
			current_instruction->op1_const->constant_value.long_constant = 1;
		}

		//We changed something
		changed = TRUE;
	}



	/**
	 * ================== Arithmetic Operation Simplifying ==========================
	 * After we do all of this folding, we can stand to ask the question of if we 
	 * have any simple arithmetic operations that can be folded together. Our first
	 * example of this is arithmetic operations with 0
	 *
	 * There are many cases here which allow simplification. Here are the first
	 * few with zero:
	 *
	 * t2 <- t4 + 0 can just become t2 <- t4
	 * t2 <- t4 - 0 can just become t2 <- t4
	 * t2 <- t4 * 0 can just become t2 <- 0 
	 * t2 <- t4 / 0 will stay the same, but we will produce an error
	 * 
	 * Logical operators(somewhat different)
	 * t2 <- t4 || 0 === test t4,t4 and setne t2(if t4 isn't 0)
	 * t2 <- t4 || (non-zero) === set t2 to be 1
	 * 
	 *
	 * These may seem trivial, but this is not so uncommon when we're doing address calculation
	 */

	//Shove these all into an array for selecting
	instruction_t* instructions[3] = {window->instruction1, window->instruction2, window->instruction3};

	//The current instruction pointer
	instruction_t* current_instruction;

	//If we have a bin op with const statement, we have an opportunity
	for(u_int16_t i = 0; i < 3; i++){
		//Grab the current instruction out
		current_instruction = instructions[i];

		//Skip if NULL
		if(current_instruction == NULL){
			continue;
		}

		/**
		 * We have a chance to do some optimizations if we see a BIN_OP_WITH_CONST. It will
		 * have been primed for us by the constant folding portion. Now, we'll search and see
		 * if we're able to simplify some instructions
		 */
		if(current_instruction->statement_type == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT){
			//If it isn't in the list here, we won't be considering it and as such shouldn't waste time
			//processing
			switch(current_instruction->op){
				case PLUS:
				case R_SHIFT:
				case L_SHIFT:
				case MINUS:
				case STAR:
				case F_SLASH:
				case MOD:
					break;
				default:
					continue;
			}

			//Grab this out for convenience
			three_addr_const_t* constant = current_instruction->op1_const;

			//If this is 0, then we can optimize
			if(is_constant_value_zero(constant) == TRUE){
				//Switch based on current instruction's op
				switch(current_instruction->op){
					//If we made it out of this conditional with the flag being set, we can simplify.
					//If this is the case, then this just becomes a regular assignment expression
					case PLUS:
					case MINUS:
					case L_SHIFT:
					case R_SHIFT:
						//We're just assigning here
						current_instruction->statement_type = THREE_ADDR_CODE_ASSN_STMT;
						//Wipe the values out
						current_instruction->op1_const = NULL;

						//Also scrap the op
						current_instruction->op = BLANK;

						//We changed something
						changed = TRUE;

						break;

					case STAR:
						//Now we're assigning a const
						current_instruction->statement_type = THREE_ADDR_CODE_ASSN_CONST_STMT;

						//The constant is still the same thing(0), let's just wipe out the ops
						if(current_instruction->op1 != NULL){
							current_instruction->op1->use_count--;
							current_instruction->op1 = NULL;
						}

						//We changed something
						changed = TRUE;

						break;

					//Just do nothing here
					default:
						break;
				}


			//Notice how we do NOT mark any change as true here. This is because, even though yes we
			//did change the instructions, the sliding window itself did not change at all. This is
			//an important note as if we did mark a change, there are cases where this could
			//cause an infinite loop
			
			//What if this is a 1? Well if it is, we can transform this statement into an inc or dec statement
			//if it's addition or subtraction, or we can turn it into a simple assignment statement if it's multiplication
			//or division
			} else if(is_constant_value_one(constant) == TRUE){
				//Switch based on the op in the current instruction
				switch(current_instruction->op){
					//If it's an addition statement, turn it into an inc statement
					/**
				 	* NOTE: for addition and subtraction, since we'll be turning this into inc/dec statements, we'll
				 	* want to first make sure that the assignees are not temporary variables. If they are temporary variables,
				 	* then doing this would mess the whole operation up
				 	*/
					case PLUS:
						//If it's temporary, we jump out
						if(current_instruction->assignee->is_temporary == TRUE){
							break;
						}

						//Now turn it into an inc statement
						current_instruction->statement_type = THREE_ADDR_CODE_INC_STMT;
						//Wipe the values out
						current_instruction->op1_const = NULL;
						current_instruction->op = BLANK;
						//We changed something
						changed = TRUE;

						break;

					case MINUS:
						//If it's temporary, we jump out
						if(current_instruction->assignee->is_temporary == TRUE){
							break;
						}

						//Change what the class is
						current_instruction->statement_type = THREE_ADDR_CODE_DEC_STMT;
						//Wipe the values out
						current_instruction->op1_const = NULL;
						current_instruction->op = BLANK;
						//We changed something
						changed = TRUE;

						break;

					//These are both the same - handle a 1 multiply, 1 divide
					case STAR:
					case F_SLASH:
						//Change it to a regular assignment statement
						current_instruction->statement_type = THREE_ADDR_CODE_ASSN_STMT;
						//Wipe the operator out
						current_instruction->op1_const = NULL;
						current_instruction->op = BLANK;
						//We changed something
						changed = TRUE;

						break;

					//Modulo by 1 will always result in 0
					case MOD:
						//Change it to a regular assignment statement
						current_instruction->statement_type = THREE_ADDR_CODE_ASSN_CONST_STMT;

						//This is blank
						current_instruction->op = BLANK;

						//We no longer even need our op1
						if(current_instruction->op1 != NULL){
							current_instruction->op1->use_count--;
							current_instruction->op1 = NULL;
						}

						//We can modify op1 const to just be 0 now. This is lazy but it
						//works, we'll just 0 out all 64 bits
						current_instruction->op1_const->constant_value.long_constant = 0;

						//We changed something
						changed = TRUE;

					//Just bail out
					default:
						break;
				}

			//What if we have a power of 2 here? For any kind of multiplication or division, this can
			//be optimized into a left or right shift if we have a compatible type(not a float)
			} else if(is_constant_power_of_2(constant) == TRUE){
				//If we have a star that's a left shift
				if(current_instruction->op == STAR){
					//Multiplication is a left shift
					current_instruction->op = L_SHIFT;
					//Update the constant with its log2 value
					update_constant_with_log2_value(current_instruction->op1_const);
					//We changed something
					changed = TRUE;

				} else if(current_instruction->op == F_SLASH){
					//Division is a right shift
					current_instruction->op = R_SHIFT;
					//Update the constant with its log2 value
					update_constant_with_log2_value(current_instruction->op1_const);
					//We changed something
					changed = TRUE;
				}
			}
		}
	}

	/**
	 * ================== Simplifying Consecutive Binary Operation with Constant statements ==============
	 * Here is an example:
	 * t2 <- arr_0 + 24
	 * t4 <- t2 + 4
	 * 
	 * We could turn this into
	 * t4 <- arr_0 + 28
	 *
	 * This is incredibly common with array address calculations, which is why we do it. We focus on the special case
	 * of two consecutive additions here for this reason. Any other two consecutive operations are usually quite uncommon
	 */
	//If instructions 1 and 2 are both BIN_OP_WITH_CONST
	if(window->instruction2 != NULL && window->instruction2->statement_type == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT
		&& window->instruction2->op == PLUS && window->instruction1->statement_type == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT 
		&& window->instruction1->op == PLUS){

		//Let's do this for convenience
		instruction_t* first = window->instruction1;
		instruction_t* second = window->instruction2;

		//Calculate this for now in case we need it
		generic_type_t* final_type = types_assignable(second->op1_const->type, first->op1_const->type);

		//If these are the same variable and the types are compatible, then we're good to go
		if(variables_equal(first->assignee, second->op1, FALSE) == TRUE && final_type != NULL){
			//What we'll do first is add the two constants. The resultant constant will be stored
			//in the second instruction's constant
			add_constants(second->op1_const, first->op1_const);

			//Manage our use state here
			replace_variable(second->op1, first->op1);

			//Now that we've done that, we'll modify the second equation's op1 to be the first equation's op1
			second->op1 = first->op1;

			//Now that this is done, we can remove the first equation
			delete_statement(first);

			//We'll reconstruct the window with the second instruction being the
			//first instruction now
			reconstruct_window(window, second);

			//This counts as a change because we deleted
			changed = TRUE;
		}
	}

	/**
	 * There is a chance that we could be left with statements that assign to themselves
	 * like this:
	 *  t11 <- t11
	 *
	 *  These are guaranteed to be useless, so we can eliminate them
	 */
	if(window->instruction1 != NULL && window->instruction1->statement_type == THREE_ADDR_CODE_ASSN_STMT
		//If we get here, we have a temp assignment who is completely useless, so we delete
		&& window->instruction1->assignee->is_temporary == TRUE
		&& variables_equal(window->instruction1->assignee, window->instruction1->op1, FALSE) == TRUE){

		//Delete it
		delete_statement(window->instruction1);

		//Rebuild now based on instruction2
		reconstruct_window(window, window->instruction2);

		//Counts as a change
		changed = TRUE;
	}


	/**
	 * There is a chance that we could be left with statements that assign to themselves
	 * like this:
	 *  t11 <- 2
	 *
	 *  Where t11 has no real usage at all. Since this is the case, we can eliminate the whole
	 *  operation
	 *
	 *  These are guaranteed to be useless, so we can eliminate them
	 */
	if(window->instruction1 != NULL && window->instruction1->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT
		//If we get here, we have a temp assignment who is completely useless, so we delete
		&& window->instruction1->assignee->is_temporary == TRUE
		//Ensure that it's not being used at all
		&& window->instruction1->assignee->use_count == 0){

		//Delete it
		delete_statement(window->instruction1);

		//Rebuild now based on instruction2
		reconstruct_window(window, window->instruction2);

		//Counts as a change
		changed = TRUE;
	}


	/**
	 * If we have a memory address statement where the stack address is 0, we can
	 * simply make this into an assignment instruction. We don't need the normal lea
	 * that others have
	 */
	if(window->instruction1->statement_type == THREE_ADDR_CODE_MEM_ADDRESS_STMT
		// Ignore global vars, they don't have stack addresses
		&& window->instruction1->op1->linked_var->membership != GLOBAL_VARIABLE){
		//We can reorgnaize this into an assignment instruction
		if(window->instruction1->op1->stack_region->base_address == 0){
			//Reset the type
			window->instruction1->statement_type = THREE_ADDR_CODE_ASSN_STMT;
			//The op1 is now the stack pointer
			window->instruction1->op1 = cfg->stack_pointer;
		}

		//Slide the window after we do this, we don't need to look at statement 1 anymore
		slide_window(window);

		//Counts as a change
		changed = TRUE;
	}


	/**
	 * Optimize loads with variable offsets into one's that have constant offsets
	 *
	 * We'll take something like:
	 * t3 <- 4
	 * load t5 <- t4[t3]
	 *
	 * And make it:
	 *
	 * load t5 <- t4[4]
	 */
	if(window->instruction1->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT
		&& window->instruction1->assignee->is_temporary == TRUE
		&& window->instruction1->assignee->use_count == 1 //Use count is just for here
		&& window->instruction2->statement_type == THREE_ADDR_CODE_LOAD_WITH_VARIABLE_OFFSET
		&& variables_equal(window->instruction1->assignee, window->instruction2->op2, FALSE) == TRUE){

		//This is now a load with constant offset
		window->instruction2->statement_type = THREE_ADDR_CODE_LOAD_WITH_CONSTANT_OFFSET;

		//We don't want to have this in here anymore
		window->instruction2->op2 = NULL;

		//Copy their constants over
		window->instruction2->offset = window->instruction1->op1_const;

		//We can delete the entire assignment statement
		delete_statement(window->instruction1);

		//Reconstruct the window now based on instruction2
		reconstruct_window(window, window->instruction2);

		//This counts as change
		changed = TRUE;
	}


	/**
	 * Optimize loads with variable offsets into one's that have constant offsets
	 *
	 * We'll take something like:
	 * t3 <- 4
	 * store t5[t3] <- t4
	 *
	 * And make it:
	 *
	 * store t5[4] <- t4
	 */
	if(window->instruction1->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT
		&& window->instruction1->assignee->is_temporary == TRUE
		&& window->instruction1->assignee->use_count == 1 //Use count is just for here
		&& window->instruction2->statement_type == THREE_ADDR_CODE_STORE_WITH_VARIABLE_OFFSET 
		&& variables_equal(window->instruction1->assignee, window->instruction2->op1, FALSE) == TRUE){

		//This is now a store with constant offset
		window->instruction2->statement_type = THREE_ADDR_CODE_STORE_WITH_CONSTANT_OFFSET;

		//We don't want to have this in here anymore
		window->instruction2->op1 = NULL;

		//Copy their constants over
		window->instruction2->offset = window->instruction1->op1_const;

		//We can delete the entire assignment statement
		delete_statement(window->instruction1);

		//Reconstruct the window now based on instruction2
		reconstruct_window(window, window->instruction2);

		//This counts as change
		changed = TRUE;
	}


	/**
	 * Optimize constant offset loads with a 0 offset into regular loads
	 *
	 * This:
	 * 	load t4 <- t3[0]
	 *
	 * can become
	 *  load t4 <- t3
	 */
	if(window->instruction1->statement_type == THREE_ADDR_CODE_LOAD_WITH_CONSTANT_OFFSET
		&& is_constant_value_zero(window->instruction1->offset) == TRUE){
		//First NULL out the constant
		window->instruction1->offset = NULL;

		//Then just make this a normal load
		window->instruction1->statement_type = THREE_ADDR_CODE_LOAD_STATEMENT;

		//Counts as a change
		changed = TRUE;
	}


	/**
	 * Optimize constant offset stores with a 0 offset into regular stores
	 *
	 * This:
	 * 	store t4[0] <- t3
	 *
	 * can become
	 *  store t4 <- t3
	 */
	if(window->instruction1->statement_type == THREE_ADDR_CODE_STORE_WITH_CONSTANT_OFFSET 
		&& is_constant_value_zero(window->instruction1->offset) == TRUE){
		//First NULL out the constant
		window->instruction1->offset = NULL;

		//Slight adjustment as well, the op1's in complex stores are not the source but in regular
		//stores they are, so we'll copy that over
		window->instruction1->op1 = window->instruction1->op2;

		//NULL out op2
		window->instruction1->op2 = NULL;

		//Then just make this a normal load
		window->instruction1->statement_type = THREE_ADDR_CODE_STORE_STATEMENT;

		//Counts as a change
		changed = TRUE;
	}

	//Return whether or not we changed the block return changed;
	return changed;
}


/**
 * Make one pass through the sliding window for simplification. This could include folding,
 * etc. Simplification happens first over the entirety of the OIR using the sliding window
 * technique. Following this, the instruction selector runs over the same area
 */
static u_int8_t simplifier_pass(cfg_t* cfg, basic_block_t* head){
	//First we'll grab the head
	basic_block_t* current = head;

	u_int8_t window_changed = FALSE;
	u_int8_t changed;

	//So long as this isn't NULL
	while(current != NULL){
		changed = FALSE;
		
		//Initialize the sliding window(very basic, more to come)
		instruction_window_t window = initialize_instruction_window(current);

		//Run through and simplify everything we can
		do{
			//Simplify the window
			changed = simplify_window(cfg, &window);

			//Set this flag if it was changed
			if(changed == TRUE){
				window_changed = TRUE;
			}

			//And slide it
			slide_window(&window);

		//So long as we aren't at the end
		} while(window.instruction1 != NULL);


		//Advance to the direct successor
		current = current->direct_successor;
	}

	//Give back whether or not it's been changed
	return window_changed;
}


/**
 * We'll make use of a while change algorithm here. We make passes
 * until we see the first pass where we experience no chnage at all.
 */
static void simplify(cfg_t* cfg, basic_block_t* head){
	while(simplifier_pass(cfg, head) == TRUE);
}


/**
 * Emit a converting move instruction directly, with no need to do instruction selection afterwards
 */
static instruction_t* emit_converting_move_instruction_direct(three_addr_var_t* destination, three_addr_var_t* source){
	//Allocate it
	instruction_t* converting_move = calloc(1, sizeof(instruction_t));

	//Select type based on signedness
	if(is_type_signed(destination->type) == TRUE){
		converting_move->instruction_type = MOVSX;
	} else {
		converting_move->instruction_type = MOVZX;
	}

	//Now load in the registers
	converting_move->destination_register = destination;
	converting_move->source_register = source;

	//And return it
	return converting_move;
}


/**
 * Handle a converting move operation and return the variable that results from it. This function will also handle the implicit conversion
 * between 32 bit and unsigned 64 bit integers
 */
static three_addr_var_t* handle_expanding_move_operation(instruction_t* after_instruction, three_addr_var_t* source, generic_type_t* desired_type){
	//A generic holder for our assignee
	three_addr_var_t* assignee;

	//Is the desired type a 64 bit integer *and* the source type a U32 or I32? If this is the case, then 
	//movzx functions are actually invalid because x86 processors operating in 64 bit mode automatically
	//zero pad when 32 bit moves happen
	if(is_type_unsigned_64_bit(desired_type) == TRUE && is_type_32_bit_int(source->type) == TRUE){
		//Emit a variable copy of the source
		assignee = emit_var_copy(source);

		//Reassign it's type to be the desired type
		assignee->type = desired_type;

		//Select the size appropriately after the type is reassigned
		assignee->variable_size = get_type_size(assignee->type);

	//Otherwise we have a normal case here
	} else {
		//Emit the assignment instruction
		instruction_t* instruction = emit_converting_move_instruction_direct(emit_temp_var(desired_type), source);

		//Now we'll insert it before the "after instruction"
		insert_instruction_before_given(instruction, after_instruction);

		//Reassign the destination register
		assignee = instruction->destination_register;
	}

	//Give back the assignee, in whatever form it may be
	return assignee;
}


/**
 * Simple utility for us to print out an instruction window in its three address code
 * (before instruction selection) format
 */
static void print_instruction_window_three_address_code(instruction_window_t* window){
	printf("----------- Instruction Window ------------\n");
	//We'll just print out all three instructions
	if(window->instruction1 != NULL){
		print_three_addr_code_stmt(stdout, window->instruction1);
	} else {
		printf("EMPTY\n");
	}

	if(window->instruction2 != NULL){
		print_three_addr_code_stmt(stdout, window->instruction2);
	} else {
		printf("EMPTY\n");
	}
	
	if(window->instruction3 != NULL){
		print_three_addr_code_stmt(stdout, window->instruction3);
	} else {
		printf("EMPTY\n");
	}

	printf("-------------------------------------------\n");
}


/**
 * Simple utility for us to print out an instruction window in the post
 * instruction selection format
 */
static void print_instruction_window(instruction_window_t* window){
	printf("----------- Instruction Window ------------\n");
	//We'll just print out all three instructions
	if(window->instruction1 != NULL){
		print_instruction(stdout, window->instruction1, PRINTING_VAR_IN_INSTRUCTION);
	} else {
		printf("EMPTY\n");
	}

	if(window->instruction2 != NULL){
		print_instruction(stdout, window->instruction2, PRINTING_VAR_IN_INSTRUCTION);
	} else {
		printf("EMPTY\n");
	}
	
	if(window->instruction3 != NULL){
		print_instruction(stdout, window->instruction3, PRINTING_VAR_IN_INSTRUCTION);
	} else {
		printf("EMPTY\n");
	}

	printf("-------------------------------------------\n");
}


/**
 * Emit a conversion instruction for division preparation
 *
 * We use this during the process of emitting division instructions
 *
 * NOTE: This is only needed for signed division
 */
static instruction_t* emit_conversion_instruction(three_addr_var_t* converted){
	//First we'll allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//We'll need the size to select the appropriate instruction
	variable_size_t size = get_type_size(converted->type);

	switch(size){
		case QUAD_WORD:
			instruction->instruction_type = CQTO;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = CLTD;
			break;
		case WORD:
			instruction->instruction_type = CWTL;
			break;
		case BYTE:
			instruction->instruction_type = CBTW;
			break;
		default:
			break;
	}

	//The source register is the so-called "converted" register. In reality,
	//this will always be %rax or a lower bit field in it
	instruction->source_register = converted;

	//There are 2 destinations here, it will always take the value in %rax and convert it to %rdx:%rax
	instruction->destination_register = emit_temp_var(converted->type);
	instruction->destination_register2 = emit_temp_var(converted->type);

	//There are actually 2 destination registers here. They occupy RDX:RAX respectively

	//And now we'll give it back
	return instruction;
}


/**
 * Emit a sete instruction
 *
 * The sete instruction is used on a byte
 */
static instruction_t* emit_sete_instruction(three_addr_var_t* destination){
	//First we'll allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//And we'll set the class
	instruction->instruction_type = SETE;

	//Finally we set the destination
	instruction->destination_register = destination;

	//And now we'll give it back
	return instruction;
}


/**
 * Emit a setne instruction
 *
 * The setne instruction is used on a byte
 */
static instruction_t* emit_setne_instruction(three_addr_var_t* destination){
	//First we'll allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//And we'll set the class
	instruction->instruction_type = SETNE;

	//Finally we set the destination
	instruction->destination_register = destination;

	//And now we'll give it back
	return instruction;
}


/**
 * Emit an ANDx instruction
 */
static instruction_t* emit_and_instruction(three_addr_var_t* destination, three_addr_var_t* source){
	//First we'll allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//We'll need the size of the variable
	variable_size_t size = get_type_size(destination->type);

	switch(size){
		case QUAD_WORD:	
			instruction->instruction_type = ANDQ;
			break;
		case DOUBLE_WORD:	
			instruction->instruction_type = ANDL;
			break;
		case WORD:	
			instruction->instruction_type = ANDW;
			break;
		case BYTE:	
			instruction->instruction_type = ANDB;
			break;
		default:
			break;
	}

	//Finally we set the destination
	instruction->destination_register = destination;
	instruction->source_register = source;

	//And now we'll give it back
	return instruction;
}


/**
 * Emit an ORx instruction
 */
static instruction_t* emit_or_instruction(three_addr_var_t* destination, three_addr_var_t* source){
	//First we'll allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//We'll need the size of the variable
	variable_size_t size = get_type_size(destination->type);

	switch(size){
		case QUAD_WORD:	
			instruction->instruction_type = ORQ;
			break;
		case DOUBLE_WORD:	
			instruction->instruction_type = ORL;
			break;
		case WORD:	
			instruction->instruction_type = ORW;
			break;
		case BYTE:	
			instruction->instruction_type = ORB;
			break;
		default:
			break;
	}

	//Finally we set the destination
	instruction->destination_register = destination;
	instruction->source_register = source;

	//And now we'll give it back
	return instruction;
}


/**
 * Emit a divX or idivX instruction
 *
 * Division instructions have no destination that need be written out. They only have two sources - a direct
 * source and an implicit source
 */
static instruction_t* emit_div_instruction(three_addr_var_t* assignee, three_addr_var_t* divisor, three_addr_var_t* dividend, three_addr_var_t* higher_order_dividend_bits, u_int8_t is_signed){
	//First we'll allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//We set the size based on the destination 
	variable_size_t size = get_type_size(assignee->type);

	//Now we'll decide this based on size and signedness
	switch (size) {
		case BYTE:
			if(is_signed == TRUE){
				instruction->instruction_type = IDIVB;
			} else {
				instruction->instruction_type = DIVB;
			}
			break;

		case WORD:
			if(is_signed == TRUE){
				instruction->instruction_type = IDIVW;
			} else {
				instruction->instruction_type = DIVW;
			}
			break;

		case DOUBLE_WORD:
			if(is_signed == TRUE){
				instruction->instruction_type = IDIVL;
			} else {
				instruction->instruction_type = DIVL;
			}
			break;

		case QUAD_WORD:
			if(is_signed == TRUE){
				instruction->instruction_type = IDIVQ;
			} else {
				instruction->instruction_type = DIVQ;
			}
			break;

		//Should never reach this
		default:
			break;
	}

	//Finally we set the sources
	instruction->source_register = divisor;
	//This implicit source is important for our uses in the register allocator
	instruction->source_register2 = dividend;
	//We will use address calc reg 1 for this purpose
	instruction->address_calc_reg1 = higher_order_dividend_bits;

	//Quotient register
	instruction->destination_register = emit_temp_var(assignee->type);
	//Remainder register
	instruction->destination_register2 = emit_temp_var(assignee->type);

	//And now we'll give it back
	return instruction;
}


/**
 * A very simple helper function that selects the right move instruction based
 * solely on variable size. Done to avoid code duplication
 */
static instruction_type_t select_move_instruction(variable_size_t size){
	//Go based on size
	switch(size){
		case BYTE:
			return MOVB;
		case WORD:
			return MOVW;
		case DOUBLE_WORD:
			return MOVL;
		case QUAD_WORD:
			return MOVQ;
		default:
			return MOVQ;
	}
}


/**
 * Select a register movement instruction based on the source and destination sizes
 */
static instruction_type_t select_register_movement_instruction(variable_size_t destination_size, variable_size_t source_size, u_int8_t is_signed){
	//If these are the exact same, then we can just call the helper and be done
	if(destination_size == source_size){
		return select_move_instruction(destination_size);
	} 

	//However, if they're not the same, we'll need some kind of converting move here
	if(is_signed == TRUE){
		return MOVSX;
	} else {
		return MOVZX;
	}
}


/**
 * A very simple helper function that selects the right add instruction based
 * solely on variable size. Done to avoid code duplication
 */
static instruction_type_t select_add_instruction(variable_size_t size){
	//Go based on size
	switch(size){
		case BYTE:
			return ADDB;
		case WORD:
			return ADDW;
		case DOUBLE_WORD:
			return ADDL;
		case QUAD_WORD:
			return ADDQ;
		default:
			return ADDQ;
	}
}


/**
 * A very simple helper function that selects the right lea instruction based
 * solely on variable size. Done to avoid code duplication
 */
static instruction_type_t select_lea_instruction(variable_size_t size){
	//Go based on size
	switch(size){
		case BYTE:
		case WORD:
			return LEAW;
		case DOUBLE_WORD:
			return LEAL;
		case QUAD_WORD:
			return LEAQ;
		default:
			return LEAQ;
	}
}


/**
 * A very simple helper function that selects the right add instruction based
 * solely on variable size. Done to avoid code duplication
 */
static instruction_type_t select_sub_instruction(variable_size_t size){
	//Go based on size
	switch(size){
		case BYTE:
			return SUBB;
		case WORD:
			return SUBW;
		case DOUBLE_WORD:
			return SUBL;
		case QUAD_WORD:
			return SUBQ;
		default:
			return SUBQ;
	}
}


/**
 * A very simple helper function that selects the right add instruction based
 * solely on variable size. Done to avoid code duplication
 */
static instruction_type_t select_cmp_instruction(variable_size_t size){
	//Go based on size
	switch(size){
		case BYTE:
			return CMPB;
		case WORD:
			return CMPW;
		case DOUBLE_WORD:
			return CMPL;
		case QUAD_WORD:
			return CMPQ;
		default:
			return CMPQ;
	}
}




/**
 * Handle the case where we can condense three instructions into one big address calculation for a to-memory move
 * t7 <- arr_0 + 340
 * t8 <- t7 + arg_0 * 4
 * (t8) <- 3
 *
 * Should become
 * mov(w/l/q) $3, 340(arr_0, arg_0, 4)

 *
 * NOTE: Does not do deletion or reordering
 */
static void handle_three_instruction_address_calc_to_memory_move(instruction_t* offset_calc, instruction_t* lea_statement, instruction_t* memory_access){
	//Select the variable size
	variable_size_t size;

	//Select the size based on what we're moving in
	if(memory_access->op1 != NULL){
		size = get_type_size(memory_access->op1->type);
	} else {
		size = get_type_size(memory_access->op1_const->type);
	}

	//Now based on the size, we can select what variety to register/immediate to memory move we have here
	switch (size) {
		case BYTE:
			memory_access->instruction_type = REG_TO_MEM_MOVB;
			break;
		case WORD:
			memory_access->instruction_type = REG_TO_MEM_MOVW;
			break;
		case DOUBLE_WORD:
			memory_access->instruction_type = REG_TO_MEM_MOVL;
			break;
		case QUAD_WORD:
			memory_access->instruction_type = REG_TO_MEM_MOVQ;
			break;
		//WE DO NOT DO FLOATS YET
		default:
			memory_access->instruction_type = REG_TO_MEM_MOVQ;
			break;
	}

	//Set the address calculation mode
	memory_access->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_OFFSET_AND_SCALE;
	
	//The offset comes from the first instruction
	memory_access->offset = offset_calc->op1_const;

	//Pull these out for our uses
	three_addr_var_t* address_calc_reg1 = offset_calc->op1;
	three_addr_var_t* address_calc_reg2 = lea_statement->op2;

	//Do we need any conversion for reg1?
	if(is_type_address_calculation_compatible(address_calc_reg1->type) == FALSE){
		//Reassign what reg1 is
		address_calc_reg1 = handle_expanding_move_operation(memory_access, address_calc_reg1, u64);
	}

	//Same exact treatment for reg2
	if(is_type_address_calculation_compatible(address_calc_reg2->type) == FALSE){
		address_calc_reg2 = handle_expanding_move_operation(memory_access, address_calc_reg2, u64);
	}

	//The first operand also comes from the first instruction
	memory_access->address_calc_reg1 = address_calc_reg1;

	//The second instruction gives us the second register and lea offset
	memory_access->address_calc_reg2 = address_calc_reg2;
	
	//There is nothing typewise to worry about with this
	memory_access->lea_multiplicator = lea_statement->lea_multiplicator;

	//Now we'll set the sources that we have
	if(memory_access->op1 != NULL){
		memory_access->source_register = memory_access->op1;
	} else {
		memory_access->source_immediate = memory_access->op1_const;
	}

	//And that's all
}


/**
 * Handle the case where we can condense three instructions into one big address calculation for a to-memory move
 * t7 <- arr_0 + 340
 * t8 <- t7 + arg_0 * 4
 * t9 <- (t8)
 *
 * Should become
 * mov(w/l/q) 340(arr_0, arg_0, 4), t9
 *
 * NOTE: Does not do deletion or reordering
 */
static void handle_three_instruction_address_calc_from_memory_move(instruction_t* offset_calc, instruction_t* lea_statement, instruction_t* memory_access){
	//We'll first select the variable size based on the destination
	variable_size_t size = get_type_size(memory_access->assignee->type);

	//Now based on the size, we can select what variety to register/immediate to memory move we have here
	switch (size) {
		case BYTE:
			memory_access->instruction_type = MEM_TO_REG_MOVB;
			break;
		case WORD:
			memory_access->instruction_type = MEM_TO_REG_MOVW;
			break;
		case DOUBLE_WORD:
			memory_access->instruction_type = MEM_TO_REG_MOVL;
			break;
		case QUAD_WORD:
			memory_access->instruction_type = MEM_TO_REG_MOVQ;
			break;
		//WE DO NOT DO FLOATS YET
		default:
			memory_access->instruction_type = MEM_TO_REG_MOVQ;
			break;
	}

	//Set the calculation mode
	memory_access->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_OFFSET_AND_SCALE;

	//Pull these out for our uses. We'll want to first determine if we need any conversions here
	three_addr_var_t* address_calc_reg1 = offset_calc->op1;
	three_addr_var_t* address_calc_reg2 = lea_statement->op2;
	
	//Do we need any conversion for reg1?
	if(is_type_address_calculation_compatible(address_calc_reg1->type) == FALSE){
		//Reassign what reg1 is
		address_calc_reg1 = handle_expanding_move_operation(memory_access, address_calc_reg1, u64);
	}

	//Same exact treatment for reg2
	if(is_type_address_calculation_compatible(address_calc_reg2->type) == FALSE){
		address_calc_reg2 = handle_expanding_move_operation(memory_access, address_calc_reg2, u64);
	}

	//The offset and first register come from the offset calculation
	memory_access->offset = offset_calc->op1_const;
	memory_access->address_calc_reg1 = address_calc_reg1;
	memory_access->address_calc_reg2 = address_calc_reg2;
	memory_access->lea_multiplicator = lea_statement->lea_multiplicator;

	//Now we'll set the destination register. We don't need to worry about any
	//immediate values here, because we can't load into an immediate
	memory_access->destination_register = memory_access->assignee;

	//And we're set
}


/**
 * t26 <- arr_0 + t25
 * t28 <- t26 + 8
 * t29 <- (t28)
 *
 * Should become
 * mov(w/l/q) 8(arr_0, t25), t29
 */
static void handle_three_instruction_registers_and_offset_only_from_memory_move(instruction_t* additive_statement, instruction_t* offset_calc, instruction_t* memory_access){
	//Need these for analysis, declaring here
	three_addr_var_t* address_calc_reg1;
	three_addr_var_t* address_calc_reg2;

	//Let's first decide what the appropriate move instruction would be
	//We'll first select the variable size based on the destination
	variable_size_t size = get_type_size(memory_access->assignee->type);

	//Now based on the size, we can select what variety to register/immediate to memory move we have here
	switch (size) {
		case BYTE:
			memory_access->instruction_type = MEM_TO_REG_MOVB;
			break;
		case WORD:
			memory_access->instruction_type = MEM_TO_REG_MOVW;
			break;
		case DOUBLE_WORD:
			memory_access->instruction_type = MEM_TO_REG_MOVL;
			break;
		case QUAD_WORD:
			memory_access->instruction_type = MEM_TO_REG_MOVQ;
			break;
		//WE DO NOT DO FLOATS YET
		default:
			memory_access->instruction_type = MEM_TO_REG_MOVQ;
			break;
	}

	//Extract these two values out for analysis
	address_calc_reg1 = additive_statement->op1;
	address_calc_reg2 = additive_statement->op2;

	//Do we need any conversion for reg1?
	if(is_type_address_calculation_compatible(address_calc_reg1->type) == FALSE){
		//Reassign what reg1 is
		address_calc_reg1 = handle_expanding_move_operation(memory_access, address_calc_reg1, u64);
	}

	//Same exact treatment for reg2
	if(is_type_address_calculation_compatible(address_calc_reg2->type) == FALSE){
		address_calc_reg2 = handle_expanding_move_operation(memory_access, address_calc_reg2, u64);
	}

	//Now that we've done any needed conversions, we can reassign
	memory_access->address_calc_reg1 = address_calc_reg1;
	memory_access->address_calc_reg2 = address_calc_reg2;

	//We'll get the offset from offset_calc
	memory_access->offset = offset_calc->op1_const;

	//Now we can put in the address calculation type
	memory_access->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_AND_OFFSET;

	//And finally, we'll set the destination appropriately
	memory_access->destination_register = memory_access->assignee;
}


/**
 * This is the to-memory equivalent of above
 *
 * t26 <- arr_0 + t25
 * t28 <- t26 + 8
 * (t28) <- 3
 *
 * Should become
 * mov(w/l/q) $3, 8(arr_0, t25)
 */
static void handle_three_instruction_registers_and_offset_only_to_memory_move(instruction_t* additive_statement, instruction_t* offset_calc, instruction_t* memory_access){
	//Declare these two holders for analysis later on
	three_addr_var_t* address_calc_reg1;
	three_addr_var_t* address_calc_reg2;

	//Let's first decide the variable size based on what we'll be moving in
	variable_size_t size;

	//Use the op1 if it's there
	if(memory_access->op1 != NULL){
		size = get_type_size(memory_access->op1->type);
	//Otherwise we need to use the constant
	} else {
		size = get_type_size(memory_access->op1_const->type);
	}

	//Now based on the size, we can select what variety to register/immediate to memory move we have here
	switch (size) {
		case BYTE:
			memory_access->instruction_type = REG_TO_MEM_MOVB;
			break;
		case WORD:
			memory_access->instruction_type = REG_TO_MEM_MOVW;
			break;
		case DOUBLE_WORD:
			memory_access->instruction_type = REG_TO_MEM_MOVL;
			break;
		case QUAD_WORD:
			memory_access->instruction_type = REG_TO_MEM_MOVQ;
			break;
		//WE DO NOT DO FLOATS YET
		default:
			memory_access->instruction_type = REG_TO_MEM_MOVQ;
			break;
	}
	
	//Let's pull these two values out to see what we're dealing with
	address_calc_reg1 = additive_statement->op1;
	address_calc_reg2 = additive_statement->op2;

	//Do we need any conversion for reg1?
	if(is_type_address_calculation_compatible(address_calc_reg1->type) == FALSE){
		//Reassign what reg1 is
		address_calc_reg1 = handle_expanding_move_operation(memory_access, address_calc_reg1, u64);
	}

	//Same exact treatment for reg2
	if(is_type_address_calculation_compatible(address_calc_reg2->type) == FALSE){
		address_calc_reg2 = handle_expanding_move_operation(memory_access, address_calc_reg2, u64);
	}

	//We'll get the first and second register from the additive statement
	memory_access->address_calc_reg1 = address_calc_reg1;
	memory_access->address_calc_reg2 = address_calc_reg2;

	//Now we can put in the address calculation type
	memory_access->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_AND_OFFSET;

	//We'll get the offset from offset_calc
	memory_access->offset = offset_calc->op1_const;

	//Now we'll set the source/source immediate based on what we're given
	//Use the op1 if it's there
	if(memory_access->op1 != NULL){
		memory_access->source_register = memory_access->op1;

	//Otherwise we need to use the constant
	} else {
		memory_access->source_immediate = memory_access->op1_const;
	}
}


/**
 * Emit a byte copy of a given source variable. This will be used for the left and right shift instructions
 * where the shift amount needed is itself a byte
 */
static three_addr_var_t* emit_byte_copy_of_variable(three_addr_var_t* source){
	//We'll want a direct copy of this variable
	three_addr_var_t* copy = emit_var_copy(source);

	//We'll then want to modify the type and size
	copy->variable_size = BYTE;
	//This one's type will be a u8
	copy->type = u8;

	//Finally give back the copy
	return copy;
}


/**
 * Handle a left shift operation. In doing a left shift, we account
 * for the possibility that we have a signed value
 */
static void handle_left_shift_instruction(instruction_t* instruction){
	//Is this a signed or unsigned instruction?
	u_int8_t is_signed = is_type_signed(instruction->assignee->type);

	//We'll also need the size of the variable
	variable_size_t size = get_type_size(instruction->assignee->type);

	switch (size) {
		case BYTE:
			if(is_signed == TRUE){
				instruction->instruction_type = SALB;
			} else {
				instruction->instruction_type = SHLB;
			}
			break;
		case WORD:
			if(is_signed == TRUE){
				instruction->instruction_type = SALW;
			} else {
				instruction->instruction_type = SHLW;
			}
			break;
		case DOUBLE_WORD:
			if(is_signed == TRUE){
				instruction->instruction_type = SALL;
			} else {
				instruction->instruction_type = SHLL;
			}
			break;
		//Everything else falls here
		default:
			if(is_signed == TRUE){
				instruction->instruction_type = SALQ;
			} else {
				instruction->instruction_type = SHLQ;
			}
			break;		
	}

	//Now we'll move over the operands
	instruction->destination_register = instruction->assignee;
	
	//We can have an immediate value or we can have a register
	if(instruction->op2 != NULL){
		//If this is a function parameter, we'll need to emit a copy instruction
		//here for the eventual precolorer to use. If we don't do this, the precolorer
		//will clash because it doesn't know whether to use the parameter register or
		//the %ecx register that shift operands must be in. This is a unique case for shifting
		//due to a quirk of x86
		if(instruction->op2->parameter_number > 0){
			//Move it on over here
			instruction_t* copy_instruction = emit_movX_instruction(emit_temp_var(instruction->op2->type), instruction->op2);
			//Add this instruction to our block
			insert_instruction_before_given(copy_instruction, instruction);

			//Now our op2 is really this one's assignee
			instruction->op2 = copy_instruction->destination_register;
		}
	
		//We will always emit the byte copy version of the source register here
		instruction->source_register = emit_byte_copy_of_variable(instruction->op2);

	//Otherwise we have an immediate source
	} else {
		instruction->source_immediate = instruction->op1_const;
	}
}


/**
 * Handle a right shift operation. This helper will determine if we
 * need an arithmetic or a logical right shift dependant on the operation
 */
static void handle_right_shift_instruction(instruction_t* instruction){
	//Is this a signed or unsigned instruction?
	u_int8_t is_signed = is_type_signed(instruction->assignee->type);

	//We'll also need the size of the variable
	variable_size_t size = get_type_size(instruction->assignee->type);

	switch (size) {
		case BYTE:
			if(is_signed == TRUE){
				instruction->instruction_type = SARB;
			} else {
				instruction->instruction_type = SHRB;
			}
			break;
		case WORD:
			if(is_signed == TRUE){
				instruction->instruction_type = SARW;
			} else {
				instruction->instruction_type = SHRW;
			}
			break;
		case DOUBLE_WORD:
			if(is_signed == TRUE){
				instruction->instruction_type = SARL;
			} else {
				instruction->instruction_type = SHRL;
			}
			break;
		//Everything else falls here
		default:
			if(is_signed == TRUE){
				instruction->instruction_type = SARQ;
			} else {
				instruction->instruction_type = SHRQ;
			}
			break;		
	}

	//Now we'll move over the operands
	instruction->destination_register = instruction->assignee;

	//We can have an immediate value or we can have a register
	if(instruction->op2 != NULL){
		//If this is a function parameter, we'll need to emit a copy instruction
		//here for the eventual precolorer to use. If we don't do this, the precolorer
		//will clash because it doesn't know whether to use the parameter register or
		//the %ecx register that shift operands must be in. This is a unique case for shifting
		//due to a quirk of x86
		if(instruction->op2->parameter_number > 0){
			//Move it on over here
			instruction_t* copy_instruction = emit_movX_instruction(emit_temp_var(instruction->op2->type), instruction->op2);
			//Add this instruction to our block
			insert_instruction_before_given(copy_instruction, instruction);

			//Now our op2 is really this one's assignee
			instruction->op2 = copy_instruction->destination_register;
		}

		//We will always emit the byte copy version of the source register here
		instruction->source_register = emit_byte_copy_of_variable(instruction->op2);

	//Otherwise we have an immediate source
	} else {
		instruction->source_immediate = instruction->op1_const;
	}
}


/**
 * Handle a bitwise inclusive or operation
 */
static void handle_bitwise_inclusive_or_instruction(instruction_t* instruction){
	//We need to know what size we're dealing with
	variable_size_t size = get_type_size(instruction->assignee->type);

	switch(size){
		case QUAD_WORD:
			instruction->instruction_type = ORQ;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = ORL;
			break;
		case WORD:
			instruction->instruction_type = ORW;
			break;
		case BYTE:
			instruction->instruction_type = ORB;
			break;
		default:
			break;
	}

	//And we always have a destination register
	instruction->destination_register = instruction->assignee;

	//We can have an immediate value or we can have a register
	if(instruction->op2 != NULL){
		//If we need to convert, we'll do that here
		if(is_expanding_move_required(instruction->assignee->type, instruction->op2->type) == TRUE){
			instruction->source_register = handle_expanding_move_operation(instruction, instruction->op2, instruction->assignee->type);

		//Otherwise it's a direct translation
		} else {
			instruction->source_register = instruction->op2;
		}

	//Otherwise we have an immediate source
	} else {
		instruction->source_immediate = instruction->op1_const;
	}
}


/**
 * Handle a bitwise and operation
 */
static void handle_bitwise_and_instruction(instruction_t* instruction){
	//We need to know what size we're dealing with
	variable_size_t size = get_type_size(instruction->assignee->type);

	switch(size){
		case QUAD_WORD:
			instruction->instruction_type = ANDQ;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = ANDL;
			break;
		case WORD:
			instruction->instruction_type = ANDW;
			break;
		case BYTE:
			instruction->instruction_type = ANDB;
			break;
		default:
			break;
	}

	//And we always have a destination register
	instruction->destination_register = instruction->assignee;

	//We can have an immediate value or we can have a register
	if(instruction->op2 != NULL){
		//If we need to convert, we'll do that here
		if(is_expanding_move_required(instruction->assignee->type, instruction->op2->type) == TRUE){
			instruction->source_register = handle_expanding_move_operation(instruction, instruction->op2, instruction->assignee->type);

		//Otherwise it's a direct translation
		} else {
			instruction->source_register = instruction->op2;
		}

	//Otherwise we have an immediate source
	} else {
		instruction->source_immediate = instruction->op1_const;
	}
}


/**
 * Handle a bitwise exclusive or operation
 */
static void handle_bitwise_exclusive_or_instruction(instruction_t* instruction){
	//We need to know what size we're dealing with
	variable_size_t size = get_type_size(instruction->assignee->type);

	switch(size){
		case QUAD_WORD:
			instruction->instruction_type = XORQ;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = XORL;
			break;
		case WORD:
			instruction->instruction_type = XORW;
			break;
		case BYTE:
			instruction->instruction_type = XORB;
			break;
		default:
			break;
	}
	
	//And we always have a destination register
	instruction->destination_register = instruction->assignee;

	//We can have an immediate value or we can have a register
	if(instruction->op2 != NULL){
		//If we need to convert, we'll do that here
		if(is_expanding_move_required(instruction->assignee->type, instruction->op2->type) == TRUE){
			instruction->source_register = handle_expanding_move_operation(instruction, instruction->op2, instruction->assignee->type);

		//Otherwise it's a direct translation
		} else {
			instruction->source_register = instruction->op2;
		}

	//Otherwise we have an immediate source
	} else {
		instruction->source_immediate = instruction->op1_const;
	}
}


/**
 * Handle a cmp operation. This is used whenever we have
 * relational operation
 */
static void handle_cmp_instruction(instruction_t* instruction){
	//Determine what our size is off the bat
	variable_size_t size = get_type_size(instruction->op1->type);

	//Select this instruction
	instruction->instruction_type = select_cmp_instruction(size);

	//Extract these for convenience
	generic_type_t* left_hand_type = instruction->op1->type;
	//For the right hand type, we only care if op2 isn't NULL. Constants won't affect us here
	generic_type_t* right_hand_type = instruction->op2 != NULL ? instruction->op2->type : left_hand_type;
	
	//Since we have a comparison instruction, we don't actually have a destination
	//register as the registers remain unmodified in this event
	if(is_expanding_move_required(right_hand_type, instruction->op1->type) == TRUE){
		//Let the helper deal with it
		instruction->source_register = handle_expanding_move_operation(instruction, instruction->op1, right_hand_type);
	} else {
		//Otherwise we assign directly
		instruction->source_register = instruction->op1;
	}

	//If we have op2, we'll use source_register2
	if(instruction->op2 != NULL){
		if(is_expanding_move_required(left_hand_type, instruction->op2->type) == TRUE){
			//Let the helper deal with it
			instruction->source_register2 = handle_expanding_move_operation(instruction, instruction->op2, left_hand_type);
		} else {
			//Otherwise we assign directly
			instruction->source_register2 = instruction->op2;
		}

	//Otherwise we have a constant source
	} else {
		//Otherwise we use an immediate value
		instruction->source_immediate = instruction->op1_const;
	}
}


/**
 * Handle a subtraction operation
 */
static void handle_subtraction_instruction(instruction_t* instruction){
	//Determine what our size is off the bat
	variable_size_t size = get_type_size(instruction->assignee->type);

	//Select the appropriate level of minus instruction
	instruction->instruction_type = select_sub_instruction(size);

	//Again we just need the source and dest registers
	instruction->destination_register = instruction->assignee;

	//If we have a register value, we add that
	if(instruction->op2 != NULL){
		//Do we need any kind of type conversion here? If so we'll do that now
		if(is_expanding_move_required(instruction->assignee->type, instruction->op2->type) == TRUE){
			//If this is needed, we'll let the helper do it
			instruction->source_register = handle_expanding_move_operation(instruction, instruction->op2, instruction->assignee->type);

		//Otherwise let this deal with it
		} else {
			instruction->source_register = instruction->op2;
		}

	} else {
		//Otherwise grab the immediate source
		instruction->source_immediate = instruction->op1_const;
	}
}


/**
 * Handle an addition operation
 *
 * There are 2 varieties of addition instructions, we can split based on if
 * op1 and assignee are the same
 *
 * CASE 1:
 * t23 <- t23 + 34
 * addl $34, t23
 */
static void handle_addition_instruction(instruction_t* instruction){
	//Determine what our size is off the bat
	variable_size_t size = get_type_size(instruction->assignee->type);

	//Grab the add instruction that we want
	instruction->instruction_type = select_add_instruction(size);

	//We'll just need to set the source immediate and destination register
	instruction->destination_register = instruction->assignee;

	//If we have a register value, we add that
	if(instruction->op2 != NULL){
		//Do we need a type conversion? If so, we'll do that here
		if(is_expanding_move_required(instruction->assignee->type, instruction->op2->type) == TRUE){
			//Let the helper function deal with this
			instruction->source_register = handle_expanding_move_operation(instruction, instruction->op2, instruction->assignee->type);

		//Otherwise we'll just do this
		} else {
			instruction->source_register = instruction->op2;
		}

	} else {
		//Otherwise grab the immediate source
		instruction->source_immediate = instruction->op1_const;
	}
}


/**
 * Handle the case where we have different assignee and op1 values
 * 
 * CASE 2:
 * t25 <- t15 + t17
 * leal (t15, t17), t25
 */
static void handle_addition_instruction_lea_modification(instruction_t* instruction){
	//Determines what instruction to use
	variable_size_t size = get_type_size(instruction->assignee->type);

	//Now we'll get the appropriate lea instruction
	instruction->instruction_type = select_lea_instruction(size);

	//We always know what the destination register will be
	instruction->destination_register = instruction->assignee;

	//We always have this
	instruction->address_calc_reg1 = instruction->op1;

	//Now, we'll need to set the appropriate address calculation mode based
	//on what we're given
	//If we have op2, we'll have 2 registers
	if(instruction->statement_type == THREE_ADDR_CODE_BIN_OP_STMT){
		//2 registers in this case
		instruction->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_ONLY;

		//Extract for analysis
		three_addr_var_t* addresss_calc_reg2 = instruction->op2;

		//Does this adhere to the same type as reg1? It must, so if it does not we will force it
		//to 
		if(is_expanding_move_required(instruction->address_calc_reg1->type, addresss_calc_reg2->type) == TRUE){
			//Let the helper deal with it
			addresss_calc_reg2 = handle_expanding_move_operation(instruction, addresss_calc_reg2, instruction->address_calc_reg1->type);
		}

		//Whether or not a type conversion happened, we can assign this here
		instruction->address_calc_reg2 = addresss_calc_reg2;

	//Otherise it's just an offset(bin_op_with_const)
	} else {
		//We'll just have an offset here
		instruction->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;
		//This is definitely not 0 if we're here
		instruction->offset = instruction->op1_const;
	}
}


/**
 * Handle an unsigned multiplication operation
 *
 * Because of the extra instructions that this will generate, this will count as
 * a multiple instruction selection pattern
 *
 * x <- a * 2;
 *
 * mov $3, %rax <- Source is always in RAX
 * mull %rcx -> result in rax
 *
 *
 * NOTE: this is always the first instruction in the instruction window
*/
static void handle_unsigned_multiplication_instruction(instruction_window_t* window){
	//Instruction 1 is the multiplication instruction
	instruction_t* multiplication_instruction = window->instruction1;

	//We'll need to know the variables size
	variable_size_t size = get_type_size(multiplication_instruction->assignee->type);

	//A temp holder for the final second source variable
	three_addr_var_t* source;
	three_addr_var_t* source2;

	//If we have a BIN_OP with const statement, we need to 
	if(multiplication_instruction->statement_type == THREE_ADDR_CODE_BIN_OP_STMT){
		//If we need to convert, we'll do that here
		if(is_expanding_move_required(multiplication_instruction->assignee->type, multiplication_instruction->op2->type) == TRUE){
			//Let the helper deal with it
			source2 = handle_expanding_move_operation(multiplication_instruction, multiplication_instruction->op2, multiplication_instruction->assignee->type);

		//Otherwise this can be moved directly
		} else {
			//We first need to move the first operand into RAX
			instruction_t* move_to_rax = emit_movX_instruction(emit_temp_var(multiplication_instruction->op2->type), multiplication_instruction->op2);

			//Insert the move to rax before the multiplication instruction
			insert_instruction_before_given(move_to_rax, multiplication_instruction);

			//This is just the destination register here
			source2 = move_to_rax->destination_register;
		}

	//Otherwise, we have a BIN_OP_WITH_CONST statement. We're actually going to need a temp assignment for the second operand(the constant)
	//here for this to work
	} else {
		//Emit the move instruction here
		instruction_t* move_to_rax = emit_const_movX_instruciton(emit_temp_var(multiplication_instruction->assignee->type), multiplication_instruction->op1_const);

		//Put it before our multiplication
		insert_instruction_before_given(move_to_rax, multiplication_instruction);

		//Our source2 now is this
		source2 = move_to_rax->destination_register;
	}

	//Let's also check is any conversions are needed for the first source register
	if(is_expanding_move_required(multiplication_instruction->assignee->type, multiplication_instruction->op1->type) == TRUE){
		source = handle_expanding_move_operation(multiplication_instruction, multiplication_instruction->op1, multiplication_instruction->assignee->type);

	//Otherwise we'll just assign this to be op1
	} else {
		source = multiplication_instruction->op1;
	}

	
	//We determine the instruction that we need based on signedness and size
	switch (size) {
		case BYTE:
			multiplication_instruction->instruction_type = MULB;
			break;
		case WORD:
			multiplication_instruction->instruction_type = MULW;
			break;
		case DOUBLE_WORD:
			multiplication_instruction->instruction_type = MULL;
			break;
		//Everything else falls here
		default:
			multiplication_instruction->instruction_type = MULQ;
			break;
	}

	//This is the case where we have two source registers
	multiplication_instruction->source_register = source;
	//The other source register is in RAX
	multiplication_instruction->source_register2 = source2;

	//This is the assignee, we just don't see it
	multiplication_instruction->destination_register = emit_temp_var(multiplication_instruction->assignee->type);

	//Once we've done all that, we need one final movement operation
	instruction_t* result_movement = emit_movX_instruction(multiplication_instruction->assignee, multiplication_instruction->destination_register);
	//This cannot be combined
	result_movement->cannot_be_combined = TRUE;

	//Insert the result movement instruction to be after the multiplication operation
	insert_instruction_after_given(result_movement, multiplication_instruction);

	//We now need to reset the window here
	reconstruct_window(window, result_movement);
}


/**
 * Handle a multiplication operation
 *
 * A multiplication operation can be different based on size and sign
 */
static void handle_signed_multiplication_instruction(instruction_t* instruction){
	//We'll need to know the variables size
	variable_size_t size = get_type_size(instruction->assignee->type);

	//We determine the instruction that we need based on signedness and size
	switch (size) {
		case BYTE:
			instruction->instruction_type = IMULB;
			break;
		case WORD:
			instruction->instruction_type = IMULW;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = IMULL;
			break;
		//Everything else falls here
		default:
			instruction->instruction_type = IMULQ;
			break;
	}

	//Following this, we'll set the assignee and source
	instruction->destination_register = instruction->assignee;

	//Are we using an immediate or register?
	if(instruction->op2 != NULL){
		//Do we need a type conversion here?
		if(is_expanding_move_required(instruction->assignee->type, instruction->op2->type) == TRUE){
			//Let the helper deal with it
			instruction->source_register = handle_expanding_move_operation(instruction, instruction->op2, instruction->assignee->type);

		//Otherwise assign directly
		} else {
			//This is the case where we have a source register
			instruction->source_register = instruction->op2;
		}
		
	} else {
		//In this case we'll have an immediate source
		instruction->source_immediate = instruction->op1_const;
	}
}


/**
 * Handle a division operation
 *
 * t4 <- t2 / t3 
 *
 * Will become:
 * movl t2, t5(rax)
 * cltd
 * idivl t3(divide by t3, we already guarantee this is a temp var(register))
 * movl t5, t4 (rax has quotient)
 * 
 * As such, this will generate additional instructions for us, making it not
 * a "single instruction" pattern
 *
 * NOTE: We guarantee that the instruction we're after is always the first
 * instruction in the window
 */
static void handle_division_instruction(instruction_window_t* window){
	//Firstly, the instruction that we're looking for is the very first one
	instruction_t* division_instruction = window->instruction1;

	//A temp holder for the final second source variable
	three_addr_var_t* dividend;
	three_addr_var_t* divisor;

	//If we need to convert, we'll do that here
	if(is_expanding_move_required(division_instruction->assignee->type, division_instruction->op1->type) == TRUE){
		//Let the helper deal with it
		dividend = handle_expanding_move_operation(division_instruction, division_instruction->op1, division_instruction->assignee->type);

	//Otherwise this can be moved directly
	} else {
		//We first need to move the first operand into RAX
		instruction_t* move_to_rax = emit_movX_instruction(emit_temp_var(division_instruction->op1->type), division_instruction->op1);

		//Insert the move to rax before the multiplication instruction
		insert_instruction_before_given(move_to_rax, division_instruction);

		//This is just the destination register here
		dividend = move_to_rax->destination_register;
	}

	//Let's determine signedness
	u_int8_t is_signed = is_type_signed(division_instruction->assignee->type);

	/**
	 * For a signed division, the CXXX instruction will 
	 * have a secondary destination that holds the higher order bits. We will
	 * capture this if needed here
	 */
	three_addr_var_t* higher_order_dividend_bits = NULL;

	//Now, we'll need the appropriate extension instruction *if* we're doing signed division
	if(is_signed == TRUE){
		//Emit the cl instruction
		instruction_t* cl_instruction = emit_conversion_instruction(dividend);

		//Capute both the lower and higher order bit fields
		dividend = cl_instruction->destination_register;
		higher_order_dividend_bits = cl_instruction->destination_register2;

		//Insert this before the given
		insert_instruction_before_given(cl_instruction, division_instruction);
	}

	//Do we need to do a type conversion? If so, we'll do a converting move here
	if(is_expanding_move_required(division_instruction->assignee->type, division_instruction->op2->type) == TRUE){
		divisor = handle_expanding_move_operation(division_instruction, division_instruction->op2, division_instruction->assignee->type);

	//Otherwise divisor is just the op2
	} else {
		divisor = division_instruction->op2;
	}

	//Now we should have what we need, so we can emit the division instruction
	instruction_t* division = emit_div_instruction(division_instruction->assignee, divisor, dividend, higher_order_dividend_bits, is_signed);

	//The quotient is the destination register
	three_addr_var_t* quotient = division->destination_register;

	//Insert this before the division instruction
	insert_instruction_before_given(division, division_instruction);

	//Once we've done all that, we need one final movement operation
	instruction_t* result_movement = emit_movX_instruction(division_instruction->assignee, quotient);
	//This cannot be combined
	result_movement->cannot_be_combined = TRUE;

	//Insert this before the original division instruction
	insert_instruction_before_given(result_movement, division_instruction);

	//Delete the division instruction
	delete_statement(division_instruction);

	//Reconstruct the window here
	reconstruct_window(window, result_movement);
}


/**
 * Handle a modulus(remainder) operation
 * Handle a division operation
 *
 * t3 <- t4 % t5
 *
 * Will become:
 * movl t4, t6 (rax)
 * cltd
 * idivl t5
 * t3 <- t7 (rdx has remainder)
 *
 * As such, this will generate additional instructions for us, making it not
 * a "single instruction" pattern
 *
 * NOTE: We guarantee that the instruction we're after is always the first
 * instruction in the window
 */
static void handle_modulus_instruction(instruction_window_t* window){
	//Firstly, the instruction that we're looking for is the very first one
	instruction_t* modulus_instruction = window->instruction1;

	//A temp holder for the final second source variable
	three_addr_var_t* dividend;
	three_addr_var_t* divisor;

	//If we need to convert, we'll do that here
	if(is_expanding_move_required(modulus_instruction->assignee->type, modulus_instruction->op1->type) == TRUE){
		//Let the helper deal with it
		dividend = handle_expanding_move_operation(modulus_instruction, modulus_instruction->op1, modulus_instruction->assignee->type);

	//Otherwise this can be moved directly
	} else {
		//We first need to move the first operand into RAX
		instruction_t* move_to_rax = emit_movX_instruction(emit_temp_var(modulus_instruction->op1->type), modulus_instruction->op1);

		//Insert the move to rax before the multiplication instruction
		insert_instruction_before_given(move_to_rax, modulus_instruction);

		//This is just the destination register here
		dividend = move_to_rax->destination_register;
	}

	//Let's determine signedness
	u_int8_t is_signed = is_type_signed(modulus_instruction->assignee->type);

	/**
	 * For a signed division, the CXXX instruction will 
	 * have a secondary destination that holds the higher order bits. We will
	 * capture this if needed here
	 */
	three_addr_var_t* higher_order_dividend_bits = NULL;

	//Now, we'll need the appropriate extension instruction *if* we're doing signed division
	if(is_signed == TRUE){
		//Emit the cl instruction
		instruction_t* cl_instruction = emit_conversion_instruction(dividend);

		//Store both here
		dividend = cl_instruction->destination_register;
		higher_order_dividend_bits = cl_instruction->destination_register2;

		//Insert this before the mod instruction
		insert_instruction_before_given(cl_instruction, modulus_instruction);
	}

	//Do we need to do a type conversion? If so, we'll do a converting move here
	if(is_expanding_move_required(modulus_instruction->assignee->type, modulus_instruction->op2->type) == TRUE){
		divisor = handle_expanding_move_operation(modulus_instruction, modulus_instruction->op2, modulus_instruction->assignee->type);

	//Otherwise source 2 is just the op2
	} else {
		divisor = modulus_instruction->op2;
	}

	//Now we should have what we need, so we can emit the division instruction
	instruction_t* division = emit_div_instruction(modulus_instruction->assignee, divisor, dividend, higher_order_dividend_bits, is_signed);
	
	//Store the remainder register here
	three_addr_var_t* remainder_register = division->destination_register2;

	//Insert this before the original modulus
	insert_instruction_before_given(division, modulus_instruction);

	//Once we've done all that, we need one final movement operation
	instruction_t* result_movement = emit_movX_instruction(modulus_instruction->assignee, remainder_register);
	//This also cannot be combined
	result_movement->cannot_be_combined = TRUE;

	//Insert this after the original modulus
	insert_instruction_after_given(result_movement, modulus_instruction);

	//Finally we'll delete the old modulus instruction, as we no longer need it
	delete_statement(modulus_instruction);

	//Reconstruct the window starting at the result movement
	reconstruct_window(window, result_movement);
}


/**
 * We can translate a bin op statement a few different ways based on the operand
 */
static void handle_binary_operation_instruction(instruction_t* instruction){
	//Go based on what we have as the operation
	switch(instruction->op){
		/**
		 * 2 options here
		 *
		 * CASE 1:
		 * t23 <- t23 + 34
		 * addl $34, t23
		 *
		 * OR
		 *
		 * CASE 2:
		 * t25 <- t15 + t17
		 * leal t25, (t15, t17)
		 */
		case PLUS:
			//This is our first case
			if(variables_equal(instruction->op1, instruction->assignee, FALSE) == TRUE){
				//Let the helper do it
				handle_addition_instruction(instruction);
			//Otherwise we need to handle case 2
			} else {
				//Let this different version do it
				handle_addition_instruction_lea_modification(instruction);
			}

			break;

		case MINUS:
			//Let the helper do it
			handle_subtraction_instruction(instruction);
			break;
		/**
		 * NOTE: By the time that we reach here, we know that any unsigned multiplication will
		 * have already been dealt with. As such, we know for a fact that this will be the signed
		 * version
		 */
		case STAR:
			//Let the helper do it
			handle_signed_multiplication_instruction(instruction);
			break;
		//Hanlde a left shift instruction
		case L_SHIFT:
			handle_left_shift_instruction(instruction);
			break;
		//Handle a right shift operation
		case R_SHIFT:
			handle_right_shift_instruction(instruction);
			break;
		//Handle the (|) operator
		case SINGLE_OR:
			handle_bitwise_inclusive_or_instruction(instruction);
			break;
		//Handle the (&) operator in a binary operation context
		case SINGLE_AND:
			handle_bitwise_and_instruction(instruction);
			break;
		case CARROT:
			handle_bitwise_exclusive_or_instruction(instruction);
			break;
		//All of these instructions require us to use the CMP or CMPQ command
		case DOUBLE_EQUALS:
		case NOT_EQUALS:
		case G_THAN:
		case G_THAN_OR_EQ:
		case L_THAN:
		case L_THAN_OR_EQ:
			//Let the helper do it
			handle_cmp_instruction(instruction);
			break;
		default:
			break;
	}
}


/**
 * Handle an increment statement
 */
static void handle_inc_instruction(instruction_t* instruction){
	//Determine the size of the variable we need
	variable_size_t size = get_type_size(instruction->assignee->type);

	//If it's a quad word, there's a different instruction to use. Otherwise
	//it's just a regular inc
	switch(size){
		case QUAD_WORD:
			instruction->instruction_type = INCQ;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = INCL;
			break;
		case WORD:
			instruction->instruction_type = INCW;
			break;
		case BYTE:
			instruction->instruction_type = INCB;
			break;
		//We shouldn't get here, but if we do, make it INCQ
		default:
			instruction->instruction_type = INCQ;
			break;
	}

	instruction->source_register = instruction->op1;

	//Set the destination as the assignee
	instruction->destination_register = instruction->assignee;
}

/**
 * Handle a decrement statement
 */
static void handle_dec_instruction(instruction_t* instruction){
	//Determine the size of the variable we need
	variable_size_t size = get_type_size(instruction->assignee->type);

	//If it's a quad word, there's a different instruction to use. Otherwise
	//it's just a regular inc
	switch(size){
		case QUAD_WORD:
			instruction->instruction_type = DECQ;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = DECL;
			break;
		case WORD:
			instruction->instruction_type = DECW;
			break;
		case BYTE:
			instruction->instruction_type = DECB;
			break;
		//We shouldn't get here, but if we do, make it INCQ
		default:
			instruction->instruction_type = DECQ;
			break;
	}

	instruction->source_register = instruction->op1;

	//Set the destination as the assignee
	instruction->destination_register = instruction->assignee;
}


/**
 * Handle the case where we have a constant to register move
 */
static void handle_constant_to_register_move_instruction(instruction_t* instruction){
	//Get the size we need first
	variable_size_t size = get_type_size(instruction->assignee->type);

	//Select based on size
	instruction->instruction_type = select_move_instruction(size);
	
	//We've already set the sources, now we set the destination as the assignee
	instruction->destination_register = instruction->assignee;
	//Set the source immediate here
	instruction->source_immediate = instruction->op1_const;
}


/**
 * Handle a simple movement instruction. In this context, simple just means that
 * we have a source and a destination, and now address calculation moves in between
 */
static void handle_simple_movement_instruction(instruction_t* instruction){
	//We have both a destination and source size to look at here
	variable_size_t destination_size = get_type_size(instruction->assignee->type);
	variable_size_t source_size = get_type_size(instruction->op1->type);

	//Set the sources and destinations
	instruction->destination_register = instruction->assignee;
	instruction->source_register = instruction->op1;

	//Use the helper to get the right sized move instruction
	instruction->instruction_type = select_register_movement_instruction(destination_size, source_size, is_type_signed(instruction->assignee->type));
}


/**
 * Handle a lea statement(in the three address code statement form)
 *
 * Lea statements(by the time we get here..) have the following in them:
 * op1: usually a memory address source
 * op2: the offset we're adding
 * lea_multiplicator: a multiple of 2 that we're multiplying op2 by
 */
static void handle_lea_statement(instruction_t* instruction){
	//Store address calc reg's 1 and 2
	three_addr_var_t* address_calc_reg1 = instruction->op1;
	three_addr_var_t* address_calc_reg2 = instruction->op2;
	
	//Select the size of our variable
	variable_size_t size = get_type_size(instruction->assignee->type);

	switch(size){
		case QUAD_WORD:
			instruction->instruction_type = LEAQ;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = LEAL;
			break;
		case BYTE:
		case WORD:
			instruction->instruction_type = LEAW;
			break;
		default:
			break;
	}

	//The base(address calc reg1) and index(address calc reg 2) registers must be the same type.
	//We determine that the base address is the dominating force, and takes precedence, so the address calc reg2
	//must adhere to this one's type
	if(is_expanding_move_required(address_calc_reg1->type, address_calc_reg2->type)){
		address_calc_reg2 = handle_expanding_move_operation(instruction, address_calc_reg2, address_calc_reg1->type);
	}

	//We already know what mode we'll need to use here
	instruction->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_AND_SCALE;

	//Now we can set the values
	instruction->destination_register = instruction->assignee;

	//Add op1 and op2
	instruction->address_calc_reg1 = address_calc_reg1;
	instruction->address_calc_reg2 = address_calc_reg2;

	//And the lea multiplicator is already in place..
}


/**
 * A branch statement always selects 2 instructions, the conditional
 * jump-to-if and the unconditional else jump
 *
 * NOTE: we know that the branch instruction here is always instruction 1
 */
static void handle_branch_instruction(instruction_window_t* window){
	//Add it in here
	instruction_t* branch_stmt = window->instruction1;

	//Grab out the if and else blocks
	basic_block_t* if_block = branch_stmt->if_block;
	basic_block_t* else_block = branch_stmt->else_block;

	//Placeholder for the jump to if instruction
	instruction_t* jump_to_if;

	switch(branch_stmt->branch_type){
		case BRANCH_A:
			jump_to_if = emit_jump_instruction_directly(if_block, JA);
			break;
		case BRANCH_AE:
			jump_to_if = emit_jump_instruction_directly(if_block, JAE);
			break;
		case BRANCH_B:
			jump_to_if = emit_jump_instruction_directly(if_block, JB);
			break;
		case BRANCH_BE:
			jump_to_if = emit_jump_instruction_directly(if_block, JBE);
			break;
		case BRANCH_E:
			jump_to_if = emit_jump_instruction_directly(if_block, JE);
			break;
		case BRANCH_NE:
			jump_to_if = emit_jump_instruction_directly(if_block, JNE);
			break;
		case BRANCH_Z:
			jump_to_if = emit_jump_instruction_directly(if_block, JZ);
			break;
		case BRANCH_NZ:
			jump_to_if = emit_jump_instruction_directly(if_block, JNZ);
			break;
		case BRANCH_G:
			jump_to_if = emit_jump_instruction_directly(if_block, JG);
			break;
		case BRANCH_GE:
			jump_to_if = emit_jump_instruction_directly(if_block, JGE);
			break;
		case BRANCH_L:
			jump_to_if = emit_jump_instruction_directly(if_block, JL);
			break;
		case BRANCH_LE:
			jump_to_if = emit_jump_instruction_directly(if_block, JLE);
			break;
		//We in reality should never reach here
		default:
			break;
	}

	//The else jump is always a direct jump no matter what
	instruction_t* jump_to_else = emit_jump_instruction_directly(else_block, JMP);

	//Grab the block our
	basic_block_t* block = branch_stmt->block_contained_in;

	//The if must go after the branch statement before the else
	add_statement(block, jump_to_if);
	//And the jump to else always goes after the if jump
	add_statement(block, jump_to_else);

	//Once this is all done, we can delete the branch
	delete_statement(branch_stmt);

	//And reset the window based on the last one
	reconstruct_window(window, jump_to_else);
}


/**
 * Handle a function call instruction
 */
static void handle_function_call(instruction_t* instruction){
	//This will be a call instruction
	instruction->instruction_type = CALL;

	//The destination register is itself the assignee
	instruction->destination_register = instruction->assignee;
}


/**
 * Handle a function call instruction
 */
static void handle_indirect_function_call(instruction_t* instruction){
	//This will be an indirect call instruction
	instruction->instruction_type = INDIRECT_CALL;

	//In this case, the source register is the function name
	instruction->source_register = instruction->op1;

	//The destination register is itself the assignee
	instruction->destination_register = instruction->assignee;
}


/**
 *	//=========================== Logical Notting =============================
 * Although it may not seem like it, logical not is actually a multiple instruction
 * pattern
 *
 * This:
 * t9 <- logical not t9
 *
 * will become
 * test t9, t9
 * sete %al
 * movzx %al, t9
 *
 * NOTE: We know that instruction1 is the one that is a logical not instruction if we
 * get here
 */
static void handle_logical_not_instruction(instruction_window_t* window){
	//Let's grab the value out for convenience
	instruction_t* logical_not = window->instruction1;

	//Now we'll need to generate three new instructions
	//First comes the test command. We're testing this against itself
	instruction_t* test_inst = emit_direct_test_instruction(logical_not->op1, logical_not->op1); 
	//Ensure that we set all these flags too
	test_inst->block_contained_in = logical_not->block_contained_in;
	test_inst->is_branch_ending = logical_not->is_branch_ending;

	//Now we'll set the AL register to 1 if we're equal here
	instruction_t* sete_inst = emit_sete_instruction(logical_not->assignee);
	//Ensure that we set all these flags too
	sete_inst->block_contained_in = logical_not->block_contained_in;
	sete_inst->is_branch_ending = logical_not->is_branch_ending;

	//Preserve this before we lose it
	instruction_t* after_logical_not = logical_not->next_statement;

	//Delete the logical not statement, we no longer need it
	delete_statement(logical_not);

	//First insert the test instruction
	insert_instruction_before_given(test_inst, after_logical_not);

	//Then insert the sete instruction
	insert_instruction_before_given(sete_inst, after_logical_not);

	//This is the new window
	reconstruct_window(window, sete_inst);
}


/**
 * A setne is a very simple one-to-one mapping
 */
static void handle_setne_instruction(instruction_t* instruction){
	//Just set the type and register
	instruction->instruction_type = SETNE;
	instruction->destination_register = instruction->assignee;
}


/**
 * Handle a logical OR instruction
 * 
 * t32 <- t32 || t19
 *
 * Will become:
 *
 * orq t19, t32 <---------- bitwise or
 * setne t33 <------------ if it isn't 0, we eval to TRUE(1)
 * movzx t33, t32  <-------------- move this into the result
 *
 * NOTE: We guarantee that the first instruction in the window is the one that
 * we're after in this case
 */
static void handle_logical_or_instruction(cfg_t* cfg, instruction_window_t* window){
	//Grab it out for convenience
	instruction_t* logical_or = window->instruction1;

	//Save the after instruction
	instruction_t* after_logical_or = window->instruction2;

	//Let's first emit the or instruction
	instruction_t* or_instruction = emit_or_instruction(logical_or->op1, logical_or->op2);

	//We'll need this type for our setne's
	generic_type_t* unsigned_int8_type = lookup_type_name_only(cfg->type_symtab, "u8")->type;

	//Now we need the setne instruction
	instruction_t* setne_instruction = emit_setne_instruction(emit_temp_var(unsigned_int8_type));

	//Following that we'll need the final movzx instruction
	instruction_t* movzx_instruction = emit_appropriate_move_statement(logical_or->assignee, setne_instruction->destination_register);

	//Select this one's size 
	logical_or->assignee->variable_size = get_type_size(logical_or->assignee->type);

	//Now we can delete the old logical or instruction
	delete_statement(logical_or);

	//First insert the or instruction
	insert_instruction_before_given(or_instruction, after_logical_or);

	//Then we need the setne
	insert_instruction_before_given(setne_instruction, after_logical_or);

	//And finally we need the movzx
	insert_instruction_before_given(movzx_instruction, after_logical_or);

	//Reconstruct the window starting at the movzbl
	reconstruct_window(window, movzx_instruction);
}


/**
 * Handle a logical and instruction
 *
 * t32 <- t32 && t19
 *
 * This will translate to:
 * testq t32, t32 <----- is this 0?
 * setne t33 <----------- if it's not make this one
 * testq t19, 19 <------- Is this 0?
 * setne t34 <------------ If it's not, make it a one
 * andq t33, t34 <---------- let's see if they're both 0, both 1, etc
 * movzx t34, t32 <---------- store t34 with the result
 *
 * Since this will spawn multiple instructions, it will be invoked from the multiple instruction
 * pattern selector
 * 
 * NOTE: We guarantee that the first instruction in the window is the one that we're after
 * in this case
 */
static void handle_logical_and_instruction(cfg_t* cfg, instruction_window_t* window){
	//Grab it out for convenience
	instruction_t* logical_and = window->instruction1;

	//Preserve this for ourselves
	instruction_t* after_logical_and = logical_and->next_statement;

	//Let's first emit our test instruction
	instruction_t* first_test = emit_direct_test_instruction(logical_and->op1, logical_and->op1);

	//We'll need this type for our setne's
	generic_type_t* unsigned_int8_type = lookup_type_name_only(cfg->type_symtab, "u8")->type;

	//Now we'll need a setne instruction that will set a new temp
	instruction_t* first_set = emit_setne_instruction(emit_temp_var(unsigned_int8_type));
	
	//Now we'll need the second test
	instruction_t* second_test = emit_direct_test_instruction(logical_and->op2, logical_and->op2);

	//Now the second setne
	instruction_t* second_set = emit_setne_instruction(emit_temp_var(unsigned_int8_type));

	//Now we'll need to ANDx these two values together to see if they're both 1
	instruction_t* and_inst = emit_and_instruction(first_set->destination_register, second_set->destination_register);

	//The final thing that we need is a movzx
	instruction_t* movzx_instruction = emit_appropriate_move_statement(logical_and->assignee, and_inst->destination_register);

	//Select this one's size 
	logical_and->assignee->variable_size = get_type_size(logical_and->assignee->type);

	//We no longer need the logical and statement
	delete_statement(logical_and);

	//Now we'll insert everything in order
	insert_instruction_before_given(first_test, after_logical_and);
	insert_instruction_before_given(first_set, after_logical_and);
	insert_instruction_before_given(second_test, after_logical_and);
	insert_instruction_before_given(second_set, after_logical_and);
	insert_instruction_before_given(and_inst, after_logical_and);
	insert_instruction_before_given(movzx_instruction, after_logical_and);
	
	//Reconstruct the window starting at the final move
	reconstruct_window(window, movzx_instruction);
}


/**
 * Handle a negation instruction. Very simple - all we need to do is select the suffix and
 * add it over
 */
static void handle_neg_instruction(instruction_t* instruction){
	//Find out what size we have
	variable_size_t size = get_type_size(instruction->assignee->type);

	switch(size){
		case QUAD_WORD:
			instruction->instruction_type = NEGQ;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = NEGL;
			break;
		case WORD:
			instruction->instruction_type = NEGW;
			break;
		case BYTE:
			instruction->instruction_type = NEGB;
			break;
		default:
			break;
	}

	//Now we'll just translate the assignee to be the destination(and source in this case) register
	instruction->destination_register = instruction->assignee;
}


/**
 * Handle a bitwise not(one's complement) instruction. Very simple - all we need to do is select the suffix and
 * add it over
 */
static void handle_not_instruction(instruction_t* instruction){
	//Find out what size we have
	variable_size_t size = get_type_size(instruction->assignee->type);

	switch(size){
		case QUAD_WORD:
			instruction->instruction_type = NOTQ;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = NOTL;
			break;
		case WORD:
			instruction->instruction_type = NOTW;
			break;
		case BYTE:
			instruction->instruction_type = NOTB;
			break;
		default:
			break;
	}

	//Now we'll just translate the assignee to be the destination(and source in this case) register
	instruction->destination_register = instruction->assignee;
}


/**
 * Handle a test instruction. The test instruction's op1 is acutally duplicated
 * to be both of its inputs in this case
 */
static void handle_test_instruction(instruction_t* instruction){
	//Find out what size we have
	variable_size_t size = get_type_size(instruction->op1->type);

	switch(size){
		case QUAD_WORD:
			instruction->instruction_type = TESTQ;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = TESTL;
			break;
		case WORD:
			instruction->instruction_type = TESTW;
			break;
		case BYTE:
			instruction->instruction_type = TESTB;
			break;
		default:
			break;
	}

	//This actually has no real destination register, the assignee was a dummy
	//It does have 2 source registers however
	instruction->source_register = instruction->op1;
	instruction->source_register2 = instruction->op2;
}


/**
 * Handle a load instruction. A load instruction is always converted into
 * a garden variety dereferencing move
 */
static void handle_load_instruction(instruction_t* instruction){
	//Size is determined by the assignee
	variable_size_t size = get_type_size(instruction->assignee->type);

	//Select the instruction type accordingly
	switch(size){
		case QUAD_WORD:
			instruction->instruction_type = MEM_TO_REG_MOVQ;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = MEM_TO_REG_MOVL;
			break;
		case WORD:
			instruction->instruction_type = MEM_TO_REG_MOVW;
			break;
		case BYTE:
			instruction->instruction_type = MEM_TO_REG_MOVB;
			break;
		default:
			break;
	}

	//This will always be a SOURCE_ONLY
	instruction->calculation_mode = ADDRESS_CALCULATION_MODE_DEREF_ONLY_SOURCE;

	//The destination is our assignee
	instruction->destination_register = instruction->assignee;

	//And the op1 is our source
	instruction->source_register = instruction->op1;
}


/**
 * Handle a load with constant offset instruction
 *
 * load t5 <- t23[8] --> movx 8(t23), t5
 *
 * This will always generate an address calculation mode of OFFSET_ONLY 
 */
static void handle_load_with_constant_offset_instruction(instruction_t* instruction){
	//Size is determined by the assignee
	variable_size_t size = get_type_size(instruction->assignee->type);

	//Select the instruction type accordingly
	switch(size){
		case QUAD_WORD:
			instruction->instruction_type = MEM_TO_REG_MOVQ;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = MEM_TO_REG_MOVL;
			break;
		case WORD:
			instruction->instruction_type = MEM_TO_REG_MOVW;
			break;
		case BYTE:
			instruction->instruction_type = MEM_TO_REG_MOVB;
			break;
		default:
			break;
	}

	//This will always be offset only
	instruction->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;

	//The destination register is always the assignee
	instruction->destination_register = instruction->assignee;

	//Op1 is our base address
	instruction->address_calc_reg1 = instruction->op1;
	//Our offset has already been set at the start - so we're good here
}


/**
 * Handle a load with variable offset instruction
 *
 * load t5 <- t23[t24] --> movx (t23, t24), t5
 *
 * This will always generate an address calculation mode of OFFSET_ONLY 
 */
static void handle_load_with_variable_offset_instruction(instruction_t* instruction){
	//Size is determined by the assignee
	variable_size_t size = get_type_size(instruction->assignee->type);

	//Select the instruction type accordingly
	switch(size){
		case QUAD_WORD:
			instruction->instruction_type = MEM_TO_REG_MOVQ;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = MEM_TO_REG_MOVL;
			break;
		case WORD:
			instruction->instruction_type = MEM_TO_REG_MOVW;
			break;
		case BYTE:
			instruction->instruction_type = MEM_TO_REG_MOVB;
			break;
		default:
			break;
	}

	//This will always be offset only
	instruction->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_ONLY;

	//The destination register is always the assignee
	instruction->destination_register = instruction->assignee;

	//Op1 is our base address
	instruction->address_calc_reg1 = instruction->op1;
	//Op2 is the variable offset
	instruction->address_calc_reg2 = instruction->op2;
}


/**
 * Handle a store instruction. This will be reorganized into a memory accessing move
 */
static void handle_store_instruction(instruction_t* instruction){
	//Size is determined by the assignee
	variable_size_t size = get_type_size(instruction->assignee->type);

	//Select the instruction type accordingly
	switch(size){
		case QUAD_WORD:
			instruction->instruction_type = REG_TO_MEM_MOVQ;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = REG_TO_MEM_MOVL;
			break;
		case WORD:
			instruction->instruction_type = REG_TO_MEM_MOVW;
			break;
		case BYTE:
			instruction->instruction_type = REG_TO_MEM_MOVB;
			break;
		default:
			break;
	}

	//This counts for our destination only
	instruction->calculation_mode = ADDRESS_CALCULATION_MODE_DEREF_ONLY_DEST;

	//This is our destination register
	instruction->destination_register = instruction->assignee;

	//And the source register is our op1 or we have an immediate
	if(instruction->op1 != NULL){
		instruction->source_register = instruction->op1;
	} else {
		instruction->source_immediate = instruction->op1_const;
	}
}


/**
 * Handle an instruction like
 *
 * store t5[4] <- t7
 *
 * movX t7, 4(t4)
 *
 * This will always be an OFFSET_ONLY calculation type
 */
static void handle_store_with_constant_offset_instruction(instruction_t* instruction){
	//Size is determined by the assignee
	variable_size_t size = get_type_size(instruction->assignee->type);

	//Select the instruction type accordingly
	switch(size){
		case QUAD_WORD:
			instruction->instruction_type = REG_TO_MEM_MOVQ;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = REG_TO_MEM_MOVL;
			break;
		case WORD:
			instruction->instruction_type = REG_TO_MEM_MOVB;
			break;
		case BYTE:
			instruction->instruction_type = REG_TO_MEM_MOVB;
			break;
		default:
			break;
	}

	//This will always be offset only
	instruction->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;

	//The base address is the assignee
	instruction->address_calc_reg1 = instruction->assignee;
	//Our offset has already been saved at the start - so we're good here

	//And the source register is our op2 or we have an immediate source
	if(instruction->op2 != NULL){
		instruction->source_register = instruction->op2;
	} else {
		instruction->source_immediate = instruction->op1_const;
	}
}


/**
 * Handle an instruction like
 *
 * store t5[t6] <- t7
 *
 * movX t7, (t4,t6)
 *
 * This will always be a REGISTERS_ONLY calculation type
 */
static void handle_store_with_variable_offset_instruction(instruction_t* instruction){
	//Size is determined by the assignee
	variable_size_t size = get_type_size(instruction->assignee->type);

	//Select the instruction type accordingly
	switch(size){
		case QUAD_WORD:
			instruction->instruction_type = REG_TO_MEM_MOVQ;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = REG_TO_MEM_MOVL;
			break;
		case WORD:
			instruction->instruction_type = REG_TO_MEM_MOVB;
			break;
		case BYTE:
			instruction->instruction_type = REG_TO_MEM_MOVB;
			break;
		default:
			break;
	}

	//This will always be offset only
	instruction->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_ONLY;

	//The base address is the assignee
	instruction->address_calc_reg1 = instruction->assignee;
	//Op1 is the address calcu register
	instruction->address_calc_reg2 = instruction->op1;

	//And the source register is our op2 or we have an immediate source
	if(instruction->op2 != NULL){
		instruction->source_register = instruction->op2;
	} else {
		instruction->source_immediate = instruction->op1_const;
	}
}


/**
 * Translate the memory address instruction into stack form
 *
 * There are two options here - a stack address and a global variable. We will 
 * handle both cases
 */
static void handle_memory_address_instruction(cfg_t* cfg, three_addr_var_t* stack_pointer, instruction_t* instruction){
	//These will always be LEA's
	instruction->instruction_type = LEAQ;

	//Destination is always the assignee
	instruction->destination_register = instruction->assignee;

	//Extract for convenience
	three_addr_var_t* address_variable = instruction->op1;
	symtab_variable_record_t* variable = address_variable->linked_var;

	//Is this a stack variable(most common case)
	if(variable->membership != GLOBAL_VARIABLE){
		//We need to grab this variable's stack offset
		u_int32_t stack_offset = variable->stack_region->base_address;

		//Once we have that, we can emit our offset constant
		three_addr_const_t* offset_constant = emit_direct_integer_or_char_constant(stack_offset, u64);

		//The offset is the offset constant
		instruction->offset = offset_constant;

		//Stack pointer is the calc reg 1
		instruction->address_calc_reg1 = stack_pointer;

		//We've only got the offset here
		instruction->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;

	//Otherwise we know that we have a global variable
	} else {
		//Signify that we have a global variable
		instruction->calculation_mode = ADDRESS_CALCULATION_MODE_GLOBAL_VAR;

		//The first address calc register is the instruction pointer
		instruction->address_calc_reg1 = cfg->instruction_pointer;

		//We'll use the at this point ignored op2 slot to hold the value of the offset
		instruction->op2 = address_variable;
	}
}


/**
 * Handle a register/immediate to memory move type instruction selection with an address calculation
 *
 * t4 <- stack_pointer_0 + 8
 * store t4 <- t3
 *
 * Into:
 *
 * mov(w/l/q) t3, 8(stack_pointer_0)
 *
 * AND
 *
 * t4 <- stack_pointer_0 + t8
 * store t4 <- t3
 *
 * Into:
 *
 * mov(w/l/q) t3, (stack_pointer_0, t8)
 *
 * DOES NOT DO DELETION/WINDOW REORDERING
 */
static void handle_two_instruction_address_calc_and_store(instruction_t* address_calculation, instruction_t* store_instruction){
	//The size is based on the store instruction's type
	variable_size_t size = get_type_size(store_instruction->assignee->type);

	//Now based on the size, we can select what variety to register/immediate to memory move we have here
	switch (size) {
		case BYTE:
			store_instruction->instruction_type = REG_TO_MEM_MOVB;
			break;
		case WORD:
			store_instruction->instruction_type = REG_TO_MEM_MOVW;
			break;
		case DOUBLE_WORD:
			store_instruction->instruction_type = REG_TO_MEM_MOVL;
			break;
		case QUAD_WORD:
			store_instruction->instruction_type = REG_TO_MEM_MOVQ;
			break;
		//WE DO NOT DO FLOATS YET
		default:
			store_instruction->instruction_type = REG_TO_MEM_MOVQ;
			break;
	}


	//Go based on what type of instruction we have here
	switch(address_calculation->statement_type){
		//Bin op with const statement, we'll have an offset only
		case THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT:
			//Our calculation mode here will be OFFSET_ONLY
			store_instruction->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;

			//The store instruction has the offset of the address calc's op1
			store_instruction->offset = address_calculation->op1_const;

			//The address calc reg 1 will be the op1 of the first instruction
			store_instruction->address_calc_reg1 = address_calculation->op1;

			break;

		//Otherwise we'll have a REGISTERS_ONLY type calculation
		case THREE_ADDR_CODE_BIN_OP_STMT:
			//This one will be of type registers_only
			store_instruction->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_ONLY;

			//Grab both registers out
			//Base address
			store_instruction->address_calc_reg1 = address_calculation->op1;

			//Extract for our work here
			three_addr_var_t* address_calc_reg2 = address_calculation->op2;

			//Do we need to convert here? This can happen
			if(is_type_address_calculation_compatible(address_calc_reg2->type) == FALSE){
				 address_calc_reg2 = handle_expanding_move_operation(store_instruction, address_calc_reg2, u64);
			}

			//Register offset
			store_instruction->address_calc_reg2 = address_calc_reg2;

			break;

		//Unreachable here
		default:
			break;
	}

	//If we have op1, then our source is op1
	if(store_instruction->op1 != NULL){
		store_instruction->source_register = store_instruction->op1;
	//Otherwise our source is the constant
	} else {
		store_instruction->source_immediate = store_instruction->op1_const;
	}
}


/**
 * Handle an address calc and load statement
 *
 * t4 <- stack_pointer_0 + 8
 * load t3 <- t4
 *
 * Into:
 *
 * mov(w/l/q) 8(stack_pointer_0), t3
 *
 * AND
 *
 * t4 <- stack_pointer_0 + t8
 * load t3 <- t4
 *
 * Into:
 *
 * mov(w/l/q) (stack_pointer_0, t8), t3
 *
 * DOES NOT DO DELETION/WINDOW REORDERING
 */
static void handle_two_instruction_address_calc_and_load(instruction_t* address_calculation, instruction_t* load_instruction){
	//Select the variable size based on the assignee
	variable_size_t size = get_type_size(load_instruction->assignee->type);

	//Now based on the size, we can select what variety to register/immediate to memory move we have here
	switch (size) {
		case BYTE:
			load_instruction->instruction_type = MEM_TO_REG_MOVB;
			break;
		case WORD:
			load_instruction->instruction_type = MEM_TO_REG_MOVW;
			break;
		case DOUBLE_WORD:
			load_instruction->instruction_type = MEM_TO_REG_MOVL;
			break;
		case QUAD_WORD:
			load_instruction->instruction_type = MEM_TO_REG_MOVQ;
			break;
		//WE DO NOT DO FLOATS YET
		default:
			load_instruction->instruction_type = MEM_TO_REG_MOVQ;
			break;
	}

	//Go based on what kind of address calc we have
	switch(address_calculation->statement_type){
		//Constant offset
		case THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT:
			//Our address calc mode here will be OFFSET_ONLY
			load_instruction->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;

			//The load's offset will be the op1_const
			load_instruction->offset = address_calculation->op1_const;

			//The address calc reg1 is just our op1
			load_instruction->address_calc_reg1 = address_calculation->op1;

			break;

		//Variable offset
		case THREE_ADDR_CODE_BIN_OP_STMT:
			//This is REGISTERS_ONLY
			load_instruction->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_ONLY;

			//Base address
			load_instruction->address_calc_reg1 = address_calculation->op1;

			//Extract for our work here
			three_addr_var_t* address_calc_reg2 = address_calculation->op2;

			//Do we need to convert here? This can happen
			if(is_type_address_calculation_compatible(address_calc_reg2->type) == FALSE){
				 address_calc_reg2 = handle_expanding_move_operation(load_instruction, address_calc_reg2, u64);
			}

			//Register offset
			load_instruction->address_calc_reg2 = address_calc_reg2;
			
			break;

		//Unreachable
		default:
			break;
	}

	//No matter what this is always set
	load_instruction->destination_register = load_instruction->assignee;
}



/**
 * Select instructions that follow a singular pattern. This one single pass will run after
 * the pattern selector ran and perform one-to-one mappings on whatever is left.
 */
static void select_instruction_patterns(cfg_t* cfg, instruction_window_t* window){
	//We could see logical and/logical or
	if(is_instruction_binary_operation(window->instruction1) == TRUE){
		switch(window->instruction1->op){
			//Handle the logical and case
			case DOUBLE_AND:
				handle_logical_and_instruction(cfg, window);
				return;

			//Handle logical or
			case DOUBLE_OR:
				handle_logical_or_instruction(cfg, window);
				return;

			//Handle division
			case F_SLASH:
				//This will generate more than one instruction
				handle_division_instruction(window);
				return;

			//Handle modulus
			case MOD:	
				//This will generate more than one instruction
				handle_modulus_instruction(window);
				return;

			//If we have a multiplication *and* it's unsigned, we go here
			case STAR:
				//Only do this if we're signed
				if(is_type_signed(window->instruction1->assignee->type) == FALSE){
					//Let the helper deal with it
					handle_unsigned_multiplication_instruction(window);
					return;
				}
				break;

			default:
				break;
		}
	}

	/**
	 * If we have an assignment that follows a relational operator, we need to do appropriate
	 * setting logic. We'll do this by inserting a setX statement inbetween the relational operator
	 * and the assignment
	 */
	if(is_instruction_binary_operation(window->instruction1) == TRUE
		&& is_operator_relational_operator(window->instruction1->op) == TRUE
		&& window->instruction2->statement_type == THREE_ADDR_CODE_ASSN_STMT
		&& variables_equal(window->instruction1->assignee, window->instruction2->op1, FALSE) == TRUE){

		//Set the comparison and assignment instructions
		instruction_t* comparison = window->instruction1;
		instruction_t* assignment = window->instruction2;

		//Handle the comparison operation here
		handle_cmp_instruction(comparison);

		//We will determine the type signedness *based* on how the op1 is. We don't want to use the assignee, because the assignee for a comparison will nearly
		//always be unsigned
		u_int8_t type_signed = is_type_signed(assignment->op1->type);

		//We'll now need to insert inbetween here
		instruction_t* set_instruction = emit_setX_instruction(comparison->op, emit_temp_var(lookup_type_name_only(cfg->type_symtab, "u8")->type), type_signed);

		//We now also need to modify the move instruction
		//It will always be movzx
		assignment->instruction_type = MOVZX;
		//Assignee and destination are the same
		assignment->destination_register = assignment->assignee;
		//The source is now this set instruction's destination
		assignment->source_register = set_instruction->destination_register;

		//Now once we have the set instruction, we need to insert it between 1 and 2
		insert_instruction_before_given(set_instruction, assignment);

		//Reconstruct the window here, starting at the end
		reconstruct_window(window, assignment);
		return;
	}


	//============================= Address Calculation Optimization  ==============================
	//These are patterns that span multiple instructions. Often we're able to
	//condense these multiple instructions into one singular x86 instruction
	//
	//We want to ensure that we get the best possible outcome for memory movement address calculations.
	//This is where *a lot* of instructions get generated, so it's worth it to spend compilation time
	//compressing these

	//Do we have a case where we have an indirect jump statement? If so we can handle that by condensing it into one
	if(window->instruction1->statement_type == THREE_ADDR_CODE_INDIR_JUMP_ADDR_CALC_STMT
		&& window->instruction2->statement_type == THREE_ADDR_CODE_INDIRECT_JUMP_STMT){
		//This will be flagged as an indirect jump
		window->instruction2->instruction_type = INDIRECT_JMP;

		//By default the true source is this, but we may need to emit a converting move
		three_addr_var_t* true_source = window->instruction1->op2;

		//What is the size of this source variable? It needs
		//to be 32 bits or more to avoid needing a conversion
		switch(true_source->variable_size){
			//These two mean that we're fine
			case QUAD_WORD:
			case DOUBLE_WORD:
				break;

			//Otherwise, a conversion is required
			default:
				//If it is signed, we'll want to preserve the signedness
				if(is_type_signed(true_source->type) == TRUE){
					true_source = handle_expanding_move_operation(window->instruction1, window->instruction1->op2, i32);

				//Otherwise, we'll use the unsigned version
				} else {
					true_source = handle_expanding_move_operation(window->instruction1, window->instruction1->op2, u32);
				}

				break;
		}

		//The source register is op1
		window->instruction2->source_register = true_source;

		//Store the jumping to block where the jump table is
		window->instruction2->if_block = window->instruction1->if_block;

		//We also have an "S" multiplicator factor that will always be a power of 2 stored in the lea_multiplicator
		window->instruction2->lea_multiplicator = window->instruction1->lea_multiplicator;

		//We're now able to delete instruction 1
		delete_statement(window->instruction1);

		//Reconstruct the window with instruction2 as the start
		reconstruct_window(window, window->instruction2);
		return;
	}


	/**
	 * Handle to memory movement with 2 operands
	 *
	 * Something like:
	 * t4 <- stack_pointer_0 + 8
	 * store t4 <- t3
	 * 
	 * Will become mov(w/l/q) t3, 8(stack_pointer_0)
	 */
	if(is_instruction_binary_operation(window->instruction1) == TRUE
		&& window->instruction1->op == PLUS
		&& window->instruction2->statement_type == THREE_ADDR_CODE_STORE_STATEMENT
		&& variables_equal(window->instruction1->assignee, window->instruction2->assignee, TRUE) == TRUE
		&& window->instruction1->assignee->use_count <= 1){

		//Let the helper deal with it
		handle_two_instruction_address_calc_and_store(window->instruction1, window->instruction2);

		//Now that we've done this, instruction 1 is useless
		delete_statement(window->instruction1);

		//And reconstruct the window based on instruction 2
		reconstruct_window(window, window->instruction2);

		//Done here
		return;
	}


	/**
	 * Handle from memory movement with 2 operands
	 *
	 * Something like:
	 * t4 <- stack_pointer_0 + 8
	 * load t3 <- t4
	 * 
	 * Will become mov(w/l/q) 8(stack_pointer_0), t3
	 */
	if(is_instruction_binary_operation(window->instruction1) == TRUE
		&& window->instruction1->op == PLUS
		&& window->instruction2->statement_type == THREE_ADDR_CODE_LOAD_STATEMENT 
		&& variables_equal(window->instruction1->assignee, window->instruction2->op1, TRUE) == TRUE
		&& window->instruction1->assignee->use_count <= 1){

		//Let the helper deal with it
		handle_two_instruction_address_calc_and_load(window->instruction1, window->instruction2);

		//Now that we've done this, instruction 1 is useless
		delete_statement(window->instruction1);

		//And reconstruct the window based on instruction 2
		reconstruct_window(window, window->instruction2);

		//Done here
		return;
	}


	//The instruction that we have here is the window's instruction 1
	instruction_t* instruction = window->instruction1;

	//Switch on whatever we have currently
	switch (instruction->statement_type) {
		case THREE_ADDR_CODE_ASSN_STMT:
			handle_simple_movement_instruction(instruction);
			break;
		case THREE_ADDR_CODE_LOGICAL_NOT_STMT:
			handle_logical_not_instruction(window);
			break;
		case THREE_ADDR_CODE_SETNE_STMT:
			handle_setne_instruction(instruction);
			break;
		case THREE_ADDR_CODE_ASSN_CONST_STMT:
			handle_constant_to_register_move_instruction(instruction);
			break;
		case THREE_ADDR_CODE_LEA_STMT:
			handle_lea_statement(instruction);
			break;
		//One-to-one mapping to nop
		case THREE_ADDR_CODE_IDLE_STMT:
			instruction->instruction_type = NOP;
			break;
		//One to one mapping here as well
		case THREE_ADDR_CODE_RET_STMT:
			instruction->instruction_type = RET;
			//We'll still store this, just in a hidden way
			instruction->source_register = instruction->op1;
			break;
		//These will always just be a JMP - the branch will have
		//more complex rules
		case THREE_ADDR_CODE_JUMP_STMT:
			instruction->instruction_type = JMP;
			break;
		//A branch statement will have more complex
		//selection rules, so we'll use a helper function
		case THREE_ADDR_CODE_BRANCH_STMT:
			handle_branch_instruction(window);
			break;
		//Special case here - we don't change anything
		case THREE_ADDR_CODE_ASM_INLINE_STMT:
			instruction->instruction_type = ASM_INLINE;
			break;
		//The translation here takes the form of a call instruction
		case THREE_ADDR_CODE_FUNC_CALL:
			handle_function_call(instruction);
			break;
		//Similarly, an indirect function call also has it's own kind of
		//instruction
		case THREE_ADDR_CODE_INDIRECT_FUNC_CALL:
			handle_indirect_function_call(instruction);
			break;
		case THREE_ADDR_CODE_INC_STMT:
			handle_inc_instruction(instruction);
			break;
		case THREE_ADDR_CODE_DEC_STMT:
			handle_dec_instruction(instruction);
			break;
		case THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT:
		case THREE_ADDR_CODE_BIN_OP_STMT:
			handle_binary_operation_instruction(instruction);
			break;
		//For a phi function, we perform an exact 1:1 mapping
		case THREE_ADDR_CODE_PHI_FUNC:
			//This is all we'll need
			instruction->instruction_type = PHI_FUNCTION;
			break;
		//Handle a neg statement
		case THREE_ADDR_CODE_NEG_STATEMENT:
			handle_neg_instruction(instruction);
			break;
		//Handle a neg statement
		case THREE_ADDR_CODE_BITWISE_NOT_STMT:
			handle_not_instruction(instruction);
			break;
		//Handle the testing statement
		case THREE_ADDR_CODE_TEST_STMT:
			handle_test_instruction(instruction);
			break;
		case THREE_ADDR_CODE_LOAD_STATEMENT:
			//Let the helper do it
			handle_load_instruction(instruction);
			break;
		case THREE_ADDR_CODE_LOAD_WITH_CONSTANT_OFFSET:
			handle_load_with_constant_offset_instruction(instruction);
			break;
		case THREE_ADDR_CODE_LOAD_WITH_VARIABLE_OFFSET:
			handle_load_with_variable_offset_instruction(instruction);
			break;
		case THREE_ADDR_CODE_STORE_STATEMENT:
			handle_store_instruction(instruction);
			break;
		case THREE_ADDR_CODE_STORE_WITH_CONSTANT_OFFSET:
			handle_store_with_constant_offset_instruction(instruction);
			break;
		case THREE_ADDR_CODE_STORE_WITH_VARIABLE_OFFSET:
			handle_store_with_variable_offset_instruction(instruction);
			break;
		case THREE_ADDR_CODE_MEM_ADDRESS_STMT:
			handle_memory_address_instruction(cfg, cfg->stack_pointer, instruction);
			break;
		default:
			break;
	}
}


/**
 * Run through every block and convert each instruction or sequence of instructions
 * from three address code to assembly statements
 */
static void select_instructions(cfg_t* cfg, basic_block_t* head_block){
	//Save the current block here
	basic_block_t* current = head_block;

	while(current != NULL){
		//Initialize the sliding window(very basic, more to come)
		instruction_window_t window = initialize_instruction_window(current);

		//Run through the window so long as we are not at the end
		do{
			//Select the instructions
			select_instruction_patterns(cfg, &window);

			//Slide the window
			slide_window(&window);

		//Keep going if we aren't at the end
		} while(window.instruction1 != NULL);

		//Advance the current up
		current = current->direct_successor;
	}
}


/**
 * A function that selects all instructions, via the peephole method. This kind of 
 * operation completely translates the CFG out of a CFG. When done, we have a straight line
 * of code that we print out
 */
void select_all_instructions(compiler_options_t* options, cfg_t* cfg){
	//Grab these two general use types first
	u64 = lookup_type_name_only(cfg->type_symtab, "u64")->type;
	i32 = lookup_type_name_only(cfg->type_symtab, "i32")->type;
	u32 = lookup_type_name_only(cfg->type_symtab, "u32")->type;
	u8 = lookup_type_name_only(cfg->type_symtab, "u8")->type;

	//Our very first step in the instruction selector is to order all of the blocks in one 
	//straight line. This step is also able to recognize and exploit some early optimizations,
	//such as when a block ends in a jump to the block right below it
	basic_block_t* head_block = order_blocks(cfg);

	//Do we need to print intermediate representations?
	u_int8_t print_irs = options->print_irs;

	//We'll first print before we simplify
	if(print_irs == TRUE){
		printf("============================== BEFORE SIMPLIFY ========================================\n");
		print_ordered_blocks(cfg, head_block, PRINT_THREE_ADDRESS_CODE);
		printf("============================== AFTER SIMPLIFY ========================================\n");
	}

	//Once we've printed, we now need to simplify the operations. OIR already comes in an expanded
	//format that is used in the optimization phase. Now, we need to take that expanded IR and
	//recognize any redundant operations, dead values, unnecessary loads, etc.
	simplify(cfg, head_block);

	//If we need to print IRS, we can do so here
	if(print_irs == TRUE){
		print_ordered_blocks(cfg, head_block, PRINT_THREE_ADDRESS_CODE);
		printf("============================== AFTER INSTRUCTION SELECTION ========================================\n");
	}

	//Once we're done simplifying, we'll use the same sliding window technique to select instructions.
	select_instructions(cfg, head_block);

	//Final IR printing if requested by user
	if(print_irs == TRUE){
		print_ordered_blocks(cfg, head_block, PRINT_INSTRUCTION);
	}
}
