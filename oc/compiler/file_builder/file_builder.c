/**
 * Author: Jack Robbins
 *
 * This file contains the implementations for the assembler.h file
 */
#include <stdio.h>
#include "file_builder.h"
#include "../utils/constants.h"

/**
 * Print an assembly block out
*/
static void print_assembly_block(FILE* fl, basic_block_t* block){
	//If this is some kind of switch block, we first print the jump table
	if(block->jump_table != NULL){
		print_jump_table(fl, block->jump_table);
	}

	//If it's a function entry block, we need to print this out
	if(block->block_type == BLOCK_TYPE_FUNC_ENTRY){
		//First we print out the local constants that the block has
		print_local_constants(fl, block->function_defined_in);

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
static void print_start_section(compiler_options_t* options, FILE* fl, cfg_t* cfg){
	//Declare the start of the new file to gas
	fprintf(fl, "\t.file\t\"%s\"\n", options->file_name);

	//Now that we've printed the text section, we need to print all basic blocks
	print_all_basic_blocks(fl, cfg);
}


/**
 * Assemble the program by first writing it to a .s file, and then
 * assembling that file into an object file
*/
u_int8_t output_generated_code(compiler_options_t* options, cfg_t* cfg){
	//If we have no output file given, we will use the default name
	
	//The output file(Null initally)
	FILE* output = NULL;

	//If the output file is NULL, we'll use "out.s"
	if(options->output_file != NULL){
		//Open the file for the purpose of writing
		output = fopen(options->output_file, "w");
	} else {
		//Open the default file
		output = fopen("out.s", "w");
	}

	//If the file is null, we fail out here
	if(output == NULL){
		char error_info[2000];
		sprintf(error_info, "[ERROR]: Could not open output file: %s\n", options->output_file != NULL ? options->output_file : "out.s");
		printf("%s", error_info);
		//1 means we failed
		return 1;
	}

	//We'll first print the text segment of the program
	print_start_section(options, output, cfg);

	//Handle all of the global vars first
	print_all_global_variables(output, &(cfg->global_variables));

	//Once we're done, close the file
	fclose(output);

	return 0;
}

