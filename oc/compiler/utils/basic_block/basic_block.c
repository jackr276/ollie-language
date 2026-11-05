/**
 * Author: Jack Robbins
 * This file contains implementations for functions defined in the header file basic_block.h
 */

#include "basic_block.h"



/**
 * Utility for deallocating a basic block
 */
void basic_block_dealloc(basic_block_t* block){
	//Just in case
	if(block == NULL){
		printf("ERROR: Attempt to deallocate a null block");
		exit(1);
	}

	//Deallocate the live variable array
	if(block->used_before_definition.internal_array != NULL){
		dynamic_array_dealloc(&(block->used_before_definition));
	}

	//Deallocate the assigned variable array
	if(block->assigned_variables.internal_array != NULL){
		dynamic_array_dealloc(&(block->assigned_variables));
	}

	//Deallocate the domninance frontier
	if(block->dominance_frontier.internal_array != NULL){
		dynamic_array_dealloc(&(block->dominance_frontier));
	}

	//Deallocate the reverse dominance frontier
	if(block->reverse_dominance_frontier.internal_array != NULL){
		dynamic_array_dealloc(&(block->reverse_dominance_frontier));
	}

	//Deallocate the dominator children frontier
	if(block->dominator_children.internal_array != NULL){
		dynamic_array_dealloc(&(block->dominator_children));
	}

	//Deallocate the liveness sets
	if(block->live_out.internal_array != NULL){
		dynamic_array_dealloc(&(block->live_out));
	}

	if(block->live_in.internal_array != NULL){
		dynamic_array_dealloc(&(block->live_in));
	}

	//Deallocate the successors
	//TODO
	if(block->successors.edges != NULL){
		dynamic_array_dealloc(&(block->));
	}

	//Deallocate the predecessors
	if(block->predecessors.internal_array != NULL){
		dynamic_array_dealloc(&(block->predecessors));
	}

	//If this is a switch statement entry block, then it will have a jump table
	if(block->jump_table != NULL){
		jump_table_dealloc(block->jump_table);
	}

	//Grab a statement cursor here
	instruction_t* cursor = block->leader_statement;
	//We'll need a temp block too
	instruction_t* temp = cursor;

	//So long as the cursor is not NULL
	while(cursor != NULL){
		temp = cursor;
		cursor = cursor->next_statement;
		//Destroy temp
		instruction_dealloc(temp);
	}
	

	//Otherwise its fine so
	free(block);
}
