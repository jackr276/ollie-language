/**
 * Simple testing suite for symtab
*/

#include "symtab.h"
#include <stdio.h>
#include <sys/types.h>

/**
 * Just run through some simple tests
*/
int main(){
	u_int16_t num_collisions = 0;

	//Level one identifiers
	char* idents_l1[] = {"x", "y", "main", "my_func", "fibonacci"};
	//Level two(inner scope) identifiers
	char* idents_l2[] = {"x", "y"};

	//Initialize the global symtab
	variable_symtab_t* symtab = initialize_variable_symtab();

	//We always initialize the scope
	initialize_variable_scope(symtab);

	for(u_int8_t i = 0; i < 5; i++){
		num_collisions += insert_variable(symtab, create_variable_record(idents_l1[i], STORAGE_CLASS_NORMAL));
	}

	printf("Collisions: %d\n", num_collisions);
	
	//make a new scope
	initialize_variable_scope(symtab);
	symtab_variable_record_t* found;

	for(u_int8_t i = 0; i < 2; i++){
		num_collisions += insert_variable(symtab, create_variable_record(idents_l2[i], STORAGE_CLASS_NORMAL));
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
	destroy_variable_symtab(symtab);
}
