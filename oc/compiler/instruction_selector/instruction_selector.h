/**
 * Author: Jack Robbins
 *
 * This header file contains the APIs for the Ollie Instruction Selector
*/

#ifndef INSTRUCTION_SELECTOR_H
#define INSTRUCTION_SELECTOR_H

#include "../utils/dynamic_array/dynamic_array.h"
#include "../cfg/cfg.h"

/**
 * A function that selects all instructions, via the peephole method. This kind of 
 * operation completely translates the CFG out of a CFG. When done, we have a straight line
 * of code that we print out
 */
void select_all_instructions(compiler_options_t* options, cfg_t* cfg);

#endif /* INTSTRUCTION_SELECTOR_H */
