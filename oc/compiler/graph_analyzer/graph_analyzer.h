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
 * Calculate the dominator sets for each and every node
 *
 * This is the union-find algorithm. As of our migration to Lengauer-Tarjan for the 
 * immediate dominator this is not strictly necessary, but it is still going to be
 * exposed via an API in case it is needed in the future
 */
void calculate_dominator_sets(basic_block_t* function_entry_block, dynamic_array_t* function_blocks);


/**
 * We will calculate:
 *  1.) Dominator Sets
 *  2.) Dominator Trees
 *  3.) Dominance Frontiers
 *  4.) Postdominator sets
 *  5.) Reverse Dominance frontiers
 *  6.) Reverse post order traversals
 *
 * For every block in the given function
 */
void calculate_all_control_flow_relations_for_function(basic_block_t* function_entry_block, dynamic_array_t* function_blocks);

/**
 * Destroy all old control relations in anticipation of new ones coming in. This
 * operates on a per-function level
 */
void cleanup_all_control_relations(dynamic_array_t* function_blocks);


#endif /* GRAPH_ANALYZER_H */
