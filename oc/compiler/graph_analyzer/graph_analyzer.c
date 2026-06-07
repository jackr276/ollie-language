/**
 * Author: Jack Robbins
 * This C file contains the implementations for APIs defined inside of the header file
 * of the same name
 */

#include "graph_analyzer.h"
#include <sys/types.h>




/**
 * We will calculate:
 *  1.) Dominator Sets
 *  2.) Dominator Trees
 *  3.) Dominance Frontiers
 *  4.) Postdominator sets
 *  5.) Reverse Dominance frontiers
 *  6.) Reverse post order traversals
 *
 * For every block in the given function. This externally facing API hides all of
 * the complexity behind it
 */
void calculate_all_control_flow_relations_for_function(basic_block_t* function_entry, dynamic_array_t* function_blocks){
	//Calculate all reverse traversals
	//TODO DOC
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
