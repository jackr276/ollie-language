/**
 * Author: Jack Robbins
 *
 * This file contains the implementation of the code generator APIs in the header file of the same name
*/

#include "code_generator.h"

/**
 * Generate the assembly code for this file
*/
void generate_assembly_code(cfg_t* cfg){
	//First we'll go through instruction selection
	printf("=============================== Instruction Selection ==================================\n");
	select_all_instructions(cfg);
	printf("=============================== Instruction Selection ==================================\n");

	printf("=============================== Register Allocation ====================================\n");
	allocate_all_registers(cfg);
	printf("=============================== Register Allocation  ===================================\n");

}
