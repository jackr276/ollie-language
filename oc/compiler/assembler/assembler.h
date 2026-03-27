/**
 * Author: Jack Robbins
 *
 * The assembler module will perform the final touches on the program and write it to a .s assembly file
 * Following this, it will invoke the GNU assembler to assemble it into an object file
*/

//Include guards
#ifndef ASSEMBLER_H 
#define ASSEMBLER_H 
#include <sys/types.h>
#include "../cfg/cfg.h"

/**
 * Perform all of the assembly and linkage that we need to do here
 */
void assemble_and_link(compiler_options_t* options, cfg_t* cfg, u_int32_t* num_errors, u_int32_t* num_warnings);

#endif /* ASSEMBLER_H */
