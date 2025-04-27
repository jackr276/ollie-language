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
typedef struct instruction_t instruction_t;


/**
 * What type of instruction do we have? This saves us a lot of space
 * as opposed to storing strings
 */
typedef enum{
	MOVW,
	MOVL,
	MOVQ,
	JMP,
	JNE,
	JE,
	JNZ,
	JZ,
	JGE,
	JG,
	JLE,
	JL,
	ADDW,
	ADDL,
	ADDQ,
	SUBW,
	SUBL,
	SUBQ,
	
} intructions_type_t;


/**
 * Each individual instruction has an operand and
 * several variable areas
 */
struct instruction_t{
};


/**
 * Print out all instructions
 */
void print_instructions(dynamic_array_t* instructions);


/**
 * A function that selects all instructions, via the peephole method. This kind of 
 * operation completely translates the CFG out of a CFG. When done, we have a straight line
 * of code that we print out
 */
dynamic_array_t* select_all_instructions(cfg_t* cfg);

/**
 * Deallocate an instruction line
 */
void instructions_dealloc(instruction_t* line);

#endif /* INTSTRUCTION_SELECTOR_H */
