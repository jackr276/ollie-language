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

#define TRUE 1
#define FALSE 0

/**
 * We'll just have one big run through here
*/
int main(int argc, char** argv){
	//We bail out if this is the case
	if(argc < 2){
		printf("[STACK_DATA_AREA_TEST]: Fatal Error. An input file must be provided\n");
		exit(1);
	}

	//Open the file
	FILE* fl = fopen(argv[1], "r");

	//Ensure we worked here
	if(fl == NULL){
		printf("[STACK_DATA_AREA_TEST]: Fatal Error. File %s could not be found or opened\n", argv[1]);
		exit(1);
	}

	//Leverage the parser to do all of the heavy lifting
	front_end_results_package_t results = parse(fl, argv[1]);

	//Lookup our main function from here
	symtab_function_record_t* main_function = lookup_function(results.function_symtab, "main");

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
	for	(u_int16_t i = 0; i < results.variable_symtab->num_sheafs; i++){
		cursor = results.variable_symtab->sheafs[i];

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

	//Now let's run through and remove everything to test that
	


	//Ensure that we can fully deallocate
	stack_data_area_dealloc(&(main_function->data_area));
}
