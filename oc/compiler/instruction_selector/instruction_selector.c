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
#include <sys/select.h>
#include <sys/types.h>

//We'll need this a lot, so we may as well have it here
static generic_type_t* u64;
static generic_type_t* i64;
static generic_type_t* u32;
static generic_type_t* i32;
static generic_type_t* u8;

//A holder for the stack pointer
three_addr_var_t* stack_pointer_variable;
//A holder for the instruction pointer
three_addr_var_t* instruction_pointer_variable;

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
 * Is the source register for a given move instruction "clean" or not. Clean means
 * that we know where it comes from *and* we know where it's going. For example, 
 * temporary variables that are not returned are known to be clean, as well as variables
 * that are entirely local. The only examples of "unclean" variables would be function
 * parameters & values that we're returning
 */
static inline u_int8_t is_source_register_clean(three_addr_var_t* source_register){
	switch(source_register->membership){
		//These are considered dirty - require a full movement instruction
		case RETURNED_VARIABLE:
			return FALSE;
		case FUNCTION_PARAMETER:
			//No linked var - must be clean
			if(source_register->linked_var == NULL){
				return TRUE;
			}

			//If this itself is the original parameter, then it's dirty
			if(IS_ORIGINAL_FUNCTION_PARAMETER(source_register->linked_var) == TRUE){
				return FALSE;
			}

			//Otherwise this is just the alias of that function parameter - so there is nothing
			//to clean up
			return TRUE;

		//Everything else - nothing to worry about
		default:
			return TRUE;
	}
}


/**
 * Is the given instruction a conversion instruction with an SSE destination register?
 *
 * Examples are statements like CVTSI2SDL, which take an i32 and turn it into an f64 
 */
static inline u_int8_t is_integer_to_sse_conversion_instruction(instruction_type_t instruction_type){
	switch(instruction_type){
		case CVTSI2SDL:
		case CVTSI2SDQ:
		case CVTSI2SSL:
		case CVTSI2SSQ:
			return TRUE;
		default:
			return FALSE;
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
static void order_blocks(cfg_t* cfg){
	//We'll first wipe the visited status on this CFG
	reset_visited_status(cfg, TRUE);
	
	//We will perform a breadth first search and use the "direct successor" area
	//of the blocks to store them all in one chain per function. The functions themselves
	//are separated and stored individually, because in ollie a function is the smallest unit
	//of procedures
	
	//We'll need to use a queue every time, we may as well just have one big one
	heap_queue_t queue = heap_queue_alloc();

	//For each function
	for(u_int16_t _ = 0; _ < cfg->function_entry_blocks.current_index; _++){
		//Grab the function block out
		basic_block_t* func_block = dynamic_array_get_at(&(cfg->function_entry_blocks), _);

		//These get reset for every function because each function has its own
		//separate ordering
		basic_block_t* previous = NULL;

		//This function start block is the begging of our BFS	
		enqueue(&queue, func_block);
		
		//So long as the queue is not empty
		while(queue_is_empty(&queue) == FALSE){
			//Grab this block off of the queue
			basic_block_t* current = dequeue(&queue);

			//If previous is NULL, this is the first block
			if(previous == NULL){
				//Keep track of what previous is
				previous = current;

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
				enqueue(&queue, direct_end_jump);
			}

			//Now we'll go through each of the successors in this node
			for(u_int16_t idx = 0; idx < current->successors.current_index; idx++){
				//Now as we go through here, if the direct end jump wasn't NULL, we'll have already added it in. We don't
				//want to have that happen again, so we'll make sure that if it's not NULL we don't double add it

				//Grab the successor
				basic_block_t* successor = dynamic_array_get_at(&(current->successors), idx);

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
					enqueue(&queue, successor);
				}
			}
		}
	}

	//Destroy the queue when done
	heap_queue_dealloc(&queue);
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
static void print_ordered_blocks(cfg_t* cfg, instruction_printing_mode_t mode){
	//Run through all of the functions
	for(u_int16_t i = 0; i < cfg->function_entry_blocks.current_index; i++){
		//Extract the entry block. This is our starting point
		basic_block_t* current = dynamic_array_get_at(&(cfg->function_entry_blocks), i);

		//So long as this one isn't NULL
		while(current != NULL){
			//Print it
			print_ordered_block(current, mode);
			//Advance to the direct successor
			current = current->direct_successor;
		}
	}

	//Print all global variables after the blocks
	print_all_global_variables(stdout, &(cfg->global_variables));
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
			constant->constant_value.signed_integer_constant = log2_of_known_power_of_2(constant->constant_value.signed_integer_constant);
			break;

		case INT_CONST_FORCE_U:
			constant->constant_value.unsigned_integer_constant = log2_of_known_power_of_2(constant->constant_value.unsigned_integer_constant);
			break;

		case LONG_CONST:
			constant->constant_value.signed_long_constant = log2_of_known_power_of_2(constant->constant_value.signed_long_constant);
			break;

		case LONG_CONST_FORCE_U:
			constant->constant_value.unsigned_long_constant = log2_of_known_power_of_2(constant->constant_value.unsigned_long_constant);
			break;

		case SHORT_CONST:
			constant->constant_value.signed_short_constant = log2_of_known_power_of_2(constant->constant_value.signed_short_constant);
			break;

		case SHORT_CONST_FORCE_U:
			constant->constant_value.unsigned_short_constant = log2_of_known_power_of_2(constant->constant_value.unsigned_short_constant);
			break;

		case BYTE_CONST:
			constant->constant_value.signed_byte_constant = log2_of_known_power_of_2(constant->constant_value.signed_byte_constant);
			break;

		case BYTE_CONST_FORCE_U:
			constant->constant_value.unsigned_byte_constant = log2_of_known_power_of_2(constant->constant_value.unsigned_byte_constant);
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
static inline void reconstruct_window(instruction_window_t* window, instruction_t* seed){
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
static inline instruction_window_t* slide_window(instruction_window_t* window){
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
	if(is_converting_move_required(assignment_instruction->assignee->type, assignment_instruction->op1->type) == TRUE){
		return FALSE;
	}

	//Otherwise if we get here, then we can
	return TRUE;
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
 * Remediate a memory address that is *not* in a memory access(load or store) context. This will primarily
 * be hit when we're taking memory addresses or doing pointer arithmetic with arrays
 */
static void remediate_memory_address_in_non_access_context(instruction_window_t* window, instruction_t* instruction){
	//Grab this out
	symtab_variable_record_t* var = instruction->op1->linked_var;

	/**
	 * Handle a standard case - we have a variable that is going to be an address
	 * on a stack
	 */
	if(var->membership != GLOBAL_VARIABLE){
		//We have no stack region - this likely means that it's a
		//reference parameter of some kind. In this case, we will *remove*
		//the special memory type of this paramter and just let it use
		//the variable as normal
		if(var->stack_region == NULL){
			//Remediate the type here
			instruction->op1->variable_type = VARIABLE_TYPE_NON_TEMP;

			//And just let it go now 
			return;
		}


		//Extract the stack offset for our use. This will determine how 
		//we process things down below
		int64_t stack_offset = var->stack_region->base_address;

		//Go based on what kind of statement that we've got here
		switch(instruction->statement_type){
			//If we have an assignment statement, we
			//can turn this into a lea with an offset or a
			//straight assignment depending on the offset
			case THREE_ADDR_CODE_ASSN_STMT:
				//Make it a lea
				if(stack_offset != 0){
					instruction->statement_type = THREE_ADDR_CODE_LEA_STMT;

					//Op1 becomes that stack pointer
					instruction->op1 = stack_pointer_variable;

					//And op1_const is our offset
					instruction->op1_const = emit_direct_integer_or_char_constant(stack_offset, u64);

					//This is a lea with an offset only
					instruction->lea_statement_type = OIR_LEA_TYPE_OFFSET_ONLY;

				//Otherwise, we'll just swap the var out with the stack pointer since
				//they're one in the same
				} else {
					instruction->op1 = stack_pointer_variable;
				}

				break;

			//For a statement like this, we will merge the existing
			//constant in. We know that the only two possible operands
			//with a memory address are Plus/minus, so we only need to 
			//account for 2 cases here
			case THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT:
				//Make it a lea
				if(stack_offset != 0){
					//Emit the constant
					three_addr_const_t* lea_constant = emit_direct_integer_or_char_constant(stack_offset, i64);

					//Simplify based on what we have
					switch(instruction->op){
						case PLUS:
							add_constants(lea_constant, instruction->op1_const);
							break;

						case MINUS:
							subtract_constants(lea_constant, instruction->op1_const);
							break;

						//This should be impossible, if we get here it's a hard out
						default:
							printf("Fatal internal compiler error. Attempt to do a binary operation that is not +/- with a memory address\n");
							exit(1);
					}

					//Wipe out the operator
					instruction->op = BLANK;

					//Op1 becomes that stack pointer
					instruction->op1 = stack_pointer_variable;

					//Op1 const is the lea constant
					instruction->op1_const = lea_constant;

					//Change the instruction type to a lea
					instruction->statement_type = THREE_ADDR_CODE_LEA_STMT;

					//This is an offset only
					instruction->lea_statement_type = OIR_LEA_TYPE_OFFSET_ONLY;

				//Otherwise, we'll just swap the var out with the stack pointer since
				//they're one in the same
				} else {
					instruction->op1 = stack_pointer_variable;
				}

				break;

			//Final and trickiest case. We need to have a memory calculation *and* a regular
			//calculation stuffed into here, but we only have 2 operands to work with. We will
			//need to use our special version of a lea for this in most cases
			case THREE_ADDR_CODE_BIN_OP_STMT:
				//Make it a lea, we'll need to use op2
				//for the second variable
				if(stack_offset != 0){
					//Create the offset constant
					three_addr_const_t* stack_offset_constant = emit_direct_integer_or_char_constant(stack_offset, i64);

					//This is now our op1_const
					instruction->op1_const = stack_offset_constant;

					//Op1 becomes the stack pointer
					instruction->op1 = stack_pointer_variable;

					//Finally declare that this is a lea statement
					instruction->statement_type = THREE_ADDR_CODE_LEA_STMT;

					//Go based on the op here
					switch(instruction->op){
						//In this case, we'd have something like t5 <- <offset>(t4, t5)
						case PLUS:
							//This is a lea statement with registers and an offset
							instruction->lea_statement_type = OIR_LEA_TYPE_REGISTERS_AND_OFFSET;
							
							//Nothing else to do here
							break;
						
						//For a minus, we'll need to circumvent the system by using a -1 multiplier
						//to make this still work for our lea. Since we have op1 - op2, we can rewrite
						//this into op1 + op2 * -1
						case MINUS:
							//Full stack here
							instruction->lea_statement_type = OIR_LEA_TYPE_REGISTERS_OFFSET_AND_SCALE;

							//-1 to mimic the subtraction
							instruction->lea_multiplier = -1;

							break;
						
						//Unreachable path - hard fail if we somehow get to this
						default:
							printf("Fatal internal compiler error: Invalid binary operand found on address calculation\n");
							exit(1);
					}

					//Wipe out the op once we're done
					instruction->op = BLANK;
					
				//Then again all we need to do here is set the op1
				//to be our stack pointer
				} else {
					instruction->op1 = stack_pointer_variable;
				}

				break;

			//This should never happen
			default:
				printf("Fatal internal compiler error: unreachable path hit in memory address remediation\n");
				exit(1);
		}
	
	/**
	 * Otherwise it is a global variable, and we will treat it as such. Global variables will generate 2 instructions on most occassions
	 * the lea instruction to grab the address and then the actual address manipulation in the binary operation. Note that for these steps,
	 * window reconstruction is required
	 */
	} else {
		//The global variable address calculation
		instruction_t* global_var_address_instruction;

		switch(instruction->statement_type){
			/**
			 * A global variable address assignment like this will turn into a leaq statement
			 */
			case THREE_ADDR_CODE_ASSN_STMT:
				//Let the helper emit the statement
				global_var_address_instruction = emit_global_variable_address_calculation_oir(instruction->assignee, instruction->op1, instruction_pointer_variable);

				//Insert this after the given instruction
				insert_instruction_after_given(global_var_address_instruction, instruction);

				//Once we've done that, the old instruction is useless to us
				delete_statement(instruction);

				//Rebuild the window based around the new instruction
				reconstruct_window(window, global_var_address_instruction);

				break;

			/**
			 * A global var address assignment like this will generate
			 * 2 separate instructions. One instruction will hold the global variable address,
			 * while the other holds the actual binary operation
			 */
			case THREE_ADDR_CODE_BIN_OP_STMT:
				//Let the helper emit the statement. We will use a temp destination for this
				global_var_address_instruction = emit_global_variable_address_calculation_oir(emit_temp_var(u64), instruction->op1, instruction_pointer_variable);

				//This goes in before the given one
				insert_instruction_before_given(global_var_address_instruction, instruction);

				//We'll now replace op1 with what our assignee here is
				instruction->op1 = global_var_address_instruction->assignee;

				//And now we'll reconstruct around our instruction just ot keep the window in order
				reconstruct_window(window, instruction);

				break;

			/**
			 * A global var address like this will generate one special instruction that is RIP relative
			 * with a constant offset
			 */
			case THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT:
				//Let the helper do all of the work
				global_var_address_instruction = emit_global_variable_address_calculation_with_offset_oir(instruction->assignee, instruction->op1, instruction_pointer_variable, instruction->op1_const);

				//This goes in before the given one
				insert_instruction_before_given(global_var_address_instruction, instruction);

				//Now we can delete the old one
				delete_statement(instruction);
				
				//And reconstruct the window around the new one
				reconstruct_window(window, global_var_address_instruction);

				break;

			//This should never happen
			default:
				printf("Fatal internal compiler error: unreachable path hit in memory address remediation\n");
				exit(1);
		}
	}
}


/**
 * The pattern optimizer takes in a window and performs hyperlocal optimzations
 * on passing instructions. If we do end up deleting instructions, we'll need
 * to take care with how that affects the window that we take in
 */
static u_int8_t simplify_window(instruction_window_t* window){
	//By default, we didn't change anything
	u_int8_t changed = FALSE;

	//Let's perform some quick checks. If we see a window where the first instruction
	//is NULL or the second one is NULL, there's nothing we can do. We'll just leave in this
	//case
	if(window->instruction1 == NULL || window->instruction2 == NULL){
		return changed;
	}

	/**
	 * Memory address rememediation - if we have non store/load
	 * instructions and we want to remediate their memory addresses,
	 * we can come through here and do so now. These may be situations
	 * where we are not doing any kind of storing or loading, but instead
	 * pointer arithmetic or grabbing memory addresses. We know for a fact
	 * that the "op1" is always going to be the memory address
	 */
	instruction_t* first = window->instruction1;
	instruction_t* second = window->instruction2; 
	instruction_t* third = window->instruction3; 

	//Check if we have any such cases for the first in the window
	switch(first->statement_type){
		case THREE_ADDR_CODE_ASSN_STMT:
		case THREE_ADDR_CODE_BIN_OP_STMT:
		case THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT:
			//If it is a memory address, then we'll do this
			if(first->op1->variable_type == VARIABLE_TYPE_MEMORY_ADDRESS){
				remediate_memory_address_in_non_access_context(window, first);
			}
			
			break;

		//By default do nothing
		default:
			break;
	}

	//Check if we have any such cases for the second in the window
	switch(second->statement_type){
		case THREE_ADDR_CODE_ASSN_STMT:
		case THREE_ADDR_CODE_BIN_OP_STMT:
		case THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT:
			//If it is a memory address, then we'll do this
			if(second->op1->variable_type == VARIABLE_TYPE_MEMORY_ADDRESS){
				remediate_memory_address_in_non_access_context(window, second);
			}
			
			break;

		//By default do nothing
		default:
			break;
	}

	//If we have a third - remember this is not a guarantee for all windows
	if(third != NULL){
		//Check if we have any such cases for the third in the window
		switch(third->statement_type){
			case THREE_ADDR_CODE_ASSN_STMT:
			case THREE_ADDR_CODE_BIN_OP_STMT:
			case THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT:
				//If it is a memory address, then we'll do this
				if(third->op1->variable_type == VARIABLE_TYPE_MEMORY_ADDRESS){
					remediate_memory_address_in_non_access_context(window, third);
				}
				
				break;

			//By default do nothing
			default:
				break;
		}
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
		&& window->instruction1->assignee->variable_type == VARIABLE_TYPE_TEMP 
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
	if(window->instruction2 != NULL 
		&& window->instruction2->statement_type == THREE_ADDR_CODE_BIN_OP_STMT
		&& window->instruction1->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT){
		//Is the variable in instruction 1 temporary *and* the same one that we're using in instruction2? Let's check.
		if(window->instruction1->assignee->variable_type == VARIABLE_TYPE_TEMP
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

			//Reconstruct using the previous instruction or instruction
			//2, if we're blanked out
			if(window->instruction2->previous_statement != NULL){
				reconstruct_window(window, window->instruction2->previous_statement);
			} else {
				reconstruct_window(window, window->instruction2);
			}

			//This does count as a change
			changed = TRUE;
		}
	}

	//Now check with 1 and 3. The prior compression may have made this more worthwhile
	if(window->instruction3 != NULL && window->instruction3->statement_type == THREE_ADDR_CODE_BIN_OP_STMT
		&& window->instruction1->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT){
		//Is the variable in instruction 1 temporary *and* the same one that we're using in instrution2? Let's check.
		if(window->instruction1->assignee->variable_type == VARIABLE_TYPE_TEMP
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
		&& window->instruction1->assignee->variable_type == VARIABLE_TYPE_TEMP
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

		//Op1 is now used one less time
		window->instruction2->op1->use_count--;

		//Null out where the old value was
		window->instruction2->op1 = NULL;

		//Instruction 2 is now simply an assign const statement
		window->instruction2->statement_type = THREE_ADDR_CODE_ASSN_CONST_STMT;

		//Instruction 1 is now completely useless *if* that was the only time that
		//his assignee was used. Otherwise, we need to keep it in
		delete_statement(window->instruction1);

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
		&& window->instruction2->assignee->variable_type == VARIABLE_TYPE_TEMP
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

		//Decrement the use counts
		window->instruction3->op1->use_count--;

		//Null out where the old value was
		window->instruction3->op1 = NULL;

		//Instruction 2 is now simply an assign const statement
		window->instruction3->statement_type = THREE_ADDR_CODE_ASSN_CONST_STMT;

		//Instruction2 is now useless
		delete_statement(window->instruction2);

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
		if(first->assignee->variable_type == VARIABLE_TYPE_TEMP && variables_equal(first->assignee, second->op1, TRUE) == TRUE
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
		&& is_load_operation(window->instruction1) == TRUE){
		//Grab these out for convenience
		instruction_t* load = window->instruction1;
		instruction_t* move = window->instruction2;
		
		//If the variables are temp and the first one's assignee is the same as the second's op1, we can fold
		if(load->assignee->variable_type == VARIABLE_TYPE_TEMP && variables_equal(load->assignee, move->op1, TRUE) == TRUE
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
		if(window->instruction1->assignee->variable_type == VARIABLE_TYPE_TEMP 
			//Make sure that this is the only use
			&& window->instruction1->assignee->use_count <= 1
			&& window->instruction1->op1->variable_type != VARIABLE_TYPE_TEMP
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
		if(first->assignee->variable_type == VARIABLE_TYPE_TEMP 
			&& third->assignee->variable_type != VARIABLE_TYPE_TEMP
			&& first->assignee->use_count <= 2 
			&& variables_equal_no_ssa(first->op1, third->assignee, FALSE) == TRUE
	 		&& variables_equal(first->assignee, second->op1, FALSE) == TRUE
	 		&& variables_equal(second->assignee, third->op1, FALSE) == TRUE){

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
	 * ====================== Translating multiplications into leas if compatible ================
	 * If we have something like:
	 * 	t27 <- t26 * 8
	 *
	 * Since 8 is a power of 2, we are actually able to translate this into a lea. We want to
	 * do this because lea instructions, when multiplying, generate fewer instructions than
	 * actually doing multiplication. This is reserved for cases where the assignee and op1
	 * are not equal. These usually arise in address calculation scenarios
	 *
	 * We will check both instructions 1 and 2 for this to get as much as we can on one go
	 */
	if(window->instruction1->statement_type == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT
		&& window->instruction1->op == STAR
		&& is_constant_lea_compatible_power_of_2(window->instruction1->op1_const) == TRUE //Must be: 1, 2, 4, 8 for lea
		&& variables_equal(window->instruction1->assignee, window->instruction1->op1, FALSE) == FALSE){

		//This is now a lea statement
		window->instruction1->statement_type = THREE_ADDR_CODE_LEA_STMT;

		//The lea type will be scale and index
		window->instruction1->lea_statement_type = OIR_LEA_TYPE_INDEX_AND_SCALE;
		
		//Knock out the op
		window->instruction1->op = BLANK;

		//Copy over from the constant to the lea multiplier
		window->instruction1->lea_multiplier = window->instruction1->op1_const->constant_value.signed_long_constant;

		//We can now null out the constant
		window->instruction1->op1_const = NULL;

		//This counts as a change
		changed = TRUE;
	}

	if(window->instruction2 != NULL
		&& window->instruction2->statement_type == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT
		&& window->instruction2->op == STAR
		&& is_constant_lea_compatible_power_of_2(window->instruction2->op1_const) == TRUE //Must be: 1, 2, 4, 8 for lea
		&& variables_equal_no_ssa(window->instruction2->assignee, window->instruction2->op1, FALSE) == FALSE){

		//This is now a lea statement
		window->instruction2->statement_type = THREE_ADDR_CODE_LEA_STMT;

		//The lea type will be scale and index
		window->instruction2->lea_statement_type = OIR_LEA_TYPE_INDEX_AND_SCALE;
		
		//Knock out the op
		window->instruction2->op = BLANK;

		//Copy over from the constant to the lea multiplier
		window->instruction2->lea_multiplier = window->instruction2->op1_const->constant_value.signed_long_constant;

		//We can now null out the constant
		window->instruction2->op1_const = NULL;

		//This counts as a change
		changed = TRUE;
	}


	/**
	 * --------------------- Folding constant assingments in LEA statements with ----------------------
	 *  In cases where we have a lea statement that uses a constant which is assigned to a temporary
	 *  variable right before it, we should eliminate that unnecessary assingment by folding that constant
	 *  into the lea statement.
	 *
	 *  CASES HANDLED:
	 *
	 *  Case 1 -> two registers plus multiplicator:
	 *  	t4 <- 4
	 *  	t5 <- (t2, t4,  4)
	 *
	 *  	Turns into:
	 *  		t5 <- 16(t2)
	 *
	 *  Case 2 -> two register, multiplicator and constant
	 *	   	t4 <- 5
	 *	   	t5 <- 500(t2, t4, 4)
	 *	   	
	 *	   	Turns into:
	 *	   		t5 <- 520(t2)
	 *
	 *  Case 3 -> two registers with no multiplicator
	 *  	t4 <- 4
	 *  	t5 <- t2 + t4
	 *
	 * 		Turns into:
	 * 			t5 <- 4(t2)
	 *
	 * 	Case 4 -> two registers and a constant
	 *  	t4 <- 4
	 *  	t5 <- 500(t2, t4)
	 *
	 * 		Turns into:
	 * 			t5 <- 504(t2)
	 *
	 * 	Case 5 -> One register and a constant
	 *  	t4 <- 4
	 *  	t5 <- 500(t4)
	 *  	
	 *  	Turns into:
	 *  		t5 <- 504(no longer a lea)
	 *
	 *  Case 6 ->  Two registers(op1)
	 *  	t4 <- 4
	 *  	t5 <- (t4, t7)
	 *
	 *  	Turns into:
	 *  		t5 <- 4(t7)
	 *
	 *  Case 7 ->  Two registers and offset(op1)
	 *  	t4 <- 4
	 *  	t5 <- 500(t4, t7)
	 *
	 *  	Turns into:
	 *  		t5 <- 504(t7)
	 *
	 *  Case 8 -> Two registers and scale
	 *  	t4 <- 4
	 *  	t5 <- (t4, t7, 4)
	 *
	 * 		Turns into:
	 * 		  t5 <- 4(, t7, 4)
	 *
	 *  Case 9 -> Two registers, offset and scale
	 *  	t4 <- 4
	 *  	t5 <- 44(t4, t7, 4)
	 *
	 * 		Turns into:
	 * 		  t5 <- 48(, t7, 4)
	 */
	if(window->instruction2 != NULL 
		&& window->instruction2->statement_type == THREE_ADDR_CODE_LEA_STMT
		&& window->instruction1->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT
		&& window->instruction1->assignee->variable_type == VARIABLE_TYPE_TEMP
		&& window->instruction1->assignee->use_count <= 1){

		//Grab this for clarity
		instruction_t* move_instruction = window->instruction1;
		instruction_t* lea_instruction = window->instruction2;

		//For holding our multipliers/constants
		three_addr_const_t* lea_constant;
		int64_t lea_multiplier;

		//First 4 cases - if op2 and the assignee are equal
		if(variables_equal(move_instruction->assignee, lea_instruction->op2, FALSE) == TRUE){
			//Go based on what kind of lea we've got here
			switch(lea_instruction->lea_statement_type){
				/**
				 * 	t4 <- 4
				 * 	t5 <- (t2, t4,  4)
				 *
				 * 	Turns into:
				 * 		t5 <- 16(t2)
				 */
				case OIR_LEA_TYPE_REGISTERS_AND_SCALE:
					//Let's extract the constants that we're after here
					lea_multiplier = lea_instruction->lea_multiplier;

					//This will become the lea's constant
					lea_constant = move_instruction->op1_const;

					//Now let's multiply the 2 together, the result will be in the lea constant
					lea_constant = multiply_constant_by_raw_int64_value(lea_constant, i64, lea_multiplier);
					
					//Finally, we can reconstruct the entire thing
					lea_instruction->op1_const = lea_constant;

					//This no longer exists
					lea_instruction->op2 = NULL;

					//The calculation type is now offset only
					lea_instruction->lea_statement_type = OIR_LEA_TYPE_OFFSET_ONLY;

					//Delete the move now
					delete_statement(move_instruction);
					
					//Reconstruct around the lea
					reconstruct_window(window, lea_instruction);

					//Counts as a change
					changed = TRUE;

					break;
					
				/**
				 * t4 <- 5
				 * t5 <- 500(t2, t4, 4)
				 *	   	
				 *	Turns into:
				 *		t5 <- 520(t2)
				 **/
				case OIR_LEA_TYPE_REGISTERS_OFFSET_AND_SCALE:
					//Let's extract the constants that we're after here
					lea_multiplier = lea_instruction->lea_multiplier;

					//This will become the lea's constant
					lea_constant = move_instruction->op1_const;

					//First step, multiply the constant by the lea multiplier
					lea_constant = multiply_constant_by_raw_int64_value(lea_constant, i64, lea_multiplier);

					//Following that, we will add it to the existing offset. The result will
					//be in the offset constant itself
					add_constants(lea_instruction->op1_const, lea_constant);

					//This no longer exists
					lea_instruction->op2 = NULL;

					//The calculation type is now offset only
					lea_instruction->lea_statement_type = OIR_LEA_TYPE_OFFSET_ONLY;

					//Delete the move now
					delete_statement(move_instruction);
					
					//Reconstruct around the lea
					reconstruct_window(window, lea_instruction);

					//Counts as a change
					changed = TRUE;

					break;


				/**
				 * t4 <- 4
				 * t5 <- t2 + t4
				 *
				 * Turns into:
				 *   t5 <- 4(t2)
				 */
				case OIR_LEA_TYPE_REGISTERS_ONLY:
					//Copy it over
					lea_instruction->op1_const = move_instruction->op1_const;

					//This no longer exists
					lea_instruction->op2 = NULL;

					//The calculation type is now offset only
					lea_instruction->lea_statement_type = OIR_LEA_TYPE_OFFSET_ONLY;

					//Delete the move now
					delete_statement(move_instruction);
					
					//Reconstruct around the lea
					reconstruct_window(window, lea_instruction);

					//Counts as a change
					changed = TRUE;

					break;

				/**
				 * t4 <- 4
				 * t5 <- 500(t2, t4)
				 *
				 * Turns into:
				 *  	t5 <- 504(t2)
				 */
				case OIR_LEA_TYPE_REGISTERS_AND_OFFSET:
					//Add the 2 together, the result is in the lea's op1_const
					add_constants(lea_instruction->op1_const, move_instruction->op1_const);

					//This no longer exists
					lea_instruction->op2 = NULL;

					//The calculation type is now offset only
					lea_instruction->lea_statement_type = OIR_LEA_TYPE_OFFSET_ONLY;

					//Delete the move now
					delete_statement(move_instruction);
					
					//Reconstruct around the lea
					reconstruct_window(window, lea_instruction);

					//Counts as a change
					changed = TRUE;

					break;

				//Otherwise - nothing for us to do with this
				default:
					break;
			}

		//The final case - we just have the op1 as equal
		} else if(variables_equal(move_instruction->assignee, lea_instruction->op1, FALSE) == TRUE){
			switch(lea_instruction->lea_statement_type){
				/**
				 * t4 <- 4
				 * t5 <- 500(t4)
				 *  	
				 * Turns into:
				 *	t5 <- 504(no longer a lea)
				 */
				case OIR_LEA_TYPE_OFFSET_ONLY:
					//Add the 2 together, the result is in the lea's op1_const
					add_constants(lea_instruction->op1_const, move_instruction->op1_const);
					
					//Null out the addressing mode and ops
					lea_instruction->lea_statement_type = OIR_LEA_TYPE_NONE;
					lea_instruction->op1 = NULL;
					lea_instruction->op2 = NULL;

					//This is actually no longer a lea now, it's a pure assignment instruction
					lea_instruction->statement_type = THREE_ADDR_CODE_ASSN_CONST_STMT;

					//Delete the move now
					delete_statement(move_instruction);
					
					//Reconstruct around the lea
					reconstruct_window(window, lea_instruction);

					//Counts as a change
					changed = TRUE;

					break;

				/**
				 * t4 <- 4
				 * t5 <- (t4, t7)
				 *
				 * Turns into:
				 *	t5 <- 4(t7)
				 */
				case OIR_LEA_TYPE_REGISTERS_ONLY:
					//Copy the constant right on over
					lea_instruction->op1_const = move_instruction->op1_const;

					//op2 becomes op1
					lea_instruction->op1 = lea_instruction->op2;

					//Now remove the old op2
					lea_instruction->op2 = NULL;

					//This is now an offset only type statement
					lea_instruction->lea_statement_type = OIR_LEA_TYPE_OFFSET_ONLY;

					//Delete the move now
					delete_statement(move_instruction);
					
					//Reconstruct around the lea
					reconstruct_window(window, lea_instruction);

					//Counts as a change
					changed = TRUE;
					
					break;

				/**
				 * t4 <- 4
				 * t5 <- 500(t4, t7)
				 *
				 * Turns into:
				 *	t5 <- 504(t7)
				 */
				case OIR_LEA_TYPE_REGISTERS_AND_OFFSET:
					//Sum the 2 together, result is in the lea's offset
					add_constants(lea_instruction->op1_const, move_instruction->op1_const);

					//op2 becomes op1
					lea_instruction->op1 = lea_instruction->op2;

					//Now remove the old op2
					lea_instruction->op2 = NULL;

					//This is now an offset only type statement
					lea_instruction->lea_statement_type = OIR_LEA_TYPE_OFFSET_ONLY;

					//Delete the move now
					delete_statement(move_instruction);
					
					//Reconstruct around the lea
					reconstruct_window(window, lea_instruction);

					//Counts as a change
					changed = TRUE;

					break;

				/**
				 * t4 <- 4
				 * t5 <- (t4, t7, 4)
				 *
				 * Turns into:
				 *   t5 <- 4(, t7, 4)
				 */
				case OIR_LEA_TYPE_REGISTERS_AND_SCALE:
					//Copy the constant over
					lea_instruction->op1_const = move_instruction->op1_const;

					//Now we need to move op2 into the op1 spot
					lea_instruction->op1 = lea_instruction->op2;

					//Null out op2
					lea_instruction->op2 = NULL;

					//This has become a scale and offset type
					lea_instruction->lea_statement_type = OIR_LEA_TYPE_INDEX_OFFSET_AND_SCALE;

					//Delete the move now
					delete_statement(move_instruction);
					
					//Reconstruct around the lea
					reconstruct_window(window, lea_instruction);

					//Counts as a change
					changed = TRUE;

					break;


				/**
				 * t4 <- 4
				 * t5 <- 44(t4, t7, 4)
				 *
				 * Turns into:
				 *  t5 <- 48(, t7, 4)
				 */
				case OIR_LEA_TYPE_REGISTERS_OFFSET_AND_SCALE:
					//Add the two together, result is in the lea
					add_constants(lea_instruction->op1_const, move_instruction->op1_const);

					//Now we need to move op2 into the op1 spot
					lea_instruction->op1 = lea_instruction->op2;

					//Null out op2
					lea_instruction->op2 = NULL;

					//This has become a scale and offset type
					lea_instruction->lea_statement_type = OIR_LEA_TYPE_INDEX_OFFSET_AND_SCALE;

					//Delete the move now
					delete_statement(move_instruction);
					
					//Reconstruct around the lea
					reconstruct_window(window, lea_instruction);

					//Counts as a change
					changed = TRUE;

					break;
					
				//By default - just do nothing
				default:
					break;
			}
		}
	}


	/**
	 * ==================== Lea statement compression ==============================
	 * Do we have 2 lea statements one after the other that can be combined? If so, we
	 * should be doing so before the instruction selection takes place to ensure that we
	 * have the minimum number of instructions possible
	 *
	 * Let's first check to see if we have 2 adjacent lea statements. If we do, we can
	 * do some more investigation
	 *
	 * Cases handled:
	 *
	 * CASE 1: 
	 * 	t4 <- (, t7, 4)
	 * 	t5 <- 4(t4)
	 * 	
	 * 	Can become:
	 * 	 t5 <- 4(, t7, 4)
	 *
	 * CASE 2: 
	 * 	t4 <- 4(, t7, 4)
	 * 	t5 <- 4(t4)
	 * 	
	 * 	Can become:
	 * 	 t5 <- 8(, t7, 4)
	 *
	 * CASE 3: RIP relative addressing
	 * 	t4 <- <global_var>(%rip)
	 * 	t5 <- 4(t4)
	 *
	 * 	t5 <- 4+<global_var>(%rip)
	 *
	 * 
	 * NOTE: This has been written to be extensible. It is by no means exhaustive yet, and
	 * there are likely many other cases not currently handled by this that should be
	 */
	if(window->instruction2 != NULL
		&& window->instruction1->statement_type == THREE_ADDR_CODE_LEA_STMT 
		&& window->instruction1->assignee->variable_type == VARIABLE_TYPE_TEMP //Make sure it's a temp var
		&& window->instruction2->statement_type == THREE_ADDR_CODE_LEA_STMT){

		//Grab these references for our convenience
		instruction_t* first_lea = window->instruction1;
		instruction_t* second_lea = window->instruction2;

		//If the first one's assignee is the second one's op1
		if(variables_equal(first_lea->assignee, second_lea->op1, FALSE) == TRUE){
			//Go based on the first one's addressing mode
			switch(first_lea->lea_statement_type){
				case OIR_LEA_TYPE_INDEX_AND_SCALE:
					//Now go based on the second one's type
					switch(second->lea_statement_type){
						/**
						 * 	t4 <- (, t7, 4)
						 * 	t5 <- 4(t4)
						 * 	
						 * 	Can become:
						 * 	 t5 <- 4(, t7, 4)
						 */
						case OIR_LEA_TYPE_OFFSET_ONLY:
							//Copy over op1
							second_lea->op1 = first_lea->op1;

							//Copy this over too
							second_lea->lea_multiplier = first_lea->lea_multiplier;

							//Now change the type of it
							second_lea->lea_statement_type = OIR_LEA_TYPE_INDEX_OFFSET_AND_SCALE;

							//Now delete the entire first lea - it's useless
							delete_statement(first_lea);

							//Rebuild around the second lea
							reconstruct_window(window, second_lea);

							//This counts as a change
							changed = TRUE;

							break;
							
						//Do nothing
						default:
							break;
					}

					break;

				case OIR_LEA_TYPE_INDEX_OFFSET_AND_SCALE:
					//Now go based on the second one's type
					switch(second->lea_statement_type){
						/**
						 * 	t4 <- 4(, t7, 4)
						 * 	t5 <- 4(t4)
						 * 	
						 * 	Can become:
						 * 	 t5 <- 8(, t7, 4)
						 */
						case OIR_LEA_TYPE_OFFSET_ONLY:
							//Add the 2 constants together - the result is in the second lea's op1_const
							add_constants(second_lea->op1_const, first_lea->op1_const);

							//Copy over op1
							second_lea->op1 = first_lea->op1;

							//Copy this over too
							second_lea->lea_multiplier = first_lea->lea_multiplier;

							//Now change the type of it
							second_lea->lea_statement_type = OIR_LEA_TYPE_INDEX_OFFSET_AND_SCALE;

							//Now delete the entire first lea - it's useless
							delete_statement(first_lea);

							//Rebuild around the second lea
							reconstruct_window(window, second_lea);

							//This counts as a change
							changed = TRUE;

							break;
							
						//Do nothing
						default:
							break;
					}

				case OIR_LEA_TYPE_RIP_RELATIVE:
					switch(second->lea_statement_type){
						/**
						 * CASE 3: RIP relative addressing
						 * 	t4 <- <global_var>(%rip)
						 * 	t5 <- 4(t4)
						 *
						 * 	t5 <- 4+<global_var>(%rip)
						 */
						case OIR_LEA_TYPE_OFFSET_ONLY:
							//Copy over the global var and offset var
							second_lea->op1 = first_lea->op1;
							second_lea->op2 = first_lea->op2;

							//Change the calculation mode
							second_lea->lea_statement_type = OIR_LEA_TYPE_RIP_RELATIVE_WITH_OFFSET;

							//Now the first statement is useless
							delete_statement(first_lea);

							//Rebuild around the second instruction
							reconstruct_window(window, second_lea);

							//This counts as a change
							changed = TRUE;

							break;

						default:
							break;
					}

					break;

				//Just do nothing by default
				default:
					break;
			}
			
		//Rarer but still possible case - is the assignee equal to the op2
		} else if(variables_equal(first_lea->assignee, second_lea->op2, FALSE) == TRUE){
			//Go based on the first one's type
			switch(first_lea->lea_statement_type){
				//Just do nothing by default
				default:
					break;
			}
		}
	}


	/**
	 * ========================= Combining lea's with constant binary operations =======================
	 * This is a rule that is not yet finalized. Scenarios will be added to it as they arise
	 *
	 * CASE 1:
	 * 	 t45 <- global_var(t3)
	 * 	 t46 <- t45 + 12
	 *
	 * 	 Turns into:
	 * 	 	t46 <- 12+global_var(t3)
	 *
	 * CASE 2:
	 * 	t45 <- 12+global_var(t3)
	 * 	t46 <- t45 + 16
	 *
	 * 	Turns into:
	 * 		t46 <- 18+global_var(t3)
	 */
	if(window->instruction2 != NULL
		&& window->instruction1->statement_type == THREE_ADDR_CODE_LEA_STMT
		&& window->instruction1->assignee->variable_type == VARIABLE_TYPE_TEMP //Make sure it's a temp var
		&& window->instruction2->statement_type == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT 
		&& variables_equal(window->instruction1->assignee, window->instruction2->op1, FALSE) == TRUE){

		//Grab these for convenience
		instruction_t* first_lea = window->instruction1;
		instruction_t* second_bin_op = window->instruction2;

		//Go based on what kind of lea we have
		switch(window->instruction1->lea_statement_type){
			/**
			 * 	 t45 <- global_var(t3)
			 * 	 t46 <- t45 + 12
			 *
			 * 	 Turns into:
			 * 	 	t46 <- 12+global_var(t3)
			 */
			case OIR_LEA_TYPE_RIP_RELATIVE:
				//Back-copy the assignee and op1_const
				first_lea->assignee = second_bin_op->assignee;
				first_lea->op1_const = second_bin_op->op1_const;

				//Change the lea type to be rip relative but with an offset
				first_lea->lea_statement_type = OIR_LEA_TYPE_RIP_RELATIVE_WITH_OFFSET;

				//The second instruction is now useless
				delete_statement(second_bin_op);

				//And we will rebuild around the first instruction
				reconstruct_window(window, first_lea);

				//This counts as a change
				changed = TRUE;
				
				break;

			/**
			 * 	t45 <- 12+global_var(t3)
			 * 	t46 <- t45 + 16
			 *
			 * 	Turns into:
			 * 		t46 <- 18+global_var(t3)
			 */
			case OIR_LEA_TYPE_RIP_RELATIVE_WITH_OFFSET:
				//Back-copy the assignee
				first_lea->assignee = second_bin_op->assignee;

				//Add the 2 constants together, the result will be in the first one's constant
				add_constants(first_lea->op1_const, second_bin_op->op1_const);
				
				//No need to change the lea type, it's already what it needs to be

				//The second instruction is now useless
				delete_statement(second_bin_op);

				//And we will rebuild around the first instruction
				reconstruct_window(window, first_lea);

				//This counts as a change
				changed = TRUE;
				break;

			//By default do nothing
			default:
				break;
		}
	}


	/**
	 * ====================== Combining loads and preceeding binary operations =============
	 *
	 * If we have:
	 *
	 * t8 <- t7 + 4
	 * load t5 <- t8
	 *
	 * We can instead combine this to be
	 * load t5 <- t7[4]
	 */
	if(window->instruction2 != NULL
		&& window->instruction2->statement_type == THREE_ADDR_CODE_LOAD_STATEMENT){
		//Go based on the first statement
		switch (window->instruction1->statement_type) {
			case THREE_ADDR_CODE_BIN_OP_STMT:
				//If the first one is used less than once and they match
				if(window->instruction1->assignee->use_count <= 1
					&& window->instruction1->op == PLUS //We can only handle addition
					&& variables_equal(window->instruction1->assignee, window->instruction2->op1, FALSE) == TRUE){

					//This is now a load with variable offset
					window->instruction2->statement_type = THREE_ADDR_CODE_LOAD_WITH_VARIABLE_OFFSET;

					//Copy these both over
					window->instruction2->op1 = window->instruction1->op1;
					window->instruction2->op2 = window->instruction1->op2;

					//Now scrap instruction 1
					delete_statement(window->instruction1);

					//Rebuild around instruction 2
					reconstruct_window(window, window->instruction2);

					//Is a change
					changed = TRUE;
				}

				break;

			//Same treatment for if we have a binary operation with const here
			case THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT:
				//If the first one is used less than once and they match
				if((window->instruction1->assignee->use_count <= 1 || window->instruction1->assignee == window->instruction1->op1)
					&& (window->instruction1->op == PLUS || window->instruction1->op == MINUS) //We can only handle addition/subtraction
					&& variables_equal(window->instruction1->assignee, window->instruction2->op1, FALSE) == TRUE){

					//This is now a load with contant offset
					window->instruction2->statement_type = THREE_ADDR_CODE_LOAD_WITH_CONSTANT_OFFSET;

					//Copy these both over
					window->instruction2->op1 = window->instruction1->op1;
					window->instruction2->offset = window->instruction1->op1_const;

					//If we have a minus, we'll just convert to a negative
					if(window->instruction1->op == MINUS){
						window->instruction2->offset->constant_value.signed_long_constant *= -1;
					}

					//Now scrap instruction 1
					delete_statement(window->instruction1);

					//Rebuild around instruction 2
					reconstruct_window(window, window->instruction2);

					//Is a change
					changed = TRUE;
				}

				break;
				
			//By default do nothing
			default:
				break;
		}
	}


	/**
	 * ====================== Combining stores and preceeding binary operations =============
	 *
	 * If we have:
	 *
	 * t8 <- t7 + 4
	 * load t8 <- t5
	 *
	 * We can instead combine this to be
	 * store t7[4] <- t5
	 */
	if(window->instruction2 != NULL
		&& window->instruction2->statement_type == THREE_ADDR_CODE_STORE_STATEMENT){
		//Go based on the first statement
		switch (window->instruction1->statement_type) {
			case THREE_ADDR_CODE_BIN_OP_STMT:
				//If the first one is used less than once and they match
				if(window->instruction1->assignee->variable_type == VARIABLE_TYPE_TEMP 
					&& window->instruction1->op == PLUS //We can only handle addition
					&& variables_equal(window->instruction1->assignee, window->instruction2->assignee, FALSE) == TRUE){

					//This is now a load with variable offset
					window->instruction2->statement_type = THREE_ADDR_CODE_STORE_WITH_VARIABLE_OFFSET;

					//Copy these both over
					window->instruction2->assignee = window->instruction1->assignee;
					window->instruction2->op1 = window->instruction1->op1;

					//Now scrap instruction 1
					delete_statement(window->instruction1);

					//Rebuild around instruction 2
					reconstruct_window(window, window->instruction2);

					//Is a change
					changed = TRUE;
				}

				break;

			//Same treatment for if we have a binary operation with const here
			case THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT:
				//If the first one is used less than once and they match
				if(window->instruction1->assignee->variable_type == VARIABLE_TYPE_TEMP
					&& (window->instruction1->op == PLUS || window->instruction1->op == MINUS) //We can only handle addition/subtraction
					&& variables_equal(window->instruction1->assignee, window->instruction2->assignee, FALSE) == TRUE){

					//This is now a load with contant offset
					window->instruction2->statement_type = THREE_ADDR_CODE_STORE_WITH_CONSTANT_OFFSET;

					//Copy these both over
					window->instruction2->assignee = window->instruction1->assignee;
					window->instruction2->offset = window->instruction1->op1_const;

					//If we have a minus, we'll just convert to a negative
					if(window->instruction1->op == MINUS){
						window->instruction2->offset->constant_value.signed_long_constant *= -1;
					}

					//Now scrap instruction 1
					delete_statement(window->instruction1);

					//Rebuild around instruction 2
					reconstruct_window(window, window->instruction2);

					//Is a change
					changed = TRUE;
				}

				break;
				
			//By default do nothing
			default:
				break;
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
		if(first->assignee->variable_type == VARIABLE_TYPE_TEMP && second->assignee->variable_type != VARIABLE_TYPE_TEMP 
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
				&& first->assignee->variable_type == VARIABLE_TYPE_TEMP 
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
			instruction_t* setne_instruction = emit_setne_code(emit_temp_var(u8), test_instruction->assignee);

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
			instruction_t* setne_instruction = emit_setne_code(emit_temp_var(u8), test_instruction->assignee);

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
			current_instruction->op1_const->constant_value.signed_long_constant = 1;
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
						if(current_instruction->assignee->variable_type == VARIABLE_TYPE_TEMP){
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
						if(current_instruction->assignee->variable_type == VARIABLE_TYPE_TEMP){
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
						current_instruction->op1_const->constant_value.signed_long_constant = 0;

						//We changed something
						changed = TRUE;

					//Just bail out
					default:
						break;
				}

			//What if we have a power of 2 here? For any kind of multiplication or division, this can
			//be optimized into a left or right shift if we have a compatible type(not a float) *and*
			//the assignee is equal to the variable being multiplied
			} else if(is_constant_power_of_2(constant) == TRUE
						&& variables_equal_no_ssa(current_instruction->assignee, current_instruction->op1, FALSE) == TRUE){
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
		&& window->instruction1->assignee->variable_type == VARIABLE_TYPE_TEMP
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
		&& window->instruction1->assignee->variable_type == VARIABLE_TYPE_TEMP
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
		&& window->instruction1->assignee->variable_type == VARIABLE_TYPE_TEMP
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
		&& window->instruction1->assignee->variable_type == VARIABLE_TYPE_TEMP
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
static u_int8_t simplifier_pass(basic_block_t* entry){
	//First we'll grab the entry
	basic_block_t* current = entry;

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
			changed = simplify_window(&window);

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
 * until we see the first pass where we experience no change at all.
 */
static void simplify(cfg_t* cfg){
	//We will do each function individually for efficiency reasons. This way, if
	//one function requires a lot of simplification, it will not drag the rest of the 
	//functions along with it in each pass
	for(u_int16_t i = 0; i < cfg->function_entry_blocks.current_index; i++){
		//Extract it
		basic_block_t* function_entry = dynamic_array_get_at(&(cfg->function_entry_blocks), i);

		//Let this keep going until we're done changing
		while(simplifier_pass(function_entry) == TRUE);
	}
}


/**
 * Select the appropriate move instruction based on the source & destination
 * sizes, the destination signedness, and anything else that we may need
 *
 * We need to know if the source is a known "clean" SSE value. SSE values are not known
 * to be clean unless we've made them ourselves in the function, so for example,
 * a register parameter in an XMM register would be assumed dirty. This affects whether
 * we use instructions like movss or movaps for floating point values
 *
 * Additionally, for any converting move instructions, we will need to have zeroing logic
 * placed in front of the instruction *if* the destination is an XMM register to maintain
 * this "clean" register idea
 */
static instruction_type_t select_move_instruction(variable_size_t destination_size, variable_size_t source_size, u_int8_t destination_signed, u_int8_t source_clean){
	//These two have the same size, we can select easily
	//and be out of here
	if(destination_size == source_size){
		switch(destination_size) {
			case BYTE:
				return MOVB;

			case WORD:
				return MOVW;

			case DOUBLE_WORD:
				return MOVL;

			case QUAD_WORD:
				return MOVQ;

			case SINGLE_PRECISION:
				if(source_clean == TRUE){
					return MOVSS;
				} else {
					return MOVAPS;
				}

			case DOUBLE_PRECISION:
				if(source_clean == TRUE){
					return MOVSD;
				} else {
					return MOVAPD;
				}

			default:
				printf("Fatal internal compiler error: undefined/invalid variable size encountered in move instruction selector\n");
				exit(1);
		}
	}

	/**
	 * Since we know that the sizes are different, we will need to do some more complicated
	 * switching logic in here
	 */
	switch(source_size){
		case SINGLE_PRECISION:
			switch(destination_size){
				case DOUBLE_PRECISION:
					return CVTSS2SD;

				case DOUBLE_WORD:
					return CVTTSS2SIL;

				case QUAD_WORD:
					return CVTTSS2SIQ;

				default:
					printf("Fatal internal compiler error: undefined/invalid destination variable size encountered in single precision move selector\n");
					exit(1);
			}

			break;

		case DOUBLE_PRECISION:
			switch(destination_size){
				case SINGLE_PRECISION:
					return CVTSD2SS;

				case DOUBLE_WORD:
					return CVTTSD2SIL;

				case QUAD_WORD:
					return CVTTSD2SIQ;

				default:
					printf("Fatal internal compiler error: undefined/invalid destination variable size encountered in double precision move selector\n");
					exit(1);
			}

			break;

		case BYTE:
			switch(destination_size){
				case WORD:
					if(destination_signed == TRUE){
						return MOVSBW;
					} else {
						return MOVZBW;
					}

				case DOUBLE_WORD:
					if(destination_signed == TRUE){
						return MOVSBL;
					} else {
						return MOVZBL;
					}

				case QUAD_WORD:
					if(destination_signed == TRUE){
						return MOVSBQ;
					} else{
						return MOVZBQ;
					}

				default:
					printf("Fatal internal compiler error: undefined/invalid destination variable size encountered in byte move selector\n");
					exit(1);
			}

			break;

		case WORD:
			switch(destination_size){
				case DOUBLE_WORD:
					if(destination_signed == TRUE){
						return MOVSWL;
					} else {
						return MOVZWL;
					}

				case QUAD_WORD:
					if(destination_signed == TRUE){
						return MOVSWQ;
					} else {
						return MOVZWQ;
					}

				default:
					printf("Fatal internal compiler error: undefined/invalid destination variable size encountered in word move selector\n");
					exit(1);
			}

			break;

		case DOUBLE_WORD:
			switch(destination_size){
				case SINGLE_PRECISION:
					return CVTSI2SSL;

				case DOUBLE_PRECISION:
					return CVTSI2SDL;

				case QUAD_WORD:
					if(destination_signed == TRUE){
						return MOVSLQ;
					} else {
						//If it's unsigned, we are able to do the implicit
						//conversion here
						return MOVQ;
					}

				default:
					printf("Fatal internal compiler error: undefined/invalid destination variable size encountered in double word move selector\n");
					exit(1);
			}

			break;

		case QUAD_WORD:
			switch(destination_size){
				case SINGLE_PRECISION:
					return CVTSI2SSQ;

				case DOUBLE_PRECISION:
					return CVTSI2SDQ;

				default:
					printf("Fatal internal compiler error: undefined/invalid destination variable size encountered in quad word move selector\n");
					exit(1);
			}

			break;

		default:
			printf("Fatal internal compiler error: undefined/invalid destination variable size encountered in converting move selector\n");
			exit(1);
	}
}


/**
 * Emit a movX instruction
 *
 * This movement instruction will handle all converting move logic internally
 */
static instruction_t* emit_move_instruction(three_addr_var_t* destination, three_addr_var_t* source){
	//First we'll allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//Is the desired type a 64 bit integer *and* the source type a U32 or I32? If this is the case, then 
	//movzx functions are actually invalid because x86 processors operating in 64 bit mode automatically
	//zero pad when 32 bit moves happen
	if(is_type_unsigned_64_bit(destination->type) == TRUE && is_type_32_bit_int(source->type) == TRUE){
		//Emit a variable copy of the source
		source = emit_var_copy(source);

		//Reassign it's type to be the desired type
		source->type = destination->type;

		//Select the size appropriately after the type is reassigned
		source->variable_size = get_type_size(destination->type);
	}

	//Link to the helper to select the instruction
	instruction->instruction_type = select_move_instruction(get_type_size(destination->type), get_type_size(source->type), is_type_signed(destination->type), is_source_register_clean(source));

	//Finally we set the destination
	instruction->destination_register = destination;
	instruction->source_register = source;

	//And now we'll give it back
	return instruction;
}


/**
 * Handle a simple movement instruction. In this context, simple just means that
 * we have a source and a destination, and now address calculation moves in between
 */
static void handle_register_movement_instruction(instruction_t* instruction){
	//Extract the assignee and the op1
	three_addr_var_t* assignee = instruction->assignee;
	three_addr_var_t* op1 = instruction->op1;

	//We have both a destination and source size to look at here
	variable_size_t destination_size = get_type_size(assignee->type);
	variable_size_t source_size = get_type_size(op1->type);

	//Is the desired type a 64 bit integer *and* the source type a U32 or I32? If this is the case, then 
	//movzx functions are actually invalid because x86 processors operating in 64 bit mode automatically
	//zero pad when 32 bit moves happen
	if(is_type_unsigned_64_bit(op1->type) == TRUE && is_type_32_bit_int(op1->type) == TRUE){
		//Emit a variable copy of the source
		op1 = emit_var_copy(op1);

		//Reassign it's type to be the desired type
		op1->type = assignee->type;

		//Select the size appropriately after the type is reassigned
		op1->variable_size = get_type_size(op1->type);
	}

	//Let the helper rule determine what our instruction is
	instruction->instruction_type = select_move_instruction(destination_size, source_size, is_type_signed(assignee->type), is_source_register_clean(op1));

	/**
	 * If we have a conversion instruction that has an SSE destination, we need to emit
	 * a special "pxor" statement beforehand to completely wipe out said register
	 */
	if(is_integer_to_sse_conversion_instruction(instruction->instruction_type) == TRUE){
		//We need to completely zero out the destination register here, so we will emit a pxor to do
		//just that
		instruction_t* pxor_instruction = emit_direct_pxor_instruction(instruction->assignee);

		//Get this in right before the given
		insert_instruction_before_given(pxor_instruction, instruction);
	}

	//Set the sources and destinations
	instruction->destination_register = instruction->assignee;
	instruction->source_register = instruction->op1;
}


/**
 * Emit a movX instruction with a constant
 *
 * This is used for when we need extra moves(after a division/modulus)
 */
instruction_t* emit_constant_move_instruction(three_addr_var_t* destination, three_addr_const_t* source){
	//First we'll allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//We set the size based on the destination 
	variable_size_t size = get_type_size(destination->type);

	switch (size) {
		case BYTE:
			instruction->instruction_type = MOVB;
			break;
		case WORD:
			instruction->instruction_type = MOVW;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = MOVL;
			break;
		case QUAD_WORD:
			instruction->instruction_type = MOVQ;
			break;
		//Should never reach this
		default:
			printf("Fatal internal compiler error: undefined/invalid destination variable size encountered in constant move instruction\n");
			exit(1);
	}

	//Finally we set the destination
	instruction->destination_register = destination;
	instruction->source_immediate = source;

	//And now we'll give it back
	return instruction;
}


/**
 * Create and insert a converting move operation where the destination's type is the desired type. This handles all of the overhead of creating,
 * finding the converting moves, and inserting
 */
static three_addr_var_t* create_and_insert_converting_move_instruction(instruction_t* after_instruction, three_addr_var_t* source, generic_type_t* destination_type){
	//Is the desired type a 64 bit integer *and* the source type a U32 or I32? If this is the case, then 
	//movzx functions are actually invalid because x86 processors operating in 64 bit mode automatically
	//zero pad when 32 bit moves happen
	if(is_type_unsigned_64_bit(destination_type) == TRUE && is_type_32_bit_int(source->type) == TRUE){
		//Emit a variable copy of the source
		three_addr_var_t* converted = emit_var_copy(source);

		//Reassign it's type to be the desired type
		converted->type = destination_type;

		//Select the size appropriately after the type is reassigned
		converted->variable_size = get_type_size(converted->type);

		//We don't need to deal with a move at all in this case, we can just leave here
		return converted;
	}

	//We have a temp var based on the destination type
	three_addr_var_t* destination_variable = emit_temp_var(destination_type);

	//Let the helper emit this entirely
	instruction_t* move_instruction = emit_move_instruction(destination_variable, source);

	//Put it in where we want in the CFG
	insert_instruction_before_given(move_instruction, after_instruction);

	//Give back what the final assignee is
	return destination_variable;
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
			printf("Fatal internal compiler error: undefined/invalid destination variable size encountered in conversion instruction\n");
			exit(1);
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
 * The setne instruction is used on a byte. We have a "relies_on" field to tell the instruction
 * scheduler what this setne relies on in the future, but this op1 is never actually displayed/printed,
 * it is just for tracking
 */
static instruction_t* emit_setne_instruction(three_addr_var_t* destination, three_addr_var_t* relies_on){
	//First we'll allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//And we'll set the class
	instruction->instruction_type = SETNE;

	//We store what this instruction relies on in it's op1 value. This is necessary for scheduling reasons,
	//but it is completely ignored at the selector level
	instruction->op1 = relies_on;

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
			printf("Fatal internal compiler error: undefined/invalid destination variable size encountered in and instruction\n");
			exit(1);
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
			printf("Fatal internal compiler error: undefined/invalid destination variable size encountered in or instruction\n");
			exit(1);
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
			printf("Fatal internal compiler error: undefined/invalid destination variable size encountered in division instrution\n");
			exit(1);
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
		case SINGLE_PRECISION:
			return ADDSS;
		case DOUBLE_PRECISION:
			return ADDSD;
		default:
			printf("Fatal internal compiler error: undefined/invalid destination variable size encountered in add instruction\n");
			exit(1);
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
			printf("Fatal internal compiler error: undefined/invalid destination variable size encountered in lea instruction\n");
			exit(1);
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
		case SINGLE_PRECISION:
			return SUBSS;
		case DOUBLE_PRECISION:
			return SUBSD;
		default:
			printf("Fatal internal compiler error: undefined/invalid destination variable size encountered in subtraction instruction\n");
			exit(1);
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
			printf("Fatal internal compiler error: undefined/invalid destination variable size encountered in cmp instruction\n");
			exit(1);
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
		if(instruction->op2->class_relative_parameter_order > 0){
			//Move it on over here
			instruction_t* copy_instruction = emit_move_instruction(emit_temp_var(instruction->op2->type), instruction->op2);
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
		if(instruction->op2->class_relative_parameter_order > 0){
			//Move it on over here
			instruction_t* copy_instruction = emit_move_instruction(emit_temp_var(instruction->op2->type), instruction->op2);
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
			printf("Fatal internal compiler error: undefined/invalid destination variable size encountered in or instruction\n");
			exit(1);
	}

	//And we always have a destination register
	instruction->destination_register = instruction->assignee;

	//We can have an immediate value or we can have a register
	if(instruction->op2 != NULL){
		//If we need to convert, we'll do that here
		if(is_converting_move_required(instruction->assignee->type, instruction->op2->type) == TRUE){
			instruction->source_register = create_and_insert_converting_move_instruction(instruction, instruction->op2, instruction->assignee->type);

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
			printf("Fatal internal compiler error: undefined/invalid destination variable size encountered in and instruction\n");
			exit(1);
	}

	//And we always have a destination register
	instruction->destination_register = instruction->assignee;

	//We can have an immediate value or we can have a register
	if(instruction->op2 != NULL){
		//If we need to convert, we'll do that here
		if(is_converting_move_required(instruction->assignee->type, instruction->op2->type) == TRUE){
			instruction->source_register = create_and_insert_converting_move_instruction(instruction, instruction->op2, instruction->assignee->type);

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
			printf("Fatal internal compiler error: undefined/invalid destination variable size encountered in xor instruction\n");
			exit(1);
	}
	
	//And we always have a destination register
	instruction->destination_register = instruction->assignee;

	//We can have an immediate value or we can have a register
	if(instruction->op2 != NULL){
		//If we need to convert, we'll do that here
		if(is_converting_move_required(instruction->assignee->type, instruction->op2->type) == TRUE){
			instruction->source_register = create_and_insert_converting_move_instruction(instruction, instruction->op2, instruction->assignee->type);

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
 * relational operation.
 *
 * All comparison instructions have a symbolic assignee created. The question
 * of whether or not to use the assignee needs to be determined here, since
 * the actual cmpX instruction does not naturally produce and output, it just
 * sets flags. We have custom logic in this block do determine whether or not 
 * that flag setting is needed
 */
static instruction_t* handle_cmp_instruction(instruction_t* instruction){
	/**
	 * First step - determine if this cmp instruction is *exclusively* used
	 * by a branch statement or if we are going to need to expand it out
	 * more. By default, we assume that we're going to have to expand it out
	 */
	u_int8_t used_by_branch = FALSE;

	//Grab a cursor to the next statement
	instruction_t* cursor = instruction->next_statement;

	//So long as the cursor is not NULL, keep
	//crawling
	while(cursor != NULL){
		//If we find out that this is a branch statement
		if(cursor->statement_type == THREE_ADDR_CODE_BRANCH_STMT){
			//This is the case that we're after. If we find that the branch relies
			//on this, then we can just get out
			if(variables_equal(cursor->op1, instruction->assignee, FALSE) == TRUE){
				used_by_branch = TRUE;
				break;
			}
		}

		//If we get to the end and it's not used by a branch, that is fine. The only
		//thing that we care about in this crawl is whether or not the above statement
		//was used by a branch instruction. If it was, then all of the extra setX
		//is unnecessary. If it wasn't then we need to be adding those extra steps

		//Advance the cursor up
		cursor = cursor->next_statement;
	}

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
	if(is_converting_move_required(right_hand_type, instruction->op1->type) == TRUE){
		//Let the helper deal with it
		instruction->source_register = create_and_insert_converting_move_instruction(instruction, instruction->op1, right_hand_type);
	} else {
		//Otherwise we assign directly
		instruction->source_register = instruction->op1;
	}

	//If we have op2, we'll use source_register2
	if(instruction->op2 != NULL){
		if(is_converting_move_required(left_hand_type, instruction->op2->type) == TRUE){
			//Let the helper deal with it
			instruction->source_register2 = create_and_insert_converting_move_instruction(instruction, instruction->op2, left_hand_type);
		} else {
			//Otherwise we assign directly
			instruction->source_register2 = instruction->op2;
		}

	//Otherwise we have a constant source
	} else {
		//Otherwise we use an immediate value
		instruction->source_immediate = instruction->op1_const;
	}


	//We expect that this is the likely case. Usually
	//a programmer is putting in comparisons to determine a branch
	//in some way
	if(used_by_branch == TRUE){
		//Just give back the instruction we modified
		return instruction;

	//We've already handled the comparison instruction by this point. Now,
	//we'll add logic that does the setX instruction and the final assignment
	} else {
		//Get the signedness based on the assignee of the cmp instruction
		u_int8_t type_signed = is_type_signed(instruction->assignee->type);

		//We'll now need to insert inbetween here. These relie on the result of the comparison instruction. The set instruction
		//is required to use a byte sized register so we can't just set the assignee and move on
		instruction_t* set_instruction = emit_setX_instruction(instruction->op, emit_temp_var(u8), instruction->op1, type_signed);

		//The set instruction goes right after the cmp instruction
		insert_instruction_after_given(set_instruction, instruction);

		//Move from the set instruction's assignee to this instruction's assignee
		instruction_t* final_move = emit_move_instruction(instruction->assignee, set_instruction->destination_register);

		//This final move goes right after the set instruction
		insert_instruction_after_given(final_move, set_instruction);

		return final_move;
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
		if(is_converting_move_required(instruction->assignee->type, instruction->op2->type) == TRUE){
			//If this is needed, we'll let the helper do it
			instruction->source_register = create_and_insert_converting_move_instruction(instruction, instruction->op2, instruction->assignee->type);

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
		if(is_converting_move_required(instruction->assignee->type, instruction->op2->type) == TRUE){
			//Let the helper function deal with this
			instruction->source_register = create_and_insert_converting_move_instruction(instruction, instruction->op2, instruction->assignee->type);

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
		if(is_converting_move_required(instruction->address_calc_reg1->type, addresss_calc_reg2->type) == TRUE){
			//Let the helper deal with it
			addresss_calc_reg2 = create_and_insert_converting_move_instruction(instruction, addresss_calc_reg2, instruction->address_calc_reg1->type);
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
		if(is_converting_move_required(multiplication_instruction->assignee->type, multiplication_instruction->op2->type) == TRUE){
			//Let the helper deal with it
			source2 = create_and_insert_converting_move_instruction(multiplication_instruction, multiplication_instruction->op2, multiplication_instruction->assignee->type);

		//Otherwise this can be moved directly
		} else {
			//We first need to move the first operand into RAX
			instruction_t* move_to_rax = emit_move_instruction(emit_temp_var(multiplication_instruction->op2->type), multiplication_instruction->op2);

			//Insert the move to rax before the multiplication instruction
			insert_instruction_before_given(move_to_rax, multiplication_instruction);

			//This is just the destination register here
			source2 = move_to_rax->destination_register;
		}

	//Otherwise, we have a BIN_OP_WITH_CONST statement. We're actually going to need a temp assignment for the second operand(the constant)
	//here for this to work
	} else {
		//Emit the move instruction here
		instruction_t* move_to_rax = emit_constant_move_instruction(emit_temp_var(multiplication_instruction->assignee->type), multiplication_instruction->op1_const);

		//Put it before our multiplication
		insert_instruction_before_given(move_to_rax, multiplication_instruction);

		//Our source2 now is this
		source2 = move_to_rax->destination_register;
	}

	//Let's also check is any conversions are needed for the first source register
	if(is_converting_move_required(multiplication_instruction->assignee->type, multiplication_instruction->op1->type) == TRUE){
		source = create_and_insert_converting_move_instruction(multiplication_instruction, multiplication_instruction->op1, multiplication_instruction->assignee->type);

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
		case QUAD_WORD:
			multiplication_instruction->instruction_type = MULQ;
			break;
		//Everything else falls here
		default:
			printf("Fatal internal compiler error: undefined/invalid destination variable size encountered in multiplication instruction\n");
			exit(1);
	}

	//This is the case where we have two source registers
	multiplication_instruction->source_register = source;
	//The other source register is in RAX
	multiplication_instruction->source_register2 = source2;

	//This is the assignee, we just don't see it
	multiplication_instruction->destination_register = emit_temp_var(multiplication_instruction->assignee->type);

	//Once we've done all that, we need one final movement operation
	instruction_t* result_movement = emit_move_instruction(multiplication_instruction->assignee, multiplication_instruction->destination_register);

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
		if(is_converting_move_required(instruction->assignee->type, instruction->op2->type) == TRUE){
			//Let the helper deal with it
			instruction->source_register = create_and_insert_converting_move_instruction(instruction, instruction->op2, instruction->assignee->type);

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
	if(is_converting_move_required(division_instruction->assignee->type, division_instruction->op1->type) == TRUE){
		//Let the helper deal with it
		dividend = create_and_insert_converting_move_instruction(division_instruction, division_instruction->op1, division_instruction->assignee->type);

	//Otherwise this can be moved directly
	} else {
		//We first need to move the first operand into RAX
		instruction_t* move_to_rax = emit_move_instruction(emit_temp_var(division_instruction->op1->type), division_instruction->op1);

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
	if(is_converting_move_required(division_instruction->assignee->type, division_instruction->op2->type) == TRUE){
		divisor = create_and_insert_converting_move_instruction(division_instruction, division_instruction->op2, division_instruction->assignee->type);

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
	instruction_t* result_movement = emit_move_instruction(division_instruction->assignee, quotient);

	//Insert this before the original division instruction
	insert_instruction_before_given(result_movement, division_instruction);

	//Delete the division instruction
	delete_statement(division_instruction);

	//Reconstruct the window here
	reconstruct_window(window, result_movement);
}


/**
 * Handle an SSE multiplication instruction. By the time we get here, we already
 * know that we're dealing with an SSE operation. This instruction will generate
 * converting moves if such moves are required
 *
 * NOTE: this assumes that "instruction1" is what we're after. This will slide the window
 * as needed
 */
static inline void handle_sse_division_instruction(instruction_window_t* window){
	//Handle using instruction 1
	instruction_t* instruction = window->instruction1;

	//Go based on what the assignee's type is
	switch(instruction->assignee->type->type_size){
		case SINGLE_PRECISION:
			instruction->instruction_type = DIVSS;
			break;
		case DOUBLE_PRECISION:
			instruction->instruction_type = DIVSD;
			break;
		default:
			printf("Fatal internal compiler error: invalid assignee size for SSE division instruction");
	}

	//The source register is the op1 and the destination is the assignee. There is never a case where we
	//will have a constant source, it is not possible for sse operations
	instruction->destination_register = instruction->assignee;
	instruction->source_register = instruction->op1;
}


/**
 * Handle an SSE multiplication instruction. By the time we get here, we already
 * know that we're dealing with an SSE operation. This instruction will generate
 * converting moves if such moves are required
 */
static inline void handle_sse_multiplication_instruction(instruction_t* instruction){
	//TODO
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
	if(is_converting_move_required(modulus_instruction->assignee->type, modulus_instruction->op1->type) == TRUE){
		//Let the helper deal with it
		dividend = create_and_insert_converting_move_instruction(modulus_instruction, modulus_instruction->op1, modulus_instruction->assignee->type);

	//Otherwise this can be moved directly
	} else {
		//We first need to move the first operand into RAX
		instruction_t* move_to_rax = emit_move_instruction(emit_temp_var(modulus_instruction->op1->type), modulus_instruction->op1);

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
	if(is_converting_move_required(modulus_instruction->assignee->type, modulus_instruction->op2->type) == TRUE){
		divisor = create_and_insert_converting_move_instruction(modulus_instruction, modulus_instruction->op2, modulus_instruction->assignee->type);

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
	instruction_t* result_movement = emit_move_instruction(modulus_instruction->assignee, remainder_register);

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
			//This is our first case. Of course ignore SSA here
			if(variables_equal_no_ssa(instruction->assignee, instruction->op1, FALSE) == TRUE){
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
	switch(size){
		case BYTE:
			instruction->instruction_type = MOVB;
			break;
		case WORD:
			instruction->instruction_type = MOVW;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = MOVL;
			break;
		case QUAD_WORD:
			instruction->instruction_type = MOVQ;
			break;
		default:
			printf("Fatal internal compiler error: undefined/incorrect variable size detected in constant to register move instruction\n");
			exit(1);
	}
	
	//We've already set the sources, now we set the destination as the assignee
	instruction->destination_register = instruction->assignee;
	//Set the source immediate here
	instruction->source_immediate = instruction->op1_const;
}


/**
 * Handle a lea statement(in the three address code statement form)
 *
 * Lea statements carry their own lea type, so it should be very easy to
 * convert into x86 addressing mode expresssions
 */
static void handle_lea_statement(instruction_t* instruction){
	//Select the size of our variable
	variable_size_t size = get_type_size(instruction->assignee->type);

	//Select the appropriate instruction first
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

	//This is always the same
	instruction->destination_register = instruction->assignee;

	//Go based on whatever the type is
	switch(instruction->lea_statement_type){
		//This converts to an addressing mode with
		//an offset only
		case OIR_LEA_TYPE_OFFSET_ONLY:
			//Set the mode
			instruction->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;
			
			//The op1 is now our address calc register
			instruction->address_calc_reg1 = instruction->op1;

			//Copy the offset constant over
			instruction->offset = instruction->op1_const;

			break;

		//Converts to an addresing mode with address calc registers
		//only
		case OIR_LEA_TYPE_REGISTERS_ONLY:
			//Set the mode
			instruction->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_ONLY;
			
			//Copy over the address calc registers
			instruction->address_calc_reg1 = instruction->op1;
			instruction->address_calc_reg2 = instruction->op2;

			//The base(address calc reg1) and index(address calc reg 2) registers must be the same type.
			//We determine that the base address is the dominating force, and takes precedence, so the address calc reg2
			//must adhere to this one's type
			if(is_converting_move_required(instruction->address_calc_reg1->type, instruction->address_calc_reg2->type) == TRUE){
				instruction->address_calc_reg2 = create_and_insert_converting_move_instruction(instruction, instruction->address_calc_reg2, instruction->address_calc_reg1->type);
			}

			break;

		//Converts to an addressing mode with the trifecta
		case OIR_LEA_TYPE_REGISTERS_OFFSET_AND_SCALE:
			//Set the mode
			instruction->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_OFFSET_AND_SCALE;

			//Copy over the address calc registers
			instruction->address_calc_reg1 = instruction->op1;
			instruction->address_calc_reg2 = instruction->op2;

			//The base(address calc reg1) and index(address calc reg 2) registers must be the same type.
			//We determine that the base address is the dominating force, and takes precedence, so the address calc reg2
			//must adhere to this one's type
			if(is_converting_move_required(instruction->address_calc_reg1->type, instruction->address_calc_reg2->type) == TRUE){
				instruction->address_calc_reg2 = create_and_insert_converting_move_instruction(instruction, instruction->address_calc_reg2, instruction->address_calc_reg1->type);
			}

			//Set the appropriate value here
			instruction->offset = instruction->op1_const;

			break;

		//Special kind to support global vars
		case OIR_LEA_TYPE_RIP_RELATIVE:
			//Set the mode
			instruction->calculation_mode = ADDRESS_CALCULATION_MODE_RIP_RELATIVE;

			//Copy over the address calc register
			instruction->address_calc_reg1 = instruction->op1;

			//Op2 holds the global var, which then gets moved over
			instruction->rip_offset_variable = instruction->op2;

			break;

		//Support RIP relative with offset addressing
		case OIR_LEA_TYPE_RIP_RELATIVE_WITH_OFFSET:
			//Set the mode
			instruction->calculation_mode = ADDRESS_CALCULATION_MODE_RIP_RELATIVE_WITH_OFFSET;

			//And the address calc registers
			instruction->address_calc_reg1 = instruction->op1;

			//Store the RIP relative offset
			instruction->rip_offset_variable = instruction->op2;

			//And store the offset here from op1_const
			instruction->offset = instruction->op1_const;

			break;
			

		//Translates to the address calc mode of the same name
		case OIR_LEA_TYPE_REGISTERS_AND_OFFSET:
			//Set the mode
			instruction->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_AND_OFFSET;

			//Copy over the address calc registers
			instruction->address_calc_reg1 = instruction->op1;
			instruction->address_calc_reg2 = instruction->op2;

			//The base(address calc reg1) and index(address calc reg 2) registers must be the same type.
			//We determine that the base address is the dominating force, and takes precedence, so the address calc reg2
			//must adhere to this one's type
			if(is_converting_move_required(instruction->address_calc_reg1->type, instruction->address_calc_reg2->type) == TRUE){
				instruction->address_calc_reg2 = create_and_insert_converting_move_instruction(instruction, instruction->address_calc_reg2, instruction->address_calc_reg1->type);
			}

			//Set the appropriate value here
			instruction->offset = instruction->op1_const;

			break;

		//Translates to the address calc mode of the same name
		case OIR_LEA_TYPE_REGISTERS_AND_SCALE:
			//Set the mode
			instruction->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_AND_SCALE;

			//Copy over the address calc registers
			instruction->address_calc_reg1 = instruction->op1;
			instruction->address_calc_reg2 = instruction->op2;

			//The base(address calc reg1) and index(address calc reg 2) registers must be the same type.
			//We determine that the base address is the dominating force, and takes precedence, so the address calc reg2
			//must adhere to this one's type
			if(is_converting_move_required(instruction->address_calc_reg1->type, instruction->address_calc_reg2->type) == TRUE){
				instruction->address_calc_reg2 = create_and_insert_converting_move_instruction(instruction, instruction->address_calc_reg2, instruction->address_calc_reg1->type);
			}

			//The scale is already stored in the multiplier

			break;

		case OIR_LEA_TYPE_INDEX_AND_SCALE:
			//Set the mode
			instruction->calculation_mode = ADDRESS_CALCULATION_MODE_INDEX_AND_SCALE;

			//Address calc reg 1 is all we have here, the scale is already stored
			instruction->address_calc_reg1 = instruction->op1;
			
			break;

		case OIR_LEA_TYPE_INDEX_OFFSET_AND_SCALE:
			//Set the mode
			instruction->calculation_mode = ADDRESS_CALCULATION_MODE_INDEX_OFFSET_AND_SCALE;

			//Address calc reg 1 is all we have here, the scale is already stored
			instruction->address_calc_reg1 = instruction->op1;

			//Copy over the offset
			instruction->offset = instruction->op1_const;
			
			break;

		//This is unreachable and should never happen. Hard error if it does
		default:
			printf("Fatal internal compiler error: Unreachable path detected in lea statement translator\n");
			exit(1);
	}
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

	//Copy the source register over here as it is a dependence
	jump_to_if->op1 = branch_stmt->op1;

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
static void handle_logical_or_instruction(instruction_window_t* window){
	//Grab it out for convenience
	instruction_t* logical_or = window->instruction1;

	//Save the after instruction
	instruction_t* after_logical_or = window->instruction2;

	//Let's first emit the or instruction
	instruction_t* or_instruction = emit_or_instruction(logical_or->op1, logical_or->op2);

	//Now we need the setne instruction
	instruction_t* setne_instruction = emit_setne_instruction(emit_temp_var(u8), logical_or->op1);

	//Flag that thsi relies on the above or instruction
	setne_instruction->op1 = logical_or->op1;

	//Following that we'll need the final movzx instruction
	instruction_t* move_instruction = emit_move_instruction(logical_or->assignee, setne_instruction->destination_register);

	//Select this one's size 
	logical_or->assignee->variable_size = get_type_size(logical_or->assignee->type);

	//Now we can delete the old logical or instruction
	delete_statement(logical_or);

	//First insert the or instruction
	insert_instruction_before_given(or_instruction, after_logical_or);

	//Then we need the setne
	insert_instruction_before_given(setne_instruction, after_logical_or);

	//And finally we need the movzx
	insert_instruction_before_given(move_instruction, after_logical_or);

	//Reconstruct the window starting at the movzbl
	reconstruct_window(window, move_instruction);
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
static void handle_logical_and_instruction(instruction_window_t* window){
	//These operands - did they already come from a setX instruction
	//or not? An example would be if we're doing something like x > y && y < z.
	//Both of these operations res
	u_int8_t op1_came_from_setX = FALSE;
	u_int8_t op2_came_from_setX = FALSE;

	//Store the result variable for both of our test paths
	three_addr_var_t* op1_result;
	three_addr_var_t* op2_result;

	//The eventual 2 things that will be anded together

	//Grab it out for convenience
	instruction_t* logical_and = window->instruction1;

	//Preserve this for ourselves
	instruction_t* after_logical_and = logical_and->next_statement;

	//Grab a cursor to see where the operands came from
	instruction_t* cursor = logical_and->previous_statement;

	//Crawl back through the block to try and see if we can tell
	//where these all came from. Worst case we need to crawl the whole
	//block, which isn't bad because these blocks aren't enormous 99.9% of
	//the time
	while(cursor != NULL){
		//Did we find where op1 got assigned?. If so, check to see
		//if the operation that made it generated a truthful byte value(0 or 1)
		//or not
		if(variables_equal(logical_and->op1, cursor->assignee, FALSE)){
			if(does_operator_generate_truthful_byte_value(cursor->op) == TRUE){
				op1_came_from_setX = TRUE;
			}

		//Give op2 the exact same treatment
		} else if(variables_equal(logical_and->op2, cursor->assignee, FALSE)){
			if(does_operator_generate_truthful_byte_value(cursor->op) == TRUE){
				op2_came_from_setX = TRUE;
			}
		}

		//Push it back
		cursor = cursor->previous_statement;
	}

	//We expect that it *not* being from
	//setX is the most likely case
	if(op1_came_from_setX == FALSE){
		//Let's first emit our test instruction
		instruction_t* test_instruction = emit_direct_test_instruction(logical_and->op1, logical_and->op1);

		//Emit a var to hold the result of op1
		op1_result = emit_temp_var(u8);

		//Now we'll need a setne(not zero) instruction that will the op1 result
		instruction_t* set_instruction = emit_setne_instruction(op1_result, logical_and->op1);

		//IMPORTANT - flag that this depends on the source register
		set_instruction->op1 = test_instruction->source_register;

		//Insert these in order. The test comes first, then the set(relies on the flags from test)
		insert_instruction_before_given(test_instruction, after_logical_and);
		insert_instruction_before_given(set_instruction, after_logical_and);

	} else {
		//If we make it here, we know that op1 already came from a setX instruction. So, we can just
		//assign here and be done
		op1_result = emit_var_copy(logical_and->op1);
		//We will emit a type-coerced version of our value
		op1_result->type = u8;
		op1_result->variable_size = get_type_size(u8);
	}

	//We expect that it *not* being from
	//setX is the most likely case
	if(op2_came_from_setX == FALSE){
		//Test the 2 together
		instruction_t* test_instruction = emit_direct_test_instruction(logical_and->op2, logical_and->op2);

		//Emit a var to hold the result of op2
		op2_result = emit_temp_var(u8);

		//Set if it's not zero
		instruction_t* set_instruction = emit_setne_instruction(op2_result, logical_and->op1);

		//IMPORTANT - flag that this depends on the source register
		set_instruction->op1 = test_instruction->source_register;

		//Insert these in order. The test comes first, then the set(relies on the flags from test)
		insert_instruction_before_given(test_instruction, after_logical_and);
		insert_instruction_before_given(set_instruction, after_logical_and);
	} else {
		//If we make it here, we know that op2 already came from a setX instruction. So, we can just
		//assign here and be done
		op2_result = emit_var_copy(logical_and->op2);
		//We will emit a type-coerced version of our value
		op2_result->type = u8;
		op2_result->variable_size = get_type_size(u8);
	}

	//Now we'll need to ANDx these two values together to see if they're both 1
	instruction_t* and_inst = emit_and_instruction(op1_result, op2_result);

	//The final thing that we need is a movzx
	instruction_t* move_instruction = emit_move_instruction(logical_and->assignee, and_inst->destination_register);

	//Select this one's size 
	logical_and->assignee->variable_size = get_type_size(logical_and->assignee->type);

	//We no longer need the logical and statement
	delete_statement(logical_and);

	//Now insert these in order. First comes the and instruction, then the final move
	insert_instruction_before_given(and_inst, after_logical_and);
	insert_instruction_before_given(move_instruction, after_logical_and);
	
	//Reconstruct the window starting at the final move
	reconstruct_window(window, move_instruction);
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
 * Emit a register to register converting move instruction directly
 *
 * This bypasses all register allocation entirely
 */
static instruction_t* emit_register_movement_instruction_directly(three_addr_var_t* destination_register, three_addr_var_t* source_register){
	//First allocate it
	instruction_t* move_instruction = calloc(1, sizeof(instruction_t));

	//We know what the source and destination are already
	move_instruction->destination_register = destination_register;
	move_instruction->source_register = source_register;

	//Grab the types
	generic_type_t* destination_type = destination_register->type;
	generic_type_t* source_type = source_register->type;

	//Now we will decide what the move instruction is
	move_instruction->instruction_type = select_move_instruction(get_type_size(destination_type), get_type_size(source_type), is_type_signed(destination_type), is_source_register_clean(source_register));

	//Give back the pointer
	return move_instruction;
}


/**
 * Handle the assignment of the source for a store instruction.
 *
 * This function will account for all edge cases(op1 vs op2 vs op1_const), as well
 * as the unique case where our source is a 32 bit integer *but* we are saving to an
 * unsigned 64 bit memory region
 */
static void handle_store_instruction_sources_and_instruction_type(instruction_t* store_instruction){
	//The destination type is always stored in the instruction itself
	generic_type_t* destination_type = store_instruction->memory_read_write_type;

	//The source type will be assigned later
	generic_type_t* source_type;

	//Go based on what we have
	switch(store_instruction->statement_type){
		//For stores like this, we either have an op1 or an immediate source
		case THREE_ADDR_CODE_STORE_STATEMENT:
			//The op1 is where we may have conversion issues
			if(store_instruction->op1 != NULL){
				//Mark that the source type is op1
				source_type = store_instruction->op1->type;

				/**
				 * This is a special edgecase where we are moving from 32 bit to 64 bit
				 * In the event that we do this, we need to emit a simple copy of the source
				 * variable and give it the 64 bit type so that we have a quad word register
				 */
				if(is_type_unsigned_64_bit(destination_type) == TRUE
					&& is_type_32_bit_int(store_instruction->op1->type) == TRUE){

					//First we duplicate it
					three_addr_var_t* duplicate_64_bit = emit_var_copy(store_instruction->op1);

					//Then we give it the type that we want
					duplicate_64_bit->type = store_instruction->assignee->type;
					duplicate_64_bit->variable_size = get_type_size(duplicate_64_bit->type);

					//And this will be our source
					store_instruction->source_register = duplicate_64_bit;

				/**
				 * In the event that a converting move is required, we need to insert the converting
				 * move in before the store instruction because x86 assembly does not allow us
				 * to do *to memory* converting moves
				 */
				} else if(is_converting_move_required(destination_type, source_type) == TRUE) {
					//Emit a temp var that is the destination's type
					three_addr_var_t* new_source = emit_temp_var(destination_type);

					//Emit a move instruction where we send the old source(op1) into here
					instruction_t* converting_move = emit_register_movement_instruction_directly(new_source, store_instruction->op1);
					
					//Insert this *right before* the store
					insert_instruction_before_given(converting_move, store_instruction);

					//Now, our source type is the new source's type
					source_type = new_source->type;

					//And the source register is the new source, not the old one
					store_instruction->source_register = new_source;

				/**
				 * In all other cases, we can just straight assign here
				 */
				} else {
					store_instruction->source_register = store_instruction->op1;
				}

			//If we get here it's a plain copy
			} else {
				//If we get here, we can just use the destination type
				source_type = destination_type;

				store_instruction->source_immediate = store_instruction->op1_const;
			}

			break;

		//For these kinds of stores, op2 would have our value
		case THREE_ADDR_CODE_STORE_WITH_CONSTANT_OFFSET:
		case THREE_ADDR_CODE_STORE_WITH_VARIABLE_OFFSET:
			//The op1 is where we may have conversion issues
			if(store_instruction->op2 != NULL){
				//Mark that the source type is op2
				source_type = store_instruction->op2->type;

				/**
				 * This is a special edgecase where we are moving from 32 bit to 64 bit
				 * In the event that we do this, we need to emit a simple copy of the source
				 * variable and give it the 64 bit type so that we have a quad word register
				 */
				if(is_type_unsigned_64_bit(destination_type) == TRUE
					&& is_type_32_bit_int(store_instruction->op2->type) == TRUE){

					//First we duplicate it
					three_addr_var_t* duplicate_64_bit = emit_var_copy(store_instruction->op2);

					//Then we give it the type that we want
					duplicate_64_bit->type = store_instruction->assignee->type;
					duplicate_64_bit->variable_size = get_type_size(duplicate_64_bit->type);

					//And this will be our source
					store_instruction->source_register = duplicate_64_bit;

				/**
				 * In the event that a converting move is required, we need to insert the converting
				 * move in before the store instruction because x86 assembly does not allow us
				 * to do *to memory* converting moves
				 */
				} else if(is_converting_move_required(destination_type, source_type) == TRUE) {
					//Emit a temp var that is the destination's type
					three_addr_var_t* new_source = emit_temp_var(destination_type);

					//Emit a move instruction where we send the old source(op1) into here
					instruction_t* converting_move = emit_register_movement_instruction_directly(new_source, store_instruction->op2);
					
					//Insert this *right before* the store
					insert_instruction_before_given(converting_move, store_instruction);

					//Now, our source type is the new source's type
					source_type = new_source->type;

					//And the source register is the new source, not the old one
					store_instruction->source_register = new_source;

				/**
				 * In all other cases, we can just straight assign here
				 */
				} else {
					store_instruction->source_register = store_instruction->op2;
				}

			} else {
				//If we get here, we can just use the destination type
				source_type = destination_type;

				//Simple copy over
				store_instruction->source_immediate = store_instruction->op1_const;
			}

			break;

		//Should never get here
		default:
			printf("Fatal internal compiler error: invalid store instruction");
			exit(1);
	}

	//Once we've done all the above assignments, we need to determine what our instruction type is. The source here is always clean, we are moving to memory
	store_instruction->instruction_type = select_move_instruction(get_type_size(destination_type), get_type_size(source_type), is_type_signed(destination_type), TRUE);
}


/**
 * This helper function will handle the source and destination assignment
 * of a load instruction. This will also handle the edge case where we are
 * loading from a 32 bit memory region into an unsigned 64 bit region
 */
static inline void handle_load_instruction_destination_assignment(instruction_t* load_instruction){
	//By default, assume it's the assignee
	three_addr_var_t* destination_register = load_instruction->assignee;

	//This is always the memory region type
	generic_type_t* memory_region_type = load_instruction->memory_read_write_type;

	//Let's look for the special case here
	if(is_type_32_bit_int(memory_region_type) == TRUE
		&& is_type_unsigned_64_bit(destination_register->type) == TRUE){

		//Duplicate the destination
		three_addr_var_t* type_adjusted_destination = emit_var_copy(destination_register);
		//Fix the type to be 32 bits
		type_adjusted_destination->type = memory_region_type;
		//Be sure to get the size too
		type_adjusted_destination->variable_size = get_type_size(type_adjusted_destination->type);

		//This is the true destination now
		load_instruction->destination_register = type_adjusted_destination;

		//And we need to adjust this to be a MOVL type
		load_instruction->instruction_type = MOVL;
	
	//Otherwise, we just assign the destination to be the destination
	//register
	} else {
		load_instruction->destination_register = destination_register;
	}
}


/**
 * Handle a load instruction. A load instruction is always converted into
 * a garden variety dereferencing move
 */
static void handle_load_instruction(instruction_t* instruction){
	//We need the destination and source sizes to determine our movement instruction
	variable_size_t destination_size = get_type_size(instruction->assignee->type);
	//Is the destination signed? This is also required inof
	u_int8_t is_destination_signed = is_type_signed(instruction->assignee->type);
	//For a load, the source size is stored in the instruction itself
	variable_size_t source_size = get_type_size(instruction->memory_read_write_type);

	//Let the helper select for us. We are passing clean as true, since we are coming from memory
	instruction->instruction_type = select_move_instruction(destination_size, source_size, is_destination_signed, TRUE);

	//Load is from memory
	instruction->memory_access_type = READ_FROM_MEMORY;

	//Invoke the helper to handle the assignee and any edge cases
	handle_load_instruction_destination_assignment(instruction);

	/**
	 * If we have a conversion instruction that has an SSE destination, we need to emit
	 * a special "pxor" statement beforehand to completely wipe out said register
	 */
	if(is_integer_to_sse_conversion_instruction(instruction->instruction_type) == TRUE){
		//We need to completely zero out the destination register here, so we will emit a pxor to do
		//just that
		instruction_t* pxor_instruction = emit_direct_pxor_instruction(instruction->assignee);

		//Get this in right before the given
		insert_instruction_before_given(pxor_instruction, instruction);
	}

	//If we have a memory address variable(super common), we'll need to
	//handle this now
	if(instruction->op1->variable_type == VARIABLE_TYPE_MEMORY_ADDRESS){
		//If this is *not* a global variable
		if(instruction->op1->linked_var->membership != GLOBAL_VARIABLE){
			//This is our stack offset, it will be needed going forward
			int64_t stack_offset = instruction->op1->linked_var->stack_region->base_address;

			//If we actually have a stack offset to deal with
			if(stack_offset != 0){
				//Let's get the offset from this memory address
				three_addr_const_t* offset = emit_direct_integer_or_char_constant(instruction->op1->linked_var->stack_region->base_address, u64);

				//We now will have something like <offset>(%rsp)
				instruction->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;

				//This will be the stack pointer
				instruction->address_calc_reg1 = stack_pointer_variable;

				//Store the offset too
				instruction->offset = offset;

			//Otherwise there's no stack offset, so we're just dereferencing the
			//stack pointer
			} else {
				//Change the mode
				instruction->calculation_mode = ADDRESS_CALCULATION_MODE_DEREF_ONLY_SOURCE;
				
				//Source is now just the stack pointer
				instruction->source_register = stack_pointer_variable;
			}

		//Otherwise, we are loading a global variable
		} else {
			//This is going to be a global variable movement
			instruction->calculation_mode = ADDRESS_CALCULATION_MODE_RIP_RELATIVE;

			//The address calc reg1 is the instruction pointer
			instruction->address_calc_reg1 = instruction_pointer_variable;

			//The offset field holds the global var's name
			instruction->rip_offset_variable = instruction->op1;
		}

	} else {
		//This will always be a SOURCE_ONLY
		instruction->calculation_mode = ADDRESS_CALCULATION_MODE_DEREF_ONLY_SOURCE;

		//And the op1 is our source
		instruction->source_register = instruction->op1;
	}
}


/**
 * Handle a load with constant offset instruction
 *
 * load t5 <- MEM<t23>[8] --> movx 16(%rsp), t5
 *
 * This will usually generate an address calculation mode of OFFSET_ONLY
 */
static void handle_load_with_constant_offset_instruction(instruction_t* instruction){
	//We need the destination and source sizes to determine our movement instruction
	variable_size_t destination_size = get_type_size(instruction->assignee->type);
	//Is the destination signed? This is also required inof
	u_int8_t is_destination_signed = is_type_signed(instruction->assignee->type);
	//For a load, the source size is stored in the destination itself
	variable_size_t source_size = get_type_size(instruction->memory_read_write_type);

	//Let the helper decide for us. We are selecting clean as true, since we are coming from memory
	instruction->instruction_type = select_move_instruction(destination_size, source_size, is_destination_signed, TRUE);

	//Load is from memory
	instruction->memory_access_type = READ_FROM_MEMORY;

	//Handle destination assignment based on op1
	handle_load_instruction_destination_assignment(instruction);

	/**
	 * If we have a conversion instruction that has an SSE destination, we need to emit
	 * a special "pxor" statement beforehand to completely wipe out said register
	 */
	if(is_integer_to_sse_conversion_instruction(instruction->instruction_type) == TRUE){
		//We need to completely zero out the destination register here, so we will emit a pxor to do
		//just that
		instruction_t* pxor_instruction = emit_direct_pxor_instruction(instruction->assignee);

		//Get this in right before the given
		insert_instruction_before_given(pxor_instruction, instruction);
	}

	//If we have a memory address variable(super common), we'll need to
	//handle this now
	if(instruction->op1->variable_type == VARIABLE_TYPE_MEMORY_ADDRESS){
		//If this is *not* a global variable
		if(instruction->op1->linked_var->membership != GLOBAL_VARIABLE){
			//This is our stack offset, it will be needed going forward
			int64_t stack_offset = instruction->op1->linked_var->stack_region->base_address;

			//If we actually have a stack offset to deal with
			if(stack_offset != 0){
				//We need to sum the existing offset with the stack offset to get an accurate picture
				sum_constant_with_raw_int64_value(instruction->offset, i64, stack_offset);

				//We now will have something like <offset>(%rsp)
				instruction->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;

				//This will be the stack pointer
				instruction->address_calc_reg1 = stack_pointer_variable;

			//Otherwise there's no stack offset, so we're just dereferencing the
			//stack pointer with the pre-existing offset
			} else {
				//Change the mode
				instruction->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;
				
				//Address calc just needs the stack pointer
				instruction->address_calc_reg1 = stack_pointer_variable;
			}

		//Otherwise, we are loading a global variable with a subsequent offset. We can use a special
		//rip-relative addressing mode to make this happen in one instruction
		} else {
			//The first address calc register is the instruction pointer
			instruction->address_calc_reg1 = instruction_pointer_variable;

			//The global var comes from op1
			instruction->rip_offset_variable = instruction->op1;

			//The offset is already where it needs to be
			//Now we just need to change the mode to make this work
			instruction->calculation_mode = ADDRESS_CALCULATION_MODE_RIP_RELATIVE_WITH_OFFSET;
		}

	//Otherwise we aren't on the stack, it's just an offset. In that case, we'll keep the
	//offset here and just
	} else {
		//This will always be a SOURCE_ONLY
		instruction->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;

		//Op1 is the only address calc needed
		instruction->address_calc_reg1 = instruction->op1;
	}
}


/**
 * Handle a load with variable offset instruction
 *
 * load t5 <- MEM<t23>[t24] --> movx 4(%rsp, t24), t5
 *
 * This usually generates addressing mode expressions with registers and offsets
 */
static void handle_load_with_variable_offset_instruction(instruction_t* instruction){
	//We need the destination and source sizes to determine our movement instruction
	variable_size_t destination_size = get_type_size(instruction->assignee->type);
	//Is the destination signed? This is also required inof
	u_int8_t is_destination_signed = is_type_signed(instruction->assignee->type);
	//For a load, the source size is stored in the destination itself
	variable_size_t source_size = get_type_size(instruction->memory_read_write_type);

	//Let the helper decide for us. We are selecting the source register as clean, since we're moving from memory
	instruction->instruction_type = select_move_instruction(destination_size, source_size, is_destination_signed, TRUE);

	//This is a read from memory type
	instruction->memory_access_type = READ_FROM_MEMORY;

	//Handle the destination assignment
	handle_load_instruction_destination_assignment(instruction);

	/**
	 * If we have a conversion instruction that has an SSE destination, we need to emit
	 * a special "pxor" statement beforehand to completely wipe out said register
	 */
	if(is_integer_to_sse_conversion_instruction(instruction->instruction_type) == TRUE){
		//We need to completely zero out the destination register here, so we will emit a pxor to do
		//just that
		instruction_t* pxor_instruction = emit_direct_pxor_instruction(instruction->assignee);

		//Get this in right before the given
		insert_instruction_before_given(pxor_instruction, instruction);
	}

	//If we have a memory address variable(super common), we'll need to
	//handle this now
	if(instruction->op1->variable_type == VARIABLE_TYPE_MEMORY_ADDRESS){
		//If this is *not* a global variable
		if(instruction->op1->linked_var->membership != GLOBAL_VARIABLE){
			//This is our stack offset, it will be needed going forward
			int64_t stack_offset = instruction->op1->linked_var->stack_region->base_address;

			//If we actually have a stack offset to deal with
			if(stack_offset != 0){
				//We'll have something like <offset>(%rsp, t4)
				instruction->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_AND_OFFSET;

				//Emit the offset
				instruction->offset = emit_direct_integer_or_char_constant(stack_offset, i64);

				//This will be the stack pointer
				instruction->address_calc_reg1 = stack_pointer_variable;

				//And this is whatever was there before
				instruction->address_calc_reg2 = instruction->op2;

				//The base(address calc reg1) and index(address calc reg 2) registers must be the same type.
				//We determine that the base address is the dominating force, and takes precedence, so the address calc reg2
				//must adhere to this one's type
				if(is_converting_move_required(instruction->address_calc_reg1->type, instruction->address_calc_reg2->type) == TRUE){
					instruction->address_calc_reg2 = create_and_insert_converting_move_instruction(instruction, instruction->address_calc_reg2, instruction->address_calc_reg1->type);
				}

			//Otherwise there's no stack offset, so we'll keep the op2 and only have registers
			} else {
				//Change the mode
				instruction->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_ONLY;
				
				//Copy both over
				instruction->address_calc_reg1 = stack_pointer_variable;
				instruction->address_calc_reg2 = instruction->op2;

				//The base(address calc reg1) and index(address calc reg 2) registers must be the same type.
				//We determine that the base address is the dominating force, and takes precedence, so the address calc reg2
				//must adhere to this one's type
				if(is_converting_move_required(instruction->address_calc_reg1->type, instruction->address_calc_reg2->type) == TRUE){
					instruction->address_calc_reg2 = create_and_insert_converting_move_instruction(instruction, instruction->address_calc_reg2, instruction->address_calc_reg1->type);
				}
			}

		//Otherwise, we are loading a global variable with a subsequent offset. We will need to first
		//load the address of said global variable, and then use that with an address calculation. We 
		//are not able to combine the 2 in such a way
		} else {
			//Let the helper do the work
			instruction_t* global_variable_address = emit_global_variable_address_calculation_x86(instruction->op1, instruction_pointer_variable, u64);

			//Now insert this before the given instruction
			insert_instruction_before_given(global_variable_address, instruction);

			//These are registers only
			instruction->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_ONLY;

			//The destination of the global variable address will be our new address calc reg 1. 
			//We already have the offset loaded in, so that remains unchanged
			instruction->address_calc_reg1 = global_variable_address->destination_register;

			//The second address calc register is whatever is in op2
			instruction->address_calc_reg2 = instruction->op2;

			//The base(address calc reg1) and index(address calc reg 2) registers must be the same type.
			//We determine that the base address is the dominating force, and takes precedence, so the address calc reg2
			//must adhere to this one's type
			if(is_converting_move_required(instruction->address_calc_reg1->type, instruction->address_calc_reg2->type) == TRUE){
				instruction->address_calc_reg2 = create_and_insert_converting_move_instruction(instruction, instruction->address_calc_reg2, instruction->address_calc_reg1->type);
			}
		}

	//Otherwise we aren't on the stack, so we can just keep both registers
	} else {
		//Just have registers here
		instruction->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_ONLY;

		//Assign over like such
		instruction->address_calc_reg1 = instruction->op1;
		instruction->address_calc_reg2 = instruction->op2;

		//The base(address calc reg1) and index(address calc reg 2) registers must be the same type.
		//We determine that the base address is the dominating force, and takes precedence, so the address calc reg2
		//must adhere to this one's type
		if(is_converting_move_required(instruction->address_calc_reg1->type, instruction->address_calc_reg2->type) == TRUE){
			instruction->address_calc_reg2 = create_and_insert_converting_move_instruction(instruction, instruction->address_calc_reg2, instruction->address_calc_reg1->type);
		}
	}
}


/**
 * Handle the base address for a load statement in all of its forms. This includes
 * global variables, stack variables, and plain variables as well. This is meant to
 * be used by the lea combiner rule. It will *not* modify addressing modes and it should
 * not be expected to give a full and complete result back. It will only modify
 * address calc reg1 and the offset if appropriate
 */
static void handle_load_statement_base_address(instruction_t* load_statement){
	//If we have a memory address variable(super common), we'll need to
	//handle this now
	if(load_statement->op1->variable_type == VARIABLE_TYPE_MEMORY_ADDRESS){
		//If this is *not* a global variable
		if(load_statement->op1->linked_var->membership != GLOBAL_VARIABLE){
			//This is our stack offset, it will be needed going forward
			int64_t stack_offset = load_statement->op1->linked_var->stack_region->base_address;

			//If we actually have a stack offset to deal with. We'll store the offset constant
			//and op1
			if(stack_offset != 0){
				//Emit the offset
				load_statement->offset = emit_direct_integer_or_char_constant(stack_offset, i64);

				//This will be the stack pointer
				load_statement->address_calc_reg1 = stack_pointer_variable;

			//Otherwise there's no stack offset, so we'll just have the stack
			//pointer
			} else {
				//Copy both over
				load_statement->address_calc_reg1 = stack_pointer_variable;
			}

		//Otherwise, we are loading a global variable with a subsequent offset. We will need to first
		//load the address of said global variable, and then use that with an address calculation. We 
		//are not able to combine the 2 in such a way
		} else {
			//Let the helper do the work
			instruction_t* global_variable_address = emit_global_variable_address_calculation_x86(load_statement->op1, instruction_pointer_variable, u64);

			//Now insert this before the given instruction
			insert_instruction_before_given(global_variable_address, load_statement);

			//The destination of the global variable address will be our new address calc reg 1. 
			//We already have the offset loaded in, so that remains unchanged
			load_statement->address_calc_reg1 = global_variable_address->destination_register;
		}

	//Otherwise we aren't on the stack, so we can just keep both registers
	} else {
		//Assign over like such
		load_statement->address_calc_reg1 = load_statement->op1;
	}
}


/**
 * Combine and select all cases where we have a variable offset load that can be combined
 * with a lea to form a singular instruction. This handles all cases, and performs the deletion
 * of the given lea statement at the end
 */
static void combine_lea_with_variable_offset_load_instruction(instruction_window_t* window, instruction_t* lea_statement, instruction_t* variable_offset_load){
	//Cache all of these now before we do any manipulations
	variable_size_t destination_size = get_type_size(variable_offset_load->assignee->type);
	u_int8_t is_destination_signed = is_type_signed(variable_offset_load->assignee->type);
	//For a load, the source size is stored in the destination itself
	variable_size_t source_size = get_type_size(variable_offset_load->memory_read_write_type);

	/**
	 * Go through all valid cases here. Note that anything where we have 2 registers in the lea
	 * will not work because we then wouldn't have enough room for the base address/rsp register
	 * in the final load. As such the ones that work here revolve around one register lea's that can
	 * be combined
	 */
	switch(lea_statement->lea_statement_type){
		/**
		 * Turns:
		 *  t4 <- 4(t5)
		 *  load t6 <- MEM<t3>[t4]
		 *
		 *  movX 8(rsp, t4), t6
		 */
		case OIR_LEA_TYPE_OFFSET_ONLY:
			//Let the helper deal with the load base address
			handle_load_statement_base_address(variable_offset_load);

			//If there are any constants to add, we'll do that now
			if(variable_offset_load->offset != NULL){
				add_constants(variable_offset_load->offset, lea_statement->op1_const);

			//Otherwise copy it over
			} else {
				variable_offset_load->offset = lea_statement->op1_const;
			}

			//Our op2 is now the first operand from the old lea
			variable_offset_load->address_calc_reg2 = lea_statement->op1;

			//The base(address calc reg1) and index(address calc reg 2) registers must be the same type.
			//We determine that the base address is the dominating force, and takes precedence, so the address calc reg2
			//must adhere to this one's type
			if(is_converting_move_required(variable_offset_load->address_calc_reg1->type, variable_offset_load->address_calc_reg2->type) == TRUE){
				variable_offset_load->address_calc_reg2 = create_and_insert_converting_move_instruction(variable_offset_load, variable_offset_load->address_calc_reg2, variable_offset_load->address_calc_reg1->type);
			}

			//This one will have an addressing type of registers and offset
			variable_offset_load->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_AND_OFFSET;

			//The lea is now useless so get rid of it
			delete_statement(lea_statement);

			break;
			
		/**
		 * Turns:
		 *  t4 <- (, t5, 4)
		 *  load t6 <- MEM<t3>[t4]
		 *
		 *  movX 16(rsp, t5, 4), t6
		 */
		case OIR_LEA_TYPE_INDEX_AND_SCALE:
			//Let the helper deal with the load base address
			handle_load_statement_base_address(variable_offset_load);

			//Copy the scale over
			variable_offset_load->lea_multiplier = lea_statement->lea_multiplier;

			//Our op2 is now the first operand from the old lea
			variable_offset_load->address_calc_reg2 = lea_statement->op1;

			//The base(address calc reg1) and index(address calc reg 2) registers must be the same type.
			//We determine that the base address is the dominating force, and takes precedence, so the address calc reg2
			//must adhere to this one's type
			if(is_converting_move_required(variable_offset_load->address_calc_reg1->type, variable_offset_load->address_calc_reg2->type) == TRUE){
				variable_offset_load->address_calc_reg2 = create_and_insert_converting_move_instruction(variable_offset_load, variable_offset_load->address_calc_reg2, variable_offset_load->address_calc_reg1->type);
			}

			//The calculation mode type depends on the constant
			if(variable_offset_load->offset != NULL){
				variable_offset_load->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_OFFSET_AND_SCALE;
			} else {
				variable_offset_load->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_AND_SCALE;
			}

			//The lea is now useless so get rid of it
			delete_statement(lea_statement);

			break;

		/**
		 * Turns:
		 *  t4 <- 4(, t5, 4)
		 *  load t6 <- MEM<t3>[t4]
		 *
		 *  movX 20(rsp, t5, 4), t6
		 */
		case OIR_LEA_TYPE_INDEX_OFFSET_AND_SCALE:
			//Let the helper deal with the load base address
			handle_load_statement_base_address(variable_offset_load);

			//If there are any constants to add, we'll do that now
			if(variable_offset_load->offset != NULL){
				add_constants(variable_offset_load->offset, lea_statement->op1_const);

			//Otherwise copy it over
			} else {
				variable_offset_load->offset = lea_statement->op1_const;
			}

			//Copy the scale over
			variable_offset_load->lea_multiplier = lea_statement->lea_multiplier;

			//Our op2 is now the first operand from the old lea
			variable_offset_load->address_calc_reg2 = lea_statement->op1;

			//The base(address calc reg1) and index(address calc reg 2) registers must be the same type.
			//We determine that the base address is the dominating force, and takes precedence, so the address calc reg2
			//must adhere to this one's type
			if(is_converting_move_required(variable_offset_load->address_calc_reg1->type, variable_offset_load->address_calc_reg2->type) == TRUE){
				variable_offset_load->address_calc_reg2 = create_and_insert_converting_move_instruction(variable_offset_load, variable_offset_load->address_calc_reg2, variable_offset_load->address_calc_reg1->type);
			}

			//This will always be a registers, offset and scale type
			variable_offset_load->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_OFFSET_AND_SCALE;

			//The lea is now useless so get rid of it
			delete_statement(lea_statement);

			break;

		//By default - if we can't handle it, we just invoke the other helpers and call
		//it quits. This ensures uniform behavior and correctness
		default:
			handle_lea_statement(lea_statement);
			handle_load_with_variable_offset_instruction(variable_offset_load);

			//Reconstruct around the last one(the load)
			reconstruct_window(window, variable_offset_load);

			//And get out
			return;
	}
	
	//NOTE: These are down here so that the default clause in the above switch can take effect and avoid doing duplicate work
	//Let the helper decide for us. We are choosing the source as clean, since it is coming from memory
	variable_offset_load->instruction_type = select_move_instruction(destination_size, source_size, is_destination_signed, TRUE);
	//This is a read from memory type
	variable_offset_load->memory_access_type = READ_FROM_MEMORY;
	//Handle the destination assignment
	handle_load_instruction_destination_assignment(variable_offset_load);

	/**
	 * If we have a conversion instruction that has an SSE destination, we need to emit
	 * a special "pxor" statement beforehand to completely wipe out said register
	 */
	if(is_integer_to_sse_conversion_instruction(variable_offset_load->instruction_type) == TRUE){
		//We need to completely zero out the destination register here, so we will emit a pxor to do
		//just that
		instruction_t* pxor_instruction = emit_direct_pxor_instruction(variable_offset_load->destination_register);

		//Get this in right before the given
		insert_instruction_before_given(pxor_instruction, variable_offset_load);
	}

	//The window always needs to be rebuilt around the last instruction that we touched
	reconstruct_window(window, variable_offset_load);
}


/**
 * Combine a lea with a regular load instruction. This is mainly intended to be used with the 
 * rip relative constant addressing, but we may extend it in the future
 */
static void combine_lea_with_regular_load_instruction(instruction_window_t* window, instruction_t* lea_statement, instruction_t* load_statement){
	//Local variable declarations
	variable_size_t destination_size;
	variable_size_t source_size;
	u_int8_t is_destination_signed;
	
	//Go based on what kind of lea we have
	switch(lea_statement->lea_statement_type){
		/**
		 * This is our main target with this rule
		 */
		case OIR_LEA_TYPE_RIP_RELATIVE:
			//We need the destination and source sizes to determine our movement instruction
			destination_size = get_type_size(load_statement->assignee->type);
			//Is the destination signed? This is also required inof
			is_destination_signed = is_type_signed(load_statement->assignee->type);
			//For a load, the source size is stored in the instruction itself
			source_size = get_type_size(load_statement->memory_read_write_type);

			//Let the helper select for us. The source is guaranteed to be clean since it's coming from memory
			load_statement->instruction_type = select_move_instruction(destination_size, source_size, is_destination_signed, TRUE);

			//Let the helper deal with the load instruction's destination
			handle_load_instruction_destination_assignment(load_statement);

			/**
			 * If we have a conversion instruction that has an SSE destination, we need to emit
			 * a special "pxor" statement beforehand to completely wipe out said register
			 */
			if(is_integer_to_sse_conversion_instruction(load_statement->instruction_type) == TRUE){
				//We need to completely zero out the destination register here, so we will emit a pxor to do
				//just that
				instruction_t* pxor_instruction = emit_direct_pxor_instruction(load_statement->destination_register);

				//Get this in right before the given
				insert_instruction_before_given(pxor_instruction, load_statement);
			}

			//We are reading from memory here
			load_statement->memory_access_type = READ_FROM_MEMORY;

			//This will be a rip-relative address
			load_statement->calculation_mode = ADDRESS_CALCULATION_MODE_RIP_RELATIVE;

			//The first thing we need is the %rip register
			load_statement->address_calc_reg1 = instruction_pointer_variable;

			//The rip offset variable is our .LCx value
			load_statement->rip_offset_variable = lea_statement->op2;

			//Now that we've gotten all we need from the lea, we can delete it
			delete_statement(lea_statement);

			//Rebuild the window based on the load statement
			reconstruct_window(window, load_statement);

			break;

		//By default, just do nothing and leave the instruction window
		//as is. It will be picked up by the rest of the selector as
		//normal
		default:
			break;
	}
}


/**
 * Handle a store instruction. This will be reorganized into a memory accessing move.
 */
static void handle_store_instruction(instruction_t* instruction){
	//Store is to memory
	instruction->memory_access_type = WRITE_TO_MEMORY;

	//If we have a memory address var here(very common)
	if(instruction->assignee->variable_type == VARIABLE_TYPE_MEMORY_ADDRESS){
		//If it is *not* a global variable(most common case)
		if(instruction->assignee->linked_var->membership != GLOBAL_VARIABLE){
			//Get the stack offset
			int64_t stack_offset = instruction->assignee->linked_var->stack_region->base_address;

			//If it's not 0, we need to do some arithmetic
			if(stack_offset != 0){
				//Let's get the offset from this memory address
				three_addr_const_t* offset = emit_direct_integer_or_char_constant(instruction->assignee->linked_var->stack_region->base_address, u64);

				//The first address calc register will be the stack pointer
				instruction->address_calc_reg1 = stack_pointer_variable;

				//And we need to store the offset
				instruction->offset = offset;

				//This counts for our destination only
				instruction->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;

			//If it is 0, we only need to deref the stack pointer
			} else {
				//This is the stack pointer, no offset is needed
				instruction->destination_register = stack_pointer_variable;

				//Just dereference the destination here, nothing more
				instruction->calculation_mode = ADDRESS_CALCULATION_MODE_DEREF_ONLY_DEST;
			}

		//Otherwise, this is a global variable so we need to take special steps to deal with it
		} else {
			//This is going to be a global variable movement
			instruction->calculation_mode = ADDRESS_CALCULATION_MODE_RIP_RELATIVE;

			//The address calc reg1 is the instruction pointer
			instruction->address_calc_reg1 = instruction_pointer_variable;

			//The global variable is held by the offset
			instruction->rip_offset_variable = instruction->assignee;
		}

	//Otherwise this is something like a pointer dereference, just a pure store so we'll
	//be using the assignee as the destination
	} else {
		//Otherwise this is just the destination register
		instruction->destination_register = instruction->assignee;

		//This counts for our destination only
		instruction->calculation_mode = ADDRESS_CALCULATION_MODE_DEREF_ONLY_DEST;
	}

	//Invoke the helper to determine the type and instruction type
	handle_store_instruction_sources_and_instruction_type(instruction);
}


/**
 * Handle an instruction like
 *
 * store MEM<t5>[4] <- t7
 *
 * movX t7, 8(%rsp)
 *
 * This will always be an OFFSET_ONLY calculation type
 */
static void handle_store_with_constant_offset_instruction(instruction_t* instruction){
	//Invoke the helper for our source assignment
	handle_store_instruction_sources_and_instruction_type(instruction);

	//This is a write regardless
	instruction->memory_access_type = WRITE_TO_MEMORY;

	//Do we have a memory address variable(very common) or not?
	if(instruction->assignee->variable_type == VARIABLE_TYPE_MEMORY_ADDRESS){
		//Grab the linked var out
		symtab_variable_record_t* linked_var = instruction->assignee->linked_var;

		//If we have a non-global variable, then we've got a stack
		//address
		if(linked_var->membership != GLOBAL_VARIABLE){
			//Get the stack offset
			int64_t stack_offset = instruction->assignee->linked_var->stack_region->base_address;

			//If it's not 0, we need to do some arithmetic with the constants
			if(stack_offset != 0){
				//This is still the stack pointer
				instruction->address_calc_reg1 = stack_pointer_variable;

				//We'll now add the stack offset to the offset that we already have
				//in the "offset variable"
				sum_constant_with_raw_int64_value(instruction->offset, i64, stack_offset);

				//Once that's done, we just need to change the address calc mode
				instruction->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;

			//Even if this is 0, we still need to account for the offset in the original
			//statement
			} else {
				//The base address is the assignee
				instruction->address_calc_reg1 = stack_pointer_variable;

				//The offset is already stored in the "offset" field
				
				//This has the address calc and the offset
				instruction->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;
			}

		//If we have a global variable, we can use a special rip-relative addressing mode to make this happen
		} else {
			//The first address calc register is the instruction pointer always
			instruction->address_calc_reg1 = instruction_pointer_variable;

			//The offset is already in place, we just need to set the rip offset variable based on the assignee
			instruction->rip_offset_variable = instruction->assignee;

			//All that we need to do now is change the calculation mode to be rip with offset
			instruction->calculation_mode = ADDRESS_CALCULATION_MODE_RIP_RELATIVE_WITH_OFFSET;
		}

	//Otherwise there is no memory address, so we just handle normally
	} else {
		//The base address is the assignee
		instruction->address_calc_reg1 = instruction->assignee;

		//The offset is already stored where we need it to be
		//Set the type
		instruction->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY; 
	}
}


/**
 * Handle an instruction like
 *
 * store MEM<t5>[t6] <- t7
 *
 * movX t7, 4(%rsp, t6)
 *
 * This will most often generate stores with offsets and registers
 */
static void handle_store_with_variable_offset_instruction(instruction_t* instruction){
	//Invoke the helper for our source assignment
	handle_store_instruction_sources_and_instruction_type(instruction);

	//This is a write regardless
	instruction->memory_access_type = WRITE_TO_MEMORY;

	//Do we have a memory address variable(very common) or not?
	if(instruction->assignee->variable_type == VARIABLE_TYPE_MEMORY_ADDRESS){
		//Grab the linked var out
		symtab_variable_record_t* linked_var = instruction->assignee->linked_var;

		//If we have a non-global variable, then we've got a stack
		//address
		if(linked_var->membership != GLOBAL_VARIABLE){
			//Get the stack offset
			int64_t stack_offset = instruction->assignee->linked_var->stack_region->base_address;

			//If it's not 0, we need to do some arithmetic with the constants
			if(stack_offset != 0){
				//Once that's done, we just need to change the address calc mode
				instruction->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_AND_OFFSET;

				//This is still the stack pointer
				instruction->address_calc_reg1 = stack_pointer_variable;

				//This is the variable offset
				instruction->address_calc_reg2 = instruction->op1;

				//We will need to have a stack offset here since the memory base address has one
				instruction->offset = emit_direct_integer_or_char_constant(stack_offset, i64);

				//The base(address calc reg1) and index(address calc reg 2) registers must be the same type.
				//We determine that the base address is the dominating force, and takes precedence, so the address calc reg2
				//must adhere to this one's type
				if(is_converting_move_required(instruction->address_calc_reg1->type, instruction->address_calc_reg2->type) == TRUE){
					instruction->address_calc_reg2 = create_and_insert_converting_move_instruction(instruction, instruction->address_calc_reg2, instruction->address_calc_reg1->type);
				}

			//Even if this is 0, we still need to account for the offset in the original
			//statement
			} else {
				//This has registers only
				instruction->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_ONLY;

				//The base address is the assignee
				instruction->address_calc_reg1 = stack_pointer_variable;

				//And the offset is op1
				instruction->address_calc_reg2 = instruction->op1;

				//The base(address calc reg1) and index(address calc reg 2) registers must be the same type.
				//We determine that the base address is the dominating force, and takes precedence, so the address calc reg2
				//must adhere to this one's type
				if(is_converting_move_required(instruction->address_calc_reg1->type, instruction->address_calc_reg2->type) == TRUE){
					instruction->address_calc_reg2 = create_and_insert_converting_move_instruction(instruction, instruction->address_calc_reg2, instruction->address_calc_reg1->type);
				}
			}

		//If we have a global variable, we will need to first load in the address and then go through and 
		//handle the value normally
		} else {
			//Let the helper do the work
			instruction_t* global_variable_address = emit_global_variable_address_calculation_x86(instruction->assignee, instruction_pointer_variable, u64);

			//Now insert this before the given instruction
			insert_instruction_before_given(global_variable_address, instruction);

			//We have 2 registers so this is registers only
			instruction->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_ONLY;

			//The destination of the global variable address will be our new address calc reg 1. 
			//We already have the offset loaded in, so that remains unchanged
			instruction->address_calc_reg1 = global_variable_address->destination_register;

			//Address calc reg 2 is op1 always
			instruction->address_calc_reg2 = instruction->op1;

			//The base(address calc reg1) and index(address calc reg 2) registers must be the same type.
			//We determine that the base address is the dominating force, and takes precedence, so the address calc reg2
			//must adhere to this one's type
			if(is_converting_move_required(instruction->address_calc_reg1->type, instruction->address_calc_reg2->type) == TRUE){
				instruction->address_calc_reg2 = create_and_insert_converting_move_instruction(instruction, instruction->address_calc_reg2, instruction->address_calc_reg1->type);
			}
		}

	//Otherwise there is no memory address, so we just handle normally
	} else {
		//The offset is already stored where we need it to be
		//Set the type
		instruction->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_ONLY; 

		//The base address is the assignee
		instruction->address_calc_reg1 = instruction->assignee;

		//And the offset is op1
		instruction->address_calc_reg2 = instruction->op1;

		//The base(address calc reg1) and index(address calc reg 2) registers must be the same type.
		//We determine that the base address is the dominating force, and takes precedence, so the address calc reg2
		//must adhere to this one's type
		if(is_converting_move_required(instruction->address_calc_reg1->type, instruction->address_calc_reg2->type) == TRUE){
			instruction->address_calc_reg2 = create_and_insert_converting_move_instruction(instruction, instruction->address_calc_reg2, instruction->address_calc_reg1->type);
		}
	}
}


/**
 * Handle the base address for a store statement in all of its forms. This includes
 * global variables, stack variables, and plain variables as well. This is meant to
 * be used by the lea combiner rule. It will *not* modify addressing modes and it should
 * not be expected to give a full and complete result back. It will only modify
 * address calc reg1 and the offset if appropriate
 */
static void handle_store_statement_base_address(instruction_t* store_instruction){
	//Do we have a memory address variable(very common) or not?
	if(store_instruction->assignee->variable_type == VARIABLE_TYPE_MEMORY_ADDRESS){
		//Grab the linked var out
		symtab_variable_record_t* linked_var = store_instruction->assignee->linked_var;

		//If we have a non-global variable, then we've got a stack
		//address
		if(linked_var->membership != GLOBAL_VARIABLE){
			//Get the stack offset
			int64_t stack_offset = linked_var->stack_region->base_address;

			//If it's not 0, we need to do some arithmetic with the constants
			if(stack_offset != 0){
				//Once that's done, we just need to change the address calc mode
				store_instruction->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_AND_OFFSET;

				//This is still the stack pointer
				store_instruction->address_calc_reg1 = stack_pointer_variable;

				//We will need to have a stack offset here since the memory base address has one
				store_instruction->offset = emit_direct_integer_or_char_constant(stack_offset, i64);

			//All that we need to do now is use the stack pointer
			} else {
				//The base address is the assignee
				store_instruction->address_calc_reg1 = stack_pointer_variable;
			}

		//If we have a global variable, we will need to first load in the address and then go through and 
		//handle the value normally
		} else {
			//Let the helper do the work
			instruction_t* global_variable_address = emit_global_variable_address_calculation_x86(store_instruction->assignee, instruction_pointer_variable, u64);

			//Now insert this before the given instruction
			insert_instruction_before_given(global_variable_address, store_instruction);

			//The destination of the global variable address will be our new address calc reg 1. 
			//We already have the offset loaded in, so that remains unchanged
			store_instruction->address_calc_reg1 = global_variable_address->destination_register;
		}

	//Otherwise there is no memory address, so we just handle normally
	} else {
		//The base address is the assignee
		store_instruction->address_calc_reg1 = store_instruction->assignee;
	}
}


/**
 * Combine and select all cases where we have a variable offset store that can be combined
 * with a lea to form a singular instruction. This handles all cases, and performs the deletion
 * of the given lea statement at the end
 */
static void combine_lea_with_variable_offset_store_instruction(instruction_window_t* window, instruction_t* lea_statement, instruction_t* variable_offset_store){
	/**
	 * Go through all valid cases here. Note that anything where we have 2 registers in the lea
	 * will not work because we then wouldn't have enough room for the base address/rsp register
	 * in the final load. As such the ones that work here revolve around one register lea's that can
	 * be combined
	 */
	switch(lea_statement->lea_statement_type){
		/**
		 * Turns:
		 *  t4 <- 4(t5)
		 *  load t6 <- MEM<t3>[t4]
		 *
		 *  movX 8(rsp, t4), t6
		 */
		case OIR_LEA_TYPE_OFFSET_ONLY:
			//Let the helper deal with the base address
			handle_store_statement_base_address(variable_offset_store);

			//If we're able to combine constants here, we will
			if(variable_offset_store->offset != NULL){
				//Add the 2, result is in the store's constant
				add_constants(variable_offset_store->offset, lea_statement->op1_const);

			//Otherwise do a straight copy
			} else {
				variable_offset_store->offset = lea_statement->op1_const;
			}

			//Now get the address calc reg 2
			variable_offset_store->address_calc_reg2 = lea_statement->op1;

			//The base(address calc reg1) and index(address calc reg 2) registers must be the same type.
			//We determine that the base address is the dominating force, and takes precedence, so the address calc reg2
			//must adhere to this one's type
			if(is_converting_move_required(variable_offset_store->address_calc_reg1->type, variable_offset_store->address_calc_reg2->type) == TRUE){
				variable_offset_store->address_calc_reg2 = create_and_insert_converting_move_instruction(variable_offset_store, variable_offset_store->address_calc_reg2, variable_offset_store->address_calc_reg1->type);
			}

			//The calculation mode here will always be registers and offset
			variable_offset_store->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_AND_OFFSET;

			//The lea is redundant
			delete_statement(lea_statement);

			//Rebuild around the final instruction(the store)
			reconstruct_window(window, variable_offset_store);

			break;
			
		/**
		 * Turns:
		 *  t4 <- (, t5, 4)
		 *  load t6 <- MEM<t3>[t4]
		 *
		 *  movX 16(rsp, t5, 4), t6
		 */
		case OIR_LEA_TYPE_INDEX_AND_SCALE:
			//Let the helper deal with the base address
			handle_store_statement_base_address(variable_offset_store);

			//Now get the address calc reg 2
			variable_offset_store->address_calc_reg2 = lea_statement->op1;

			//Copy over the lea multiplier
			variable_offset_store->lea_multiplier = lea_statement->lea_multiplier;

			//The base(address calc reg1) and index(address calc reg 2) registers must be the same type.
			//We determine that the base address is the dominating force, and takes precedence, so the address calc reg2
			//must adhere to this one's type
			if(is_converting_move_required(variable_offset_store->address_calc_reg1->type, variable_offset_store->address_calc_reg2->type) == TRUE){
				variable_offset_store->address_calc_reg2 = create_and_insert_converting_move_instruction(variable_offset_store, variable_offset_store->address_calc_reg2, variable_offset_store->address_calc_reg1->type);
			}

			//Determine the calculation mode based on the present of offset
			if(variable_offset_store->offset != NULL){
				variable_offset_store->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_OFFSET_AND_SCALE;
			} else {
				variable_offset_store->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_AND_SCALE;
			}

			//The lea is redundant
			delete_statement(lea_statement);

			//Rebuild around the final instruction(the store)
			reconstruct_window(window, variable_offset_store);

			break;

		/**
		 * Turns:
		 *  t4 <- 4(, t5, 4)
		 *  load t6 <- MEM<t3>[t4]
		 *
		 *  movX 20(rsp, t5, 4), t6
		 */
		case OIR_LEA_TYPE_INDEX_OFFSET_AND_SCALE:
			//Let the helper deal with the base address
			handle_store_statement_base_address(variable_offset_store);

			//If we're able to combine constants here, we will
			if(variable_offset_store->offset != NULL){
				//Add the 2, result is in the store's constant
				add_constants(variable_offset_store->offset, lea_statement->op1_const);

			//Otherwise do a straight copy
			} else {
				variable_offset_store->offset = lea_statement->op1_const;
			}

			//Now get the address calc reg 2
			variable_offset_store->address_calc_reg2 = lea_statement->op1;

			//Copy over the lea multiplier
			variable_offset_store->lea_multiplier = lea_statement->lea_multiplier;

			//The base(address calc reg1) and index(address calc reg 2) registers must be the same type.
			//We determine that the base address is the dominating force, and takes precedence, so the address calc reg2
			//must adhere to this one's type
			if(is_converting_move_required(variable_offset_store->address_calc_reg1->type, variable_offset_store->address_calc_reg2->type) == TRUE){
				variable_offset_store->address_calc_reg2 = create_and_insert_converting_move_instruction(variable_offset_store, variable_offset_store->address_calc_reg2, variable_offset_store->address_calc_reg1->type);
			}

			//This will always have registers, an offset and a scale
			variable_offset_store->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_OFFSET_AND_SCALE;

			//The lea is redundant
			delete_statement(lea_statement);

			//Rebuild around the final instruction(the store)
			reconstruct_window(window, variable_offset_store);

			break;

		//By default - if we can't handle it, we just invoke the other helpers and call
		//it quits. This ensures uniform behavior and correctness
		default:
			handle_lea_statement(lea_statement);
			handle_store_with_variable_offset_instruction(variable_offset_store);

			//Reconstruct around the last one(the load)
			reconstruct_window(window, variable_offset_store);

			//And get out
			return;
	}
	
	//NOTE: These are down here so that the default clause in the above switch can take effect and avoid doing duplicate work
	//Invoke the helper for our source assignment
	handle_store_instruction_sources_and_instruction_type(variable_offset_store);
	//This is a write regardless
	variable_offset_store->memory_access_type = WRITE_TO_MEMORY;

	//The window always needs to be rebuilt around the last instruction that we touched
	reconstruct_window(window, variable_offset_store);
}


/**
 * Select instructions that follow a singular pattern. This one single pass will run after
 * the pattern selector ran and perform one-to-one mappings on whatever is left.
 */
static void select_instruction_patterns(instruction_window_t* window){
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
					true_source = create_and_insert_converting_move_instruction(window->instruction1, window->instruction1->op2, i32);

				//Otherwise, we'll use the unsigned version
				} else {
					true_source = create_and_insert_converting_move_instruction(window->instruction1, window->instruction1->op2, u32);
				}

				break;
		}

		//The source register is op1
		window->instruction2->source_register = true_source;

		//Store the jumping to block where the jump table is
		window->instruction2->if_block = window->instruction1->if_block;

		//We also have an "S" multiplicator factor that will always be a power of 2 stored in the lea_multiplier
		window->instruction2->lea_multiplier = window->instruction1->lea_multiplier;

		//We're now able to delete instruction 1
		delete_statement(window->instruction1);

		//Reconstruct the window with instruction2 as the start
		reconstruct_window(window, window->instruction2);
		return;
	}


	/**
	 * Compressing lea constant loads with the rip-relative addressing that
	 * comes before them
	 *
	 * This would be something like:
	 *  t4 <- .LC0(%rip)
	 *  t5 <- load t4
	 *
	 *  We can combine this to be
	 * 	t5 <- .LC0(%rip)
	 */
	if(window->instruction2 != NULL
		&& window->instruction2->statement_type == THREE_ADDR_CODE_LOAD_STATEMENT
		&& window->instruction1->statement_type == THREE_ADDR_CODE_LEA_STMT
		&& window->instruction1->assignee->variable_type == VARIABLE_TYPE_TEMP
		&& variables_equal(window->instruction1->assignee, window->instruction2->op1, TRUE) == TRUE){

		//Invoke a special helper here that will deal with the selection for us and also
		//modify our window
		combine_lea_with_regular_load_instruction(window, window->instruction1, window->instruction2);
		return;
	}


	/**
	 * Compressing variable offset loads with leas that come beforehand. We do this to turn
	 * 2 expressions into one big addressing expression
	 */
	if(window->instruction2 != NULL
		&& window->instruction2->statement_type == THREE_ADDR_CODE_LOAD_WITH_VARIABLE_OFFSET
		&& window->instruction1->statement_type == THREE_ADDR_CODE_LEA_STMT
		&& window->instruction1->lea_statement_type != OIR_LEA_TYPE_RIP_RELATIVE //Nothing to do if we have this
		//Is the lea's assignee equal to the offset of the load
		&& variables_equal(window->instruction1->assignee, window->instruction2->op2, TRUE) == TRUE){

		//Let the helper deal with it. This helper handles all possible cases, so once it's done this whole
		//rule is done and we can return
		combine_lea_with_variable_offset_load_instruction(window, window->instruction1, window->instruction2);
		return;
	}

	/**
	 * Compressing variable offset stores with leas that come beforehand. We do this to turn
	 * 2 expressions into one big addressing expression
	 */
	if(window->instruction2 != NULL
		&& window->instruction2->statement_type == THREE_ADDR_CODE_STORE_WITH_VARIABLE_OFFSET 
		&& window->instruction1->statement_type == THREE_ADDR_CODE_LEA_STMT
		&& window->instruction1->lea_statement_type != OIR_LEA_TYPE_RIP_RELATIVE //Nothing to do if we have this
		//Is the lea's assignee equal to the offset(op1) of the store
		&& variables_equal(window->instruction1->assignee, window->instruction2->op1, TRUE) == TRUE){

		//Let the helper deal with it. This helper handles all possible cases, so once it's done this whole
		//rule is done and we can return
		combine_lea_with_variable_offset_store_instruction(window, window->instruction1, window->instruction2);
		return;
	}

	//We could see logical and/logical or
	if(is_instruction_binary_operation(window->instruction1) == TRUE){
		switch(window->instruction1->op){
			//Handle the logical and case
			case DOUBLE_AND:
				//TODO FLOAT VERSION NEEDED
				handle_logical_and_instruction(window);
				return;

			//Handle logical or
			case DOUBLE_OR:
				//TODO FLOAT VERSION NEEDED
				handle_logical_or_instruction(window);
				return;

			//Handle division
			case F_SLASH:
				//Likely the most common case - it's not a float
				if(IS_FLOATING_POINT(window->instruction1->assignee->type) == FALSE){
					handle_division_instruction(window);

				//Otherwise we have a floating point division here so we need
				//to handle it appropriately
				} else {
					handle_sse_division_instruction(window);
				}

				return;

			//Handle modulus
			case MOD:	
				//This will generate more than one instruction
				handle_modulus_instruction(window);
				return;

			//If we have a multiplication *and* it's unsigned, we go here
			case STAR:
				//Likely the most common case - it's not a float
				if(IS_FLOATING_POINT(window->instruction1->assignee->type) == FALSE){
					//Only do this if we're unsigned
					if(is_type_signed(window->instruction1->assignee->type) == FALSE){
						//Let the helper deal with it
						handle_unsigned_multiplication_instruction(window);
						return;
					}

				//Otherwise we have a floating point multiplication here so we need
				//to handle it appropriately
				} else {
					handle_sse_multiplication_instruction(window->instruction1);
				}

				break;

			default:
				break;
		}
	}

	//The instruction that we have here is the window's instruction 1
	instruction_t* instruction = window->instruction1;

	//Switch on whatever we have currently
	switch (instruction->statement_type) {
		case THREE_ADDR_CODE_ASSN_STMT:
			handle_register_movement_instruction(instruction);
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
			/**
			 * Some comparison instructions need us to set the
			 * value of them after the fact, while others don't
			 * (think ones that are relied on by branches). So, we will
			 * invoke a special rule for relational and equality operators
			 * to handle these special cases
			 */
			switch(instruction->op){
				case DOUBLE_EQUALS:
				case NOT_EQUALS:
				case G_THAN:
				case G_THAN_OR_EQ:
				case L_THAN:
				case L_THAN_OR_EQ:
					//This helper does all of the heavy lifting. It will
					//return the last instruction that it created/touched
					instruction = handle_cmp_instruction(instruction);

					//Rebuild the window based on this
					reconstruct_window(window, instruction);

					break;

				//All other cases - just handle the instruction
				//normally
				default:
					handle_binary_operation_instruction(instruction);
					break;
			}

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
		default:
			break;
	}
}


/**
 * Run through every block and convert each instruction or sequence of instructions
 * from three address code to assembly statements
 */
static void select_instructions(cfg_t* cfg){
	//We will again do instruction selection on a per-function level basis
	for(u_int16_t i = 0; i < cfg->function_entry_blocks.current_index; i++){
		//Extract the entry
		basic_block_t* function_entry = dynamic_array_get_at(&(cfg->function_entry_blocks), i);

		//Save the current block here
		basic_block_t* current = function_entry;

		while(current != NULL){
			//Initialize the sliding window(very basic, more to come)
			instruction_window_t window = initialize_instruction_window(current);

			//Run through the window so long as we are not at the end
			do{
				//Select the instructions
				select_instruction_patterns(&window);

				//Slide the window
				slide_window(&window);

			//Keep going if we aren't at the end
			} while(window.instruction1 != NULL);

			//Advance the current up
			current = current->direct_successor;
		}
	}
}


/**
 * A function that selects all instructions, via the peephole method. This kind of 
 * operation completely translates the CFG out of a CFG. When done, we have a straight line
 * of code that we print out
 */
void select_all_instructions(compiler_options_t* options, cfg_t* cfg){
	//Grab these two general use types first
	u64 = lookup_type_name_only(cfg->type_symtab, "u64", NOT_MUTABLE)->type;
	i64 = lookup_type_name_only(cfg->type_symtab, "i64", NOT_MUTABLE)->type;
	i32 = lookup_type_name_only(cfg->type_symtab, "i32", NOT_MUTABLE)->type;
	u32 = lookup_type_name_only(cfg->type_symtab, "u32", NOT_MUTABLE)->type;
	u8 = lookup_type_name_only(cfg->type_symtab, "u8", NOT_MUTABLE)->type;

	//Stash the stack pointer & instruction pointer
	stack_pointer_variable = cfg->stack_pointer;
	instruction_pointer_variable = cfg->instruction_pointer;

	//Our very first step in the instruction selector is to order all of the blocks in one 
	//straight line. This step is also able to recognize and exploit some early optimizations,
	//such as when a block ends in a jump to the block right below it
	order_blocks(cfg);

	//Do we need to print intermediate representations?
	u_int8_t print_irs = options->print_irs;

	//We'll first print before we simplify
	if(print_irs == TRUE){
		printf("============================== BEFORE SIMPLIFY ========================================\n");
		print_ordered_blocks(cfg, PRINT_THREE_ADDRESS_CODE);
		printf("============================== AFTER SIMPLIFY ========================================\n");
	}

	//Once we've printed, we now need to simplify the operations. OIR already comes in an expanded
	//format that is used in the optimization phase. Now, we need to take that expanded IR and
	//recognize any redundant operations, dead values, unnecessary loads, etc.
	simplify(cfg);

	//If we need to print IRS, we can do so here
	if(print_irs == TRUE){
		print_ordered_blocks(cfg, PRINT_THREE_ADDRESS_CODE);
		printf("============================== AFTER INSTRUCTION SELECTION ========================================\n");
	}

	//Once we're done simplifying, we'll use the same sliding window technique to select instructions.
	select_instructions(cfg);

	//Final IR printing if requested by user
	if(print_irs == TRUE){
		print_ordered_blocks(cfg, PRINT_INSTRUCTION);
	}
}
