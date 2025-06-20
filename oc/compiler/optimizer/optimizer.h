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
 * Invoke the ollie optimizer
 */
cfg_t* optimize(cfg_t* cfg);

#endif /* OPTIMIZER_H */
