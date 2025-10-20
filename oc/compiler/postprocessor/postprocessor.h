/**
 * Author: Jack Robbins
 *
 * The postprocessor runs one final branch_reduce() cycle on the post register allocation
 * code. This allows it to account for statement coalescence and instruction selector 
 * optimizations. It will also delete any redundant move statements. Finally, it will
 * reorder the blocks in the pattern that minimizes the number of jumps
*/

#ifndef OLLIE_POSTPROCESSOR_H
#define OLLIE_POSTPROCESSOR_H

//Link to CFG
#include "../cfg/cfg.h"

/**
 * The postprocess function performs all post-allocation cleanup/optimization 
 * tasks and returns the ordered CFG in file-ready form
 */
void postprocess(cfg_t* cfg);

#endif /* OLLIE_POSTPROCESSOR_H */
