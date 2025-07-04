/**
 * Author: Jack Robbins
 *
 * The assembler module will perform the final touches on the program and write it to a .s assembly file
 * Following this, it will invoke the GNU assembler to assemble it into an object file
*/

//Include guards
#ifndef FILE_BUILDER_H 
#define FILE_BUILDER_H
#include <sys/types.h>
#include "../cfg/cfg.h"

/**
 * Assemble the program by first writing it to a .s file, and then
 * assembling that file into an object file
*/
u_int8_t output_generated_code(compiler_options_t* options, cfg_t* cfg);

#endif /* FILE_BUILDER_H */
