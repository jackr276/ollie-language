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
#include "../queue/heap_queue.h"
#include <stdio.h>
#include <sys/types.h>

//For standardization across all modules
#define TRUE 1
#define FALSE 0

//The window for our "sliding window" optimizer
typedef struct instruction_window_t instruction_window_t;


/**
 * What is the status of our sliding window? Are we at the beginning,
 * middle or end of the sequence?
 */
typedef enum {
	WINDOW_AT_START,
	WINDOW_AT_MIDDLE,
	WINDOW_AT_END,
} window_status_t;


/**
 * The widow that we have here will store three instructions at once. This allows
 * us to look at three instruction patterns at any given time.
 */
struct instruction_window_t{
	//We store three instructions and a status
	three_addr_code_stmt_t* instruction1;
	three_addr_code_stmt_t* instruction2;
	three_addr_code_stmt_t* instruction3;
	//This will tell us, at a quick glance, whether we're at the beginning,
	//middle or end of a sequence
	window_status_t status;
};


/**
 * Does the block that we're passing in end in a direct(jmp) jump to
 * the very next block. If so, we'll return what block the jump goes to.
 * If not, we'll return null.
 */
static basic_block_t* does_block_end_in_jump(basic_block_t* block){
	//Initially we have a NULL here
	basic_block_t* jumps_to = NULL;

	//If we have an exit statement that is a direct jump, then we've hit our match
	if(block->exit_statement != NULL && block->exit_statement->CLASS == THREE_ADDR_CODE_JUMP_STMT
	 && block->exit_statement->jump_type == JUMP_TYPE_JMP){
		jumps_to = block->exit_statement->jumping_to_block;
	}

	//Give back whatever we found
	return jumps_to;
}


/**
 * Simple utility for us to print out an instruction window
 */
static void print_instruction_window(instruction_window_t* window){
	printf("----------- Instruction Window ------------\n");
	//We'll just print out all three instructions
	if(window->instruction1 != NULL){
		print_three_addr_code_stmt(window->instruction1);
	} else {
		printf("EMPTY\n");
	}

	if(window->instruction2 != NULL){
		print_three_addr_code_stmt(window->instruction2);
	} else {
		printf("EMPTY\n");
	}
	
	if(window->instruction3 != NULL){
		print_three_addr_code_stmt(window->instruction3);
	} else {
		printf("EMPTY\n");
	}

	printf("-------------------------------------------\n");
}


/**
 * Advance the window up by 1 instruction. This means that the lowest instruction slides
 * out of our window, and the one next to the highest instruction slides into it
 */
static instruction_window_t* slide_window(instruction_window_t* window){
	//It should be fairly easy to slide -- except in the case
	//where we're at the end of a block. In that case, we need to slide to that
	//new block or we'll need an entirely new window if we end in a discrete jump
	//to said block

	
	//If the third operation is not the end, then we're good to just bump everything up
	//This is the simplest case, and allows us to just bump everything up and get out
	if(window->instruction3->next_statement != NULL){
		window->instruction1 = window->instruction1->next_statement;
		window->instruction2 = window->instruction2->next_statement;
		window->instruction3 = window->instruction3->next_statement;
		//We're in the thick of it here
		window->status = WINDOW_AT_MIDDLE;
		
		//Nowhere else to go here
		return window;
	
	//This means that we don't have a full block, and are likely reaching the end
	} else {
		window->instruction1 = window->instruction1->next_statement;
		window->instruction2 = window->instruction2->next_statement;
		window->instruction3 = window->instruction3->next_statement;
		//We're in the thick of it here
		window->status = WINDOW_AT_END;

		return window;
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

	//If the next one is NULL, we have 2 NULL instructions
	//following this. This is very rare, but it could happen
	if(window.instruction1->next_statement == NULL){
		window.instruction2 = NULL;
		window.instruction3 = NULL;
	} else {
		//Otherwise we know we have a second instruction
		window.instruction2 = window.instruction1->next_statement;
		//No such checks are needed for instruction 3, we have no possibility of a null pointer
		//error here
		window.instruction3 = window.instruction2->next_statement;
	}

	//We're at the beginning here by default
	if(window.instruction2 == NULL || window.instruction3 == NULL){
		window.status = WINDOW_AT_END;
	} else {
		window.status = WINDOW_AT_START;
	}
	
	//And now we give back the window
	return window;
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
	 */

	//If we see a constant assingment first and then we see a an assignment
	if(window->instruction1->CLASS == THREE_ADDR_CODE_ASSN_CONST_STMT 
	 	&& window->instruction2->CLASS == THREE_ADDR_CODE_ASSN_STMT){
		
		//If the first assignee is what we're assigning to the next one, we can fold. We only do this when
		//we deal with temp variables. At this point in the program, all non-temp variables have been
		//deemed important, so we wouldn't want to remove their assignments
		if(window->instruction1->assignee->is_temporary == TRUE &&
			variables_equal(window->instruction1->assignee, window->instruction2->op1, FALSE) == TRUE){
			//Grab this out for convenience
			three_addr_code_stmt_t* constant_assingment = window->instruction2;

			//Now we'll modify this to be an assignment const statement
			constant_assingment->op1_const = window->instruction1->op1_const;

			//Modify the type of the assignment
			constant_assingment->CLASS = THREE_ADDR_CODE_ASSN_CONST_STMT;

			//Once we've done this, the first statement is entirely useless
			delete_statement(cfg, window->instruction1->block_contained_in, window->instruction1);

			//Once we've deleted the statement, we'll need to completely rewire the block
			
			//The second instruction is now the first one
			window->instruction1 = constant_assingment;
			//Now instruction 2 becomes instruction 3
			window->instruction2 = window->instruction3;

			//We could have a case where instruction 3 is NULL here, let's account for that
			if(window->instruction2 == NULL || window->instruction2->next_statement == NULL){
				window->instruction3 = NULL;
				//We're at the end if this happens
				window->status = WINDOW_AT_END;
			} else {
				window->instruction3 = window->instruction2->next_statement;
			}

			//Whatever happened here, we did change something
			changed = TRUE;
		}
	}

	//This is the same case as above, we'll just now check instructions 2 and 3
	if(window->instruction2 != NULL && window->instruction2->CLASS == THREE_ADDR_CODE_ASSN_CONST_STMT 
	 	&& window->instruction3 != NULL && window->instruction3->CLASS == THREE_ADDR_CODE_ASSN_STMT){

		//If the first assignee is what we're assigning to the next one, we can fold. We only do this when
		//we deal with temp variables. At this point in the program, all non-temp variables have been
		//deemed important, so we wouldn't want to remove their assignments
		if(window->instruction2->assignee->is_temporary == TRUE &&
			variables_equal(window->instruction2->assignee, window->instruction3->op1, FALSE) == TRUE){
			//Grab this out for convenience
			three_addr_code_stmt_t* constant_assingment = window->instruction3;

			//Now we'll modify this to be an assignment const statement
			constant_assingment->op1_const = window->instruction2->op1_const;

			//Modify the type of the assignment
			constant_assingment->CLASS = THREE_ADDR_CODE_ASSN_CONST_STMT;

			//Once we've done this, the first statement is entirely useless
			delete_statement(cfg, window->instruction2->block_contained_in, window->instruction2);

			//Once we've deleted the statement, we'll need to completely rewire the block
			
			//The second instruction is now the first one
			window->instruction2 = constant_assingment;
			//Now instruction 2 becomes instruction 3
			window->instruction3 = window->instruction3->next_statement;

			//We could have a case where instruction 3 is NULL here, let's account for that
			if(window->instruction3 == NULL){
				//We're at the end if this happens
				window->status = WINDOW_AT_END;
			}

			//Whatever happened here, we did change something
			changed = TRUE;
		}
	}

	/**
	 * --------------------- Folding constant assignments in arithmetic expressions ----------------
	 *  In cases where we have a binary operation that is not a BIN_OP_WITH_CONST, but after simplification
	 *  could be, we want to eliminate unnecessary register pressure by having consts directly in the arithmetic expression 
	 */
	if(window->instruction2 != NULL && window->instruction2->CLASS == THREE_ADDR_CODE_BIN_OP_STMT
		&& window->instruction1->CLASS == THREE_ADDR_CODE_ASSN_CONST_STMT){
		//Is the variable in instruction 1 temporary *and* the same one that we're using in instrution2? Let's check.
		if(window->instruction1->assignee->is_temporary == TRUE &&
			variables_equal(window->instruction1->assignee, window->instruction2->op2, FALSE) == TRUE){
			//If we make it in here, we know that we may have an opportunity to optimize. We simply 
			//Grab this out for convenience
			three_addr_code_stmt_t* const_assingment = window->instruction1;

			//Let's mark that this is now a binary op with const statement
			window->instruction2->CLASS = THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT;

			//We'll want to NULL out the secondary variable in the operation
			window->instruction2->op2 = NULL;
			
			//We'll replace it with the op1 const that we've gotten from the prior instruction
			window->instruction2->op1_const = const_assingment->op1_const;

			//We can now delete the very first statement
			delete_statement(cfg, window->instruction1->block_contained_in, window->instruction1);

			//Following this, we'll shift everything appropriately now that instruction1 is gone
			window->instruction1 = window->instruction2;
			window->instruction2 = window->instruction3;

			//If this is NULL, mark that we're at the end
			if(window->instruction2 == NULL){
				window->instruction3 = NULL;
				window->status = WINDOW_AT_END;
			} else {
				//Otherwise we'll shift this forward
				window->instruction3 = window->instruction2->next_statement;
				//Make sure that we still mark if need be
				if(window->instruction3 == NULL){
					window->status = WINDOW_AT_END;
				}
			}

			//This does count as a change
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
	 */
	if(window->instruction2 != NULL && window->instruction2->CLASS == THREE_ADDR_CODE_LEA_STMT
		&& window->instruction1->CLASS == THREE_ADDR_CODE_ASSN_CONST_STMT){
		//If the first instruction's assignee is temporary and it matches the lea statement, then we have a match
		if(window->instruction1->assignee->is_temporary == TRUE &&
	 		variables_equal(window->instruction1->assignee, window->instruction2->op2, FALSE) == TRUE){
			//Here we have our case for a lea optimization
		}

	}

	/**
	 * ------------------------ Optimizing adjacent statements into LEA statements ----------------
	 *  In cases where we have a multiplication statement next to an addition statement, or vice versa,
	 *  odds are we can write it as a lea statement
	 *
	 */

	//Return whether or not we changed the block
	return changed;
}


/**
 * Make one pass through the sliding window for simplification. This could include folding,
 * etc. Simplification happens first over the entirety of the OIR using the sliding window
 * technique. Following this, the instruction selector runs over the same area
 */
static void simplify(cfg_t* cfg, basic_block_t* head){
	//First we'll grab the head
	basic_block_t* current = head;

	//So long as this isn't NULL
	while(current != NULL){
		//Initialize the sliding window(very basic, more to come)
		instruction_window_t window = initialize_instruction_window(current);

		//Simplify it
		simplify_window(cfg, &window);

		//Did we change the block? If not, we need to slide
		u_int8_t changed;

		//So long as the window status is not end
		while(window.status != WINDOW_AT_END) {
			//Simplify the window
			changed = simplify_window(cfg, &window);

			//And slide it
			if(changed == FALSE){
				slide_window(&window);
			}
		} 

		//Advance to the direct successor
		current = current->direct_successor;
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

	//If the global variables are not null, then these
	//are the global variables
	if(cfg->global_variables != NULL){
		head_block = cfg->global_variables;
		previous = head_block;
	} else {
		previous = head_block = NULL;
	}
	
	//We'll need to use a queue every time, we may as well just have one big one
	heap_queue_t* queue = heap_queue_alloc();

	//For each function
	for(u_int16_t _ = 0; _ < cfg->function_blocks->current_index; _++){
		//Grab the function block out
		basic_block_t* func_block = dynamic_array_get_at(cfg->function_blocks, _);

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
					delete_statement(cfg, previous, previous->exit_statement);
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

	//Give back the head block
	return head_block;
}


/**
 * Print a block our for reading
*/
static void print_ordered_block(basic_block_t* block){
	//If this is some kind of switch block, we first print the jump table
	if(block->block_type == BLOCK_TYPE_SWITCH || block->jump_table.nodes != NULL){
		print_jump_table(&(block->jump_table));
	}

	//If it's a function entry block, we need to print this out
	if(block->block_type == BLOCK_TYPE_FUNC_ENTRY){
		printf("%s:\n", block->func_record->func_name);
	} else {
		printf(".L%d:\n", block->block_id);
	}

	//Now grab a cursor and print out every statement that we 
	//have
	three_addr_code_stmt_t* cursor = block->leader_statement;

	//So long as it isn't null
	while(cursor != NULL){
		//Hand off to printing method
		print_three_addr_code_stmt(cursor);
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
static void print_ordered_blocks(basic_block_t* head_block){
	//Run through the direct successors so long as the block is not null
	basic_block_t* current = head_block;

	//So long as this one isn't NULL
	while(current != NULL){
		//Print it
		print_ordered_block(current);
		//Advance to the direct successor
		current = current->direct_successor;
	}
}


/**
 * Run through and print every instruction in the selector
*/
void print_instructions(dynamic_array_t* instructions){

}


/**
 * A function that selects all instructions, via the peephole method. This kind of 
 * operation completely translates the CFG out of a CFG. When done, we have a straight line
 * of code that we print out
 */
dynamic_array_t* select_all_instructions(cfg_t* cfg){
	//Our very first step in the instruction selector is to order all of the blocks in one 
	//straight line. This step is also able to recognize and exploit some early optimizations,
	//such as when a block ends in a jump to the block right below it
	basic_block_t* head_block = order_blocks(cfg);

	//DEBUG
	//We'll first print before we simplify
	printf("============================== BEFORE SIMPLIFY ========================================\n");
	print_ordered_blocks(head_block);

	printf("============================== AFTER SIMPLIFY ========================================\n");
	//Once we've printed, we now need to simplify the operations. OIR already comes in an expanded
	//format that is used in the optimization phase. Now, we need to take that expanded IR and
	//recognize any redundant operations, dead values, unnecessary loads, etc.
	simplify(cfg, head_block);
	print_ordered_blocks(head_block);

	printf("============================== AFTER INSTRUCTION SELECTION ========================================\n");
	//Once we're done simplifying, we'll use the same sliding window technique to select instructions
	

	//FOR NOW
	return NULL;
}

