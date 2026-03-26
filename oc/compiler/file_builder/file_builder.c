/**
 * Author: Jack Robbins
 *
 * This file contains the implementations for the assembler.h file
 */
#include <stdio.h>
#include <sys/types.h>
#include "file_builder.h"
#include "../utils/constants.h"
#include "../utils/dynamic_string/dynamic_string.h"

//The current tmp file id
static u_int32_t current_tmp_file_id = 0;

//For any/all error printing
static char error_info[2000];


/**
 * Helper that grabs the file id for us
 */
static inline u_int32_t increment_and_get_tmp_file_id(){
	return current_tmp_file_id++;
}


/**
 * Print an assembly block out
*/
static inline void print_assembly_block(FILE* fl, basic_block_t* block){
	//If this is some kind of switch block, we first print the jump table
	if(block->jump_table != NULL){
		print_jump_table(fl, block->jump_table);
	}

	//If it's a function entry block, we need to print this out
	if(block->block_type == BLOCK_TYPE_FUNC_ENTRY){
		//Now print the .text signifer so that GAS knows that this goes into .text
		fprintf(fl, "\t.text\n");

		//If this is a public function, we'll print out the ".globl" so
		//that it can be exposed to ld
		if(block->function_defined_in->signature->internal_types.function_type->is_public == TRUE){
			fprintf(fl, "\t.globl %s\n", block->function_defined_in->func_name.string);
		}

		//Now regardless of what kind of function it is, we'll use the @function tag
		//to tell AS that this is a function
		fprintf(fl, "\t.type %s, @function\n", block->function_defined_in->func_name.string);

		//Then the function name
		fprintf(fl, "%s:\n", block->function_defined_in->func_name.string);
	} else {
		fprintf(fl, ".L%d:\n", block->block_id);
	}

	//Now grab a cursor and print out every statement that we 
	//have
	instruction_t* cursor = block->leader_statement;

	//So long as it isn't null
	while(cursor != NULL){
		//We actually no longer need these
		if(cursor->instruction_type != PHI_FUNCTION){
			//Print a tab in the beginning for spacing
			fprintf(fl, "\t");
			print_instruction(fl, cursor, PRINTING_REGISTERS);
		}

		//Move along to the next one
		cursor = cursor->next_statement;
	}
}


/**
 * Print all assembly blocks in a CFG in order. Remember, by the time that we reach
 * here, these blocks will all already be in order from the block ordering procedure
 */
static void print_all_basic_blocks(FILE* fl, cfg_t* cfg){
	//Run through all blocks here
	for(u_int16_t i = 0; i < cfg->function_entry_blocks.current_index; i++){
		//Grab the head block out
		basic_block_t* current = dynamic_array_get_at(&(cfg->function_entry_blocks), i);

		//We can use the direct successor strategy here
		while(current != NULL){
			//Print it out
			print_assembly_block(fl, current);

			//Advance the pointer
			current = current->direct_successor;
		}
	}
}


/**
 * Print the .text section by running through and printing all of our basic blocks in assembly
 */
static inline void print_start_section(compiler_options_t* options, FILE* fl, cfg_t* cfg){
	//Declare the start of the new file to gas
	fprintf(fl, "\t.file\t\"%s\"\n", options->file_name);

	//Now that we've printed the text section, we need to print all basic blocks
	print_all_basic_blocks(fl, cfg);
}


/**
 * Assemble the program by writing it to a .s file
*/
u_int8_t output_generated_assembly(compiler_options_t* options, cfg_t* cfg, dynamic_string_t* assembly_output){
	//The output file(Null initally)
	FILE* output = NULL;

	/**
	 * If we are specifically requesting that we go to assembly,
	 * we will need to just write the file out to whatever the
	 * user wanted. If we are not, then we put the file inside
	 * of tmp/ocX, where X is the temp file name
	 */
	if(options->go_to_assembly == FALSE){
		char file_name[1000];
		sprintf(file_name, "/tmp/oc/ocAsm%d", increment_and_get_tmp_file_id());

		//Open the temp file here
		output = fopen(file_name, "w");

		//If the file is null, we fail out here
		if(output == NULL){
			sprintf(error_info, "[ERROR]: Could not open output file: %s\n", file_name);
			printf("%s", error_info);
			return 1;
		}

	} else {
		//Open the file for the purpose of writing
		output = fopen(options->output_file, "w");

		//If the file is null, we fail out here
		if(output == NULL){
			sprintf(error_info, "[ERROR]: Could not open output file: %s\n", options->output_file != NULL ? options->output_file : "out.s");
			printf("%s", error_info);
			return 1;
		}
	}

	//We'll first print the text segment of the program
	print_start_section(options, output, cfg);

	//Handle all of the global vars first
	print_all_global_variables(output, &(cfg->global_variables));

	//Print all of the local constants as well
	print_local_constants(output, &(cfg->local_string_constants), &(cfg->local_f32_constants), &(cfg->local_f64_constants), &(cfg->local_xmm128_constants));

	//Once we're done, close the file
	fclose(output);

	return 0;
}


/**
 * Take the generated assembly and convert it to an object file using GAS
 */
u_int8_t assemble_code(compiler_options_t* options){
	//TODO
	return FAILURE;

}
