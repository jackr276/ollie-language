/**
 * Author: Jack Robbins
 *
 * This is the implementation file for the ollie optimizer. Currently
 * it is implemented as one monolothic block
*/
#include "optimizer.h"
#include "../queue/heap_queue.h"
#include <stdio.h>
#include <sys/select.h>
#include <sys/types.h>

//Standard true and false definitions
#define TRUE 1
#define FALSE 0


/**
 * Combine two blocks into one
 */
static void combine(cfg_t* cfg, basic_block_t* a, basic_block_t* b){
	//If b is null, we just return a. This in reality should never happen
	if(b == NULL){
		return;
	}

	//What if a was never even assigned?
	if(a->exit_statement == NULL){
		a->leader_statement = b->leader_statement;
		a->exit_statement = b->exit_statement;

	//If the leader statement is NULL - we really don't need to do anything. If it's not however, we
	//will need to add everything in
	} else if(b->leader_statement != NULL){
		//Otherwise it's a "true merge"
		//The leader statement in b will be connected to a's tail
		a->exit_statement->next_statement = b->leader_statement;
		//Connect backwards too
		b->leader_statement->previous_statement = a->exit_statement;
		//Now once they're connected we'll set a's exit to be b's exit
		a->exit_statement = b->exit_statement;
	}

	//In our case for "combine" - we know for a fact that "b" only had one predecessor - which is "a"
	//As such, we won't even bother looking at the predecessors

	//Now merge successors
	for(u_int16_t i = 0; b->successors != NULL && i < b->successors->current_index; i++){
		//Add b's successors to be a's successors
		add_successor_only(a, dynamic_array_get_at(b->successors, i));
	}

	//FOR EACH Successor of B, it will have a reference to B as a predecessor.
	//This is now wrong though. So, for each successor of B, it will need
	//to have A as predecessor
	for(u_int8_t i = 0; b->successors != NULL && i < b->successors->current_index; i++){
		//Grab the block first
		basic_block_t* successor_block = b->successors->internal_array[i];

		//Now for each of the predecessors that equals b, it needs to now point to A
		for(u_int8_t i = 0; successor_block->predecessors != NULL && i < successor_block->predecessors->current_index; i++){
			//If it's pointing to b, it needs to be updated
			if(successor_block->predecessors->internal_array[i] == b){
				//Update it to now be correct
				successor_block->predecessors->internal_array[i] = a;
			}
		}
	}

	//Also make note of any direct succession
	a->direct_successor = b->direct_successor;

	//Make a note of this too
	//a->ends_in_conditional_branch = b->ends_in_conditional_branch;

	//Copy over the block type and terminal type
	if(a->block_type != BLOCK_TYPE_FUNC_ENTRY){
		a->block_type = b->block_type;
	}

	//Copy this over too
	a->block_terminal_type = b->block_terminal_type;

	//For each statement in b, all of it's old statements are now "defined" in a
	three_addr_code_stmt_t* b_stmt = b->leader_statement;

	//Modify these "block contained in" references to be A
	while(b_stmt != NULL){
		b_stmt->block_contained_in = a;

		//Push it up
		b_stmt = b_stmt->next_statement;
	}
	
	//We'll remove this from the list of created blocks
	dynamic_array_delete(cfg->created_blocks, b);

	//And finally we'll deallocate b
	//TODO FIX - DOES NOT WORK
	//basic_block_dealloc(b);
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

	//If this was a jump statement, update the number of jump statements
	if(stmt->CLASS == THREE_ADDR_CODE_JUMP_STMT){
		//Decrement
		block->num_jumps -= 1;
	}
}


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
	//TODO FIX - DOES NOT WORK
	//basic_block_dealloc(empty_block);
}


/**
 * Delete all branching statements in the current block. We know if a statement is branching if it is 
 * markes as branch ending. 
 *
 * NOTE: This should only be called after we have identified this block as a candidate for block folding
 */
static void delete_all_branching_statements(cfg_t* cfg, basic_block_t* block){
	//We'll always start from the end and work our way up
	three_addr_code_stmt_t* current = block->exit_statement;
	//To hold while we delete
	three_addr_code_stmt_t* temp;

	//So long as this is NULL and it's branch ending
	while(current != NULL && current->is_branch_ending == TRUE){
		temp = current;
		//Advance this
		current = current->previous_statement;
		//Then we delete
		delete_statement(cfg, block, temp);
	}

	//After we've gotten here we're all done
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

		//Do we end in a jump statement? - this is the precursor to all optimizations in branch reduce
		if(current->exit_statement != NULL && current->exit_statement->CLASS == THREE_ADDR_CODE_JUMP_STMT){
			//Now let's do a touch more work to see if we end in a conditional branch. We end in a conditional
			//branch if the last two statements are jump statements. We already know that the last one 
			//is, now we just need to check if the other one is
			u_int8_t ends_in_branch = FALSE;

			//If the prior statement is not NULL and it's a jump, we end in a conditional
			if(current->exit_statement->previous_statement != NULL && current->exit_statement->previous_statement->CLASS == THREE_ADDR_CODE_JUMP_STMT
			  && current->exit_statement->previous_statement->op != JUMP){
				ends_in_branch = TRUE;
			}

			//============================== REDUNDANT CONDITIONAL REMOVAL(FOLD) =================================
			// If we have a block that ends in a conditional branch where all targets are the exact same, then
			// the conditional branch is useless. We can replace the entire conditional with what's called a fold
			if(ends_in_branch == TRUE){
				//So we do end in a conditional branch. Now the question is if we have the same jump target for all of our conditional
				//jumps. Initially, we're going to assume that we don't
				u_int8_t redundant_branch = FALSE;
				
				//What are we jumping to?
				basic_block_t* end_branch_target = NULL;
				
				//Grab a statement cursor. This time, since we care about what we end with, we'll crawl from the bottom up
				three_addr_code_stmt_t* stmt = current->exit_statement;

				//So long as we are still branch ending and seeing statements
				while(stmt != NULL && stmt->is_branch_ending == TRUE){
					//If it isn't a jump statement, just move along
					if(stmt->CLASS != THREE_ADDR_CODE_JUMP_STMT){
						stmt = stmt->previous_statement;
						continue;
					}

					//Otherwise it is a jump statement
					//If we haven't seen one yet, then we'll mark it here
					if(end_branch_target == NULL){
						end_branch_target = stmt->jumping_to_block;
					//Otherwise we have seen one. If these do not match, then we're done
					//here
					} else if(end_branch_target != stmt->jumping_to_block) {
						//If this is the case, we're done. Set the flag to false and get
						//out
						redundant_branch = FALSE;
						break;
					//If we get all of the way down here, the two matched
					} else {
						//As of right now, we've seen a redundant branch
						redundant_branch = TRUE;
					}

					//If we made it here, go to the prior statement
					stmt = stmt->previous_statement;
				}

				//If this is true all of the way down here, we've found one
				if(redundant_branch == TRUE){
					//We'll need to eliminate all of the branch ending statements
					delete_all_branching_statements(cfg, current);
					//Once we're done deleting, we'll emit a jump right to that jumping to
					//block. This constitutes our "fold"
					emit_jmp_stmt(current, end_branch_target, JUMP_TYPE_JMP, TRUE);
					//This does mean that we've changed
					changed = TRUE;
				}
				//And now we're set, onto the next optimization
			}

			//The block that we're jumping to
			basic_block_t* jumping_to_block = current->exit_statement->jumping_to_block;
			
			//=============================== EMPTY BLOCK REMOVAL ============================================
			//If this is the only thing that is in here, then the entire block is redundant and just serves as a branching
			//point. As such, we'll replace branches to i with branches to whatever it jumps to
			if(ends_in_branch == FALSE && current->leader_statement == current->exit_statement && current->block_type != BLOCK_TYPE_FUNC_ENTRY){
				//Replace any and all statements that jump to "current" with ones that jump to whichever block it
				//unconditionally jumps to
				replace_all_jump_targets(cfg, current, jumping_to_block);

				//This counts as a change
				changed = TRUE;
			}

			//============================== BLOCK MERGING =================================================
			//This is another special case -- if the block we're jumping to only has one predecessor, then
			//we may as well avoid the jump and just merge the two
			if(jumping_to_block->predecessors->current_index == 1){
				//We need to check here -- is there only ONE jump to the jumping to block inside of this
				//block? If there is only one, then we are all set to merge

				//Grab a statement cursor
				three_addr_code_stmt_t* cursor = current->exit_statement->previous_statement;

				//Are we good to go?
				u_int8_t good_to_merge = TRUE;

				while(cursor != NULL){
					//If we have another jump, we are NOT good to merge
					if(cursor->CLASS == THREE_ADDR_CODE_JUMP_STMT && cursor->jumping_to_block == jumping_to_block){
						good_to_merge = FALSE;
						break;
					}

					//Another option here - if this is short circuit eligible, then merging like this would ruin the
					//detection of short circuiting. So if we see this, we also will NOT merge
					if(cursor->is_short_circuit_eligible == TRUE){
						good_to_merge = FALSE;
						break;
					}

					//Otherwise we keep going up
					cursor = cursor->previous_statement;
				}

				if(good_to_merge == TRUE){
					//We will combine(merge) the current block and the one that it's jumping to
					//Remove the statement that jumps to the one we're about to merge
					delete_statement(cfg, current, current->exit_statement); 

					//By that same token, we no longer was current to have the jumping to block as a successor
					dynamic_array_delete(current->successors, jumping_to_block);

					//Now we'll actually merge the blocks
					combine(cfg, current, jumping_to_block);
				
					//This will count as a change
					changed = TRUE;

					//This is an endgame optimization. Once we've done this, there no longer is a branch for branch
					//hoisting to look at. As such, if this happens, we'll continue to the next iteration
					continue;
				}
			}

			//=============================== BRANCH HOISTING ==================================================
			// The final special case - if we discover a that the block we're jumping to is empty and ends entirely
			// in a conditional branch, then we can copy all of that conditional branch code into the branch
			// that we're coming from
			//
			// If the leader is branch ending AND the block ends in a conditional, this means that the block itself is entirely
			// conditional
			// If the very first statement is branch ending and NOT a direct jump? If it is, we have a candidate for hoisting
			if(ends_in_branch == FALSE && jumping_to_block->leader_statement != NULL && jumping_to_block->leader_statement->is_branch_ending == TRUE
				&& jumping_to_block->leader_statement->CLASS != THREE_ADDR_CODE_JUMP_STMT){
	 			//Let's check now and see if it's truly a conditional branch only
				
				//If it's not a jump, we're out here
	 			if(jumping_to_block->exit_statement == NULL || jumping_to_block->exit_statement->CLASS != THREE_ADDR_CODE_JUMP_STMT){
	 				continue;
	 			}

				//Final check: If the statement right before the exit also isn't a jump, we don't have a branch
				if(jumping_to_block->exit_statement->previous_statement == NULL || jumping_to_block->exit_statement->previous_statement->CLASS != THREE_ADDR_CODE_JUMP_STMT){
					continue;
				}

				//If we made it all the way down here, we know that we have:
				//	1.) A block whose leader statement is branch ending
				//	2.) A block whose end statements are jumps
				//	3.) This leads us to believe we can "hoist" the block
				
				//Set this flag, we have changed here
				changed = TRUE;
	 
				//We want to remove the current block as a predecessor of this block
				dynamic_array_delete(jumping_to_block->predecessors, current);

				//Also, we'll want to remove this block as a successor of the current block
				dynamic_array_delete(current->successors, jumping_to_block);

				//We'll delete the statement that jumps from current to the jumping_to_block(this is the exit statement, remember from above)
				delete_statement(cfg, current, current->exit_statement);

				//If we make it here we know that we have some kind of conditional branching logic here in the jumping to block, we'll need to create
				//a complete copy of it. This new copy will then be added into the current block in lieu of the jump statement that we just deleted
	
				//The leader of this new string of statement 
				three_addr_code_stmt_t* head = NULL;
				//The one that we're currently operating on
				three_addr_code_stmt_t* tail = NULL;

				//We'll use this cursor to run through the entirety of the jumping to block's code
				three_addr_code_stmt_t* cursor = jumping_to_block->leader_statement;
				
				//So long as we have stuff to add
				while(cursor != NULL){
					//Get a complete copy of the statement
					three_addr_code_stmt_t* new = copy_three_addr_code_stmt(cursor);

					//If we're adding the very first one
					if(head == NULL){
						head = new;
						tail = new;
					//Otherwise add to the end
					} else {
						//Add it in
						tail->next_statement = new;
						new->previous_statement = tail;
						tail = new;
					}

					//One last thing -- if this is a jump statement, we'll need to update the predecessor and successor
					//lists accordingly
					if(cursor->CLASS == THREE_ADDR_CODE_JUMP_STMT){
						//Whatever we're jumping to is now a successor of cursor
						add_successor(current, cursor->jumping_to_block);
					}

					//Advance the cursor up
					cursor = cursor->next_statement;
				}

				//When we get to the very end, we now need to add everything into the current block's statement set
				current->exit_statement->next_statement = head;
				head->previous_statement = current->exit_statement;
				//Reassign exit
				current->exit_statement = tail;
				//And we're done
			}
		}
	}

	//Give back whether or not we changed
	return changed;
}


/**
 * Handle a compound and statement optimization
 */
static void optimize_compound_and_jump(cfg_t* cfg, basic_block_t* block, three_addr_code_stmt_t* stmt, basic_block_t* if_target, basic_block_t* else_target){
	//Starting off-we're given the and stmt as a parameter, and our two jumps
	//Let's look and see where the two variables that make up the and statement are defined. We know for a fact
	//that op1 will always come before op2. As such, we will look for where op1 is last assigned
	three_addr_var_t* op1 = stmt->op1;
	//Grab a statement cursor
	three_addr_code_stmt_t* cursor = stmt;

	//Run backwards until we find where op1 is the assignee
	while(cursor != NULL && variables_equal(op1, cursor->assignee) == FALSE){
		//Keep advancing backward
		cursor = cursor->previous_statement;
	}
	
	//Once we get out here, we have the statement that assigns op1. Since this is an "and" target,
	//we'll jump to ELSE if we have a bad result here(result being zero) because that would cause
	//the rest of the and to be false
	//Jump to else here
	three_addr_code_stmt_t* jump_to_else_stmt = emit_jmp_stmt_three_addr_code(else_target, JUMP_TYPE_JZ);
	//Make sure to mark that this is branch ending
	jump_to_else_stmt->is_branch_ending = TRUE;

	//We'll now need to insert this statement right after where op1 is assigned at cursor
	three_addr_code_stmt_t* after = cursor->next_statement;

	//The jump statement is now in between the two
	cursor->next_statement = jump_to_else_stmt;
	jump_to_else_stmt->previous_statement = cursor;

	//And we'll also update the references for after
	jump_to_else_stmt->next_statement = after;
	after->previous_statement = jump_to_else_stmt;

	//And even better, we now don't need the compound and at all. We can delete the whole stmt
	delete_statement(cfg, block, stmt);
}



/**
 * Hande a compound or statement optimization
 */
static void optimize_compound_or_jump(cfg_t* cfg, basic_block_t* block, three_addr_code_stmt_t* stmt, basic_block_t* if_target, basic_block_t* else_target){
	//Starting off-we're given the and stmt as a parameter, and our two jumps
	//Let's look and see where the two variables that make up the and statement are defined. We know for a fact
	//that op1 will always come before op2. As such, we will look for where op1 is last assigned
	three_addr_var_t* op1 = stmt->op1;
	//Grab a statement cursor
	three_addr_code_stmt_t* cursor = stmt;

	//Run backwards until we find where op1 is the assignee
	while(cursor != NULL && variables_equal(op1, cursor->assignee) == FALSE){
		//Keep advancing backward
		cursor = cursor->previous_statement;
	}
	
	//Once we get out here, we have the statement that assigns op1. Since this is an "or" target,
	//we'll jump to IF we have a good result here(result being not zero) because that would cause
	//rest of the or to be true
	//Jump to else here
	three_addr_code_stmt_t* jump_to_if_stmt = emit_jmp_stmt_three_addr_code(if_target, JUMP_TYPE_JNZ);
	//Make sure to mark that this is branch ending
	jump_to_if_stmt->is_branch_ending = TRUE;

	//We'll now need to insert this statement right after where op1 is assigned at cursor
	three_addr_code_stmt_t* after = cursor->next_statement;

	//The jump statement is now in between the two
	cursor->next_statement = jump_to_if_stmt;
	jump_to_if_stmt->previous_statement = cursor;

	//And we'll also update the references for after
	jump_to_if_stmt->next_statement = after;
	after->previous_statement = jump_to_if_stmt;

	//And even better, we now don't need the compound and at all. We can delete the whole stmt
	delete_statement(cfg, block, stmt);
}


/**
 * The compound logic optimizer will go through and look for compound and or or statements
 * that are parts of branch endings and see if they're able to be short-circuited. These
 * statements have been pre-marked by the cfg constructor, so whichever survive until here are going to 
 * be optimized
 *
 *
 * Here is a brief example:
 * t9 <- 0x2
 * t10 <- x_0 < t9
 * t11 <- 0x1
 * t12 <- x_0 != t11
 * t13 <- t10 && t12 <-------- COMPOUND JUMP
 * jnz .L8
 * jmp .L9
 *
 * .L8():
 * t14 <- 0x2
 * t15 <- x_0 * t14
 * x_2 <- t15
 * jmp .L5
 *
 * .L9():
 * t16 <- 0x3
 * t17 <- x_0 + t16
 * x_1 <- t17
 * jmp .L5	
 *
 * We could optimize this statement by realizing that if the first condition fails(t10), there is no chance
 * for the next one to succeed, and as such we can jump immediately after t10 is defined
 *
 * TURNS INTO THIS:
 *  
 * t9 <- 0x2
 * t10 <- x_0 < t9
 * jz .L9 <--------------------- Optimized jump-to-else
 * t11 <- 0x1
 * t12 <- x_0 != t11
 * jnz .L8 <-------------------- Optimized jump-to-if
 * --------------------------------t13 <- t10 && t12 <-------------------- No longer a need for this one
 * jnz .L8
 * jmp .L9
 *
 * .L8():
 * t14 <- 0x2
 * t15 <- x_0 * t14
 * x_2 <- t15
 * jmp .L5
 *
 * .L9():
 * t16 <- 0x3
 * t17 <- x_0 + t16
 * x_1 <- t17
 * jmp .L5	
 */
static void optimize_compound_logic(cfg_t* cfg){
	//For every single block in the CFG
	for(u_int16_t _ = 0; _ < cfg->created_blocks->current_index; _++){
		//Grab the block out
		basic_block_t* block = dynamic_array_get_at(cfg->created_blocks, _);

		//If this is the global var block or it has no statements, bail out
		if(block->is_global_var_block == TRUE || block->leader_statement == NULL){
			continue;
		}

		//We always have two targets: the if(conditional) target and the else(nonconditional) target
		basic_block_t* if_target;
		basic_block_t* else_target;


		//If we don't end in two jumps, this isn't going to work. The exit must be a direct jump 
		if(block->exit_statement->CLASS != THREE_ADDR_CODE_JUMP_STMT || block->exit_statement->jump_type != JUMP_TYPE_JMP){
			continue;
		}

		//We made it here, so we know that this is the else target
		else_target = block->exit_statement->jumping_to_block;

		//Now we need to check for the if target. If it's null, not a jump statement, or a direct jump, we're out of here
		if(block->exit_statement->previous_statement == NULL || block->exit_statement->previous_statement->CLASS != THREE_ADDR_CODE_JUMP_STMT
			|| block->exit_statement->previous_statement->jump_type == JUMP_TYPE_JMP){
			continue;
		}

		//This will be our if target
		if_target = block->exit_statement->previous_statement->jumping_to_block;

		//Grab a statement cursor
		three_addr_code_stmt_t* cursor = block->exit_statement;

		//Store all of our eligible statements in this block. This will be done in a FIFO
		//fashion
		dynamic_array_t* eligible_statements = NULL;

		//Let's run through and see if we can find a statement that's eligible for short circuiting.
		while(cursor != NULL){
			//If we make it here, then we've found something that is eligible for a compound logic optimization
			if(cursor->is_short_circuit_eligible == TRUE && cursor->is_branch_ending == TRUE){
				//For now we'll add this to the list of potential ones
				if(eligible_statements == NULL){
					//If we didn't have one of these, make it now
					eligible_statements = dynamic_array_alloc();
				}

				//Add the cursor. We will iterate over these statements in the order we found them,
				//so going through in
				dynamic_array_add(eligible_statements, cursor);
			}

			//move it back
			cursor = cursor->previous_statement;
		}

		//Now we'll iterate over the array and process what we have
		for(u_int16_t i = 0; eligible_statements != NULL && i < eligible_statements->current_index; i++){
			//Grab the block out
			three_addr_code_stmt_t* stmt = dynamic_array_get_at(eligible_statements, i);

			//Make the helper call. These are treated differently based on what their
			//operators are, so we'll need to use the appropriate call
			if(stmt->op == DOUBLE_AND){
				//Invoke the and helper
				optimize_compound_and_jump(cfg, block, stmt, if_target, else_target);
			} else {
				//Invoke the or helper
				optimize_compound_or_jump(cfg, block, stmt, if_target, else_target);
			}
		}

		//If we had anything, deallocate it
		if(eligible_statements != NULL){
			dynamic_array_dealloc(eligible_statements);
		}
	}
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
					//Perform the deletion and advancement
					three_addr_code_stmt_t* temp = stmt;
					//Advance it
					stmt = stmt->next_statement;
					//Delete it
					delete_statement(cfg, block, temp);
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
				printf("DELETING: ");
				print_three_addr_code_stmt(stmt);
				printf(" because it is not marked\n");
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
						//Mark it
						block->contains_mark = TRUE;
						//Mark the block it's in
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
					//Mark the block
					block->contains_mark = TRUE;
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
			} else if(current_stmt->CLASS == THREE_ADDR_CODE_LABEL_STMT){
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
			} else if(current_stmt->CLASS == THREE_ADDR_CODE_JUMP_STMT && current->block_type == BLOCK_TYPE_FOR_STMT_UPDATE){
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

			/**
			 * We will look for the trinity of a branch. That is, two jumps preceeded by some other statement. These three 
			 * things in tandem make up a branch. If we find those, we mark them all
			 */

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
 * After mark and sweep and clean run, we'll almost certainly have a litany of blocks in all
 * of the dominance relations that are now useless. As such, we'll need to completely recompute all
 * of these key values
 */
static void recompute_all_dominance_relations(cfg_t* cfg){
	//First, we'll go through and completely blow away anything related to
	//a dominator in the entirety of the cfg
	for(u_int16_t _ = 0; _ < cfg->created_blocks->current_index; _++){
		//Grab the given block out
		basic_block_t* block = dynamic_array_get_at(cfg->created_blocks, _);

		//Now we're going to reset everything about this block
		block->immediate_dominator = NULL;
		block->immediate_postdominator = NULL;

		if(block->dominator_set != NULL){
			dynamic_array_dealloc(block->dominator_set);
			block->dominator_set = NULL;
		}

		if(block->postdominator_set != NULL){
			dynamic_array_dealloc(block->postdominator_set);
			block->postdominator_set = NULL;
		}

		if(block->dominance_frontier != NULL){
			dynamic_array_dealloc(block->dominance_frontier);
			block->dominance_frontier = NULL;
		}

		if(block->dominator_children != NULL){
			dynamic_array_dealloc(block->dominator_children);
			block->dominator_children = NULL;
		}

		if(block->reverse_dominance_frontier != NULL){
			dynamic_array_dealloc(block->reverse_dominance_frontier);
			block->reverse_dominance_frontier = NULL;
		}
	}

	//Now that that's finished, we can go back and calculate all of the control relations again
	calculate_all_control_relations(cfg, TRUE);
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

	//PASS 4: Clean algorithm
	//Clean follows after sweep because during the sweep process, we will likely delete the contents of
	//entire block. Clean uses 4 different steps in a specific order to eliminate control flow
	//that has been made useless by sweep()
	clean(cfg);

	//PASS 4: Recalculate everything
	//Now that we've marked, sweeped and cleaned, odds are that all of our control relations will be off due to deletions of blocks, statements,
	//etc. So, to remedy this, we will recalculate everything in the CFG
	//cleanup_all_control_relations(cfg);
	recompute_all_dominance_relations(cfg);

	//PASS 5: Shortcircuiting logic optimization
	//Sometimes, we are able avoid extra work by using short-circuiting logic for compound logical statements(&& and ||). This
	//only works for these kinds of statements, so the optimization is very specific. When the CFG is constructed, compound logic
	//statements like these are marked as eligible for short-circuiting optimizations. Now that we've gone through and deleted everything
	//that is useless, it's worth it to look at these statements and optimize them in one special pass
	optimize_compound_logic(cfg);


	return cfg;
}

