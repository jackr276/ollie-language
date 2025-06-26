/**
 * Author: Jack Robbins
 *
 * This file contains the implementations for the assembler.h file
 */
#include <stdio.h>
#include "file_builder.h"

//For standardization across all modules
#define TRUE 1
#define FALSE 0


static void print_text_section(compiler_options_t* options, FILE* fl, cfg_t* cfg){
	//Declare the start of the new file to gas
	fprintf(fl, "\t.file\t\"%s\"\n", options->file_name);

	//Print the .text declaration part of the program
	fprintf(fl, "\t.text\n");

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
	print_text_section(options, output, cfg);

	//Once we're done, close the file
	fclose(output);

	return 0;
}

