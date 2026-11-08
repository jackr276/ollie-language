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
 * Delete all blocks that are not reachable from the function entry blocks. To do this, 
 * we will perform a DFS from the function block and flag every block that can be reached. 
 * Follwoing that, if we see that there are any blocks with a value of (-1), then we delete
 * them because they cannot be reached from the function entry
 *
 * This function returns TRUE if there was at least one unreachable block found
 */
u_int8_t delete_all_unreachable_blocks(basic_block_t* function_entry, dynamic_array_t* function_blocks);

/**
 * Special exposes post order traversal API. The postorder traversal is needed
 * specifically in branch reduction in the optimizer/postprocessor. In this case,
 * we'll need a pre-allocated dynamic array to be passed in
 */
void get_post_order_traversal(dynamic_array_t* function_blocks, basic_block_t* function_entry_block, dynamic_array_t* post_order_traversal);

/**
 * Get the reverse post order traversal over the reverse CFG(successors are predecessors and vice versa). This on-demand traversal
 * grabber requires a pre-allocated array to be passed in that will store the traversal
 */
void get_reverse_post_order_reverse_cfg_traversal(dynamic_array_t* function_blocks, basic_block_t* function_exit_block, dynamic_array_t* reverse_post_order_traversal);

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
 *  1.) Immediate dominators using Lengauer-Tarjan
 *  2.) Dominator Trees
 *  3.) Dominance Frontiers
 *  4.) Identify potential infinitie execution with Tarjan SCCs
 *  5.) Add analysis only edges on infinite SCCs
 *  6.) Immediate Postdominators with analysis edges
 *  7.) Reverse Dominance frontiers with analysis edgse
 *  8.) Remove all virtual edges
 *
 * For every block in the given function. This externally facing API hides all of
 * the complexity behind it
 *
 * NOTE: this function expects to be operating on a set of function blocks that has had unreachable blocks eliminated
 */
void calculate_all_control_flow_relations_for_function(basic_block_t* function_entry_block, basic_block_t* function_exit_block, dynamic_array_t* function_blocks);

/**
 * Destroy all old control relations in anticipation of new ones coming in. This
 * operates on a per-function level
 */
void cleanup_all_control_relations(dynamic_array_t* function_blocks);


#endif /* GRAPH_ANALYZER_H */
