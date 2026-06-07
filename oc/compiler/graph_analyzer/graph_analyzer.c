/**
 * Author: Jack Robbins
 * This C file contains the implementations for APIs defined inside of the header file
 * of the same name
 */

#include "graph_analyzer.h"
#include <sys/types.h>


/**
 * Run through an entire array of function blocks and reset the status for
 * every single one. We assume that the caller knows what they are doing, and
 * that the blocks inside of the array are really the correct blocks
 */
static inline void reset_visit_status_for_function(dynamic_array_t* function_blocks){
	//Run through all of the blocks
	for(u_int32_t i = 0; i < function_blocks->current_index; i++){
		//Extract the current block
		basic_block_t* current = dynamic_array_get_at(function_blocks, i);

		//Flag it as false
		current->visited = FALSE;
	}
}


/**
 * A recursive post order simplifies the code, so it's what we'll use here
 */
static void post_order_traversal_rec(dynamic_array_t* post_order_traversal, basic_block_t* entry){
	//If we've visited this one before, skip
	if(entry->visited == TRUE){
		return;
	}

	//Otherwise mark that we've visited
	entry->visited = TRUE;

	//Run through every successor
	for(u_int16_t _ = 0; _ < entry->successors.current_index; _++){
		//Recursive call to every child first
		post_order_traversal_rec(post_order_traversal, dynamic_array_get_at(&(entry->successors), _));
	}
	
	//Now we'll finally visit the node
	dynamic_array_add(post_order_traversal, entry);

	//And we're done
}


/**
 * Get and return the regular postorder traversal for a function-level CFG
 *
 * NOTE: This assumes that the caller has already wiped the function's visited
 * status clean
 */
dynamic_array_t compute_post_order_traversal(basic_block_t* entry){
	//Create our dynamic array
	dynamic_array_t post_order_traversal = dynamic_array_alloc();

	//Make the recursive call
	post_order_traversal_rec(&post_order_traversal, entry);

	//Give the traversal back
	return post_order_traversal;
}


/**
 * Calculate all reverse traversals for a given function. A reverse traversal is simply a traversal on the graph
 * where every successor is a predecessor, and every predecessor is a successor. This is needed for the postdominance
 * computation
 */
static void calculate_all_reverse_traversals(basic_block_t* function_entry_block, dynamic_array_t* function_blocks){
	//Set the RPO to be null
	if(function_entry_block->reverse_post_order.internal_array != NULL){
		dynamic_array_dealloc(&(function_entry_block->reverse_post_order));
	}

	//Set the RPO reverse CFG to be null
	if(function_entry_block->reverse_post_order_reverse_cfg.internal_array != NULL){
		dynamic_array_dealloc(&(function_entry_block->reverse_post_order_reverse_cfg));
	}

	//Reset the function visited status
	reset_visit_status_for_function(function_blocks);

	//Compute the reverse post order traversal
	function_entry_block->reverse_post_order = compute_post_order_traversal(function_entry_block);

	//Reset the function visited status
	reset_visit_status_for_function(function_blocks);

	//Now use the reverse CFG(successors are predecessors, and vice versa)
	function_entry_block->reverse_post_order_reverse_cfg = compute_reverse_post_order_traversal_reverse_cfg(function_entry_block);
}


/**
 * We will calculate:
 *  1.) Reverse post order traversals
 *  2.) Dominator Sets
 *  3.) Dominator Trees
 *  4.) Dominance Frontiers
 *  5.) Postdominator sets
 *  6.) Reverse Dominance frontiers
 *
 * For every block in the given function. This externally facing API hides all of
 * the complexity behind it
 */
void calculate_all_control_flow_relations_for_function(basic_block_t* function_entry_block, dynamic_array_t* function_blocks){
	calculate_all_reverse_traversals(function_entry_block, function_blocks);
	
	//We first need to calculate the dominator sets of every single node
	//TODO DOC
	calculate_dominator_sets(function_entry_block, function_blocks);

	//Now we'll build the dominator tree up
	//TODO DOC
	build_dominator_trees(function_blocks);

	//Now calculate the dominance frontier for every single block
	//TODO DOC
	calculate_dominance_frontiers(function_blocks);

	//Calculate the postdominator sets for later analysis in the optimizer
	//TODO DOC
	calculate_postdominator_sets(function_entry_block, function_blocks);

	//We'll also now calculate the reverse dominance frontier that will be used
	//in later analysis by the optimizer
	//TODO DOC
	calculate_reverse_dominance_frontiers(function_blocks);
}


/**
 * Destroy all old control relations in anticipation of new ones coming in. This 
 * operates on a per-function level
 */
void cleanup_all_control_relations(dynamic_array_t* function_blocks){
	//For each block in the CFG
	for(u_int32_t i = 0; i < function_blocks->current_index; i++){
		//Grab the block out
		basic_block_t* block = dynamic_array_get_at(function_blocks, i);

		if(block->postdominator_set.internal_array != NULL){
			dynamic_array_dealloc(&(block->postdominator_set));
		}

		if(block->dominator_set.internal_array != NULL){
			dynamic_array_dealloc(&(block->dominator_set));
		}

		if(block->dominator_children.internal_array != NULL){
			dynamic_array_dealloc(&(block->dominator_children));
		}

		if(block->dominance_frontier.internal_array != NULL){
			dynamic_array_dealloc(&(block->dominance_frontier));
		}

		if(block->reverse_dominance_frontier.internal_array != NULL){
			dynamic_array_dealloc(&(block->reverse_dominance_frontier));
		}

		if(block->reverse_post_order_reverse_cfg.internal_array != NULL){
			dynamic_array_dealloc(&(block->reverse_post_order_reverse_cfg));
		}

		if(block->reverse_post_order.internal_array != NULL){
			dynamic_array_dealloc(&(block->reverse_post_order));
		}
	}
}
