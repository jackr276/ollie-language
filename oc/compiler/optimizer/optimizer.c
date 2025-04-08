/**
 * Author: Jack Robbins
 *
 * This is the implementation file for the ollie optimizer. Currently
 * it is implemented as one monolothic block
*/

#include "optimizer.h"


/**
 * The clean algorithm will remove all useless control flow structures, ideally
 * resulting in a simplified CFG. This should be done after we use mark and sweep
 * to get rid of useless code, because that may lead to empty blocks that we can clean up here
 */
static void clean(cfg_t* cfg){

}


/**
 * The sweep algorithm will go through and remove every operation that has not been marked
 */
static void sweep(cfg_t* cfg){

}


/**
 * The mark algorithm will go through and mark every operation(three address code statement) as
 * critical or noncritical. We will then go back through and see which operations are setting
 * those critical values
 */
static void mark(cfg_t* cfg){

}


/**
 * The mark-and-sweep dead code elimintation algorithm. This helper function will invoke
 * both for us in the correct order, all that we need do is call it
 */
static void mark_and_sweep(cfg_t* cfg){


}


/**
 * The generic optimize function. We have the option to specific how many passes the optimizer
 * runs for in it's current iteration
*/
cfg_t* optimize(cfg_t* cfg, call_graph_node_t* call_graph, u_int8_t num_passes){
	//First thing we'll do is reset the visited status of the CFG. This just ensures
	//that we won't have any issues with the CFG in terms of traversal
	reset_visited_status(cfg);

	//STEP 1: USELESS CODE ELIMINATION

	//We will first use mark and sweep to eliminate any/all dead code that we can find. Code is dead
	//if
	mark_and_sweep(cfg);

	//Now that we're done with that, any/all useless code should be removed
	
	//STEP 2: USELESS CONTROL FLOW ELIMINATION
	
	//Next, we will eliminate any/all useless control flow from the function. This is done by the clean()
	//algorithm
	
	
	return cfg;

}

