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
	//Initialize our type symtab
	type_symtab_t* type_symtab = type_symtab_alloc();

	//Initialize the given type scope
	initialize_type_scope(type_symtab);

	//Add all immutable and non-immutable version of our basic type
	//into the type system
	u_int16_t basic_type_collision_count = add_all_basic_types(type_symtab);

	//We can try asserting this - will see how it works out
	assert(basic_type_collision_count == 0);

	//Deinitialize it down here
	type_symtab_dealloc(type_symtab);
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
