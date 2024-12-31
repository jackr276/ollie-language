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
	symtab_t* global = initialize_global_symtab();

	//Keep a cursor here
	symtab_t* current = initialize_scope(global);
	
	for(u_int8_t i = 0; i < 5; i++){
		num_collisions += insert(current, create_record(idents_l1[i], current->lexical_level, 0));
	}

	printf("Collisions: %d\n", num_collisions);
	
	//make a new scope
	current = initialize_scope(current);
	symtab_record_t* found;

	for(u_int8_t i = 0; i < 2; i++){
		num_collisions += insert(current, create_record(idents_l2[i], current->lexical_level, 0));
	}


	for(u_int8_t i = 0; i < 2; i++){
		found = lookup(current, idents_l2[i]); 
		print_record(found);
	}

	found = lookup(current, "my_func"); 
	print_record(found);

	current = finalize_scope(current);

	for(u_int8_t i = 0; i < 2; i++){
		found = lookup(current, idents_l2[i]); 
		print_record(found);
	}
	
	found = lookup(current, "my_func"); 
	print_record(found);

	//Destroy it with the top reference
	destroy_symtab(global);
}
