/**
 * Author: Jack Robbins
 * This module contains everything needed to compute CFG dominance relations in
 * Ollie.
 */

//Include guards
#ifndef GRAPH_ANALYZER_H
#define GRAPH_ANALYZER_H

//Link to the CFG
#include "../cfg/cfg.h"

/**
 * Special exposes post order traversal API. The postorder traversal is needed
 * specifically in branch reduction in the optimizer/postprocessor. In this case,
 * we'll need a pre-allocated dynamic array to be passed in
 */
void get_post_order_traversal(basic_block_t* function_entry_block, dynamic_array_t* post_order_traversal);

/**
 * Get the nearest marked postdominator of a given block
 *
 * NOTE: in order for this to be accurate, we must have already computed
 * the immediate postdominators of all blocks within the function that this
 * block comes from
 */
basic_block_t* get_nearest_marked_postdominator(basic_block_t* block);

/**
 * We will calculate:
 *  1.) Reverse post order traversals
 *  2.) Immediate dominators
 *  3.) Dominator Trees
 *  4.) Dominance Frontiers
 *  5.) Immediate Postdominators
 *  6.) Postdominator sets
 *  7.) Reverse Dominance frontiers
 *
 * For every block in the given function. This externally facing API hides all of
 * the complexity behind it
 */
void calculate_all_control_flow_relations_for_function(basic_block_t* function_entry_block, basic_block_t* function_exit_block, dynamic_array_t* function_blocks);

/**
 * Destroy all old control relations in anticipation of new ones coming in. This
 * operates on a per-function level
 */
void cleanup_all_control_relations(dynamic_array_t* function_blocks);


#endif /* GRAPH_ANALYZER_H */
