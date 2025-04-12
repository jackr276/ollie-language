/**
 * Author: Jack Robbins
 *
 * This is the implementation file for the ollie optimizer. Currently
 * it is implemented as one monolothic block
*/
#include "optimizer.h"
#include "../queue/heap_queue.h"
#include <stdio.h>
#include <sys/types.h>

//Standard true and false definitions
#define TRUE 1
#define FALSE 0


/**
 * Replace all targets that jump to "empty block" with "replacement". This is a helper 
 * function for the "Empty Block Removal" step of clean()
 */
static void replace_all_jump_targets(cfg_t* cfg, basic_block_t* empty_block, basic_block_t* replacement){
	//For everything in the predecessor set of the empty block
	for(u_int16_t _ = 0; _ < empty_block->predecessors->current_index; _++){
		//Grab a given predecessor out
		basic_block_t* predecessor = dynamic_array_get_at(empty_block->predecessors, _);

		//We'll firstly remove the empty block as a successor
		dynamic_array_delete(predecessor->successors, empty_block); 
		//We won't even bother modifying the empty block's predecessors -- it's being deleted anyways

		//Now we'll go through every single statement in this block. If it's a jump statement whose target
		//is the empty block, we'll replace that reference with the replacement
		three_addr_code_stmt_t* current_stmt = predecessor->leader_statement;

		//So long as this isn't null
		while(current_stmt != NULL){
			//If it's a jump statement AND the jump target is the empty block, we're interested
			if(current_stmt->CLASS == THREE_ADDR_CODE_JUMP_STMT && current_stmt->jumping_to_block == empty_block){
				//Update the reference
				current_stmt->jumping_to_block = replacement;
				//Be sure to add the new block as a successor 
				add_successor(predecessor, replacement);
			}

			//Advance it
			current_stmt = current_stmt->next_statement;
		}
	}

	//We also want to remove this block from the predecessors of the replacement
	dynamic_array_delete(replacement->predecessors, empty_block);
	
	//This block is now entirely useless, so we delete it
	dynamic_array_delete(cfg->created_blocks, empty_block);

	//Deallocate this
	//basic_block_dealloc(empty_block);
}


/**
 * The branch reduce function is what we use on each pass of the function
 * postorder
 *
 * Procedure branch_reduce():
 * 	for each block in postorder
 * 		if i ends in a conditional branch
 * 			if both targets are identical then
 * 				replace branch with a jump
 *
 * 		if i ends in a jump to j then
 * 			if i is empty then
 * 				replace transfers to i with transfers to j
 * 			if j has only one predecessor then
 * 				merge i and j
 * 			if j is empty and ends in a conditional branch then
 * 				overwrite i's jump with a copy of j's branch
 */
static u_int8_t branch_reduce(cfg_t* cfg, dynamic_array_t* postorder){
	//Have we seen a change? By default we assume not
	u_int8_t changed = FALSE;

	//Our current block
	basic_block_t* current;

	//For each block in postorder
	for(u_int16_t _ = 0; _ < postorder->current_index; _++){
		//Grab the current block out
		current = dynamic_array_get_at(postorder, _);

		//Does this block end in a conditional branch?	
		if(current->ends_in_conditional_branch == TRUE){
			

		//Do we end in a jump statement?
		} else if(current->exit_statement != NULL && current->exit_statement->CLASS == THREE_ADDR_CODE_JUMP_STMT){
			//=============================== EMPTY BLOCK REMOVAL ============================================
			//If this is the only thing that is in here, then the entire block is redundant and just serves as a branching
			//point. As such, we'll replace branches to i with branches to whatever it jumps to
			if(current->leader_statement == current->exit_statement && current->block_type != BLOCK_TYPE_FUNC_ENTRY){
				//Replace any and all statements that jump to "current" with ones that jump to whichever block it
				//unconditionally jumps to
				replace_all_jump_targets(cfg, current, current->exit_statement->jumping_to_block);

				//This counts as a change
				changed = TRUE;
			}
		}
		//Otherwise we're all set

	}

	//Give back whether or not we changed
	return changed;
}


/**
 * The clean algorithm will remove all useless control flow structures, ideally
 * resulting in a simplified CFG. This should be done after we use mark and sweep to get rid of useless code,
 * because that may lead to empty blocks that we can clean up here
 *
 * Algorithm:
 *
 * Procedure clean():
 * 	while changed
 * 	 compute Postorder of CFG
 * 	 branch_reduce()
 *
 * Procedure branch_reduce():
 * 	for each block in postorder
 * 	
 * 	if i ends in a conditional branch
 * 		if both targets are identical then
 * 			replace branch with a jump
 *
 * 	if i ends in a jump to j then
 * 		if i is empty then
 * 			replace transfers to i with transfers to j
 * 		if j has only one predecessor then
 * 			merge i and j
 * 		if j is empty and ends in a conditional branch then
 * 			overwrite i's jump with a copy of j's branch
 */
static void clean(cfg_t* cfg){
	//For each function in the CFG
	for(u_int16_t _ = 0; _ < cfg->function_blocks->current_index; _++){
		//Have we seen change(modification) at all?
		u_int8_t changed;

		//Grab the function block out
		basic_block_t* function_entry = dynamic_array_get_at(cfg->function_blocks, _);

		//The postorder traversal array
		dynamic_array_t* postorder;

		//Now we'll do the actual clean algorithm
		do {
			//Compute the new postorder
			postorder = compute_post_order_traversal(function_entry);

			//Call onepass() for the reduction
			changed = branch_reduce(cfg, postorder);

			//We can free up the old postorder now
			dynamic_array_dealloc(postorder);
			
		//We keep going so long as branch_reduce changes something 
		} while(changed == TRUE);
	}
}


/**
 * Delete a statement from the CFG - handling any/all edge cases that may arise
 */
static void delete_statement(cfg_t* cfg, basic_block_t* block, three_addr_code_stmt_t* stmt){
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

	//What if it's the exit statement?
	} else if(block->exit_statement == stmt){
		three_addr_code_stmt_t* previous = stmt->previous_statement;
		//Nothing at the end
		previous->next_statement = NULL;

		//This now is the exit statement
		block->exit_statement = previous;
		
	//Otherwise, we have one in the middle
	} else {
		//Regular middle deletion here
		three_addr_code_stmt_t* previous = stmt->previous_statement;
		three_addr_code_stmt_t* next = stmt->next_statement;
		previous->next_statement = next;
		next->previous_statement = previous;
	}
}


/**
 * To find the nearest marked postdominator, we can do a breadth-first
 * search starting at our block B. Whenever we find a node that is both:
 * 	a.) A postdominator of B
 * 	b.) marked
 * we'll have our answer
 */
static basic_block_t* nearest_marked_postdominator(cfg_t* cfg, basic_block_t* B){
	//We'll need a queue for the BFS
	heap_queue_t* queue = heap_queue_alloc();

	//First, we'll reset every single block here
	reset_visited_status(cfg);

	//Seed the search with B
	enqueue(queue, B);

	//The nearest marked postdominator and a holder for our candidates
	basic_block_t* nearest_marked_postdominator = NULL;
	basic_block_t* candidate;

	//So long as the queue is not empty
	while(queue_is_empty(queue) == HEAP_QUEUE_NOT_EMPTY){
		//Grab the block off
		candidate = dequeue(queue);
		
		//If we've been here before, continue;
		if(candidate->visited == TRUE){
			continue;
		}

		//Mark this for later
		candidate->visited = TRUE;

		//Now let's check for our criterion.
		//We want:
		//	it to be in the postdominator set
		//	it to have a mark
		//	it to not equal itself
		if(dynamic_array_contains(B->postdominator_set, candidate) != NOT_FOUND
		  && candidate->contains_mark == TRUE && B != candidate){
			//We've found it, so we're done
			nearest_marked_postdominator = candidate;
			//Get out
			break;
		}

		//Otherwise, we didn't find anything, so we'll keep going
		//Enqueue all of the successors
		for(u_int16_t i = 0; candidate->successors != NULL && i < candidate->successors->current_index; i++){
			//Grab the successor out
			basic_block_t* successor = dynamic_array_get_at(candidate->successors, i);

			//If it's already been visited, we won't bother with it. If it hasn't been visited, we'll add it in
			if(successor->visited == FALSE){
				enqueue(queue, successor);
			}
		}
	}

	//Destroy the queue when done
	heap_queue_dealloc(queue);

	//And give this back
	return nearest_marked_postdominator;
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
			//If it's useful, ignore it
			if(stmt->mark == TRUE){
				stmt = stmt->next_statement;
				continue;
			}

			//Otherwise we know that the statement is unmarked(useless)
			//There are two options when this happens. If it's just a normal statement, we'll just delete it 
			//and that's the end of things. If it's a branch that we've identified as useless, then we'll
			//replace that branch with a jump to it's nearest marked postdominator
			//
			//
			//We've encountered a jump statement of some kind
			if(stmt->is_branch_ending == TRUE){
				//Grab the block out. We need to do this here because we're about to be deleting blocks,
				//and we'll lose the reference if we do
				basic_block_t* block = stmt->block_contained_in;

				//What we'll need to do is delete everythin here that is branch ending
				//and useless
				while(stmt != NULL && stmt->is_branch_ending == TRUE && stmt->mark == FALSE){
					//Delete it
					delete_statement(cfg, block, stmt);
					//Perform the deletion and advancement
					three_addr_code_stmt_t* temp = stmt;
					//Advance it
					stmt = stmt->next_statement;
					//Destroy it
					three_addr_stmt_dealloc(temp);
				}

				//We'll first find the nearest marked postdominator
				basic_block_t* immediate_postdominator = nearest_marked_postdominator(cfg, block);

				//We'll then emit a jump to that node
				three_addr_code_stmt_t* jump_stmt = emit_jmp_stmt_three_addr_code(immediate_postdominator, JUMP_TYPE_JMP);
				//Add this statement in
				add_statement(block, jump_stmt);
				//It is also now a successor
				add_successor(block, immediate_postdominator);

				//And go onto the next iteration
				continue;

			//Otherwise we delete the statement
			} else {
				delete_statement(cfg, stmt->block_contained_in, stmt);
				//Perform the deletion and advancement
				three_addr_code_stmt_t* temp = stmt;
				stmt = stmt->next_statement;
				three_addr_stmt_dealloc(temp);

			}
		}
	}
}


/**
 * Mark definitions(assignment) of a three address variable within
 * the current function. If a definition is not marked, it must be added to the worklist
 */
static void mark_and_add_definition(cfg_t* cfg, three_addr_var_t* variable, symtab_function_record_t* current_function, dynamic_array_t* worklist){
	//If this is NULL, just leave
	if(variable == NULL || current_function == NULL){
		return;
	}

	//Run through everything here
	for(u_int16_t _ = 0; _ < cfg->created_blocks->current_index; _++){
		//Grab the block out
		basic_block_t* block = dynamic_array_get_at(cfg->created_blocks, _);

		//If it's not in the current function and it's temporary, get rid of it
		if(variable->is_temporary == TRUE && block->function_defined_in != current_function){
			continue;
		}

		//If this does assign the variable, we'll look through it
		if(variable->is_temporary == FALSE){
			//Let's find where we assign it
			three_addr_code_stmt_t* stmt = block->leader_statement;

			//So long as this isn't NULL
			while(stmt != NULL){
				//Is the assignee our variable AND it's unmarked?
				if(stmt->mark == FALSE && stmt->assignee != NULL && stmt->assignee->linked_var == variable->linked_var){
					if(strcmp(stmt->assignee->var_name, variable->var_name) == 0){
						//Add this in
						dynamic_array_add(worklist, stmt);
						//Mark it
						stmt->mark = TRUE;
						//And we're done
						return;
					}
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
					return;
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
			} else if(current_stmt->CLASS == THREE_ADDR_CODE_DIR_JUMP_STMT){
				current_stmt->mark = TRUE;
				//Add it to the list
				dynamic_array_add(worklist, current_stmt);
				//The block now has a mark
				current->contains_mark = TRUE;
			} else if(current_stmt->CLASS == THREE_ADDR_CODE_IDLE_STMT){
				current_stmt->mark = TRUE;
				//Add it to the list
				dynamic_array_add(worklist, current_stmt);
				//The block now has a mark
				current->contains_mark = TRUE;
			//We need to check - are we manipulating any global variables here? If we
			//are, those are also considered important
			} else if(current_stmt->assignee != NULL && current_stmt->assignee->is_temporary == FALSE){
				//If we have an assignee and that assignee is a global variable, then this is marked as
				//important
				if(current_stmt->assignee->linked_var->is_global == TRUE){
					current_stmt->mark = TRUE;
					//Add it to the list
					dynamic_array_add(worklist, current_stmt);
					//The block now has a mark
					current->contains_mark = TRUE;
				//If we have a pointer type and are assigning to a derefence of a function parameter(inout mode), we are modifying the value of that pointer
				} else if(current_stmt->assignee->linked_var->is_function_paramater == TRUE 
						&& current_stmt->assignee->type->type_class == TYPE_CLASS_POINTER 
						&& current_stmt->assignee->indirection_level > 0){
					//Mark it
					current_stmt->mark = TRUE;
					//Add it to the list
					dynamic_array_add(worklist, current_stmt);
					//The block now has a mark
					current->contains_mark = TRUE;
				}
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

		//If it's a phi function, now we need to go back and mark everything that it came from
		if(stmt->CLASS == THREE_ADDR_CODE_PHI_FUNC){
			dynamic_array_t* phi_function_parameters = stmt->phi_function_parameters;
			//Add this in here
			for(u_int16_t i = 0; phi_function_parameters != NULL && i < phi_function_parameters->current_index; i++){
				//Grab the param out
				three_addr_var_t* phi_func_param = dynamic_array_get_at(phi_function_parameters, i);

				//Add the definitions in
				mark_and_add_definition(cfg, phi_func_param, stmt->function, worklist);
			}

		//Otherwise if we have a function call, every single thing in that function call is important
		} else if(stmt->CLASS == THREE_ADDR_CODE_FUNC_CALL){
			//Grab the parameters out
			dynamic_array_t* params = stmt->function_parameters;

			//Run through them all and mark them
			for(u_int16_t i = 0; params != NULL && i < params->current_index; i++){
				mark_and_add_definition(cfg, dynamic_array_get_at(params, i), stmt->function, worklist);
			}

		} else {
			//We need to mark the place where each definition is set
			mark_and_add_definition(cfg, stmt->op1, stmt->function, worklist);
			mark_and_add_definition(cfg, stmt->op2, stmt->function, worklist);
		}

		//Grab this out for convenience
		basic_block_t* block = stmt->block_contained_in;

		//Now for everything in this statement's block's RDF, we'll mark it's block-ending branches
		//as useful
		for(u_int16_t i = 0; block->reverse_dominance_frontier != NULL && i < block->reverse_dominance_frontier->current_index; i++){
			//Grab the RDF block out
			basic_block_t* rdf_block = dynamic_array_get_at(block->reverse_dominance_frontier, i);

			//Now we'll go through this block and mark all of the operations as needed
			three_addr_code_stmt_t* rdf_block_stmt = rdf_block->leader_statement;

			//Run through and mark each statement in the RDF that is flagged as "branch ending". As in, it's 
			//important to our operations as a whole to get to this important instruction where we currently are
			while(rdf_block_stmt != NULL){
				//For each statement that has NOT been marked but IS loop ending, we'll flag it
				if(rdf_block_stmt->mark == FALSE && rdf_block_stmt->is_branch_ending == TRUE){
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
 * The generic optimize function. We have the option to specific how many passes the optimizer
 * runs for in it's current iteration
*/
cfg_t* optimize(cfg_t* cfg, call_graph_node_t* call_graph, u_int8_t num_passes){
	//First thing we'll do is reset the visited status of the CFG. This just ensures
	//that we won't have any issues with the CFG in terms of traversal
	reset_visited_status(cfg);

	//PASS 1: Mark algorithm
	//The mark algorithm marks all useful operations. It will perform one full pass of the program
	mark(cfg);

	//PASS 2: Sweep algorithm
	//Sweep follows directly after mark because it eliminates anything that is unmarked. If sweep
	//comes across branch ending statements that are unmarked, it will replace them with a jump to the
	//nearest marked postdominator
	sweep(cfg);
	
	//PASS 3: Clean algorithm
	//Clean follows after sweep because during the sweep process, we will likely delete the contents of
	//entire block. Clean uses 4 different steps in a specific order to eliminate control flow
	//that has been made useless by sweep()
	clean(cfg);

	//PASS 4: Recalculate everything
	//Now that we've marked, sweeped and cleaned, odds are that all of our control relations will be off due to deletions of blocks, statements,
	//etc. So, to remedy this, we will recalculate everything in the CFG
	//cleanup_all_control_relations(cfg);
	//calculate_all_control_relations(cfg, TRUE);
	
	return cfg;
}

