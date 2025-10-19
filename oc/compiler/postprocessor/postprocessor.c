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
 * The postprocess function performs all post-allocation cleanup/optimization 
 * tasks and returns the ordered CFG in file-ready form
 */
/**
 * In the postprocess step, we will run through every statement and perform a few
 * optimizations:
 *   1.) If we see an operation like movq %rax, %rax - it is useless so we will delete it
 *   2.) If we see this
 *   		.L2:
 *   			-- stuff ---
 *   			jmp .L3
 *
 *   		.L3
 *
 * 			*AND* .L3 has *one* predecessor(.L2), we can combine the two. We don't care
 * 			about liveness information anymore, so this is all we'll need to do
 */
void postprocess(cfg_t* cfg){
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
