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
		dynamic_string_t string = dynamic_string_alloc();
		dynamic_string_set(&string, idents_l1[i]);

		num_collisions += insert_variable(symtab, create_variable_record(string));
	}

	printf("Collisions: %d\n", num_collisions);
	
	//make a new scope
	initialize_variable_scope(symtab);
	symtab_variable_record_t* found;

	for(u_int8_t i = 0; i < 2; i++){
		dynamic_string_t string = dynamic_string_alloc();
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

	//We really should be seeing no collisions for basic type insertion
	assert(basic_type_collision_count == 0);

	//Print it out
	printf("Collisions for basic types: %d\n", basic_type_collision_count);

	//Let's try retrieving all of these now - both mutable and immutable versions
	//Immutable versions
	symtab_type_record_t* immutable_char = lookup_type_name_only(type_symtab, "char", NOT_MUTABLE);
	assert(immutable_char != NULL);

	symtab_type_record_t* immutable_u8 = lookup_type_name_only(type_symtab, "u8", NOT_MUTABLE);
	assert(immutable_u8 != NULL);

	symtab_type_record_t* immutable_i8 = lookup_type_name_only(type_symtab, "i8", NOT_MUTABLE);
	assert(immutable_i8 != NULL);

	symtab_type_record_t* immutable_u16 = lookup_type_name_only(type_symtab, "u16", NOT_MUTABLE);
	assert(immutable_u16 != NULL);

	symtab_type_record_t* immutable_i16 = lookup_type_name_only(type_symtab, "i16", NOT_MUTABLE);
	assert(immutable_i16 != NULL);

	symtab_type_record_t* immutable_u32 = lookup_type_name_only(type_symtab, "u32", NOT_MUTABLE);
	assert(immutable_u32 != NULL);

	symtab_type_record_t* immutable_i32 = lookup_type_name_only(type_symtab, "i32", NOT_MUTABLE);
	assert(immutable_i32 != NULL);

	symtab_type_record_t* immutable_u64 = lookup_type_name_only(type_symtab, "u64", NOT_MUTABLE);
	assert(immutable_u64 != NULL);

	symtab_type_record_t* immutable_i64 = lookup_type_name_only(type_symtab, "i64", NOT_MUTABLE);
	assert(immutable_i64 != NULL);

	//Now get all of our mutable versions
	symtab_type_record_t* mutable_char = lookup_type_name_only(type_symtab, "char", MUTABLE);
	assert(mutable_char != NULL);

	symtab_type_record_t* mutable_u8 = lookup_type_name_only(type_symtab, "u8", MUTABLE);
	assert(mutable_u8 != NULL);

	symtab_type_record_t* mutable_i8 = lookup_type_name_only(type_symtab, "i8", MUTABLE);
	assert(mutable_i8 != NULL);

	symtab_type_record_t* mutable_u16 = lookup_type_name_only(type_symtab, "u16", MUTABLE);
	assert(mutable_u16 != NULL);

	symtab_type_record_t* mutable_i16 = lookup_type_name_only(type_symtab, "i16", MUTABLE);
	assert(mutable_i16 != NULL);

	symtab_type_record_t* mutable_u32 = lookup_type_name_only(type_symtab, "u32", MUTABLE);
	assert(mutable_u32 != NULL);

	symtab_type_record_t* mutable_i32 = lookup_type_name_only(type_symtab, "i32", MUTABLE);
	assert(mutable_i32 != NULL);

	symtab_type_record_t* mutable_u64 = lookup_type_name_only(type_symtab, "u64", MUTABLE);
	assert(mutable_u64 != NULL);

	symtab_type_record_t* mutable_i64 = lookup_type_name_only(type_symtab, "i64", MUTABLE);
	assert(mutable_i64 != NULL);

	//These all need to be *distinct*, so assert that they are not equal
	assert(mutable_i8 != immutable_i8);
	assert(mutable_u8 != immutable_u8);
	assert(mutable_i16 != immutable_i16);
	assert(mutable_u16 != immutable_u16);
	assert(mutable_i32 != immutable_i32);
	assert(mutable_u32 != immutable_u32);
	assert(mutable_i64 != immutable_i64);
	assert(mutable_u64 != immutable_u64);
	assert(mutable_char != immutable_char);

	//Grab these more complex ones while we're at it
	symtab_type_record_t* mutable_char_ptr = lookup_type_name_only(type_symtab, "char*", MUTABLE);
	assert(mutable_char_ptr != NULL);

	symtab_type_record_t* immutable_char_ptr = lookup_type_name_only(type_symtab, "char*", NOT_MUTABLE);
	assert(immutable_char_ptr != NULL);

	//Finalize the type scope
	finalize_type_scope(type_symtab);

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
