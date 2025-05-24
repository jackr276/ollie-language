/**
 * Author: Jack Robbins
 *
 * This header file defines the APIs for the Ollie compiler's back-end, the code generator
*/

#ifndef CODE_GEN_H
#define CODE_GEN_H
//Link to the instruction selector. This is the first part
//of codegen
#include "../instruction_selector/instruction_selector.h"
#include "../register_allocator/register_allocator.h"

/**
 * Generate the assembly code for this file
*/
void generate_assembly_code(cfg_t* cfg);


#endif
