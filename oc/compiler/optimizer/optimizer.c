/**
 * Author: Jack Robbins
 *
 * This is the implementation file for the ollie optimizer. Currently
 * it is implemented as one monolothic block
*/

#include "optimizer.h"
#include <sys/types.h>


/**
 * The clean algorithm will remove all useless control flow structures, ideally
 * resulting in a simplified CFG. This should be done after we use mark and sweep
 * to get rid of useless code, because that may lead to empty blocks that we can clean up here
 */
static void clean(cfg_t* cfg){

}


/**
 * The sweep algorithm will go through and remove every operation that has not been marked
 */
static void sweep(cfg_t* cfg){

}


/**
 * The mark algorithm will go through and mark every operation(three address code statement) as
 * critical or noncritical. We will then go back through and see which operations are setting
 * those critical values
 */
static void mark(cfg_t* cfg){
	//First we'll need a worklist
	dynamic_array_t* worklist = dynamic_array_alloc();

	//Now we'll go through every single operation in every single block
	for(u_int16_t _ = 0; _ < cfg->created_blocks->current_index; _++){
		//Grab the block we'll work on
		basic_block_t* current = dynamic_array_get_at(cfg->created_blocks, _);

		//Grab a cursor to the current statement
		three_addr_code_stmt_t* current_stmt = current->leader_statement;

		//Now we'll run through every statement(operation) in this block
		//TODO this is NOT complete
		while(current_stmt != NULL){
			//Clear it's mark
			current_stmt->mark = FALSE;

			//Is it a return stmt? If so, whatever it's returning is useful
			if(current_stmt->CLASS == THREE_ADDR_CODE_RET_STMT){
				//Mark this as useful
				current_stmt->mark = TRUE;
				//Add it to the list
				dynamic_array_add(worklist, current_stmt);
				//The block now has a mark
				current->contains_mark = TRUE;

			//Asm inline statements are always useful
			} else if(current_stmt->CLASS == THREE_ADDR_CODE_ASM_INLINE_STMT){
				current_stmt->mark = TRUE;
				//Add it to the list
				dynamic_array_add(worklist, current_stmt);
				//The block now has a mark
				current->contains_mark = TRUE;

			//Is it a function call? Always useful as well
			} else if(current_stmt->CLASS == THREE_ADDR_CODE_FUNC_CALL){
				current_stmt->mark = TRUE;
				//Add it to the list
				dynamic_array_add(worklist, current_stmt);
				//The block now has a mark
				current->contains_mark = TRUE;

			//Jump statments are also always useful
			} else if(current_stmt->CLASS == THREE_ADDR_CODE_JUMP_STMT){
				current_stmt->mark = TRUE;
				//Add it to the list
				dynamic_array_add(worklist, current_stmt);
				//The block now has a mark
				current->contains_mark = TRUE;
			}

			//Advance the current statement up
			current_stmt = current_stmt->next_statement;
		}
	}

	//Now that we've marked everything that is initially critical, we'll go through and trace
	//these values back through the code
	while(dynamic_array_is_empty(worklist) == FALSE){
		//Grab out the operation from the worklist(delete from back-most efficient)
		three_addr_code_stmt_t* stmt = dynamic_array_delete_from_back(worklist);

		//We need to mark the place where each definition is set
		

	}

	//And get rid of the worklist
	dynamic_array_dealloc(worklist);
}


/**
 * The mark-and-sweep dead code elimintation algorithm. This helper function will invoke
 * both for us in the correct order, all that we need do is call it
 */
static void mark_and_sweep(cfg_t* cfg){
	//First we'll use the mark algorithm to mark all non-essential operations
	mark(cfg);

	//Now once everything has been marked, we'll invoke sweep to get rid of all
	//dead operations
	sweep(cfg);
}


/**
 * The generic optimize function. We have the option to specific how many passes the optimizer
 * runs for in it's current iteration
*/
cfg_t* optimize(cfg_t* cfg, call_graph_node_t* call_graph, u_int8_t num_passes){
	//First thing we'll do is reset the visited status of the CFG. This just ensures
	//that we won't have any issues with the CFG in terms of traversal
	reset_visited_status(cfg);

	//STEP 1: USELESS CODE ELIMINATION

	//We will first use mark and sweep to eliminate any/all dead code that we can find. Code is dead
	//if
	mark_and_sweep(cfg);

	//Now that we're done with that, any/all useless code should be removed
	
	//STEP 2: USELESS CONTROL FLOW ELIMINATION
	
	//Next, we will eliminate any/all useless control flow from the function. This is done by the clean()
	//algorithm
	
	
	return cfg;

}

