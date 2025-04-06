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
 * Invoke the optimizer with a certain number of passes. At least
 * one pass is guaranteed, but the user can specify more
 *
 * TODO - we may need more parameters here, this is a start
 */
cfg_t* optimize(cfg_t* cfg, call_graph_node_t* call_graph, u_int8_t num_passes);

#endif /* OPTIMIZER_H */
