/**
 * Author: Jack Robbins
 *
 * The implementation of the symbol table. All hashing is done via the FNV-1a algorithm
*/

#include "symtab.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "../ast/ast.h"
//For error printing
#include "../utils/queue/min_priority_queue.h"
#include "../utils/constants.h"

//The starting offset basis for FNV-1a64
#define OFFSET_BASIS 14695981039346656037ULL

//The FNV prime for 64 bit hashes
#define FNV_PRIME 1099511628211ULL

//The finalizer constants for the avalance finalizer
#define FINALIZER_CONSTANT_1 0xff51afd7ed558ccdULL
#define FINALIZER_CONSTANT_2 0xc4ceb9fe1a85ec53ULL

/**
 * Print a generic warning for the symtab system
 */
#define PRINT_WARNING(info, line_number) \
	fprintf(stdout, "\n[LINE %d: COMPILER WARNING]: %s\n", line_number, info)


//Define a list of salts that can be used for mutable types
static const u_int64_t mutability_salts[] = {
	0xA3B1956359A1F3D1ULL,
	0xC9E3779B97F4A7C1ULL,
	0x123456789ABCDEF0ULL,
	0xF0E1D2C3B4A59687ULL,
    0x0FEDCBA987654321ULL,
    0x9E3779B97F4A7C15ULL,
    0x6A09E667F3BCC908ULL,
    0xBB67AE8584CAA73BULL,
    0x3C6EF372FE94F82BULL,
    0xA54FF53A5F1D36F1ULL,
    0x510E527FADE682D1ULL,
    0x9B05688C2B3E6C1FULL,
    0x1F83D9ABFB41BD6BULL,
    0x5BE0CD19137E2179ULL,
    0x8F1BBCDC68C4CFAFULL,
    0xCBB41EF6F7F651C1ULL
};


/**
 * Dynamically allocate a function symtab
 */
function_symtab_t* function_symtab_alloc(){
	function_symtab_t* symtab = (function_symtab_t*)calloc(1, sizeof(function_symtab_t));
	//The function symtab's lexical scope is always global
	symtab->current_lexical_scope = 0;

	return symtab;
}


/**
 * Dynamically allocate a variable symtab
 */
variable_symtab_t* variable_symtab_alloc(){
	variable_symtab_t* symtab = (variable_symtab_t*)calloc(1, sizeof(variable_symtab_t));
	//We also need to allocate the sheafs array
	symtab->sheafs = dynamic_array_alloc();

	symtab->current_lexical_scope = 0;
	//Nothing has been initialized yet
	symtab->current = NULL;

	return symtab;
}


/**
 * Dynamically allocate a type symtab
 */
type_symtab_t* type_symtab_alloc(){
	type_symtab_t* symtab = (type_symtab_t*)calloc(1, sizeof(type_symtab_t));
	//We also need to allocate the sheafs array
	symtab->sheafs = dynamic_array_alloc();

	symtab->current_lexical_scope = 0;
	//Nothing has been initialized yet
	symtab->current = NULL;

	return symtab;
}


/**
 * Initialize a symbol table for compiler macros 
 */
macro_symtab_t* macro_symtab_alloc(){
	macro_symtab_t* symtab = calloc(1, sizeof(macro_symtab_t));
	return symtab;
}


/**
 * Initialize a new lexical scope. This involves making a new sheaf and
 * adding it in
*/
void initialize_variable_scope(variable_symtab_t* symtab){
	//Allocate the current sheaf
	symtab_variable_sheaf_t* current = (symtab_variable_sheaf_t*)calloc(1, sizeof(symtab_variable_sheaf_t));

	//Add it to the array
	dynamic_array_add(&(symtab->sheafs), current);
	
	//Increment(down the chain)
	symtab->current_lexical_scope++;

	//Store this here
	current->lexical_level = symtab->current_lexical_scope;

	//Now we'll link back to the previous one level
	current->previous_level = symtab->current;
	
	//Set this so it's up-to-date
	symtab->current = current;
}


/**
 * Initialize a new lexical scope. This involves making a new sheaf and
 * adding it in
*/
void initialize_type_scope(type_symtab_t* symtab){
	symtab_type_sheaf_t* current = (symtab_type_sheaf_t*)calloc(1, sizeof(symtab_type_sheaf_t));

	//Add this into the dynamic array
	dynamic_array_add(&(symtab->sheafs), current);

	//Increment(down the chain)
	symtab->current_lexical_scope++;

	//Store this here
	current->lexical_level = symtab->current_lexical_scope;

	//Now we'll link back to the previous one level
	current->previous_level = symtab->current;
	
	//Set this so it's up-to-date
	symtab->current = current;
}


/**
 * Finalize the scope, for the purposes of this project, finalizing the scope just means going
 * up by one level
 */
void finalize_variable_scope(variable_symtab_t* symtab){
	//Back out of this one as it's finalized
	symtab->current = symtab->current->previous_level;

	//Go back up one
	symtab->current_lexical_scope--;
}


/**
 * Finalize the scope, for the purposes of this project, finalizing the scope just means going
 * up by one level
 */
void finalize_type_scope(type_symtab_t* symtab){
	//Back out of this one as it's finalized
	symtab->current = symtab->current->previous_level;

	//Go back up one
	symtab->current_lexical_scope--;
}


/**
 * Hash a name before entry/search into the hash table
 *
 * FNV-1a 64 bit hash:
 * 	hash <- FNV_prime
 *
 * 	for each hashable value:
 * 		hash ^= value
 * 		hash *= FNV_PRIME
 * 		
 * 	key % keyspace
 *
 * 	return key
*/
static inline u_int64_t hash_variable(char* name){
	//Char pointer for the name
	char* cursor = name;

	//The hash we have
	u_int64_t hash = OFFSET_BASIS;

	//Iterate through the cursor here
	for(; *cursor != '\0'; cursor++){
		hash ^= *cursor;
		hash *= FNV_PRIME;
	}

	//We will perform avalanching here by shifting, multiplying and shifting. The shifting
	//itself ensures that the higher order bits effect all of the lower order ones
	hash ^= hash >> 33;
	hash *= FINALIZER_CONSTANT_1;
	hash ^= hash >> 33;
	hash *= FINALIZER_CONSTANT_2;
	hash ^= hash >> 33;

	//Cut it down to our keyspace
	return hash & (VARIABLE_KEYSPACE - 1);
}


/**
 * Hash a name before entry/search into the hash table
 *
 * FNV-1a 64 bit hash:
 * 	hash <- FNV_prime
 *
 * 	for each hashable value:
 * 		hash ^= value
 * 		hash *= FNV_PRIME
 * 		
 * 	key % keyspace
 *
 * 	return key
*/
static inline u_int64_t hash_macro_name(char* name){
	//Char pointer for the name
	char* cursor = name;

	//The hash we have
	u_int64_t hash = OFFSET_BASIS;

	//Iterate through the cursor here
	for(; *cursor != '\0'; cursor++){
		hash ^= *cursor;
		hash *= FNV_PRIME;
	}

	//We will perform avalanching here by shifting, multiplying and shifting. The shifting
	//itself ensures that the higher order bits effect all of the lower order ones
	hash ^= hash >> 33;
	hash *= FINALIZER_CONSTANT_1;
	hash ^= hash >> 33;
	hash *= FINALIZER_CONSTANT_2;
	hash ^= hash >> 33;

	//Cut it down to our keyspace
	return hash & (MACRO_KEYSPACE - 1);
}


/**
 * Hash a name before entry/search into the hash table
 *
 * FNV-1a 64 bit hash:
 * 	hash <- FNV_prime
 *
 * 	for each hashable value:
 * 		hash ^= value
 * 		hash *= FNV_PRIME
 * 		
 * 	key % keyspace
 *
 * 	return key
*/
static inline u_int64_t hash_function(char* name){
	//Char pointer for the name
	char* cursor = name;

	//The hash we have
	u_int64_t hash = OFFSET_BASIS;

	//Iterate through the cursor here
	for(; *cursor != '\0'; cursor++){
		hash ^= *cursor;
		hash *= FNV_PRIME;
	}

	//We will perform avalanching here by shifting, multiplying and shifting. The shifting
	//itself ensures that the higher order bits effect all of the lower order ones
	hash ^= hash >> 33;
	hash *= FINALIZER_CONSTANT_1;
	hash ^= hash >> 33;
	hash *= FINALIZER_CONSTANT_2;
	hash ^= hash >> 33;

	//Cut it down to our keyspace
	return hash & (FUNCTION_KEYSPACE - 1);
}


/**
 * A helper function that will hash the name of a type
 *
 * FNV-1a 64 bit hash:
 * 	hash <- FNV_prime
 *
 * 	for each hashable value:
 * 		hash ^= value
 * 		hash *= FNV_PRIME
 * 		
 * 	key % keyspace
 *
 * 	return key
 */
static inline u_int64_t hash_type_name(char* type_name, mutability_type_t mutability){
	//Char pointer for the name
	char* cursor = type_name;

	//The hash we have
	u_int64_t hash = OFFSET_BASIS;

	//Iterate through the cursor here
	for(; *cursor != '\0'; cursor++){
		hash ^= *cursor;
		hash *= FNV_PRIME;
	}

	//If this is mutable, we will keep going by adding
	//a duplicated version of the type's first character
	//onto the hash. This should(in most cases) make the hash
	//entirely different from the non-mutable version
	if(mutability == MUTABLE){
		//To make the hashes different, we will pick from one of
		//3 salts based on the first character in the type name
		hash ^= mutability_salts[*type_name % 16];
		hash *= FNV_PRIME;
	}

	//We will perform avalanching here by shifting, multiplying and shifting. The shifting
	//itself ensures that the higher order bits effect all of the lower order ones
	hash ^= hash >> 33;
	hash *= FINALIZER_CONSTANT_1;
	hash ^= hash >> 33;
	hash *= FINALIZER_CONSTANT_2;
	hash ^= hash >> 33;

	//Cut it down to our keyspace
	return hash & (TYPE_KEYSPACE - 1);
}


/**
 * A helper function that will hash the name of an array type
 *
 * FNV-1a 64 bit hash:
 * 	hash <- FNV_prime
 *
 * 	for each hashable value:
 * 		hash ^= value
 * 		hash *= FNV_PRIME
 * 		
 * 	key % keyspace
 *
 * 	return key
 */
static inline u_int64_t hash_array_type_name(char* type_name, u_int32_t num_members, mutability_type_t mutability){
	//Char pointer for the name
	char* cursor = type_name;

	//The hash we have
	u_int64_t hash = OFFSET_BASIS;

	//Iterate through the cursor here
	for(; *cursor != '\0'; cursor++){
		hash ^= *cursor;
		hash *= FNV_PRIME;
	}

	//This is an array, we'll add the bounds in to further
	//stop collisions
	hash ^= num_members;
	hash *= FNV_PRIME;

	//If this is mutable, we will keep going by adding
	//a duplicated version of the type's first character
	//onto the hash. This should(in most cases) make the hash
	//entirely different from the non-mutable version
	if(mutability == MUTABLE){
		//To make the hashes different, we will pick from one of
		//3 salts based on the first character in the type name
		hash ^= mutability_salts[*type_name % 16];
		hash *= FNV_PRIME;
	}

	//We will perform avalanching here by shifting, multiplying and shifting. The shifting
	//itself ensures that the higher order bits effect all of the lower order ones
	hash ^= hash >> 33;
	hash *= FINALIZER_CONSTANT_1;
	hash ^= hash >> 33;
	hash *= FINALIZER_CONSTANT_2;
	hash ^= hash >> 33;

	//Cut it down to our keyspace
	return hash & (TYPE_KEYSPACE - 1);
}


/**
 * For arrays, type hashing will include their values
 *
 * For *mutable types*, the type hasher concatenates a
 * "`" onto the end to make the hash *different* from
 * the non-mutable version. This should allow for a faster lookup
 *
 * FNV-1a 64 bit hash:
 * 	hash <- FNV_prime
 *
 * 	for each hashable value:
 * 		hash ^= value
 * 		hash *= FNV_PRIME
 * 		
 * 	key % keyspace
 *
 * 	return key
*/
static inline u_int64_t hash_type(generic_type_t* type){
	//Pointer to the type name
	char* type_name = type->type_name.string;

	//Char pointer for the name that will change
	char* cursor = type_name;

	//The hash we have
	u_int64_t hash = OFFSET_BASIS;

	//Iterate through the cursor here
	for(; *cursor != '\0'; cursor++){
		hash ^= *cursor;
		hash *= FNV_PRIME;
	}

	//If this is an array, we'll add the bounds in
	if(type->type_class == TYPE_CLASS_ARRAY){
		hash ^= type->internal_values.num_members;
		hash *= FNV_PRIME;
	}

	//If this is mutable, we will keep going by adding
	//a duplicated version of the type's first character
	//onto the hash. This should(in most cases) make the hash
	//entirely different from the non-mutable version
	if(type->mutability == MUTABLE){
		//To make the hashes different, we will pick from one of
		//3 salts based on the first character in the type name
		hash ^= mutability_salts[*type_name % 16];
		hash *= FNV_PRIME;
	}

	//We will perform avalanching here by shifting, multiplying and shifting. The shifting
	//itself ensures that the higher order bits effect all of the lower order ones
	hash ^= hash >> 33;
	hash *= FINALIZER_CONSTANT_1;
	hash ^= hash >> 33;
	hash *= FINALIZER_CONSTANT_2;
	hash ^= hash >> 33;

	//Cut it down to our keyspace
	return hash & (TYPE_KEYSPACE - 1);
}


/**
 * Dynamically allocate a variable record
*/
symtab_variable_record_t* create_variable_record(dynamic_string_t name){
	//Allocate it
	symtab_variable_record_t* record = (symtab_variable_record_t*)calloc(1, sizeof(symtab_variable_record_t));

	//Store the name
	record->var_name = name;
	//Hash it and store it to avoid to repeated hashing
	record->hash = hash_variable(name.string);
	//The current generation is always 1 at first
	record->current_generation = 1;

	//For eventual SSA generation
	record->counter_stack.stack = NULL;
	record->counter_stack.top_index = 0;
	record->counter_stack.current_size = 0;

	return record;
}


/**
 * Create a variable for a memory address that is not from an actual var
 */
symtab_variable_record_t* create_temp_memory_address_variable(generic_type_t* type, variable_symtab_t* variable_symtab, stack_region_t* stack_region, u_int32_t temp_id){
	//And here is the special part - we'll need to make a symtab record
	//for this variable and add it in
	char variable_name[100];
	//Grab a new temp var number from here. We use the
	//^ because it is illegal for variables typed in by the
	//user to have that, so we will not have collisions
	sprintf(variable_name, "^t%d", temp_id);

	//Create and set the name here
	dynamic_string_t string = dynamic_string_alloc();
	dynamic_string_set(&string, variable_name);

	//Now create and add the symtab record for this variable
	symtab_variable_record_t* record = create_variable_record(string);
	//Store the type here
	record->type_defined_as = type;

	//Store the stack region too
	record->stack_region = stack_region;
	
	//Insert this into the variable symtab
	insert_variable(variable_symtab, record);

	//The current generation is always 1 at first
	record->current_generation = 1;

	//For eventual SSA generation
	record->counter_stack.stack = NULL;
	record->counter_stack.top_index = 0;
	record->counter_stack.current_size = 0;

	//And give it back
	return record;
}


/**
 * Create and return a ternary variable. A ternary variable is halfway
 * between a temp and a full fledged non-temp variable. It will have a 
 * symtab record, and as such will be picked up by the phi function
 * inserted. It will also not be declared as temp
 */
symtab_variable_record_t* create_ternary_variable(generic_type_t* type, variable_symtab_t* variable_symtab, u_int32_t temp_id){
	//And here is the special part - we'll need to make a symtab record
	//for this variable and add it in
	char variable_name[100];
	//Grab a new temp var number from here. We use the
	//^ because it is illegal for variables typed in by the
	//user to have that, so we will not have collisions
	sprintf(variable_name, "^t%d", temp_id);

	//Create and set the name here
	dynamic_string_t string = dynamic_string_alloc();
	dynamic_string_set(&string, variable_name);

	//Now create and add the symtab record for this variable
	symtab_variable_record_t* record = create_variable_record(string);
	//Store the type here
	record->type_defined_as = type;

	//Insert this into the variable symtab
	insert_variable(variable_symtab, record);

	//The current generation is always 1 at first
	record->current_generation = 1;

	//For eventual SSA generation
	record->counter_stack.stack = NULL;
	record->counter_stack.top_index = 0;
	record->counter_stack.current_size = 0;

	//And give it back
	return record;
}


/**
 * Create and return a function parameter alis variable. A parameter alias variable is halfway
 * between a temp and a full fledged non-temp variable. It will have a 
 * symtab record, and as such will be picked up by the phi function
 * inserted. It will also not be declared as temp
 */
symtab_variable_record_t* create_parameter_alias_variable(symtab_variable_record_t* aliases, variable_symtab_t* variable_symtab, u_int32_t temp_id){
	//And here is the special part - we'll need to make a symtab record
	//for this variable and add it in
	char variable_name[100];
	//Grab a new temp var number from here. We use the
	//^ because it is illegal for variables typed in by the
	//user to have that, so we will not have collisions
	sprintf(variable_name, "^t%d", temp_id);

	//Create and set the name here
	dynamic_string_t string = dynamic_string_alloc();
	dynamic_string_set(&string, variable_name);

	//Now create and add the symtab record for this variable
	symtab_variable_record_t* record = create_variable_record(string);
	//Store the type here
	record->type_defined_as = aliases->type_defined_as;

	//Copy over the stack info as well - this is important for references
	record->stack_region = aliases->stack_region;
	record->stack_variable = aliases->stack_variable;

	//This is still a function parameter at heart
	record->membership = FUNCTION_PARAMETER;

	//Insert this into the variable symtab
	insert_variable(variable_symtab, record);

	//The current generation is always 1 at first
	record->current_generation = 1;

	//For eventual SSA generation
	record->counter_stack.stack = NULL;
	record->counter_stack.top_index = 0;
	record->counter_stack.current_size = 0;

	//And give it back
	return record;
}


/**
 * Add a parameter to a function and perform all internal bookkeeping needed
 *
 * *Stack Parameters*
 * Every function internally maintains a stack structure *separate* from the local stack that is used for
 * passing function parameters via the stack. If we notice that we are adding a function parameter that
 * is more than the max per class register passing value, we will add that into the specialized stack
 * data area
 */
void add_function_parameter(symtab_function_record_t* function_record, symtab_variable_record_t* variable_record){
	//Store it in the function's parameters
	dynamic_array_add(&(function_record->function_parameters), variable_record);
	
	//Store what function this came from
	variable_record->function_declared_in = function_record;

	//Do we need to pass via stack? If so add it here
	if(variable_record->class_relative_function_parameter_order > MAX_PER_CLASS_REGISTER_PASSED_PARAMS){
		//Allocate it if need be
		if(function_record->stack_passed_parameters.stack_regions.internal_array == NULL){
			//This is specifically a parameter passing stack region. We must be sure to mention that
			stack_data_area_alloc(&(function_record->stack_passed_parameters), STACK_TYPE_PARAMETER_PASSING);
		}

		//Add this type into said stack region
		variable_record->stack_region = create_stack_region_for_type(&(function_record->stack_passed_parameters), variable_record->type_defined_as);

		//This is a stack variable, we need to note it as such
		variable_record->stack_variable = TRUE;

		//Flag that this is passed via the stack
		variable_record->passed_by_stack = TRUE;

		//Flag that this function contains stack params
		function_record->contains_stack_params = TRUE;
	}
}


/**
 * Dynamically allocate a function record
*/
symtab_function_record_t* create_function_record(dynamic_string_t name, u_int8_t is_public, u_int8_t is_inlined, u_int32_t line_number){
	//Allocate it
	symtab_function_record_t* record = calloc(1, sizeof(symtab_function_record_t));

	//Allocate the data area internally
	stack_data_area_alloc(&(record->local_stack), STACK_TYPE_FUNCTION_LOCAL);

	//Allocate the array for all function blocks
	record->function_blocks = dynamic_array_alloc();

	//Allocate space for the function parameter
	record->function_parameters = dynamic_array_alloc();

	//Copy the name over
	record->func_name = name;
	//Hash it and store it to avoid to repeated hashing
	record->hash = hash_function(name.string);

	//Throw in whether or not it's public or private
	record->function_visibility = is_public == TRUE ? FUNCTION_VISIBILITY_PUBLIC : FUNCTION_VISIBILITY_PRIVATE;

	//Store the line number
	record->line_number = line_number;

	//Allocate the list of all functions that this calls
	record->called_functions = dynamic_set_alloc();

	//Store the inline status
	record->inlined = is_inlined;

	//We know that we need to create this immediately
	record->signature = create_function_pointer_type(is_public, is_inlined, line_number, NOT_MUTABLE);

	//And give it back
	return record;
}


/**
 * Dynamically allocate and create a type record
 *
 * The hash_type function automatically allows us to distinguish between
 * mutable and immutable values
 */
symtab_type_record_t* create_type_record(generic_type_t* type){
	//Allocate it
	symtab_type_record_t* record = calloc(1, sizeof(symtab_type_record_t));

	//Hash the type name and store it
	record->hash = hash_type(type);
	//Assign the type
	record->type = type;

	return record;
}


/**
 * Create a macro record for the macro table
 */
symtab_macro_record_t* create_macro_record(dynamic_string_t name, u_int32_t line_number){
	//Allocate the space needed for the record
	symtab_macro_record_t* record = calloc(1, sizeof(symtab_macro_record_t));

	//Get & store the hash here
	record->hash = hash_macro_name(name.string);

	//Allocate the token array here as well
	record->tokens = token_array_alloc();

	//We don't know if this will or will not be needed yet - so give it a blank allocation for now
	record->parameters = initialize_blank_token_array(); 

	//Store the line number where this was defined
	record->line_number = line_number;

	//Store the name as well
	record->name = name;

	//And give back the record
	return record;
}


/**
 * Insert a record into the function symbol table. This assumes that the user
 * has already checked to see if this record exists in the table
 *
 * RETURNS 0 if no collision, 1 if collision
 */
u_int8_t insert_function(function_symtab_t* symtab, symtab_function_record_t* record){
	//Assign this a unique identifier. Once we've assigned the unique ID, bump the
	//overall function ID for the next go around
	record->function_id = symtab->current_function_id;
	(symtab->current_function_id)++;

	//If there's no collision
	if(symtab->records[record->hash] == NULL){
		//Store it and get out
		symtab->records[record->hash] = record;
		return 0;
	}
	
	//Otherwise if we get here there was a collision
	//Grab the head record
	symtab_function_record_t* cursor = symtab->records[record->hash];

	//Get to the very last node
	while(cursor->next != NULL){
		cursor = cursor->next;
	}

	//Now that cursor points to the very last node, we can add it in
	cursor->next = record;
	//This should be null anyways, but it never hurts to double check
	record->next = NULL;

	//1 = success, but there was a collision
	return 1;
}


/**
 * Insert a macro into the symtab
 */
u_int8_t insert_macro(macro_symtab_t* symtab, symtab_macro_record_t* record){
	//Grab a cursor to whatever is in the hash's spot
	symtab_macro_record_t* cursor = symtab->records[record->hash];

	//No collision. Just insert and move on
	if(cursor == NULL){
		symtab->records[record->hash] = record;
		//Return 0 - no collision
		return 0;
	}

	//Otherwise we have a collision, so we need to drill down
	//to the end
	while(cursor != NULL){
		//Keep advancing it up
		cursor = cursor->next;
	}

	//Now that we're at the end, we will append our record to the cursor
	cursor->next = record;

	//It should already be NULL, but this doesn't hurt
	record->next = NULL;

	//We did indeed have a collision here
	return 1;
}


/**
 * Inserts a variable record into the symtab. This assumes that the user has already checked to see if
 * this record exists in the table
 */
u_int8_t insert_variable(variable_symtab_t* symtab, symtab_variable_record_t* record){
	//While we're at it store this
	record->lexical_level = symtab->current_lexical_scope;

	//No collision here, just store and get out
	if(symtab->current->records[record->hash] == NULL){
		//Store this and get out
		symtab->current->records[record->hash] = record;
		//0 = success, no collision
		return 0;
	}

	//Otherwise, there is a collision
	//Grab the head record
	symtab_variable_record_t* cursor = symtab->current->records[record->hash];

	//Get to the very last node
	while(cursor->next != NULL){
		cursor = cursor->next;
	}

	//Now that cursor points to the very last node, we can add it in
	cursor->next = record;
	//This should be null anyways, but it never hurts to double check
	record->next = NULL;

	//1 = success, but there was a collision
	return 1;
}


/**
 * Inserts a type record into the symtab. This assumes that the user has already checked to see if
 * this record exists in the table
 */
u_int8_t insert_type(type_symtab_t* symtab, symtab_type_record_t* record){
	//While we're at it store this
	record->lexical_level = symtab->current_lexical_scope;

	//No collision here, just store and get out
	if(symtab->current->records[record->hash] == NULL){
		//Store this and get out
		symtab->current->records[record->hash] = record;
		//0 = success, no collision
		return 0;
	}

	//Otherwise, there is a collision
	//Grab the head record
	symtab_type_record_t* cursor = symtab->current->records[record->hash];

	//Get to the very last node
	while(cursor->next != NULL){
		cursor = cursor->next;
	}

	//Now that cursor points to the very last node, we can add it in
	cursor->next = record;
	//This should be null anyways, but it never hurts to double check
	record->next = NULL;

	//1 = success, but there was a collision
	return 1;
}


/**
 * A helper function that adds all basic types to the type symtab
 *
 * NOTE: This helper creates both *mutable and immutable* versions
 * of all of our basic types
 */
u_int16_t add_all_basic_types(type_symtab_t* symtab){
	//Store the number of collisions that we have
	u_int16_t num_collisions = 0;

	generic_type_t* type;

	//Add in void type
	type = create_basic_type("void", VOID, NOT_MUTABLE);
	num_collisions += insert_type(symtab, create_type_record(type));

	// ================================ Immutable versions of our primitive types ================================
	//s_int8 type
	type = create_basic_type("i8", I8, NOT_MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));

	//u_int8 type
	type = create_basic_type("u8", U8, NOT_MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));

	//Bool type
	type = create_basic_type("bool", BOOL, NOT_MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));

	//char type
	type = create_basic_type("char", CHAR, NOT_MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));

	//Save this for the next one to avoid confusion
	generic_type_t* char_type = type;

	//char* type
	type = create_pointer_type(char_type, 0, NOT_MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));

	//Create "char*" type
	type = create_pointer_type(type, 0, NOT_MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));

	//u_int16 type
	type = create_basic_type("u16", U16, NOT_MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));
		
	//s_int16 type
	type = create_basic_type("i16", I16, NOT_MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));
	
	//s_int32 type
	type = create_basic_type("i32", I32, NOT_MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));
	
	//u_int32 type
	type = create_basic_type("u32", U32, NOT_MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));
	
	//u_int64 type
	type = create_basic_type("u64", U64, NOT_MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));
	
	//s_int64 type
	type = create_basic_type("i64", I64, NOT_MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));

	//float32 type
	type = create_basic_type("f32", F32, NOT_MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));
	
	//float64 type
	type = create_basic_type("f64", F64, NOT_MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));
	// ================================ Immutable versions of our primitive types ================================
	
	// ================================ Mutable versions of our primitive types ================================
	// This mutable void type only exists to internally support a mutable void* pointer
	type = create_basic_type("void", VOID, MUTABLE);
	num_collisions += insert_type(symtab, create_type_record(type));

	//s_int8 type
	type = create_basic_type("i8", I8, MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));

	//u_int8 type
	type = create_basic_type("u8", U8, MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));

	//Bool type
	type = create_basic_type("bool", BOOL, MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));

	//char type
	type = create_basic_type("char", CHAR, MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));

	//Save this for the next one to avoid confusion
	char_type = type;

	//char* type
	type = create_pointer_type(char_type, 0, MUTABLE);
	num_collisions += insert_type(symtab, create_type_record(type));

	//Create "char*" type
	type = create_pointer_type(type, 0, MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));
	
	//u_int16 type
	type = create_basic_type("u16", U16, MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));
		
	//s_int16 type
	type = create_basic_type("i16", I16, MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));
	
	//s_int32 type
	type = create_basic_type("i32", I32, MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));
	
	//u_int32 type
	type = create_basic_type("u32", U32, MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));
	
	//u_int64 type
	type = create_basic_type("u64", U64, MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));
	
	//s_int64 type
	type = create_basic_type("i64", I64, MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));

	//float32 type
	type = create_basic_type("f32", F32, MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));
	
	//float64 type
	type = create_basic_type("f64", F64, MUTABLE);
	num_collisions += insert_type(symtab,  create_type_record(type));

	// ================================ Mutable versions of our primitive types ==============================
	
	// This is for observability in the test suites - if we have 
	// more than 1 or 2 collisions here, then we have a serious problem
	return num_collisions;
}


/** 
 * Create the stack pointer(rsp) variable for us to use throughout
 */
symtab_variable_record_t* initialize_stack_pointer(type_symtab_t* types){
	//Create the var name
	dynamic_string_t variable_name = dynamic_string_alloc();

	//Set to be stack pointer
	dynamic_string_set(&variable_name, "stack_pointer");

	symtab_variable_record_t* stack_pointer = create_variable_record(variable_name);
	//Set this type as a label(address)
	stack_pointer->type_defined_as = lookup_type_name_only(types, "u64", NOT_MUTABLE)->type;

	//Give it back
	return stack_pointer;
}


/** 
 * Create the instruction pointer(rip) variable for us to use throughout
 */
symtab_variable_record_t* initialize_instruction_pointer(type_symtab_t* types){
	//Create the var name
	dynamic_string_t variable_name = dynamic_string_alloc();

	//Set to be instruction pointer(rip)
	dynamic_string_set(&variable_name, "rip");

	symtab_variable_record_t* instruction_pointer = create_variable_record(variable_name);
	//Set this type as a label(address)
	instruction_pointer->type_defined_as = lookup_type_name_only(types, "u64", NOT_MUTABLE)->type;

	//Give it back
	return instruction_pointer;
}


/**
 * Lookup the record in the symtab that corresponds to the following name.
 * 
 * There is only one lexical scope for functions, so this symtab is quite simple
 */
symtab_function_record_t* lookup_function(function_symtab_t* symtab, char* name){
	//Let's grab it's hash
	u_int64_t h = hash_function(name); 

	//Grab whatever record is at that hash
	symtab_function_record_t* record_cursor = symtab->records[h];
		
	//We could have had collisions so we'll have to hunt here
	while(record_cursor != NULL){
		//If we find the right one, then we can get out
		if(strncmp(record_cursor->func_name.string, name, record_cursor->func_name.current_length) == 0){
			return record_cursor;
		}
		//Advance it if we didn't have the right name
		record_cursor = record_cursor->next;
	}

	//When we make it down here, we found nothing so
	return NULL;
}


/**
 * Lookup a macro in the symtab. This is a simpler lookup then most because
 * there are no nested lexical scopes here, we only need to check one table
 */
symtab_macro_record_t* lookup_macro(macro_symtab_t* symtab, char* name){
	//Grab the name's hash
	u_int64_t hash = hash_macro_name(name);

	//Go to this area in the hash table
	symtab_macro_record_t* cursor = symtab->records[hash];

	//Remember that we could have collisions here, so we're going
	//to need to account for that
	while(cursor != NULL){
		//If this is a match, then we're set
		if(strncmp(cursor->name.string, name, cursor->name.current_length) == 0){
			return cursor;
		}

		//Bump it up
		cursor = cursor->next;
	}

	//If we make it all of the way down here, then we have no match, so return NULL
	return NULL;
}


/**
 * Lookup the record in the symtab that corresponds to the following name.
 * 
 * We are ALWAYS biased to the most local(in scope) version of the name. If we
 * do not find it in the local scope, we then search the outer scope, until there are
 * no more outer scopes to search
 */
symtab_variable_record_t* lookup_variable(variable_symtab_t* symtab, char* name){
	//Grab the hash
	u_int64_t h = hash_variable(name);

	//Define the cursor so we don't mess with the original reference
	symtab_variable_sheaf_t* cursor = symtab->current;
	symtab_variable_record_t* records_cursor;

	while(cursor != NULL){
		//As long as the previous level is not null
		records_cursor = cursor->records[h];
		
		//We could have had collisions so we'll have to hunt here
		while(records_cursor != NULL){
			//If we find the right one, then we can get out
			if(strncmp(records_cursor->var_name.string, name, records_cursor->var_name.current_length) == 0){
				return records_cursor;
			}
			//Advance it
			records_cursor = records_cursor->next;
		}

		//Go up to a higher scope
		cursor = cursor->previous_level;
	}

	//We found nothing
	return NULL;
}


/**
 * Lookup the record in the symtab that corresponds to the following name. This function
 * will specifically ONLY check the local scope
 */
symtab_variable_record_t* lookup_variable_local_scope(variable_symtab_t* symtab, char* name){
	//Grab the hash
	u_int64_t h = hash_variable(name);

	//A cursor for records iterating
	symtab_variable_record_t* records_cursor;

	//We only deal with the current level
	records_cursor = symtab->current->records[h];
	
	//We could have had collisions so we'll have to hunt here
	while(records_cursor != NULL){
		//If we find the right one, then we can get out
		if(strncmp(records_cursor->var_name.string, name, records_cursor->var_name.current_length) == 0){
			return records_cursor;
		}
		//Advance it
		records_cursor = records_cursor->next;
	}

	//Otherwise if we get here there's no match, so
	return NULL;
}


/**
 * Lookup a variable in all lower scopes. This is specifically and only intended for
 * jump statements
 */
symtab_variable_record_t* lookup_variable_lower_scope(variable_symtab_t* symtab, char* name){
	//Grab the hash
	u_int64_t h = hash_variable(name);

	//Define the cursor so we don't mess with the original reference
	symtab_variable_sheaf_t* cursor;
	//A cursor for records iterating
	symtab_variable_record_t* records_cursor;

	//So long as the cursor is not null
	for(u_int16_t i = 0; i < symtab->sheafs.current_index; i++){
		//Grab the current sheaf
		cursor = dynamic_array_get_at(&(symtab->sheafs), i);

		//Grab a records cursor
		records_cursor = cursor->records[h];

		//We could have had collisions so we'll hunt here
		while(records_cursor != NULL){
		
			//If we find the right one, then we can get out
			if(strncmp(records_cursor->var_name.string, name, records_cursor->var_name.current_length) == 0){
				return records_cursor;
			}

			//Advance it
			records_cursor = records_cursor->next;
		}
	}

	//If we found nothing give back NULL
	return NULL;
}


/**
 * Lookup a type name in the symtab by the name only. This does not
 * do the array bound comparison that we need for strict equality
 */
symtab_type_record_t* lookup_type_name_only(type_symtab_t* symtab, char* name, mutability_type_t mutability){
	//Grab the hash
	u_int64_t h = hash_type_name(name, mutability);

	//Define the cursor so we don't mess with the original reference
	symtab_type_sheaf_t* cursor = symtab->current;
	symtab_type_record_t* records_cursor;

	while(cursor != NULL){
		//As long as the previous level is not null
		records_cursor = cursor->records[h];
		
		//We could have had collisions so we'll have to hunt here
		while(records_cursor != NULL){
			//If we find the right one, then we can get out
			if(strncmp(records_cursor->type->type_name.string, name, records_cursor->type->type_name.current_length) == 0
				//The mutability must also match
				&& records_cursor->type->mutability == mutability){

				//Give it back
				return records_cursor;
			}
			//Advance it
			records_cursor = records_cursor->next;
		}

		//Go up to a higher scope
		cursor = cursor->previous_level;
	}

	//We found nothing
	return NULL;
}


/**
 * Specifically look for a pointer type to the given type in the symtab
 *
 * This function exists so that we do not need to allocate memory in the parser
 * just to free it
 */
symtab_type_record_t* lookup_pointer_type(type_symtab_t* symtab, generic_type_t* points_to, mutability_type_t mutability){
	//Grab an array for the type name
	char type_name[MAX_IDENT_LENGTH];

	//Get the name in there by a copy
	strcpy(type_name, points_to->type_name.string);

	//Append the pointer to it
	strcat(type_name, "*");

	//Now get the hash
	u_int64_t hash = hash_type_name(type_name, mutability);

	//Grab the current lexical scope. We will search here and down
	symtab_type_sheaf_t* sheaf_cursor = symtab->current;
	symtab_type_record_t* record_cursor;

	//Go through all of the scopes
	while(sheaf_cursor != NULL){
		//Grab the record at the hash
		record_cursor = sheaf_cursor->records[hash];
		
		//We could have had collisions so we'll have to hunt here
		while(record_cursor != NULL){
			//If we find the right one, then we can get out
			if(strncmp(record_cursor->type->type_name.string, type_name, record_cursor->type->type_name.current_length) == 0){
				//We have a match
				return record_cursor;
			}

			//Otherwise no match, we advance it
			record_cursor = record_cursor->next;
		}

		//Go up to a higher scope
		sheaf_cursor = sheaf_cursor->previous_level;
	}

	//If we get all the way down here and it's a bust, return NULL
	return NULL;
}


/**
 * Specifically look for a reference type to the given type in the symtab
 */
symtab_type_record_t* lookup_reference_type(type_symtab_t* symtab, generic_type_t* references, mutability_type_t mutability){
	//Grab an array for the type name
	char type_name[MAX_IDENT_LENGTH];

	//Get the name in there by a copy
	strcpy(type_name, references->type_name.string);

	//Append the reference token to it
	strcat(type_name, "&");

	//Now get the hash
	u_int64_t hash = hash_type_name(type_name, mutability);

	//Grab the current lexical scope. We will search here and down
	symtab_type_sheaf_t* sheaf_cursor = symtab->current;
	symtab_type_record_t* record_cursor;

	//Go through all of the scopes
	while(sheaf_cursor != NULL){
		//Grab the record at the hash
		record_cursor = sheaf_cursor->records[hash];
		
		//We could have had collisions so we'll have to hunt here
		while(record_cursor != NULL){
			//If we find the right one, then we can get out
			if(strncmp(record_cursor->type->type_name.string, type_name, record_cursor->type->type_name.current_length) == 0){
				//We have a match
				return record_cursor;
			}

			//Otherwise no match, we advance it
			record_cursor = record_cursor->next;
		}

		//Go up to a higher scope
		sheaf_cursor = sheaf_cursor->previous_level;
	}

	//If we get all the way down here and it's a bust, return NULL
	return NULL;
}


/**
 * Specifically look for an array type with the given type as a member in the symtab
 */
symtab_type_record_t* lookup_array_type(type_symtab_t* symtab, generic_type_t* member_type, u_int32_t num_members, mutability_type_t mutability){
	//Grab an array for the type name
	char type_name[MAX_IDENT_LENGTH];

	//Get the name in there by a copy
	strcpy(type_name, member_type->type_name.string);

	//Append the array signifiers to it
	strcat(type_name, "[]");

	//Now get the hash. We need to be using a special helper for this
	u_int64_t hash = hash_array_type_name(type_name, num_members, mutability);

	//Grab the current lexical scope. We will search here and down
	symtab_type_sheaf_t* sheaf_cursor = symtab->current;
	symtab_type_record_t* record_cursor;

	//Go through all of the scopes
	while(sheaf_cursor != NULL){
		//Grab the record at the hash
		record_cursor = sheaf_cursor->records[hash];

		//We could have had collisions so we'll have to hunt here
		while(record_cursor != NULL){
			//If it's not an array we don't care
			if(record_cursor->type->type_class != TYPE_CLASS_ARRAY){
				record_cursor = record_cursor->next;
				continue;
			}

			//If we find the right one, then we can get out
			if(strncmp(record_cursor->type->type_name.string, type_name, record_cursor->type->type_name.current_length) == 0
				//The member counts also need to match
				&& record_cursor->type->internal_values.num_members == num_members){

				//We have a match
				return record_cursor;
			}

			//Otherwise no match, we advance it
			record_cursor = record_cursor->next;
		}

		//Go up to a higher scope
		sheaf_cursor = sheaf_cursor->previous_level;
	}

	//If we get all the way down here and it's a bust, return NULL
	return NULL;
}


/**
 * Lookup the record in the symtab that corresponds to the following name.
 * 
 * We are ALWAYS biased to the most local(in scope) version of the name. If we
 * do not find it in the local scope, we then search the outer scope, until there are
 * no more outer scopes to search
 */
symtab_type_record_t* lookup_type(type_symtab_t* symtab, generic_type_t* type){
	//Fail out if we have this
	if(type == NULL){
		return NULL;
	}

	//Grab the hash
	u_int64_t h = hash_type(type);

	//Define the cursor so we don't mess with the original reference
	symtab_type_sheaf_t* cursor = symtab->current;
	symtab_type_record_t* records_cursor;

	while(cursor != NULL){
		//As long as the previous level is not null
		records_cursor = cursor->records[h];
		
		//We could have had collisions so we'll have to hunt here
		while(records_cursor != NULL){
			//If we find the right one, then we can get out
			if(strncmp(records_cursor->type->type_name.string, type->type_name.string, type->type_name.current_length) == 0){
				//If we have an array type, we must compare bounds and they must match
				if(type->type_class == TYPE_CLASS_ARRAY
					&& type->internal_values.num_members != records_cursor->type->internal_values.num_members){
					return FALSE;
				}

				//No array type + successful array type end here
				return records_cursor;
			}
			//Advance it
			records_cursor = records_cursor->next;
		}

		//Go up to a higher scope
		cursor = cursor->previous_level;
	}

	//We found nothing
	return NULL;
}


/**
 * A record printer that is used for development/error messages
 */
void print_function_record(symtab_function_record_t* record){
	//Safety check
	if(record == NULL){
		printf("NULL RECORD\n");
		return;
	}

	printf("Record: {\n");
	printf("Name: %s,\n", record->func_name.string);
	printf("Hash: %ld,\n", record->hash);
	printf("}\n");
}


/**
 * Record that a given source function calls the target
 *
 * This always goes as: source calls target
 */
void add_function_call(symtab_function_record_t* source, symtab_function_record_t* target){
	//Add it into the list of functions called by the source. Since we use a set here, we are
	//guaranteed to never add the function in more than once even if the source function calls
	//it multiple times in the body
	dynamic_set_add(&(source->called_functions), target);

	//This function has been called
	target->called = TRUE;
}


/**
 * A record printer that is used for development/error messages
 */
void print_variable_record(symtab_variable_record_t* record){
	//Safety check
	if(record == NULL){
		printf("NULL RECORD\n");
		return;
	}

	printf("Record: {\n");
	printf("Name: %s,\n", record->var_name.string);
	printf("Hash: %ld,\n", record->hash);
	printf("Lexical Level: %d,\n", record->lexical_level);
	printf("}\n");
}


/**
 * A record printer that is used for development/error messages
 */
void print_type_record(symtab_type_record_t* record){
	//Safety check
	if(record == NULL){
		printf("NULL RECORD\n");
		return;
	}

	printf("Record: {\n");
	printf("Name: %s,\n", record->type->type_name.string);
	printf("Hash: %ld,\n", record->hash);
	printf("Lexical Level: %d,\n", record->lexical_level);
	printf("}\n");
}


/**
 * Print a function name out in a stylised way
 */
void print_function_name(symtab_function_record_t* record){
	if(record->signature->internal_types.function_type->is_public == TRUE){
		printf("\t---> %d | pub fn %s(", record->line_number, record->func_name.string);
	} else {
		printf("\t---> %d | fn %s(", record->line_number, record->func_name.string);
	}

	//Print out the params
	for(u_int8_t i = 0; i < record->function_parameters.current_index; i++){
		symtab_variable_record_t* current_parameter = dynamic_array_get_at(&(record->function_parameters), i);

		//Print if it's mutable
		if(current_parameter->type_defined_as->mutability == MUTABLE){
			printf("mut ");
		}

		printf("%s : %s", current_parameter->var_name.string, current_parameter->type_defined_as->type_name.string);
		//Comma if needed
		if(i < record->function_parameters.current_index - 1){
			printf(", ");
		}
	}

	//Final closing paren and return type
	if(record->return_type != NULL){
		printf(") -> %s", record->return_type->type_name.string);
	} else {
		printf(") -> (null)");
	}

	//If it was defined implicitly, we'll print a semicol
	if(record->defined == 0){
		printf(";\n");
	} else {
		printf("{...\n");
	}
}


/**
 * Print a variable name out in a stylized way
 * Intended for error messages
 */
void print_variable_name(symtab_variable_record_t* record){
	//Go based on the membership
	switch(record->membership){
		case FUNCTION_PARAMETER:
			print_function_name(record->function_declared_in);
			break;
		case LABEL_VARIABLE:
			printf("\n---> %d | %s:\n", record->line_number, record->var_name.string);
			break;
		case ENUM_MEMBER:
			//The var name
			printf("{\n\t\t...\n\t\t...\t\t\n---> %d |\t %s", record->line_number, record->var_name.string);
			break;
		case STRUCT_MEMBER:
			//The var name
			printf("{\n\t\t...\n\t\t...\t\t\n---> %d |\t %s : %s", record->line_number, record->var_name.string, record->type_defined_as->type_name.string);
			break;
		default:
			//Line num
			printf("\n---> %d | ", record->line_number);

			//Declare or let
			record->declare_or_let == 0 ? printf("declare ") : printf("let ");

			//The var name
			printf("%s : ", record->var_name.string);

			//The type name
			printf("%s%s", (record->type_defined_as->mutability == MUTABLE ? "mut ": ""),
		  					record->type_defined_as->type_name.string);
			
			//We'll print out some abbreviated stuff with the let record
			if(record->declare_or_let == 1){
				printf("= <initializer>;\n\n");
			} else {
				printf(";\n");
			}

			break;
	}
}


/**
 * Print a type name. Intended for error messages
 */
void print_type_name(symtab_type_record_t* record){
	//Print out where it was declared
	if(record->type->type_class == TYPE_CLASS_BASIC){
		printf("---> BASIC TYPE | ");
	} else {
		printf("---> %d | ", record->type->line_number);
	}

	//The mut specifier
	if(record->type->mutability == MUTABLE){
		printf("mut ");
	}

	//Then print out the name
	printf("%s\n\n", record->type->type_name.string);
}


/**
 * Print the call graph's adjacency matrix/transitive closure out for debugging
 */
void print_call_graph_adjacency_matrix(FILE* fl, function_symtab_t* function_symtab){
	fprintf(fl, "=============== Function Call Graph ========================\n");
	
	//We need a min priority queue for this
	min_priority_queue_t min_priority_queue = min_priority_queue_alloc();

	//Run through and print all of these out first
	for(u_int32_t i = 0; i < FUNCTION_KEYSPACE; i++){
		//Skip ahead
		if(function_symtab->records[i] == NULL){
			continue;
		}

		//Otherwise grab it out
		symtab_function_record_t* cursor = function_symtab->records[i];

		//Crawl the whole thing
		while(cursor != NULL){
			//Use the min priority queue to insert based on the function ID
			min_priority_queue_enqueue(&min_priority_queue, cursor, cursor->function_id);

			//Bump it up
			cursor = cursor->next;
		}
	}

	//Now run through the priority queue and print the functions out
	while(min_priority_queue_is_empty(&min_priority_queue) == FALSE){
		//Get the function off
		symtab_function_record_t* function = min_priority_queue_dequeue(&min_priority_queue);

		//Now print it's name and ID out
		fprintf(fl, "[%d]: %s\n", function->function_id, function->func_name.string);
	}

	//Dividing newline
	fprintf(fl, "\n");

	//Now we're done so deallocate it
	min_priority_queue_dealloc(&min_priority_queue);

	//Run through the entire symtab first and print out all of the functions with their
	//IDs for the user

	//Get the overall count
	u_int32_t function_count = function_symtab->current_function_id;

	fprintf(fl, "============= Adjacency Matrix ==============\n");

	//Run through each row
	for(u_int32_t i = 0; i < function_count; i++){
		//Print out the row number
		fprintf(fl, "[%2d]: ", i);

		//Now print out the columns
		for(u_int32_t j = 0; j < function_count; j++){
			//Will be 1(connected) or 0
			fprintf(fl, "%d ", function_symtab->call_graph_matrix[i * function_count + j]);
		}

		//Final newline
		fprintf(fl, "\n");
	}

	fprintf(fl, "============= Adjacency Matrix ==============\n");
	fprintf(fl, "============= Transitive Closure ==============\n");

	//Run through each row
	for(u_int32_t i = 0; i < function_count; i++){
		//Print out the row number
		fprintf(fl, "[%2d]: ", i);

		//Now print out the columns
		for(u_int32_t j = 0; j < function_count; j++){
			//Will be 1(connected) or 0
			fprintf(fl, "%d ", function_symtab->call_graph_transitive_closure[i * function_count + j]);
		}

		//Final newline
		fprintf(fl, "\n");
	}

	fprintf(fl, "============= Transitive Closure ==============\n");
	fprintf(fl, "=============== Function Call Graph ========================\n");
}


/**
 * Determine whether or not a function is directly recursive using the function
 * symtab's adjacency matrix
 */
u_int8_t is_function_directly_recursive(function_symtab_t* symtab, symtab_function_record_t* record){
	//Extract for our uses
	u_int32_t function_id = record->function_id;
	u_int32_t num_functions = symtab->current_function_id;

	//Extract the value contained at adjacency_matrix[func_id][func_id]
	return symtab->call_graph_matrix[function_id * num_functions + function_id];

}


/**
 * Determine whether or not a function is recursive(direct or indirect) using the function
 * symtab's transitive closure 
 */
u_int8_t is_function_recursive(function_symtab_t* symtab, symtab_function_record_t* record){
	//Extract for our uses
	u_int32_t function_id = record->function_id;
	u_int32_t num_functions = symtab->current_function_id;

	//Extract the value contained at transitive_closure[func_id][func_id]
	return symtab->call_graph_transitive_closure[function_id * num_functions + function_id];
}


/**
 * Crawl the symtab and check for any unused functions. We generate some hopefully helpful
 * warnings here for the user
 */
void check_for_unused_functions(function_symtab_t* symtab, u_int32_t* num_warnings){
	//For any/all error printing
	char info[1000];
	//For temporary holding
	symtab_function_record_t* record;

	//Create a min priority queue for ordering error messages
	min_priority_queue_t queue = min_priority_queue_alloc();

	//Run through all keyspace records
	for(u_int16_t i = 0; i < FUNCTION_KEYSPACE; i++){
		record = symtab->records[i];

		//We could have chaining here, so run through just in case
		while(record != NULL){
			//If one of these 3 error conditions is true, we will print a warning
			if((record->called == 0 && record->defined == 0)
				|| (record->called == 0 && record->defined == 1)
				|| (record->called == 1 && record->defined == 0)){

				//Enqueue using the line number as priority
				min_priority_queue_enqueue(&queue, record, record->line_number);
			}

			//Advance record up
			record = record->next;
		}

		//Now that we have everything loaded into a queue by line number, we will go through
		//and print each individual error
		while(min_priority_queue_is_empty(&queue) == FALSE){
			//Get it off of the queue
			record = min_priority_queue_dequeue(&queue);
		
			if(record->called == FALSE && record->defined == FALSE){
				//Generate a warning here
				(*num_warnings)++;

				sprintf(info, "Function \"%s\" is never defined and never called. First defined here:", record->func_name.string);
				PRINT_WARNING(info, record->line_number);
				//Also print where the function was defined
				print_function_name(record);

			//Only generate here if we have a private function. Public functions may be called from external files
			} else if(record->called == FALSE && record->defined == TRUE && record->function_visibility == FUNCTION_VISIBILITY_PRIVATE){
				//Generate a warning here
				(*num_warnings)++;

				sprintf(info, "Function \"%s\" is defined but never called. First defined here:", record->func_name.string);
				PRINT_WARNING(info, record->line_number);
				//Also print where the function was defined
				print_function_name(record);

			} else if(record->called == 1 && record->defined == 0){
				//Generate a warning here
				(*num_warnings)++;

				sprintf(info, "Function \"%s\" is called but never explicitly defined. First declared here:", record->func_name.string);
				PRINT_WARNING(info, record->line_number);
				//Also print where the function was defined
				print_function_name(record);

			}
		}
	}

	//Destroy the queue
	min_priority_queue_dealloc(&queue);
}


/**
 * If a variable is declared as "mut"(mutable) but is never assigned to throughout it's
 * entire lifetime, that mut keyword is not needed
 */
void check_for_var_errors(variable_symtab_t* symtab, u_int32_t* num_warnings){
	//For any/all error printing
	char info[1000];
	//For record holding
	symtab_variable_record_t* record;

	//Create a min priority queue for ordering error messages
	min_priority_queue_t queue = min_priority_queue_alloc();

	//So long as we have a sheaf
	for(u_int16_t i = 0; i < symtab->sheafs.current_index; i++){
		//Grab the actual sheaf out
		symtab_variable_sheaf_t* sheaf = dynamic_array_get_at(&(symtab->sheafs), i);

		//Now we'll run through every variable in here
		for(u_int32_t i = 0; i < VARIABLE_KEYSPACE; i++){
			record = sheaf->records[i];

			//So long as the record is not NULL(we need to account for collisions)
			while(record != NULL){
				//If it's a label or struct, don't bother with it
				switch(record->membership){
					case LABEL_VARIABLE:
					case STRUCT_MEMBER:
						record = record->next;
						continue;
					default:
						break;
				}

				//Only put it into the queue if it meets one of 2 warning conditions
				if((record->initialized == FALSE && is_memory_address_type(record->type_defined_as) == FALSE)
					|| (record->type_defined_as->mutability == MUTABLE && record->mutated == FALSE)){
					//Add it into the priority queue. The priority goes by line number
					min_priority_queue_enqueue(&queue, record, record->line_number);
				}

				//Push it up
				record = record->next;
			}
		}
	}

	//Now that we've loaded up the priority queue, we will go through and print out appropriate error messages
	while(min_priority_queue_is_empty(&queue) == FALSE){
		//Dequeue the value
		record = min_priority_queue_dequeue(&queue);

		//We have a non initialized variable
		if(record->initialized == FALSE && is_memory_address_type(record->type_defined_as) == FALSE){
			sprintf(info, "Variable \"%s\" may never be initialized. First defined here:", record->var_name.string);
			PRINT_WARNING(info, record->line_number);
			print_variable_name(record);
			(*num_warnings)++;
			//Go to the next iteration
			continue;
		}

		//If it's mutable but never mutated
		if(record->type_defined_as->mutability == MUTABLE && record->mutated == FALSE){
			sprintf(info, "Variable \"%s\" is declared as mutable but never mutated. Consider removing the \"mut\" keyword. First defined here:", record->var_name.string);
			PRINT_WARNING(info, record->line_number);
			print_variable_name(record);
			(*num_warnings)++;
		}
	}

	//Destroy the queue
	min_priority_queue_dealloc(&queue);
}


/**
 * Compute the transitive closure of the call graph. This is done using Floyd-Warshall.
 *
 * NOTE: this graph is *not* acyclic. It is totally possible(and often common) for call
 * cycles to arise
 *
 * This function assumes that the regular adjacency matrix has already been computed
 */
static inline void compute_call_graph_transitive_closure(function_symtab_t* symtab){
	//Extract the number of functions
	u_int32_t number_of_functions = symtab->current_function_id;

	//Allocate the transitive closure
	symtab->call_graph_transitive_closure = calloc(number_of_functions * number_of_functions, sizeof(u_int8_t));

	//Copy over the regular adjacency matrix to this
	memcpy(symtab->call_graph_transitive_closure, symtab->call_graph_matrix, number_of_functions * number_of_functions * sizeof(u_int8_t));

	//Now that we've made the copy, we can start on the actual transitive closure
	
	//For each node i(intermediate node)
	for(u_int32_t i = 0; i < number_of_functions; i++){
		for(u_int32_t j = 0; j < number_of_functions; j++){
			for(u_int32_t k = 0; k < number_of_functions; k++){
				//If there's a path from j to i, and a path from i to k
				if(symtab->call_graph_transitive_closure[j * number_of_functions + i] == TRUE
					&& symtab->call_graph_transitive_closure[i * number_of_functions + k] == TRUE){
					//Flag that there is a path from j to k
					symtab->call_graph_transitive_closure[j * number_of_functions + k] = TRUE;
				}
			}
		}
	}
}


/**
 * This function is intended to be called after parsing is complete.
 * Within it, we will finalize the function symtab including constructing
 * the adjacency matrix for the call graph
 */
void finalize_function_symtab(function_symtab_t* symtab){
	//Extract the number of functions
	u_int32_t number_of_functions = symtab->current_function_id;

	//Now that we have all of the possible functions added in, we need to create the
	//overall adjacency matrix for all of these functions
	symtab->call_graph_matrix = calloc(number_of_functions * number_of_functions, sizeof(u_int8_t));

	//We will now go through and populate the adjacency matrix. We need to run through the entire
	//hashmap to do this
	for(u_int32_t i = 0; i < FUNCTION_KEYSPACE; i++){
		//Totally possible for this to happen
		if(symtab->records[i] == NULL){
			continue;
		}

		//Otherwise, we actually have a space that is populated so we need to
		//populate here. Remember, every record is a linked list so we need
		//to explore all of the nodes
		symtab_function_record_t* cursor = symtab->records[i];

		//So long as the cursor is not NULL
		while(cursor != NULL){
			//Grab the cursor's unique function ID
			u_int32_t cursor_id = cursor->function_id;

			//Run through all of the functions that this function
			//itself calls
			for(u_int16_t j = 0; j < cursor->called_functions.current_index; j++){
				//Extract it
				symtab_function_record_t* called_function = dynamic_set_get_at(&(cursor->called_functions), j);

				//Now let's get his ID
				u_int32_t called_function_id = called_function->function_id;

				//Insert this call into the adjacency matrix
				symtab->call_graph_matrix[cursor_id * number_of_functions + called_function_id] = TRUE;
			}

			//Bump it up to the next one
			cursor = cursor->next;
		}
	}

	//Now that we have the regular call graph created, we will create the transitive closure
	compute_call_graph_transitive_closure(symtab);
}


/**
 * Provide a function that will destroy the function symtab completely
 */
void function_symtab_dealloc(function_symtab_t* symtab){
	//For temporary holding
	symtab_function_record_t* record;
	symtab_function_record_t* temp;

	//Run through and free all function records
	for(u_int16_t i = 0; i < FUNCTION_KEYSPACE; i++){
		record = symtab->records[i];

		//We could have chaining here, so run through just in case
		while(record != NULL){
			temp = record;
			record = record->next;

			//Destroy the call graph infrastructure
			dynamic_set_dealloc(&(temp->called_functions));

			//Destroy the block storage
			dynamic_array_dealloc(&(temp->function_blocks));

			//Destroy the parameters
			dynamic_array_dealloc(&(temp->function_parameters));

			//Dealloate the function type
			type_dealloc(temp->signature);

			//Deallocate the data area itself
			stack_data_area_dealloc(&(temp->local_stack));

			free(temp);
		}
	}

	//Free the adjacency matrix
	free(symtab->call_graph_matrix);

	//Free the entire symtab at the very end
	free(symtab);
}


/**
 * Private helper that deallocates a variable
 */
static inline void variable_dealloc(symtab_variable_record_t* variable){
	//If we have a lightstack that's linked, destroy that
	lightstack_dealloc(&(variable->counter_stack));

	//Free the overall variable
	free(variable);
}


/**
 * Provide a function that will destroy the variable symtab completely
 */
void variable_symtab_dealloc(variable_symtab_t* symtab){
	symtab_variable_sheaf_t* cursor;
	symtab_variable_record_t* record;
	symtab_variable_record_t* temp;

	//Run through all of the sheafs
	for	(u_int16_t i = 0; i < symtab->sheafs.current_index; i++){
		//Grab the current sheaf out
		cursor = dynamic_array_get_at(&(symtab->sheafs), i);

		//Now we'll free all non-null records
		for(u_int16_t j = 0; j < VARIABLE_KEYSPACE; j++){
			record = cursor->records[j];

			//We could have chaining here, so run through just in case
			while(record != NULL){
				temp = record;
				record = record->next;
				variable_dealloc(temp);
			}
		}
		//Free the sheaf
		free(cursor);
	}

	//Deallocate the dynamic array
	dynamic_array_dealloc(&(symtab->sheafs));
	
	//Finally free the symtab itself
	free(symtab);
}

/**
 * Provide a function that will destroy the variable symtab completely
 */
void type_symtab_dealloc(type_symtab_t* symtab){
	symtab_type_sheaf_t* cursor;
	symtab_type_record_t* record;
	symtab_type_record_t* temp;

	//Run through all of the sheafs
	for	(u_int16_t i = 0; i < symtab->sheafs.current_index; i++){
		//Grab the current sheaf
		cursor = dynamic_array_get_at(&(symtab->sheafs), i);

		//Now we'll free all non-null records
		for(u_int16_t j = 0; j < TYPE_KEYSPACE; j++){
			record = cursor->records[j];

			//We could have chaining here, so run through just in case
			while(record != NULL){
				temp = record;
				record = record->next;
				//Destroy the actual type while here
				type_dealloc(temp->type);
				free(temp);
			}
		}
		//Free the sheaf
		free(cursor);
	}

	//Destroy the dynamic array
	dynamic_array_dealloc(&(symtab->sheafs));

	//Finally free the symtab itself
	free(symtab);
}


/**
 * Destroy a macro symtab
 */
void macro_symtab_dealloc(macro_symtab_t* symtab){
	//Create temp/cursor for traversal
	symtab_macro_record_t* cursor = NULL;
	symtab_macro_record_t* temp;

	//Run through every single macro record
	for(u_int32_t i = 0; i < MACRO_KEYSPACE; i++){
		//Extract it
		cursor = symtab->records[i];

		//Run through any collision records
		while(cursor != NULL){
			//Reassign
			temp = cursor;

			//Advance it up
			cursor = cursor->next;

			//Deallocate both of the internal arrays if appropriate
			token_array_dealloc(&(temp->tokens));
			if(temp->parameters.internal_array != NULL){
				token_array_dealloc(&(temp->parameters));
			}

			//Dealloc
			free(temp);
		}
	}

	//At the very end free the overall control structure
	free(symtab);
}
