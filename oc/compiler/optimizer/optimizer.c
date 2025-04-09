/**
 * Author: Jack Robbins
 *
 * This is the implementation file for the ollie optimizer. Currently
 * it is implemented as one monolothic block
*/

#include "optimizer.h"
#include <stdio.h>
#include <sys/types.h>

//Standard true and false definitions
#define TRUE 1
#define FALSE 0


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
	//For each and every operation in every basic block
	for(u_int16_t _ = 0; _ < cfg->created_blocks->current_index; _++){
		//Grab the block out
		basic_block_t* block = dynamic_array_get_at(cfg->created_blocks, _);

		//If it's the global var block don't bother with it
		if(block->is_global_var_block == TRUE){
			continue;
		}

		//Grab the statement out
		three_addr_code_stmt_t* stmt = block->leader_statement;

		//For each statement in the block
		while(stmt != NULL){
			//If the statement is unmarked(useless)
			if(stmt->mark == FALSE){
				//It's a conditional jump
				if(stmt->CLASS == THREE_ADDR_CODE_JUMP_STMT && stmt->op != JUMP){
					//TODO add me in

				//Otherwise we delete the statement
				} else {
					//If it's the leader statement, we'll just update the references
					if(block->leader_statement == stmt){
						//Special case - it's the only statement. We'll just delete it here
						if(block->leader_statement->next_statement == NULL){
							//Just remove it entirely
							block->leader_statement = NULL;
							block->exit_statement = NULL;
						//Otherwise it is the leader, but we have more
						} else {
							//Update the reference
							block->leader_statement = stmt->next_statement;
							//Set this to NULL
							block->leader_statement->previous_statement = NULL;
						}
					//Otherwise, we have one in the middle
					} else {
						//Set the previous one's old one to be the next one
						stmt->previous_statement->next_statement = stmt->next_statement;
						//Update the old reference too
						stmt->next_statement->previous_statement = stmt->previous_statement;
					}

					//Store this
					three_addr_code_stmt_t* temp = stmt->next_statement;
					//We always destroy it at the very end
					three_addr_stmt_dealloc(stmt);
					//Reassign
					stmt = temp;
				}

			} else {
				//Advance this up
				stmt = stmt->next_statement;
			}
		}
	}
}


/**
 * Does the block assign this variable? We'll do a simple linear scan to find out
 */
static u_int8_t does_block_assign_variable(basic_block_t* block, symtab_variable_record_t* variable){
	//Sanity check - if this is NULL then it's false by default
	if(block->assigned_variables == NULL){
		return FALSE;
	}

	//We'll need to use a special comparison here because we're comparing variables, not plain addresses.
	//We will compare the linked var of each block in the assigned variables dynamic array
	for(u_int16_t i = 0; i < block->assigned_variables->current_index; i++){
		//Grab the variable out
		three_addr_var_t* var = dynamic_array_get_at(block->assigned_variables, i);
		
		//Now we'll compare the linked variable to the record
		if(var->linked_var == variable){
			//If we found it bail out
			return TRUE;
		}
	}

	//If we make it all of the way down here and we didn't find it, fail out
	return FALSE;
}


/**
 * Mark definitions(assignment) of a three address variable within
 * the current function. If a definition is not marked, it must be added to the worklist
 */
static void mark_and_add_definitions(cfg_t* cfg, three_addr_var_t* variable, symtab_function_record_t* current_function, dynamic_array_t* worklist){
	//If this is NULL, just leave
	if(variable == NULL || current_function == NULL){
		return;
	}

	//Run through everything here
	for(u_int16_t _ = 0; _ < cfg->created_blocks->current_index; _++){
		//Grab the block out
		basic_block_t* block = dynamic_array_get_at(cfg->created_blocks, _);

		//If it's not in the current function, get rid of it
		if(block->function_defined_in != current_function){
			continue;
		}

		//If this does assign the variable, we'll look through it
		if(variable->is_temporary == FALSE){
			//Let's find where we assign it
			three_addr_code_stmt_t* stmt = block->leader_statement;

			//So long as this isn't NULL
			while(stmt != NULL){
				//Is the assignee our variable AND it's unmarked?
				if(stmt->assignee != NULL && stmt->assignee->linked_var == variable->linked_var && stmt->mark == FALSE){
					//Add this in
					dynamic_array_add(worklist, stmt);
					//Mark it
					stmt->mark = TRUE;
				}

				//Advance the statement
				stmt = stmt->next_statement;
			}

		//If it's a temp var, the search is not so easy. We'll need to crawl through
		//each statement and see if the assignee has the same temp number
		} else {
			//Let's find where we assign it
			three_addr_code_stmt_t* stmt = block->leader_statement;

			//So long as this isn't NULL
			while(stmt != NULL){
				//Is the assignee our variable AND it's unmarked?
				if(stmt->assignee != NULL && stmt->assignee->temp_var_number == variable->temp_var_number && stmt->mark == FALSE){
					//Add this in
					dynamic_array_add(worklist, stmt);
					//Mark it
					stmt->mark = TRUE;
				}

				//Advance the statement
				stmt = stmt->next_statement;
			}
		}
	}
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
			} else if(current_stmt->CLASS == THREE_ADDR_CODE_DIR_JUMP_STMT){
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
		mark_and_add_definitions(cfg, stmt->assignee, stmt->function, worklist);
		mark_and_add_definitions(cfg, stmt->op1, stmt->function, worklist);
		mark_and_add_definitions(cfg, stmt->op2, stmt->function, worklist);

		//Grab this out for convenience
		basic_block_t* block = stmt->block_contained_in;

		//Now for everything in this statement's block's RDF, we'll mark it's block-ending branches
		//as useful
		for(u_int16_t i = 0; block->reverse_dominance_frontier != NULL && i < block->reverse_dominance_frontier->current_index; i++){
			//Grab the RDF block out
			basic_block_t* rdf_block = dynamic_array_get_at(block->reverse_dominance_frontier, i);

			//Now we'll go through this block and mark all of the operations as needed
			//TODO THIS IS NOT CORRECT FULLY
			three_addr_code_stmt_t* rdf_block_stmt = rdf_block->leader_statement;

			//Run through and mark each statement TODO NOT RIGHT
			while(rdf_block_stmt != NULL){
				if(rdf_block_stmt->mark == FALSE){
					rdf_block_stmt->mark = TRUE;
					//Add into the worklist
					dynamic_array_add(worklist, rdf_block_stmt);
					//Mark the block
					rdf_block->contains_mark = TRUE;
				}

				//Advance it
				rdf_block_stmt = rdf_block_stmt->next_statement;
			}
		}
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
	//sweep(cfg);
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

