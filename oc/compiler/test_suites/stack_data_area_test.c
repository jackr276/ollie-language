/**
 * Author: Jack Robbins
 *
 * This test performs a complete run of the stack data area
*/

//The symtab has what we need roped in
#include "../symtab/symtab.h"
//We'll also need three address vars
#include "../instruction/instruction.h"


/**
 * We'll just have one big run through here
*/
int main(){
	//Create the dummy function record. This is where our data area will be
	symtab_function_record_t* dummy_function = create_function_record("DUMMY", STORAGE_CLASS_NORMAL);

	//Sample blank print
	print_stack_data_area(&(dummy_function->data_area));



	//Ensure that we can fully deallocate
	stack_data_area_dealloc(&(dummy_function->data_area));
}
