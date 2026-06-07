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
void calculate_all_control_flow_relations_for_function(basic_block_t* function_entry, dynamic_array_t* function_blocks);

/**
 * Destroy all old control relations in anticipation of new ones coming in. This
 * operates on a per-function level
 */
void cleanup_all_control_relations(dynamic_array_t* function_blocks);


#endif /* GRAPH_ANALYZER_H */
