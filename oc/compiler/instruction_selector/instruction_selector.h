/**
 * Author: Jack Robbins
 *
 * This header file contains the APIs for the Ollie Instruction Selector
*/

#ifndef INSTRUCTION_SELECTOR_H
#define INSTRUCTION_SELECTOR_H

#include "../dynamic_array/dynamic_array.h"
#include "../cfg/cfg.h"


/**
 * Define a back end "generated instructions" segment
 * that consists of one large, straight line of
 * OIR statements. 
 */
typedef struct instructions_t instructions_t;

struct instructions_t{
	dynamic_array_t* statements;
	//TODO will add more
};


/**
 * Print out all instructions
 */
void print_instructions(instructions_t* instructions);


/**
 * A function that selects all instructions, via the peephole method. This kind of 
 * operation completely translates the CFG out of a CFG. When done, we have a straight line
 * of code that we print out
 */
instructions_t* select_all_instructions(cfg_t* cfg);

/**
 * Deallocate an instruction line
 */
void instructions_dealloc(instructions_t* line);

#endif /* INTSTRUCTION_SELECTOR_H */
