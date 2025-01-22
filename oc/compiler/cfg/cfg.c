/**
 * The implementation file for all CFG related operations
*/

#include "cfg.h"
#include <stdio.h>
#include <sys/types.h>

//Our atomically incrementing integer
static u_int16_t current_block_id = 0;

/**
 * A helper function that makes a new block id. This ensures we have an atomically
 * increasing block ID
 */
static u_int16_t increment_and_get(){
	current_block_id++;
	return current_block_id;
}


/**
 * Allocate a basic block using calloc. NO data assignment
 * happens in this function
*/
basic_block_t* basic_block_alloc(){
	//Allocate the block
	basic_block_t* created = calloc(1, sizeof(basic_block_t));
	//Grab the unique ID for this block
	created->block_id = increment_and_get();
	return created;
}


/**
 * Add a predecessor to the target block. When we add a predecessor, the target
 * block is also implicitly made a successor of said predecessor
 */
void add_predecessor(basic_block_t* target, basic_block_t* predecessor){
	//Let's check this
	if(target->num_predecessors == MAX_PREDECESSORS){
		//Internal error for the programmer
		printf("CFG ERROR. YOU MUST INCREASE THE NUMBER OF PREDECESSORS");
		exit(1);
	}

	//Otherwise we're set here
	//Add this in
	target->predecessors[target->num_predecessors] = predecessor;
	//Increment how many we have
	(target->num_predecessors)++;

	//We also need to reverse the roles and add target as a successor to "predecessor"
	//Let's check this
	if(predecessor->num_successors == MAX_SUCCESSORS){
		//Internal error for the programmer
		printf("CFG ERROR. YOU MUST INCREASE THE NUMBER OF SUCCESSORS");
		exit(1);
	}

	//Otherwise we're set here
	//Add this in
	predecessor->successors[predecessor->num_successors] = target;
	//Increment how many we have
	(predecessor->num_successors)++;
}


/**
 * Add a successor to the target block
 */
void add_successor(basic_block_t* target, basic_block_t* successor){
	//Let's check this
	if(target->num_successors == MAX_SUCCESSORS){
		//Internal error for the programmer
		printf("CFG ERROR. YOU MUST INCREASE THE NUMBER OF SUCCESSORS");
		exit(1);
	}

	//Otherwise we're set here
	//Add this in
	target->successors[target->num_successors] = successor;
	//Increment how many we have
	(target->num_successors)++;

	//Now we'll also need to add in target as a predecessor of successor
	//Let's check this
	if(successor->num_predecessors == MAX_PREDECESSORS){
		//Internal error for the programmer
		printf("CFG ERROR. YOU MUST INCREASE THE NUMBER OF PREDECESSORS");
		exit(1);
	}

	//Otherwise we're set here
	//Add this in
	successor->predecessors[successor->num_predecessors] = target;
	//Increment how many we have
	(successor->num_predecessors)++;
}


/**
 * Deallocate a basic block
*/
void basic_block_dealloc(basic_block_t* block){
	//Just in case
	if(block == NULL){
		printf("ERROR: Attempt to deallocate a null block");
		exit(1);
	}

	//Otherwise its fine so
	free(block);
}
