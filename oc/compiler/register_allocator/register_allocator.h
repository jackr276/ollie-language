/**
 * Author: Jack Robbins
 *
 * This header file contains the APIs for the register allocator submodule
*/

//Include guards
#ifndef REGISTER_ALLOCATOR_H
#define REGISTER_ALLOCATOR_H

#include "../cfg/cfg.h"

/**
 * Perform our register allocation algorithm on the entire cfg. This is the only
 * API that is exposed to the rest of the compiler
 */
void allocate_all_registers(compiler_options_t* options, cfg_t* cfg);

#endif /* REGISTER_ALLOCATOR_H */
