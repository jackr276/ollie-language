/**
 * Author: Jack Robbins
 * This file contains implementations for functions defined in the header file basic_block.h
 */

#include "basic_block.h"
#include <stdlib.h>

/**
 * Most normal blocks will rarely exceed 8 predecessors/successors, so this is our default size
 */
#define DEFAULT_EDGE_SET_SIZE 8


//================================== CFG Edge Set Utility ========================================
/**
 * Allocate a CFG edge set using the default size
 */
edge_set_t edge_set_alloc(){
	/**
	 * Stack allocate an empty edge set
	 */
	edge_set_t set = EMPTY_EDGE_SET;

	/**
	 * Allocate the underlying array and set the other values
	 */
	set.edges = calloc(DEFAULT_EDGE_SET_SIZE, sizeof(cfg_edge_t));
	set.max_index = DEFAULT_EDGE_SET_SIZE;
	set.current_index = 0;
	return set;
}


/**
 * Dynamically resize an edge set to accomodate the given index if need be
 */
void edge_set_dynamic_resize_if_needed(edge_set_t* edge_set, int32_t index){
	/**
	 * No need to resize here if we're smaller than the current index
	 */
	if(index < edge_set->current_index){
		return;
	}

	/**
	 * Otherwise we need a resize. We will resize to be double the size of
	 * the requested index
	 */
	edge_set->max_index = index * 2;
	edge_set->edges = realloc(edge_set->edges, sizeof(cfg_edge_t) * edge_set->max_index);
}


/**
 * Deallocate a CFG edge set
 */
void edge_set_dealloc(edge_set_t* edge_set){
	/**
	 * Free the underlying edge set array
	 */
	if(edge_set->edges != NULL){
		free(edge_set->edges);
	}

	/**
	 * Zero these out for safety
	 */
	edge_set->edges = NULL;
	edge_set->current_index = 0;
	edge_set->max_index = 0;
}

//================================== CFG Edge Set Utility ========================================




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

	/**
	 * Deallocate both of the edge sets
	 */
	edge_set_dealloc(&(block->predecessors));
	edge_set_dealloc(&(block->successors));

	//If this is a switch statement entry block, then it will have a jump table
	if(block->jump_table != NULL){
		jump_table_dealloc(block->jump_table);
	}

	//Otherwise its fine so
	free(block);
}
