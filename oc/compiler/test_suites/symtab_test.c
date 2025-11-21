/**
 * Simple testing suite for symtab
*/

//Link to the symtab
#include "../symtab/symtab.h"
#include <assert.h>
#include <stdio.h>
#include <sys/types.h>

/**
 * Run through and test the variable subsystem first
 */
static void test_variables(){
	u_int16_t num_collisions = 0;

	//Level one identifiers
	char* idents_l1[] = {"x", "y", "main", "my_func", "fibonacci"};
	//Level two(inner scope) identifiers
	char* idents_l2[] = {"x", "y"};

	//Initialize the global symtab
	variable_symtab_t* symtab = variable_symtab_alloc();

	//We always initialize the scope
	initialize_variable_scope(symtab);

	for(u_int8_t i = 0; i < 5; i++){
		dynamic_string_t string;
		dynamic_string_alloc(&string);
		dynamic_string_set(&string, idents_l1[i]);

		num_collisions += insert_variable(symtab, create_variable_record(string));
	}

	printf("Collisions: %d\n", num_collisions);
	
	//make a new scope
	initialize_variable_scope(symtab);
	symtab_variable_record_t* found;

	for(u_int8_t i = 0; i < 2; i++){
		dynamic_string_t string;
		dynamic_string_alloc(&string);
		dynamic_string_set(&string, idents_l2[i]);

		num_collisions += insert_variable(symtab, create_variable_record(string));
	}


	for(u_int8_t i = 0; i < 2; i++){
		found = lookup_variable(symtab, idents_l2[i]); 
		print_variable_record(found);
	}

	found = lookup_variable(symtab, "my_func"); 
	print_variable_record(found);

	finalize_variable_scope(symtab);

	for(u_int8_t i = 0; i < 2; i++){
		found = lookup_variable(symtab, idents_l2[i]); 
		print_variable_record(found);
	}
	
	found = lookup_variable(symtab, "my_func"); 
	print_variable_record(found);

	//Destroy it with the top reference
	variable_symtab_dealloc(symtab);
}


/**
 * Test our ability to handle types and type lookups
 * This is especially important for mutable and immutable types
 */
static void test_types(){
	//TODO IMPLEMENTME
}


/**
 * Just run through some simple tests
*/
int main(){
	//Run the variable test first
	test_variables();

	//Now test our type system
	test_types();
}
