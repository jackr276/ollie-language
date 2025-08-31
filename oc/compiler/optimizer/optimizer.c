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
		basic_block_t* successor = dynamic_array_get_at(b->successors, i);

		//Add b's successors to be a's successors
		add_successor_only(a, successor);

		//Now for each of the predecessors that equals b, it needs to now point to A
		for(u_int16_t j = 0; successor->predecessors != NULL && j < successor->predecessors->current_index; j++){
			//If it's pointing to b, it needs to be updated
			if(successor->predecessors->internal_array[j] == b){
				//Update it to now be correct
				successor->predecessors->internal_array[j] = a;
			}
		}
	}

	//Also make note of any direct succession
	a->direct_successor = b->direct_successor;

	//Copy over the block type and terminal type
	if(a->block_type != BLOCK_TYPE_FUNC_ENTRY){
		a->block_type = b->block_type;
	}

	//If b is a switch statment start block, we'll copy the jump table
	if(b->jump_table != NULL){
		a->jump_table = b->jump_table;
	}

	//If b is going to execute more than a, and it's now becoming a part of a, 
	//then a will need to execute more as well. As such, we take the highest of the two
	if(a->estimated_execution_frequency < b->estimated_execution_frequency){
		a->estimated_execution_frequency = b->estimated_execution_frequency;
	}

	//Copy this over too
	a->block_terminal_type = b->block_terminal_type;

	//For each statement in b, all of it's old statements are now "defined" in a
	instruction_t* b_stmt = b->leader_statement;

	//Modify these "block contained in" references to be A
	while(b_stmt != NULL){
		b_stmt->block_contained_in = a;

		//Push it up
		b_stmt = b_stmt->next_statement;
	}
	
	//We'll remove this from the list of created blocks
	dynamic_array_delete(cfg->created_blocks, b);
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
		
		//Run through the jump table and replace all of those targets as well. Most of the time,
		//we won't hit this because num_nodes will be 0. In the times that we do though, this is
		//what will ensure that switch statements are not corrupted by the optimization process
		if(predecessor->jump_table != NULL){
			for(u_int16_t idx = 0; idx < predecessor->jump_table->num_nodes; idx++){
				//If this equals the other node, we'll need to replace it
				if(dynamic_array_get_at(predecessor->jump_table->nodes, idx) == empty_block){
					//This now points to the replacement
					dynamic_array_set_at(predecessor->jump_table->nodes, replacement, idx);
				}
			}
		}

		//Now we'll go through every single statement in this block. If it's a jump statement whose target
		//is the empty block, we'll replace that reference with the replacement
		instruction_t* current_stmt = predecessor->leader_statement;

		//So long as this isn't null
		while(current_stmt != NULL){
			//If it's a jump statement AND the jump target is the empty block, we're interested
			if(current_stmt->statement_type == THREE_ADDR_CODE_JUMP_STMT && current_stmt->jumping_to_block == empty_block){
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
}


/**
 * Delete all branching statements in the current block. We know if a statement is branching if it is 
 * marks as branch ending. 
 *
 * NOTE: This should only be called after we have identified this block as a candidate for block folding
 */
static void delete_all_branching_statements(basic_block_t* block){
	//We'll always start from the end and work our way up
	instruction_t* current = block->exit_statement;
	//To hold while we delete
	instruction_t* temp;

	//So long as this is NULL and it's branch ending
	while(current != NULL && current->is_branch_ending == TRUE){
		temp = current;
		//Advance this
		current = current->previous_statement;
		//Then we delete
		delete_statement(temp);
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
		if(current->exit_statement != NULL && current->exit_statement->statement_type == THREE_ADDR_CODE_JUMP_STMT){
			//Now let's do a touch more work to see if we end in a conditional branch. We end in a conditional
			//branch if the last two statements are jump statements. We already know that the last one 
			//is, now we just need to check if the other one is
			u_int8_t ends_in_branch = FALSE;

			//If the prior statement is not NULL and it's a jump, we end in a conditional
			if(current->exit_statement->previous_statement != NULL && current->exit_statement->previous_statement->statement_type == THREE_ADDR_CODE_JUMP_STMT
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
				instruction_t* stmt = current->exit_statement;

				//So long as we are still branch ending and seeing statements
				while(stmt != NULL && stmt->is_branch_ending == TRUE){
					//If it isn't a jump statement, just move along
					if(stmt->statement_type != THREE_ADDR_CODE_JUMP_STMT){
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
					delete_all_branching_statements(current);
					//Once we're done deleting, we'll emit a jump right to that jumping to
					//block. This constitutes our "fold"
					emit_jump(current, end_branch_target, NULL, JUMP_TYPE_JMP, TRUE, FALSE);
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

				//There is no point in sticking around here. In getting to this area, we know that there was
				//only one statement in here. As such, any attempt to block merge this would be in error and not desirable, because
				//the block merging algorithm requires that there be more than one statement.
				//We'll continue here, and allow the next iteration to go forward
				continue;
			}

			//============================== BLOCK MERGING =================================================
			//This is another special case -- if the block we're jumping to only has one predecessor, then
			//we may as well avoid the jump and just merge the two
			if(jumping_to_block->predecessors->current_index == 1 && jumping_to_block->block_type != BLOCK_TYPE_LABEL){
				//We need to check here -- is there only ONE jump to the jumping to block inside of this
				//block? If there is only one, then we are all set to merge

				//Grab a statement cursor
				instruction_t* cursor = current->exit_statement->previous_statement;

				//Are we good to merge?
				u_int8_t good_to_merge = TRUE;

				while(cursor != NULL){
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
					delete_statement(current->exit_statement); 

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
				&& jumping_to_block->leader_statement->statement_type != THREE_ADDR_CODE_JUMP_STMT){
	 			//Let's check now and see if it's truly a conditional branch only
				
				//If it's not a jump, we're out here
	 			if(jumping_to_block->exit_statement == NULL || jumping_to_block->exit_statement->statement_type != THREE_ADDR_CODE_JUMP_STMT){
	 				continue;
	 			}

				//Final check: If the statement right before the exit also isn't a jump, we don't have a branch
				if(jumping_to_block->exit_statement->previous_statement == NULL || jumping_to_block->exit_statement->previous_statement->statement_type != THREE_ADDR_CODE_JUMP_STMT){
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
				delete_statement(current->exit_statement);

				//If we make it here we know that we have some kind of conditional branching logic here in the jumping to block, we'll need to create
				//a complete copy of it. This new copy will then be added into the current block in lieu of the jump statement that we just deleted
	
				//The leader of this new string of statement 
				instruction_t* head = NULL;
				//The one that we're currently operating on
				instruction_t* tail = NULL;

				//We'll use this cursor to run through the entirety of the jumping to block's code
				instruction_t* cursor = jumping_to_block->leader_statement;
				
				//So long as we have stuff to add
				while(cursor != NULL){
					//Get a complete copy of the statement
					instruction_t* new = copy_instruction(cursor);

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
					if(cursor->statement_type == THREE_ADDR_CODE_JUMP_STMT){
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
 *
 * t36 <- t35 >= t34
 * t37 <- 0x20
 * t38 <- b_1
 * t39 <- t38 <= t37
 * t40 <- t36 && t39
 * jz .L16 <----------- else target
 * jmp .L17 <---------- affirmative target
 *
 * This should become
 * t36 <- t35 >= t34
 * jl .L16 <-------- else target
 * t38 <- b_1
 * t39 <- t38 <= t37
 * jg .L16 <-------- else target
 * jmp .L17
 */
static void optimize_compound_and_jump_inverse(cfg_t* cfg, basic_block_t* block, instruction_t* stmt, basic_block_t* if_target, basic_block_t* else_target){
	//Starting off-we're given the and stmt as a parameter, and our two jumps
	//Let's look and see where the two variables that make up the and statement are defined. We know for a fact
	//that op1 will always come before op2. As such, we will look for where op1 is last assigned
	three_addr_var_t* op1 = stmt->op1;
	//Grab a statement cursor. We start at the previous one because we've no need
	//for the one we're currently on
	instruction_t* cursor = stmt->previous_statement;

		//Run backwards until we find where op1 is the assignee
	while(cursor != NULL && variables_equal(op1, cursor->assignee, FALSE) == FALSE){
		//Keep advancing backward
		cursor = cursor->previous_statement;
	}

	//Is our first op signed
	u_int8_t first_op_signed = is_type_signed(cursor->assignee->type);

	//Once we get out here, we have the statement that assigns op1. Since this is an "and" target,
	//we'll jump to ELSE if we have a bad result here(result being zero) because that would cause
	//the rest of the and to be false
	
	//We need to select the appropriate jump type for our statement. We want an inverse jump, 
	//because we're jumping if this condition fails
	jump_type_t jump = select_appropriate_jump_stmt(cursor->op, JUMP_CATEGORY_INVERSE, first_op_signed);
	
	//Jump to else here
	instruction_t* jump_to_else_stmt = emit_jmp_instruction(else_target, jump);
	//Mark where this came from
	jump_to_else_stmt->block_contained_in = block;
	//Make sure to mark that this is branch ending
	jump_to_else_stmt->is_branch_ending = TRUE;

	//We'll now need to insert this statement right after where op1 is assigned at cursor
	instruction_t* after = cursor->next_statement;

	//The jump statement is now in between the two
	cursor->next_statement = jump_to_else_stmt;
	jump_to_else_stmt->previous_statement = cursor;

	//And we'll also update the references for after
	jump_to_else_stmt->next_statement = after;
	after->previous_statement = jump_to_else_stmt;

	//Hang onto these
	instruction_t* previous = stmt->previous_statement;
	instruction_t* next = stmt->next_statement;
	instruction_t* final_jump = next->next_statement;

	//And even better, we now don't need the compound and at all. We can delete the whole stmt
	delete_statement(stmt);

	//We also no longer need the following jump statement
	delete_statement(next);

	//Our second op signedness - unsigned by default
	u_int8_t second_op_signed = is_type_signed(previous->assignee->type);

	//Now, we'll construct an entirely new statement based on what we have as the previous's operator
	//We'll do a direct jump here - if it's affirmative
	jump = select_appropriate_jump_stmt(previous->op, JUMP_CATEGORY_INVERSE, second_op_signed);

	//Now we'll jump to else
	instruction_t* final_cond_jump = emit_jmp_instruction(else_target, jump);
	//Mark where this came from
	final_cond_jump->block_contained_in = block;

	//We'll now add this one in right as the previous one
	previous->next_statement = final_cond_jump;
	final_cond_jump->previous_statement = previous;
	final_cond_jump->next_statement = final_jump;
	final_jump->previous_statement = final_cond_jump;
}


/**
 * Hande a compound or statement optimization in the inverse case
 *
 * t34 <- 0x58
 * t35 <- x_6
 * t36 <- t35 == t34
 * t37 <- 0x20
 * t38 <- b_1
 * t39 <- t38 <= t37
 * t40 <- t36 || t39
 * jz .L16 <------------- else target
 * jmp .L17 <------------ affirmative target
 *
 * This should become:
 * t34 <- 0x58
 * t35 <- x_6
 * t36 <- t35 == t34
 * je .L17 <----- good case, we go to if
 * t37 <- 0x20
 * t38 <- b_1
 * t39 <- t38 <= t37
 * jg .L16 <------ both failed, go to else
 * jmp .L17  <------- it worked, go to if
 *
 */
static void optimize_compound_or_jump_inverse(cfg_t* cfg, basic_block_t* block, instruction_t* stmt, basic_block_t* if_target, basic_block_t* else_target){
	//Starting off-we're given the and stmt as a parameter, and our two jumps
	//Let's look and see where the two variables that make up the and statement are defined. We know for a fact
	//that op1 will always come before op2. As such, we will look for where op1 is last assigned
	three_addr_var_t* op1 = stmt->op1;
	//Grab a statement cursor. We start at the previous one because we've no need
	//for the one we're currently on
	instruction_t* cursor = stmt->previous_statement;

	//Run backwards until we find where op1 is the assignee
	while(cursor != NULL && variables_equal(op1, cursor->assignee, FALSE) == FALSE){
		//Keep advancing backward
		cursor = cursor->previous_statement;
	}

	//Is our first op signed
	u_int8_t first_op_signed = is_type_signed(cursor->assignee->type);

	//We need to select the appropriate jump type for our statement. We want a regular jump, 
	//because we're jumping if this condition succeeds
	jump_type_t jump = select_appropriate_jump_stmt(cursor->op, JUMP_CATEGORY_NORMAL, first_op_signed);

	//Once we get out here, we have the statement that assigns op1. Since this is an "or" target,
	//we'll jump to IF we have a good result here(result being not zero) because that would cause
	//rest of the or to be true
	//Jump to else here
	instruction_t* jump_to_if_stmt = emit_jmp_instruction(if_target, jump);
	//Mark where this came from
	jump_to_if_stmt->block_contained_in = block;
	//Make sure to mark that this is branch ending
	jump_to_if_stmt->is_branch_ending = TRUE;

	//We'll now need to insert this statement right after where op1 is assigned at cursor
	instruction_t* after = cursor->next_statement;

	//The jump statement is now in between the two
	cursor->next_statement = jump_to_if_stmt;
	jump_to_if_stmt->previous_statement = cursor;

	//And we'll also update the references for after
	jump_to_if_stmt->next_statement = after;
	after->previous_statement = jump_to_if_stmt;

	//Hang onto these
	instruction_t* previous = stmt->previous_statement;
	instruction_t* next = stmt->next_statement;
	instruction_t* final_jump = next->next_statement;

	//And even better, we now don't need the compound and at all. We can delete the whole stmt
	delete_statement(stmt);

	//We also no longer need the following jump statement
	delete_statement(next);

	//Our second op signedness - unsigned by default
	u_int8_t second_op_signed = is_type_signed(previous->assignee->type);

	//Now if this fails, we know that we're going to the else case. As such, 
	//we'll select the inverse jump here
	jump = select_appropriate_jump_stmt(previous->op, JUMP_CATEGORY_INVERSE, second_op_signed);

	//Now we'll emit the jump to else
	instruction_t* final_cond_jump = emit_jmp_instruction(else_target, jump);
	//Mark where this came from
	final_cond_jump->block_contained_in = block;

	//We'll now add this one in right as the previous one
	previous->next_statement = final_cond_jump;
	final_cond_jump->previous_statement = previous;
	final_cond_jump->next_statement = final_jump;
	final_jump->previous_statement = final_cond_jump;
}


/**
 * Handle a compound and statement optimization
 *
 * t5 <- x_0
 * t6 <- 3
 * t5 <- t5 < t6
 * t7 <- x_0
 * t8 <- 1
 * t7 <- t7 != t8
 * t5 <- t5 && t7
 * jnz .L12
 * jmp .L13
 */
static void optimize_compound_and_jump(cfg_t* cfg, basic_block_t* block, instruction_t* stmt, basic_block_t* if_target, basic_block_t* else_target){
	//Starting off-we're given the and stmt as a parameter, and our two jumps
	//Let's look and see where the two variables that make up the and statement are defined. We know for a fact
	//that op1 will always come before op2. As such, we will look for where op1 is last assigned
	three_addr_var_t* op1 = stmt->op1;
	//Grab a statement cursor. We start at the previous one because we've no need
	//for the one we're currently on
	instruction_t* cursor = stmt->previous_statement;

	//Run backwards until we find where op1 is the assignee
	while(cursor != NULL && variables_equal(op1, cursor->assignee, FALSE) == FALSE){
		//Keep advancing backward
		cursor = cursor->previous_statement;
	}

	//Is our first op signed
	u_int8_t first_op_signed = is_type_signed(cursor->assignee->type);

	//Once we get out here, we have the statement that assigns op1. Since this is an "and" target,
	//we'll jump to ELSE if we have a bad result here(result being zero) because that would cause
	//the rest of the and to be false
	
	//We need to select the appropriate jump type for our statement. We want an inverse jump, 
	//because we're jumping if this condition fails
	jump_type_t jump = select_appropriate_jump_stmt(cursor->op, JUMP_CATEGORY_INVERSE, first_op_signed);
	
	//Jump to else here
	instruction_t* jump_to_else_stmt = emit_jmp_instruction(else_target, jump);
	//Mark where this came from
	jump_to_else_stmt->block_contained_in = block;
	//Make sure to mark that this is branch ending
	jump_to_else_stmt->is_branch_ending = TRUE;

	//We'll now need to insert this statement right after where op1 is assigned at cursor
	instruction_t* after = cursor->next_statement;

	//The jump statement is now in between the two
	cursor->next_statement = jump_to_else_stmt;
	jump_to_else_stmt->previous_statement = cursor;

	//And we'll also update the references for after
	jump_to_else_stmt->next_statement = after;
	after->previous_statement = jump_to_else_stmt;

	//Hang onto these
	instruction_t* previous = stmt->previous_statement;
	instruction_t* next = stmt->next_statement;
	instruction_t* final_jump = next->next_statement;

	//And even better, we now don't need the compound and at all. We can delete the whole stmt
	delete_statement(stmt);

	//We also no longer need the following jump statement
	delete_statement(next);

	//Our second op signedness - unsigned by default
	u_int8_t second_op_signed = is_type_signed(previous->assignee->type);

	//Now, we'll construct an entirely new statement based on what we have as the previous's operator
	//We'll do a direct jump here - if it's affirmative
	jump = select_appropriate_jump_stmt(previous->op, JUMP_CATEGORY_NORMAL, second_op_signed);

	//Now we'll emit the jump to if
	instruction_t* final_cond_jump = emit_jmp_instruction(if_target, jump);
	//Mark where this came from
	final_cond_jump->block_contained_in = block;

	//We'll now add this one in right as the previous one
	previous->next_statement = final_cond_jump;
	final_cond_jump->previous_statement = previous;
	final_cond_jump->next_statement = final_jump;
	final_jump->previous_statement = final_cond_jump;
}


/**
 * Hande a compound or statement optimization
 */
static void optimize_compound_or_jump(cfg_t* cfg, basic_block_t* block, instruction_t* stmt, basic_block_t* if_target, basic_block_t* else_target){
	//Starting off-we're given the and stmt as a parameter, and our two jumps
	//Let's look and see where the two variables that make up the and statement are defined. We know for a fact
	//that op1 will always come before op2. As such, we will look for where op1 is last assigned
	three_addr_var_t* op1 = stmt->op1;
	//Grab a statement cursor. We start at the previous one because we've no need
	//for the one we're currently on
	instruction_t* cursor = stmt->previous_statement;

	//Run backwards until we find where op1 is the assignee
	while(cursor != NULL && variables_equal(op1, cursor->assignee, FALSE) == FALSE){
		//Keep advancing backward
		cursor = cursor->previous_statement;
	}

	//Is our first op signed
	u_int8_t first_op_signed = is_type_signed(cursor->assignee->type);

	//We need to select the appropriate jump type for our statement. We want a regular jump, 
	//because we're jumping if this condition succeeds
	jump_type_t jump = select_appropriate_jump_stmt(cursor->op, JUMP_CATEGORY_NORMAL, first_op_signed);

	//Once we get out here, we have the statement that assigns op1. Since this is an "or" target,
	//we'll jump to IF we have a good result here(result being not zero) because that would cause
	//rest of the or to be true
	//Jump to else here
	instruction_t* jump_to_if_stmt = emit_jmp_instruction(if_target, jump);
	//Mark where this came from
	jump_to_if_stmt->block_contained_in = block;
	//Make sure to mark that this is branch ending
	jump_to_if_stmt->is_branch_ending = TRUE;

	//We'll now need to insert this statement right after where op1 is assigned at cursor
	instruction_t* after = cursor->next_statement;

	//The jump statement is now in between the two
	cursor->next_statement = jump_to_if_stmt;
	jump_to_if_stmt->previous_statement = cursor;

	//And we'll also update the references for after
	jump_to_if_stmt->next_statement = after;
	after->previous_statement = jump_to_if_stmt;

	//Hang onto these
	instruction_t* previous = stmt->previous_statement;
	instruction_t* next = stmt->next_statement;
	instruction_t* final_jump = next->next_statement;

	//And even better, we now don't need the compound and at all. We can delete the whole stmt
	delete_statement(stmt);

	//We also no longer need the following jump statement
	delete_statement(next);

	//Our second op signedness - unsigned by default
	u_int8_t second_op_signed = is_type_signed(previous->assignee->type);

	//Now, we'll construct an entirely new statement based on what we have as the previous's operator
	//We'll do a direct jump here - if it's affirmative
	jump = select_appropriate_jump_stmt(previous->op, JUMP_CATEGORY_NORMAL, second_op_signed);

	//Now we'll emit the jump to if
	instruction_t* final_cond_jump = emit_jmp_instruction(if_target, jump);
	//Mark where this came from
	final_cond_jump->block_contained_in = block;

	//We'll now add this one in right as the previous one
	previous->next_statement = final_cond_jump;
	final_cond_jump->previous_statement = previous;
	final_cond_jump->next_statement = final_jump;
	final_jump->previous_statement = final_cond_jump;
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
		if(block->leader_statement == NULL){
			continue;
		}

		//We always have two targets: the if(conditional) target and the else(nonconditional) target
		basic_block_t* if_target;
		basic_block_t* else_target;


		//If we don't end in two jumps, this isn't going to work. The exit must be a direct jump 
		if(block->exit_statement->statement_type != THREE_ADDR_CODE_JUMP_STMT || block->exit_statement->jump_type != JUMP_TYPE_JMP){
			continue;
		}

		//Now we need to check for the if target. If it's null, not a jump statement, or a direct jump, we're out of here
		if(block->exit_statement->previous_statement == NULL || block->exit_statement->previous_statement->statement_type != THREE_ADDR_CODE_JUMP_STMT
			|| block->exit_statement->previous_statement->jump_type == JUMP_TYPE_JMP){
			continue;
		}

		//Do we need to use the inverse jumping methodology?
		u_int8_t use_inverse_jump = FALSE;

		//If this IS an inverse jump, then our else clause is actually the conditional
		if(block->exit_statement->previous_statement->inverse_jump == TRUE){
			printf("HERE in block: .L%d\n", block->block_id);
			//These will be inverse of what we normally have
			//We made it here, so we know that this is the else target
			else_target = block->exit_statement->previous_statement->jumping_to_block;

			//This will be our if target
			if_target = block->exit_statement->jumping_to_block;

			//Set the flag
			use_inverse_jump = TRUE;

		//Otherwise it's a normal conditional scenario
		} else {
			//We made it here, so we know that this is the else target
			else_target = block->exit_statement->jumping_to_block;

			//This will be our if target
			if_target = block->exit_statement->previous_statement->jumping_to_block;

			//Set the flag
			use_inverse_jump = FALSE;
		}

		//Grab a statement cursor
		instruction_t* cursor = block->exit_statement;

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
			instruction_t* stmt = dynamic_array_get_at(eligible_statements, i);

			//Make the helper call. These are treated differently based on what their
			//operators are, so we'll need to use the appropriate call
			if(stmt->op == DOUBLE_AND){
				//No inverse jump, this is the common case
				if(use_inverse_jump == FALSE){
					//Invoke the and helper
					optimize_compound_and_jump(cfg, block, stmt, if_target, else_target);
				} else {
					//Invoke the inverse function
					optimize_compound_and_jump_inverse(cfg, block, stmt, if_target, else_target);
				}
			//Otherwise we have the double or
			} else {
				//No inverse jump, this is the common case
				if(use_inverse_jump == FALSE){
					//Invoke the or helper
					optimize_compound_or_jump(cfg, block, stmt, if_target, else_target);
				} else {
					//Use the inverse helper
					optimize_compound_or_jump_inverse(cfg, block, stmt, if_target, else_target);
				}
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
	for(u_int16_t _ = 0; _ < cfg->function_entry_blocks->current_index; _++){
		//Have we seen change(modification) at all?
		u_int8_t changed;

		//Grab the function block out
		basic_block_t* function_entry = dynamic_array_get_at(cfg->function_entry_blocks, _);

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
	reset_visited_status(cfg, FALSE);

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

		//Grab the statement out
		instruction_t* stmt = block->leader_statement;

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

			//We've encountered a jump statement of some kind. Now this is interesting. We do NOT delete
			//solitary jumps. We only delete jumps if they are a part of a conditional branch that has been deemed useless
			if(stmt->statement_type == THREE_ADDR_CODE_JUMP_STMT){
				//If it's not a conditional jump - we don't care. just go onto the next
				if(stmt->jump_type == JUMP_TYPE_JMP){
					//One thing we can check for here: if we see an unconditional jump(which means we got here)
					//followed by another unconditional jump, then the second unconditional jump is useless.
					//We can as such delete it
					//Advance the statement
					stmt = stmt->next_statement;

					/**
					 * If we get here, this means we have something like:
					 *  jmp .L8
					 *  jmp .L9 <---------- USELESS
					 *
					 * As such, we'll delete the .L9 jump and update successors
					 */
					if(stmt != NULL && stmt->statement_type == THREE_ADDR_CODE_JUMP_STMT && stmt->jump_type == JUMP_TYPE_JMP){
						instruction_t* temp = stmt;
						//Advance stmt
						stmt = stmt->next_statement;
						//Remove the statement
						delete_statement(temp);
						//And we're done
					}

					continue;
				}

				//If we make it down here then we know that this statement is some kind of custom jump. We're assuming	
				//that whatever conditional it was jumping on has been deleted, simply because we made it this far
				instruction_t* jump_to_if = stmt;

				//Let's see if we also have a jump to else here
				stmt = stmt->next_statement;

				//If this is not some jump to else, we're done here
				//If it's marked, we definitely don't want it
				if(stmt->mark == TRUE){
					stmt = stmt->next_statement;
					continue;
				//If it's not a jump, we'll also just delete
				} else if(stmt->statement_type != THREE_ADDR_CODE_JUMP_STMT){
					//Perform the deletion and advancement
					instruction_t* temp = stmt;
					stmt = stmt->next_statement;
					//Delete the statement, now that we know it is not a jump
					delete_statement(temp);
					instruction_dealloc(temp);
					continue;
				//One final snag we could catch - if it's a jump, but a conditional one, we'll also
				//leave it alone
				} else if(stmt->jump_type != JUMP_TYPE_JMP){
					stmt = stmt->next_statement;
					continue;
				}

				//Grab the block out. We need to do this here because we're about to be deleting blocks,
				//and we'll lose the reference if we do
				basic_block_t* block = stmt->block_contained_in;

				//So if we get down here, we know that this statement is unamrked and its a direct jump.
				//At this point, we have our conditional branch
				instruction_t* jump_to_else = stmt;

				//Now we can delete these both
				delete_statement(jump_to_else);
				delete_statement(jump_to_if);

				//We'll first find the nearest marked postdominator
				basic_block_t* immediate_postdominator = nearest_marked_postdominator(cfg, block);
				//We'll then emit a jump to that node
				instruction_t* jump_stmt = emit_jmp_instruction(immediate_postdominator, JUMP_TYPE_JMP);
				//Add this statement in
				add_statement(block, jump_stmt);
				//It is also now a successor
				add_successor(block, immediate_postdominator);
				//We're done with this part
				break;

			//Otherwise we delete the statement. Jump statements are ALWAYS considered useful
			} else {
				//Perform the deletion and advancement
				instruction_t* temp = stmt;

				//If we are deleting an indirect jump address calculation statement,
				//then this statements jump table is useless
				if(temp->statement_type == THREE_ADDR_CODE_INDIR_JUMP_ADDR_CALC_STMT){
					//We'll need to deallocate this jump table
					jump_table_dealloc(temp->jumping_to_block);

					//We also want to flag this as null in the block that this statement
					//comes from
					basic_block_t* block = temp->block_contained_in; 
					block->jump_table = NULL;
				}

				//If we have a stack pointer, this came from an allocation. We'll 
				//need to update the stack accordingly if we're deleting this
				if(temp->op1 != NULL && temp->op1->is_stack_pointer == TRUE){
					//Delete the assignee from the stack pointer
					remove_variable_from_stack(&(temp->function->data_area), temp->assignee);
				}

				//Advance the statement
				stmt = stmt->next_statement;
				//Delete the statement, now that we know it is not a jump
				delete_statement(temp);
				instruction_dealloc(temp);
			}
		}
	}
}


/**
 * Mark all statements that write to a given field in a structure. We're able to be more specific
 * here because a construct's layout is determined when the parser hits it. As such, if we're only
 * ever using a certain field, we need only worry about writes to that given field
 */
static void mark_and_add_all_field_writes(cfg_t* cfg, dynamic_array_t* worklist, symtab_variable_record_t* variable){
	//Run through every single block in the CFG
	for(u_int16_t _ = 0; _ < cfg->created_blocks->current_index; _++){
		//Grab the given block out
		basic_block_t* current = dynamic_array_get_at(cfg->created_blocks, _);	

		//If this block does not match the function that we're currently in, and the variable
		//itself is not global, we'll skip it
		if(variable->function_declared_in != NULL && variable->function_declared_in != current->function_defined_in){
			//Skip to the next one, this can't possibly be what we want
			continue;
		}

		//If we make it down here, we know that this block is writing to said memory address. Now we just need to figure
		//out the statements that are doing it
		
		//Grab a cursor out	of this block. We'll need to traverse to see which statements in here are important
		instruction_t* cursor = current->exit_statement;
		
		//Run through every statement in here
		while(cursor != NULL){
			//This will be in our "related write var" field. All we need to do is see
			//if the related write field matches our var
			if(cursor->assignee != NULL && cursor->assignee->related_memory_address != NULL
				&& cursor->assignee->access_type == MEMORY_ACCESS_WRITE
				&& cursor->assignee->related_memory_address == variable){

				//This is a case where we mark
				if(cursor->mark == FALSE){
					//Mark the statement itself
					cursor->mark = TRUE;
					dynamic_array_add(worklist, cursor);

					//Keep track of the old assignee
					three_addr_var_t* old_assignee = cursor->assignee;

					//Push it back by one to start
					cursor = cursor->previous_statement;

					//Keep going so long as we don't know where this came from. We need to ignore
					//indirection levels for this to work
					while(variables_equal(old_assignee, cursor->assignee, TRUE) == FALSE){
						cursor = cursor->previous_statement;
					}

					//Once we get here we know we got it
					cursor->mark = TRUE;
					dynamic_array_add(worklist, cursor);
				}
			}

			cursor = cursor->previous_statement;
		}	
	}
}


/**
 * Mark all definitions regardless of SSA level for a given variable. This rule is explicitly
 * used whenever we have a memory address assignment(&) that requires us to mark every single
 * write to the field whose address is being taken as important
 */
static void mark_and_add_all_definitions(cfg_t* cfg, three_addr_var_t* variable, symtab_function_record_t* current_function, dynamic_array_t* worklist){
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
			instruction_t* stmt = block->exit_statement;

			//So long as this isn't NULL
			while(stmt != NULL){
				//Is the assignee our variable AND it's unmarked?
				if(stmt->mark == FALSE && stmt->assignee != NULL && stmt->assignee->linked_var == variable->linked_var){
					//Add this in
					dynamic_array_add(worklist, stmt);
					//Mark it
					stmt->mark = TRUE;
					//Mark it
					block->contains_mark = TRUE;
				}

				//Advance the statement
				stmt = stmt->previous_statement;
			}

		//If it's a temp var, the search is not so easy. We'll need to crawl through
		//each statement and see if the assignee has the same temp number
		} else {
			//Let's find where we assign it
			instruction_t* stmt = block->exit_statement;

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

					//Since this is a temp var, we don't need to keep hunting
					return;
				}

				//Advance the statement
				stmt = stmt->previous_statement;
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

	//If we're marking a variable that is a memory address type, then we need to ensure that all writes
	//to said memory address are preserved
 	if(variable->linked_var != NULL){
		if(is_memory_address_type(variable->linked_var->type_defined_as) == TRUE){
			mark_and_add_all_field_writes(cfg, worklist, variable->linked_var);
		}
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
			instruction_t* stmt = block->exit_statement;

			//So long as this isn't NULL
			while(stmt != NULL){
				//Is the assignee our variable AND it's unmarked?
				if(stmt->mark == FALSE && stmt->assignee != NULL && stmt->assignee->linked_var == variable->linked_var){
					if(stmt->assignee->ssa_generation == variable->ssa_generation){
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
				stmt = stmt->previous_statement;
			}

		//If it's a temp var, the search is not so easy. We'll need to crawl through
		//each statement and see if the assignee has the same temp number
		} else {
			//Let's find where we assign it
			instruction_t* stmt = block->exit_statement;

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
				stmt = stmt->previous_statement;
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
		instruction_t* current_stmt = current->leader_statement;

		//For later storage
		symtab_variable_record_t* related_memory_address;

		//Now we'll run through every statement(operation) in this block
		while(current_stmt != NULL){
			//Clear it's mark
			current_stmt->mark = FALSE;

			//Go through statement by statement. In these
			//special types of statements like return statements,
			//function call statements, etc, we'll mark values as
			//important
			switch(current_stmt->statement_type){
				case THREE_ADDR_CODE_RET_STMT:
					//Mark this as useful
					current_stmt->mark = TRUE;
					//Add it to the list
					dynamic_array_add(worklist, current_stmt);
					//The block now has a mark
					current->contains_mark = TRUE;
					break;

				//These are added by the user and considered to
				//always be of use
				case THREE_ADDR_CODE_ASM_INLINE_STMT:
					current_stmt->mark = TRUE;
					//Add it to the list
					dynamic_array_add(worklist, current_stmt);
					//The block now has a mark
					current->contains_mark = TRUE;
					break;

				//Since we don't know whether or not a function
				//that is being called performs an important task,
				//we also always consider it to be important
				case THREE_ADDR_CODE_FUNC_CALL:
					current_stmt->mark = TRUE;
					//Add it to the list
					dynamic_array_add(worklist, current_stmt);
					//The block now has a mark
					current->contains_mark = TRUE;
					break;

				//Indirect function calls are the same as function calls. They will
				//always count becuase we do not know whether or not the indirectly
				//called function performs some important task. As such, we will 
				//mark it as important
				case THREE_ADDR_CODE_INDIRECT_FUNC_CALL:
					current_stmt->mark = TRUE;
					//Add it to the list
					dynamic_array_add(worklist, current_stmt);
					//The block now has a mark
					current->contains_mark = TRUE;
					break;

				//And finally idle statements are considered important
				//because they literally do nothing, so if the user
				//put them there, we'll assume that it was for a good reason
				case THREE_ADDR_CODE_IDLE_STMT:
					current_stmt->mark = TRUE;
					//Add it to the list
					dynamic_array_add(worklist, current_stmt);
					//The block now has a mark
					current->contains_mark = TRUE;
					break;

				//Let's see what other special cases we have
				default:
					break;
				}

			//Advance the current statement up
			current_stmt = current_stmt->next_statement;
		}

		//If this one's block type is a function entry block, we'll
		//need to do some more checking for parameter optimizations
		if(current->block_type == BLOCK_TYPE_FUNC_ENTRY){
			//Grab the record out
			symtab_function_record_t* function_record = current->function_defined_in;

			//We'll need to crawl through all of the parameters in here
			for(u_int16_t i = 0; i < function_record->number_of_params; i++){
				//Grab the associated variable out
				symtab_variable_record_t* parameter_variable = function_record->func_params[i].associate_var;

				//If this variable is a pointer, then we need to go through and mark/add all field writes that
				//reference it
				if(parameter_variable->type_defined_as->type_class == TYPE_CLASS_POINTER){
					mark_and_add_all_field_writes(cfg, worklist, parameter_variable);
				}
			}
		}
	}

	//Now that we've marked everything that is initially critical, we'll go through and trace
	//these values back through the code
	while(dynamic_array_is_empty(worklist) == FALSE){
		//Grab out the operation from the worklist(delete from back-most efficient)
		instruction_t* stmt = dynamic_array_delete_from_back(worklist);
		//Generic array for holding parameters
		dynamic_array_t* params;

		//There are several unique cases that require extra attention
		switch(stmt->statement_type){
			//If it's a phi function, now we need to go back and mark everything that it came from
			case THREE_ADDR_CODE_PHI_FUNC:
				params = stmt->phi_function_parameters;
				//Add this in here
				for(u_int16_t i = 0; params != NULL && i < params->current_index; i++){
					//Grab the param out
					three_addr_var_t* phi_func_param = dynamic_array_get_at(params, i);

					//Add the definitions in
					mark_and_add_definition(cfg, phi_func_param, stmt->function, worklist);
				}

				break;

			//If we have a function call, everything in the function call
			//is important
			case THREE_ADDR_CODE_FUNC_CALL:
				//Grab the parameters out
				params = stmt->function_parameters;

				//Run through them all and mark them
				for(u_int16_t i = 0; params != NULL && i < params->current_index; i++){
					mark_and_add_definition(cfg, dynamic_array_get_at(params, i), stmt->function, worklist);
				}

				break;

			//An indirect function call behaves similarly to a function call, but we'll also
			//need to mark it's "op1" value as important. This is the value that stores
			//the memory address of the function that we're calling
			case THREE_ADDR_CODE_INDIRECT_FUNC_CALL:
				//Mark the op1 of this function as being important
				mark_and_add_definition(cfg, stmt->op1, stmt->function, worklist);

				//Grab the parameters out
				params = stmt->function_parameters;

				//Run through them all and mark them
				for(u_int16_t i = 0; params != NULL && i < params->current_index; i++){
					mark_and_add_definition(cfg, dynamic_array_get_at(params, i), stmt->function, worklist);
				}

				break;

			//A memory address assignment requires that we mark every single write to a given variable inside of a function, regardless
			//of order
			case THREE_ADDR_CODE_MEM_ADDR_ASSIGNMENT:
				//Force the optimizer to mark all definitions of a given variable, regardless of where they are in a function
				mark_and_add_all_definitions(cfg, stmt->op1, stmt->function, worklist);
				break;

			//In all other cases, we'll just mark and add the two operands 
			default:
				//We need to mark the place where each definition is set
				mark_and_add_definition(cfg, stmt->op1, stmt->function, worklist);
				mark_and_add_definition(cfg, stmt->op2, stmt->function, worklist);

				break;
		}

		//Grab this out for convenience
		basic_block_t* block = stmt->block_contained_in;

		//Now for everything in this statement's block's RDF, we'll mark it's block-ending branches
		//as useful
		for(u_int16_t i = 0; block->reverse_dominance_frontier != NULL && i < block->reverse_dominance_frontier->current_index; i++){
			//Grab the block out of the RDF
			basic_block_t* rdf_block = dynamic_array_get_at(block->reverse_dominance_frontier, i);

			//If this is a switch statement block, then we'll simply mark everything
			//We'll know it's a switch statement block based on whether or not we have an indirect jump here.
			//Indirect jumps in Ollie are only ever used in switch statements
			if(rdf_block->exit_statement->statement_type == THREE_ADDR_CODE_INDIRECT_JUMP_STMT){
				//Run through and mark everything in it
				instruction_t* cursor = rdf_block->leader_statement;

				//Keep going through and marking
				while(cursor != NULL){
					if(cursor->mark == FALSE){
						cursor->mark = TRUE;
						//If it's not a jump, add to worklist
						if(cursor->statement_type != THREE_ADDR_CODE_JUMP_STMT){
							dynamic_array_add(worklist, cursor);
						}
					}

					//Advance to the next
					cursor = cursor->next_statement;
				}

				//Go to the next iteration, this block is done
				continue;
			}


			/**
			 * This is the pattern we are on the lookout for. These kinds of patterns
			 * always appear at the bottom of a block, and as such we will only search
			 * from the bottom up
			 *
			 * Important branch
			 * t1 <- a && b <------ condition
			 * jne .L8 <--------- if
			 * jmp .L9 <---------- else
			 */

			//Grab the exit statement. We will crawl our way from the bottom up here
			instruction_t* rdf_block_stmt = rdf_block->exit_statement;
			
			/**
			 * If the exit statement is:
			 * 	1.) NULL or
			 * 	2.) Not a jump statement or
			 * 	3.) Not a direct(jmp) statement
			 *
			 * we don't care to look any further, so we'll continue to the next one
			 */
			if(rdf_block_stmt == NULL || rdf_block_stmt->statement_type != THREE_ADDR_CODE_JUMP_STMT 
				|| rdf_block_stmt->jump_type != JUMP_TYPE_JMP){
				continue;
			}

			//Otherwise, this is a jump statement and it's our "jump to else" statement. 
			instruction_t* jump_to_else = rdf_block_stmt;

			//Advance the statement back by one
			rdf_block_stmt = rdf_block_stmt->previous_statement;

			/**
			 * Now we're looking for the conditional jump statement. This is where a lot
			 * of blocks will be disqualified, because most blocks end in a jump, not a
			 * conditional branch
			 *
			 * If this statement:
			 * 	1.) Is null or
			 * 	2.) Not a jump statement or
			 * 	3.) A direct(jmp) statement and not a conditional jump
			 *
			 * we don't care to look any further
			 */
			if(rdf_block_stmt == NULL || rdf_block_stmt->statement_type != THREE_ADDR_CODE_JUMP_STMT 
				|| rdf_block_stmt->jump_type == JUMP_TYPE_JMP){
				continue;
			}

			//If we make it here, then we've found the conditional part of our jump. This
			//will be our jump to if
			instruction_t* jump_to_if = rdf_block_stmt;

			//Mark and add the definitions of the op1 that this conditional jump relies on as important
			mark_and_add_definition(cfg, jump_to_if->op1, stmt->function, worklist);

			//Now mark the jump to if. We don't need to add this one to
			//any list - there's nothing else to mark
			if(jump_to_if->mark == FALSE){
				printf("Marking if statement: ");
				print_three_addr_code_stmt(stdout, jump_to_if);
				printf("\n");

				//Mark
				jump_to_if->mark = TRUE;
			}

			//Mark the jump to else. We also don't need to add this one
			//to any list - there's nothing else to mark
			if(jump_to_else->mark == FALSE){
				printf("Marking else statement: ");
				print_three_addr_code_stmt(stdout, jump_to_else);
				printf("\n");
				//Mark
				jump_to_else->mark = TRUE;
			}

			//And we're done - we'll let this loop to the next statement
		}
	}

	//And get rid of the worklist
	dynamic_array_dealloc(worklist);
}


/**
 * Estimate all execution frequencies in the CFG
 *
 * All execution frequencies are already done by this point. What we'll
 * do now is go through and update them using some simple rules
 */
static void estimate_execution_frequencies(cfg_t* cfg){
	//Run through all of the created blocks
	for(u_int16_t _ = 0; _ < cfg->created_blocks->current_index; _++){
		//Grab the given block out
		basic_block_t* block = dynamic_array_get_at(cfg->created_blocks, _);

		//If we have a return statement, we won't do any updates to it. These
		//are guarnateed to only execute once. Also, if we have no predecessors, we also
		//won't bother going further
		if(block->block_terminal_type == BLOCK_TERM_TYPE_RET 
			|| block->predecessors == NULL
			|| block->predecessors->current_index == 0){
			continue;
		}

		//The sum of the execution frequencies
		u_int16_t sum_execution_freq = 0;

		//Now run through all of the predecessors
		for(u_int16_t i = 0; i < block->predecessors->current_index; i++){
			//Grab it out
			basic_block_t* predecessor = dynamic_array_get_at(block->predecessors, i);

			//Add this to the overall sum
			sum_execution_freq += predecessor->estimated_execution_frequency;
		}

		//Now we'll get the average
		u_int16_t average_frequency = sum_execution_freq / block->predecessors->current_index;

		//If this average is *more* than what we currently have, we'll update the estimated cost
		if(average_frequency > block->estimated_execution_frequency){
			block->estimated_execution_frequency = average_frequency;
		}
	}
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
	calculate_all_control_relations(cfg, TRUE, TRUE);
}


/**
 * The generic optimize function. We have the option to specific how many passes the optimizer
 * runs for in it's current iteration
*/
cfg_t* optimize(cfg_t* cfg){
	//First thing we'll do is reset the visited status of the CFG. This just ensures
	//that we won't have any issues with the CFG in terms of traversal
	reset_visited_status(cfg, FALSE);

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

	//PASS 5: Estimate execution frequencies
	//This will become important in the register allocation later on. We'll need to estimate how often a block will be executed in order
	//to decide where to allocate registers appropriately.
	estimate_execution_frequencies(cfg);

	//PASS 5: Shortcircuiting logic optimization
	//Sometimes, we are able avoid extra work by using short-circuiting logic for compound logical statements(&& and ||). This
	//only works for these kinds of statements, so the optimization is very specific. When the CFG is constructed, compound logic
	//statements like these are marked as eligible for short-circuiting optimizations. Now that we've gone through and deleted everything
	//that is useless, it's worth it to look at these statements and optimize them in one special pass
	optimize_compound_logic(cfg);

	//Give back the CFG
	return cfg;
}
