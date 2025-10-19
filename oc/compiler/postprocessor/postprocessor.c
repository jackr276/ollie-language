/**
 * Author: Jack Robbins
 *
 * This file contains the implementations of the API defined in
 * the postprocessor.h header file
*/

//Link to the namesake header
#include "postprocessor.h"

#define TRUE 1
#define FALSE 0 

/**
 * Combine two blocks into one. This is different than other combine methods,
 * because post register-allocation, we do not really care about anything like
 * used variables, dominance relations, etc.
 *
 * Combine B into A
 *
 * After this happens, B no longer exists
 */
static instruction_t* combine_blocks(basic_block_t* a, basic_block_t* b){
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

	//Copy over the block type and terminal type
	if(a->block_type != BLOCK_TYPE_FUNC_ENTRY){
		a->block_type = b->block_type;
	}

	//If b is a switch statment start block, we'll copy the jump table
	if(b->jump_table != NULL){
		a->jump_table = b->jump_table;
	}


	//A's direct successor is now b's direct successor
	a->direct_successor = b->direct_successor;

	//For each statement in b, all of it's old statements are now "defined" in a
	instruction_t* b_stmt = b->leader_statement;

	//Modify these "block contained in" references to be A
	while(b_stmt != NULL){
		b_stmt->block_contained_in = a;

		//Push it up
		b_stmt = b_stmt->next_statement;
	}

	//Always return b's leader
	return b->leader_statement;
}


/**
 * Post register allocation, it is possible that the register allocator
 * could've given us something like: movq %rax, %rax. This is entirely
 * useless, and as such we will eliminate instructions like these
 *
 * This is akin to mark & sweep in the optimizer, though much more simple
 */
static void remove_useless_moves(cfg_t* cfg){
	//Grab the head block
	basic_block_t* current = cfg->head_block;

	//So long as we have blocks to traverse
	while(current != NULL){
		//Grab an instruction cursor
		instruction_t* current_instruction = current->leader_statement;

		//Run through all instructions
		while(current_instruction != NULL){
			//It's not a pure copy, so leave
			if(is_instruction_pure_copy(current_instruction) == TRUE){
				//Extract for convenience
				live_range_t* destination_live_range = current_instruction->destination_register->associated_live_range;
				live_range_t* source_live_range = current_instruction->source_register->associated_live_range;

				//We have a pure copy, so we can delete
				if(source_live_range->reg == destination_live_range->reg){
					instruction_t* holder = current_instruction;

					//Push this one up
					current_instruction = current_instruction->next_statement;

					//Delete the holder
					delete_statement(holder);

				//Otherwise push it up
				} else {
					current_instruction = current_instruction->next_statement;
				}

			//Otherwise push it up
			} else {
				current_instruction = current_instruction->next_statement;
			}
		}

		//Push it up
		current = current->direct_successor;
	}
}


/**
 * Replace all targets that jump to "empty block" with "replacement". This is a helper 
 * function for the "Empty Block Removal" step of clean()
 */
static void replace_all_branch_targets(basic_block_t* empty_block, basic_block_t* replacement){
	//Use a clone since we are mutating
	dynamic_array_t* clone = clone_dynamic_array(empty_block->predecessors);

	//For everything in the predecessor set of the empty block
	for(u_int16_t _ = 0; _ < clone->current_index; _++){
		//Grab a given predecessor out
		basic_block_t* predecessor = dynamic_array_get_at(clone, _);

		//The empty block is no longer a successor of this predecessor
		delete_successor(predecessor, empty_block);
		
		//Run through the jump table and replace all of those targets as well. Most of the time,
		//we won't hit this because num_nodes will be 0. In the times that we do though, this is
		//what will ensure that switch statements are not corrupted by the optimization process
		if(predecessor->jump_table != NULL){
			for(u_int16_t idx = 0; idx < predecessor->jump_table->num_nodes; idx++){
				//If this equals the other node, we'll need to replace it
				if(dynamic_array_get_at(predecessor->jump_table->nodes, idx) == empty_block){
					//This now points to the replacement
					dynamic_array_set_at(predecessor->jump_table->nodes, replacement, idx);

					//The replacement is now a successor of this predecessor
					add_successor(predecessor, replacement);
				}
			}
		}

		//We always will be starting at the exit statement. Branches/jumps
		//can only happen at the end
		instruction_t* current_statement = predecessor->exit_statement;

		//Run through all statements - there may be jumps mixed in here and
		//there, so we don't have the luxury of only looking at the end 
		//statement
		while(current_statement != NULL){
			//Go based on the type
			switch(current_statement->instruction_type){
				//Some kind of jump - this is what we are looking for
				case JMP:
				case JE:
				case JNE:
				case JZ:
				case JNZ:
				case JG:
				case JL:
				case JGE:
				case JLE:
				case JA:
				case JB:
				case JAE:
				case JBE:
					//If this is the empty block, then replace it
					if(current_statement->if_block == empty_block){
						current_statement->if_block = replacement;
						//This is now a successor
						add_successor(predecessor, replacement);
					}

					break;


				//Not of interest to us so do nothing
				default:
					break;
			}

			//Push it up
			current_statement = current_statement->previous_statement;
		}
	}

	//The empty block now no longer has the replacement as a successor
	delete_successor(empty_block, replacement);

	//Destroy the clone array
	dynamic_array_dealloc(clone);
}


/**
 * The branch reduce function is what we use on each pass of the function
 * postorder
 *
 * This is really just a slimmed-down version of branch_reduce in the optimizer
 *
 * NOTE: there is no long a consideration for branches here
 *
 * Procedure branch_reduce_postprocess():
 * 	for each block in postorder
 * 		if i ends in a jump to j then
 * 			if i is empty then
 * 				replace transfers to i with transfers to j
 * 			if j has only one predecessor then
 * 				merge i and j
 */
static u_int8_t branch_reduce_postprocess(cfg_t* cfg, dynamic_array_t* postorder){
	//Have we seen a change? By default we assume not
	u_int8_t changed = FALSE;

	//Our current block
	basic_block_t* current;

	/**
	 * For each block in postorder
	 */
	for(u_int16_t _ = 0; _ < postorder->current_index; _++){
		//Grab the current block out
		current = dynamic_array_get_at(postorder, _);

		/**
		 * If block i ends in a jump to j then..
		 */
		if(current->exit_statement != NULL
			&& current->exit_statement->instruction_type == JMP){
			//Extract the block(j) that we're going to
			basic_block_t* jumping_to_block = current->exit_statement->if_block;

			/**
			 * If i is empty then
			 * 	replace transfers to i with transfers to j
			 */
			//We know it's empty if these are the same
			if(current->exit_statement == current->leader_statement
				&& current->block_type != BLOCK_TYPE_FUNC_ENTRY){
				//Replace all jumps to the current block with those to the jumping block
				replace_all_branch_targets(current, jumping_to_block);

				//Current is no longer in the picture
				dynamic_array_delete(cfg->created_blocks, current);

				//Counts as a change
				changed = TRUE;

				//We are done here, no need to continue on
				continue;
			}

			/**
			 * If j only has one predecessor then
			 * 	merge i and j
			 */
			if(jumping_to_block->predecessors->current_index == 1){
				//Delete the jump statement because it's now useless
				delete_statement(current->exit_statement);

				//Decouple these as predecessors/successors
				delete_successor(current, jumping_to_block);

				//Combine the two
				combine_blocks(current, jumping_to_block);

				//Counts as a change 
				changed = TRUE;

				//And we're done here
				continue;
			}
		}
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
 * Procedure condense():
 * 	while changed
 * 	 compute Postorder of CFG
 * 	 branch_reduce_postprocess()
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
			changed = branch_reduce_postprocess(cfg, postorder);

			//We can free up the old postorder now
			dynamic_array_dealloc(postorder);
			
		//We keep going so long as branch_reduce changes something 
		} while(changed == TRUE);
	}
}


/**
 * The postprocess function performs all post-allocation cleanup/optimization 
 * tasks and returns the ordered CFG in file-ready form
 */
/**
 * In the postprocess step, we will run through every statement and perform a few
 * optimizations:
 */
void postprocess(cfg_t* cfg){
	/**
	 * PASS 1: remove any/all useless move operations from the CFG
	 */
	remove_useless_moves(cfg);



	//Grab the head block
	basic_block_t* current = cfg->head_block;

	//Go until we hit the end
	while(current != NULL){
		//Grab a cursor
		instruction_t* current_instruction = current->leader_statement;

		//Run through all instructions
		while(current_instruction != NULL){
			//It's not a pure copy, so leave
			if(is_instruction_pure_copy(current_instruction) == TRUE){
				//Extract for convenience
				live_range_t* destination_live_range = current_instruction->destination_register->associated_live_range;
				live_range_t* source_live_range = current_instruction->source_register->associated_live_range;

				//We have a pure copy, so we can delete
				if(source_live_range->reg == destination_live_range->reg){
					instruction_t* holder = current_instruction;

					//Push this one up
					current_instruction = current_instruction->next_statement;

					//Delete the holder
					delete_statement(holder);
				} else {
					current_instruction = current_instruction->next_statement;
				}

				continue;
			}

			//If we have a jump instruction here
			if(current_instruction->instruction_type == JMP){
				//Extract where we're jumping to
				basic_block_t* jumping_to_block = current_instruction->if_block;

				//If the direct sucessor is the jumping to block, there are a few actions
				//that we may be able to take
				if(current->direct_successor == jumping_to_block){
					//We can combine the two blocks into one
					if(jumping_to_block->predecessors->current_index == 1){
						//Delete the jump statement
						delete_statement(current_instruction);

						//Combine the two blocks here
						current_instruction = combine_blocks(current, jumping_to_block);

						//The jumping to block is no longer a place here
						dynamic_array_delete(cfg->created_blocks, jumping_to_block);
					
					/**
					 * Otherwise, we should still be able to just delete this jump instruction
					 */
					} else {
						//Temp holder
						instruction_t* temp = current_instruction;

						//Advance this
						current_instruction = current_instruction->next_statement;

						//Just delete the temp instruction
						delete_statement(temp);
					}

					continue;
				}
			}

			current_instruction = current_instruction->next_statement;
		}

		//Push it up
		current = current->direct_successor;
	}
}
