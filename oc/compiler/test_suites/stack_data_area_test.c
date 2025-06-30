/**
 * Author: Jack Robbins
 *
 * This test performs a complete run of the stack data area
*/

//The symtab has what we need roped in
#include "../symtab/symtab.h"
//We'll also need three address vars
#include "../instruction/instruction.h"
#include "../parser/parser.h"
#include "../dynamic_array/dynamic_array.h"
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

//For standardization across all modules
#define TRUE 1
#define FALSE 0


/**
 * We'll use this helper function to process the compiler flags and return a structure that
 * tells us what we need to do throughout the compiler
 */
static compiler_options_t* parse_and_store_options(int argc, char** argv){
	//Allocate it
	compiler_options_t* options = calloc(1, sizeof(compiler_options_t));
	
	//For storing our opt
	int opt;

	//Run through all of our options
	while((opt = getopt(argc, argv, "iatdhsf:o:?")) != -1){
		//Switch based on opt
		switch(opt){
			//Invalid option
			case '?':
				printf("Invalid option: %c\n", optopt);
				exit(0);
			//After we print help we exit
			case 'h':
				exit(0);
			//Specify that we want to print intermediate representations
			case 'i':
				options->print_irs = TRUE;
				break;
			//Time execution for performance test
			case 't':
				options->time_execution = TRUE;
				break;
			//Store the input file name
			case 'f':
				options->file_name = optarg;
				break;
			//Turn on debug printing
			case 'd':
				options->enable_debug_printing = TRUE;
				break;
			//Output to assembly only
			case 'a':
				options->go_to_assembly = TRUE;
				break;
			//Specify that we want a summary to be shown
			case 's':
				options->show_summary = TRUE;
				break;
			//Specific output file
			case 'o':
				options->output_file = optarg;
				break;
		}
	}

	//This is an error, so we'll fail out here
	if(options->file_name == NULL){
		printf("[COMPILER ERROR]: No input file name provided. Use -f <filename> to specify a .ol source file\n");
		exit(1);
	}

	//Give back the options we got in the structure
	return options;
}




/**
 * We'll just have one big run through here
*/
int main(int argc, char** argv){
	//Grab the compiler options
	compiler_options_t* options = parse_and_store_options(argc, argv);

	//Leverage the parser to do all of the heavy lifting
	front_end_results_package_t* results = parse(options);

	//Lookup our main function from here
	symtab_function_record_t* main_function = lookup_function(results->function_symtab, "main");

	//Sample blank print. Should say blank
	print_stack_data_area(&(main_function->data_area));

	//Now let's go through and start adding things into the stack. This is quick and dirty, we're
	//just trying to test what's going on here
	
	//Run through the entire variable symtab and add what would be immediately eligible(arrays, constructs)
	symtab_variable_sheaf_t* cursor;
	symtab_variable_record_t* record;
	symtab_variable_record_t* temp;

	//Create a dynamic array to hold all of the vars we make
	dynamic_array_t* array_of_vars = dynamic_array_alloc();

	//Run through all of the sheafs
	for	(u_int16_t i = 0; i < results->variable_symtab->sheafs->current_index; i++){
		cursor = dynamic_array_get_at(results->variable_symtab->sheafs, i);

		//Look for anything in the records that is an array
		for(u_int16_t j = 0; j < KEYSPACE; j++){
			record = cursor->records[j];

			//We could have chaining here, so run through just in case
			while(record != NULL){
					//Emit the variable
					three_addr_var_t* var = emit_var(record, FALSE);
				
					//Store for later
					dynamic_array_add(array_of_vars, var);

					//Add it into the stack
					add_variable_to_stack(&(main_function->data_area), var);

					//Let's print it out to see what we have
					print_stack_data_area(&(main_function->data_area));

				temp = record;
				record = record->next;
			}
		}
	}

	//Perform the alignment
	align_stack_data_area(&(main_function->data_area));
	printf("Total size: %d\n", main_function->data_area.total_size);

	printf("###################### Now testing removal ####################\n");

	//Now let's run through and remove everything to test that
	for(u_int16_t i = 0; i < array_of_vars->current_index; i++){
		//Delete it
		remove_variable_from_stack(&(main_function->data_area), dynamic_array_get_at(array_of_vars, i));
		//Reprint the whole thing
		print_stack_data_area(&(main_function->data_area));
	}

	//We can scrap the dynamic array once here
	dynamic_array_dealloc(array_of_vars);

	//Ensure that we can fully deallocate
	stack_data_area_dealloc(&(main_function->data_area));
}
