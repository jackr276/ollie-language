/**
 * Author: Jack Robbins
 *
 * This file contains the implementation of the code generator APIs in the header file of the same name
*/

#include "code_generator.h"
#include <sys/types.h>

//For standardization across all modules
#define TRUE 1
#define FALSE 0

/**
 * Generate the assembly code for this file
*/
void generate_assembly_code(compiler_options_t* options, cfg_t* cfg){
	//Grab this out so we don't need to load every time
	u_int8_t print_irs = options->print_irs;

	//First we'll go through instruction selection
	if(print_irs == TRUE){
		printf("=============================== Instruction Selection ==================================\n");
	}
	
	//Run the instruction selector. This simplifies and selects instructions
	select_all_instructions(options, cfg);

	if(print_irs == TRUE){
		printf("=============================== Instruction Selection ==================================\n");
		printf("=============================== Register Allocation ====================================\n");
	}
	
	//Run the register allocator. This will take the OIR version and truly put it into assembler-ready code
	allocate_all_registers(options, cfg);

	if(print_irs == TRUE){
		printf("=============================== Register Allocation  ===================================\n");
	}
}
