/**
 * Author: Jack Robbins
 *
 * This file contains the implementations of the API defined in
 * the postprocessor.h header file
*/

//Link to the namesake header
#include "postprocessor.h"
#include "../utils/queue/heap_queue.h"
#include "../utils/constants.h"

/**
 * Combine two blocks into one. This is different than other combine methods,
 * because post register-allocation, we do not really care about anything like
 * used variables, dominance relations, etc.
 *
 * Combine B into A
 *
 * After this happens, B no longer exists
 */
static instruction_t* combine_blocks(cfg_t* cfg, basic_block_t* a, basic_block_t* b){
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

	//For each statement in b, all of it's old statements are now "defined" in a
	instruction_t* b_stmt = b->leader_statement;

	//Modify these "block contained in" references to be A
	while(b_stmt != NULL){
		b_stmt->block_contained_in = a;

		//Push it up
		b_stmt = b_stmt->next_statement;
	}

	//Block b no longer exists
	dynamic_array_delete(cfg->created_blocks, b);

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
				combine_blocks(cfg, current, jumping_to_block);

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
static void condense(cfg_t* cfg){
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
 * Does the block in question end in a jmp instruction? If so,
 * give back what it's jumping ot
 */
static basic_block_t* does_block_end_in_jump_instruction(basic_block_t* block){
	//If it's null then leave
	if(block->exit_statement == NULL){
		return NULL;
	}

	//Go based on our type here
	switch(block->exit_statement->instruction_type){
		//Direct jump, just use the if block
		case JMP:
			return block->exit_statement->if_block;

		//By default no
		default:
			return NULL;
	}
}


/**
 * Once we've done all of the reduction that we see fit to do, we'll need to 
 * find a way to reorder the blocks since it is likely that the control flow
 * changed
 */
static basic_block_t* reorder_blocks(cfg_t* cfg){
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
				basic_block_t* end_jumps_to = does_block_end_in_jump_instruction(previous);

				//If we do AND what we're jumping to is the direct successor, then we'll
				//delete the jump statement as it is now unnecessary
				if(end_jumps_to == previous->direct_successor){
					//Get rid of this jump as it's no longer needed
					delete_statement(previous->exit_statement);
				}

				//Add this in as well
				previous = current;
			}

			//Make sure that we flag this as visited
			current->visited = TRUE;

			//Let's first check for our special case - us jumping to a given block as the very last statement. If
			//this turns back something that isn't null, it'll be the first thing we add in
			basic_block_t* direct_end_jump = does_block_end_in_jump_instruction(current);

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

	/**
	 * PASS 2: perform a modified branch reduction to condense the code
	*/
	condense(cfg);

	/**
	 * PASS 3: final reordering
	*/
	reorder_blocks(cfg);
}
