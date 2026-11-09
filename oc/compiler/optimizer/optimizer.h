/**
 * Author: Jack Robbins
 *
 * The compiler "middle-end", known as the ollie optimizer
 *
 * This subsystem is very closed off compared to the others. Nothing in it will need
 * to be used besides the generic "optimize" function. Any actual dependencies that are needed
 * will be included in the implementation .c file
*/

//Include guards
#ifndef OPTIMIZER_H
#define OPTIMIZER_H

//Link to the CFG - the only thing we need in this header
#include "../cfg/cfg.h"
#include <sys/types.h>

/**
 * Determine what kind of simplification that we need to do. Some simplifications
 * require a complete rework of the control flow
 */
typedef enum {
	NO_SIMPLIFICATION,
	SIMPLIFICATION_INSTRUCTIONS_ONLY,
	SIMPLIFICATION_INSTRUCTIONS_AND_CONTROL_FLOW,
} simplification_type_t;

/**
 * Perform a mark and sweep dead code elimination pass over the entire function CFG. This will remove all
 * OIR statements and control flow that is not deemed to be necessary by the mark() algorithm. In the event
 * that control flow was altered(branch changes, etc), we will recompute all control flow relations for this
 * function. This guarantees that values like dominators, dominance frontiers, reverse dominance frontiers, etc
 * remain accurate after DCE runs
 */
simplification_type_t perform_mark_and_sweep_pass(dynamic_array_t* function_blocks, basic_block_t* function_entry, basic_block_t* function_exit);

/**
 * A branch reduction pass first runs the clean() algorithm, which checks to see if blocks/branches have become useless or
 * redundant and adjusts them accordingly. Following that, *if* we did end up deleting/reordering blocks, we'll recompute
 * all control flow relations. An important note is that this function will only recompute control flow relations if it 
 * is necessary. Control flow relations are not cheap to compute so we should not be finding them blindly
 */
simplification_type_t perform_branch_reduction_pass(dynamic_array_t* function_blocks, basic_block_t* function_entry, basic_block_t* function_exit);

/**
 * Invoke the ollie optimizer
 */
cfg_t* optimize(cfg_t* cfg);

#endif /* OPTIMIZER_H */
