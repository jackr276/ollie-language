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

/**
 * Atomically increment and return the local constant id
 */
#define INCREMENT_AND_GET_LOCAL_CONSTANT_ID local_constant_id++

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

//Keep an atomically incrementing integer for the local constant ID
static u_int32_t local_constant_id = 0;

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
 * Initialize a symbol table for constants
 */
constants_symtab_t* constants_symtab_alloc(){
	//Simply allocate with the standard allocator
	constants_symtab_t* symtab = calloc(1, sizeof(constants_symtab_t));
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
static u_int64_t hash_variable(char* name){
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
static u_int64_t hash_constant(char* name){
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
	return hash & (CONSTANT_KEYSPACE - 1);
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
static u_int64_t hash_function(char* name){
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
static u_int64_t hash_type_name(char* type_name, mutability_type_t mutability){
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
static u_int64_t hash_array_type_name(char* type_name, u_int32_t num_members, mutability_type_t mutability){
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
static u_int64_t hash_type(generic_type_t* type){
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
 */
u_int8_t add_function_parameter(symtab_function_record_t* function_record, symtab_variable_record_t* variable_record){
	//We have too many parameters, fail out
	if(function_record->number_of_params == 6){
		return FAILURE;
	}

	//Store it in the function's parameters
	function_record->func_params[function_record->number_of_params] = variable_record;
	
	//Store what function this came from
	variable_record->function_declared_in = function_record;

	//Increment the count
	(function_record->number_of_params)++;

	//All went well
	return SUCCESS;
}

/**
 * Dynamically allocate a function record
*/
symtab_function_record_t* create_function_record(dynamic_string_t name, u_int8_t is_public, u_int8_t is_inlined, u_int32_t line_number){
	//Allocate it
	symtab_function_record_t* record = calloc(1, sizeof(symtab_function_record_t));

	//Allocate the data area internally
	stack_data_area_alloc(&(record->data_area));

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
	//
	//TODO INLINE HANDLING
	//
	record->signature = create_function_pointer_type(is_public, line_number, NOT_MUTABLE);

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
 * Dynamically allocate and create a constant record
 * NOTE: we just need the name here to make the hash
 */
symtab_constant_record_t* create_constant_record(dynamic_string_t name){
	//Allocate it
	symtab_constant_record_t* record = calloc(1, sizeof(symtab_constant_record_t));
	
	//Hash the name and store it
	record->hash = hash_constant(name.string);
	//Store the name
	record->name = name;
	//Everything else will be handled by caller, just give this back
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
 * Insert a constant into the symtab. This assumes that the user has already checked
 * to see if this constant exists or not
 *
 * RETURNS 0 if no collision, 1 if collision
 */
u_int8_t insert_constant(constants_symtab_t* symtab, symtab_constant_record_t* record){
	//Let's see if we have a collision or not
	if(symtab->records[record->hash] == NULL){
		//No collision, just store it and we're done here
		symtab->records[record->hash] = record;
		return 0;
	}

	//Otherwise there is a collision, so we'll need to store this new record
	//at the end of the linked list
	symtab_constant_record_t* cursor = symtab->records[record->hash];
	
	//So long as the next isn't null, we keep drilling
	while(cursor->next != NULL){
		cursor = cursor->next;
	}

	//Now that we're here, we append this guy onto the end
	cursor->next = record;
	//This should be null, but insurance never hurts
	record->next = NULL;

	//We had a collision here
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
		if(strcmp(record_cursor->func_name.string, name) == 0){
			return record_cursor;
		}
		//Advance it if we didn't have the right name
		record_cursor = record_cursor->next;
	}

	//When we make it down here, we found nothing so
	return NULL;
}


/**
 * Lookup the record in the constants symtab that corresponds to the following name, if
 * such a record exists. There is only one lexical scope for constants(global scope) so
 * this lookup should be quite easy
 */
symtab_constant_record_t* lookup_constant(constants_symtab_t* symtab, char* name){
	//First we'll grab the hash
	u_int64_t h = hash_constant(name);

	//Grab whatever record is at that hash
	symtab_constant_record_t* cursor = symtab->records[h];

	//So long as this isn't null, we'll do string comparisons to find our match. Recall
	//that it is possible to have collisions, so two records having the same hash is not 
	//always enough to know for sure
	while(cursor != NULL){
		if(strcmp(cursor->name.string, name) == 0){
			return cursor;
		}

		//Otherwise we keep moving
		cursor = cursor->next;
	}

	//If we made it here, that means there's no match, so return null
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
			if(strcmp(records_cursor->var_name.string, name) == 0){
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
		if(strcmp(records_cursor->var_name.string, name) == 0){
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
			if(strcmp(records_cursor->var_name.string, name) == 0){
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
			if(strcmp(records_cursor->type->type_name.string, name) == 0
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
 * Create a local constant and return the pointer to it
 */
local_constant_t* string_local_constant_alloc(generic_type_t* type, dynamic_string_t* value){
	//Dynamically allocate it
	local_constant_t* local_const = calloc(1, sizeof(local_constant_t));

	//Store the type as well
	local_const->type = type;

	//Copy the dynamic string in
	local_const->local_constant_value.string_value = clone_dynamic_string(value);

	//Now we'll add the ID
	local_const->local_constant_id = INCREMENT_AND_GET_LOCAL_CONSTANT_ID;

	//Store what type we have
	local_const->local_constant_type = LOCAL_CONSTANT_TYPE_STRING;

	//And finally we'll add it back in
	return local_const;
}


/**
 * Create an F32 local constant
 */
local_constant_t* f32_local_constant_alloc(generic_type_t* f32_type, float value){
	//Dynamically allocate it
	local_constant_t* local_const = calloc(1, sizeof(local_constant_t));

	//Store the type as well
	local_const->type = f32_type;

	//Copy the dynamic string in. We cannot print out floats directly, so we instead
	//use the bits that make up the float and cast them to an i32 *without rounding*
	local_const->local_constant_value.float_bit_equivalent = *((int32_t*)(&value));

	//Now we'll add the ID
	local_const->local_constant_id = INCREMENT_AND_GET_LOCAL_CONSTANT_ID;

	//Store what type we have
	local_const->local_constant_type = LOCAL_CONSTANT_TYPE_F32;

	//And finally we'll add it back in
	return local_const;
}


/**
 * Create an F32 local constant
 */
local_constant_t* f64_local_constant_alloc(generic_type_t* f64_type, double value){
	//Dynamically allocate it
	local_constant_t* local_const = calloc(1, sizeof(local_constant_t));

	//Store the type as well
	local_const->type = f64_type;

	//Copy the dynamic string in. We cannot print out floats directly, so we instead
	//use the bits that make up the float and cast them to an i32 *without rounding*
	local_const->local_constant_value.float_bit_equivalent = *((int64_t*)(&value));

	//Now we'll add the ID
	local_const->local_constant_id = INCREMENT_AND_GET_LOCAL_CONSTANT_ID;

	//Store what type we have
	local_const->local_constant_type = LOCAL_CONSTANT_TYPE_F64;

	//And finally we'll add it back in
	return local_const;
}


/**
 * Add a local constant to a function
 */
void add_local_constant_to_function(symtab_function_record_t* function, local_constant_t* constant){
	//Go based on what the possible values are
	switch(constant->local_constant_type){
		case LOCAL_CONSTANT_TYPE_STRING:
			//If we have no local constants, then we'll need to allocate
			//the array
			if(function->local_string_constants.internal_array == NULL){
				function->local_string_constants = dynamic_set_alloc();
			}

			dynamic_set_add(&(function->local_string_constants), constant);

			break;

		case LOCAL_CONSTANT_TYPE_F32:
			//If we have no local constants, then we'll need to allocate
			//the array
			if(function->local_f32_constants.internal_array == NULL){
				function->local_f32_constants = dynamic_set_alloc();
			}

			dynamic_set_add(&(function->local_f32_constants), constant);

			break;

		case LOCAL_CONSTANT_TYPE_F64:
			//If we have no local constants, then we'll need to allocate
			//the array
			if(function->local_f64_constants.internal_array == NULL){
				function->local_f64_constants = dynamic_set_alloc();
			}

			dynamic_set_add(&(function->local_f64_constants), constant);

			break;
	}
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
			if(strcmp(record_cursor->type->type_name.string, type_name) == 0){
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
			if(strcmp(record_cursor->type->type_name.string, type_name) == 0){
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
			if(strcmp(record_cursor->type->type_name.string, type_name) == 0
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
			if(strcmp(records_cursor->type->type_name.string, type->type_name.string) == 0){
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
 * Print the local constants(.LCx) that are inside of a function
 */
void print_local_constants(FILE* fl, symtab_function_record_t* record){
	//Let's first print the function's string constants
	if(record->local_string_constants.current_index != 0){
		//Print out what section we are in
		fprintf(fl, "\t.section .rodata.str1.1\n");

		//Run through every string constant
		for(u_int16_t i = 0; i < record->local_string_constants.current_index; i++){
			//Grab the constant out
			local_constant_t* constant = dynamic_set_get_at(&(record->local_string_constants), i);

			//Now print out every local string constant
			fprintf(fl, ".LC%d:\n\t.string \"%s\"\n", constant->local_constant_id, constant->local_constant_value.string_value.string);
		}
	}

	//Now print the f32 constants
	if(record->local_f32_constants.current_index != 0){
		//Print out that we are in the 4 byte prog-bits section
		fprintf(fl, "\t.section .rodata.cst4,\"aM\",@progbits,4\n");

		//Run through all constants
		for(u_int16_t i = 0; i < record->local_f32_constants.current_index; i++){
			//Grab the constant out
			local_constant_t* constant = dynamic_set_get_at(&(record->local_f32_constants), i);

			//Extract the floating point equivalent using the mask
			int32_t float_equivalent = constant->local_constant_value.float_bit_equivalent & 0xFFFFFFFF;

			//Otherwise, we'll begin to print, starting with the constant name
			fprintf(fl, "\t.align 4\n.LC%d:\n\t.long %d\n", constant->local_constant_id, float_equivalent);
		}
	}

	//Now print the f64 constants
	if(record->local_f64_constants.current_index != 0){
		//Print out that we are in the 8 byte prog-bits section
		fprintf(fl, "\t.section .rodata.cst8,\"aM\",@progbits,8\n");

		//Run through all constants
		for(u_int16_t i = 0; i < record->local_f64_constants.current_index; i++){
			//Grab the constant out
			local_constant_t* constant = dynamic_set_get_at(&(record->local_f64_constants), i);

			//These are in little-endian order. Lower 32 bits comes first, then the upper 32 bits
			int32_t lower32 = constant->local_constant_value.float_bit_equivalent & 0xFFFFFFFF;
			int32_t upper32 = (constant->local_constant_value.float_bit_equivalent >> 32) & 0xFFFFFFFF;

			//Otherwise, we'll begin to print, starting with the constant name
			fprintf(fl, "\t.align 4\n.LC%d:\n\t.long %d\n\t.long %d\n", constant->local_constant_id, lower32, upper32);
		}
	}
}


/**
 * Get a string local constant whose value matches the given constant
 *
 * Returns NULL if no matching constant can be found
 */
local_constant_t* get_string_local_constant(symtab_function_record_t* record, char* string_value){
	//Run through all of the local constants
	for(u_int16_t i = 0; i < record->local_string_constants.current_index; i++){
		//Extract the candidate
		local_constant_t* candidate = dynamic_set_get_at(&(record->local_string_constants), i);

		//If we have a match then we're good here, we'll return the candidate and leave
		if(strcmp(candidate->local_constant_value.string_value.string, string_value) == 0){
			return candidate;
		}
	}

	//If we get here we didn't find it
	return NULL;
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
 * Get an f32 local constant whose value matches the given constant
 *
 * Returns NULL if no matching constant can be found
 */
local_constant_t* get_f32_local_constant(symtab_function_record_t* record, float float_value){
	//Run through all of the local constants
	for(u_int16_t i = 0; i < record->local_f32_constants.current_index; i++){
		//Extract the candidate
		local_constant_t* candidate = dynamic_set_get_at(&(record->local_f32_constants), i);

		//We will be comparing the values at a byte level. We do not compare the raw values because
		//that would use FP comparison
		if(candidate->local_constant_value.float_bit_equivalent == *((u_int32_t*)&float_value)){
			return candidate;
		}
	}

	//If we get here we didn't find it
	return NULL;
}


/**
 * Get an f64 local constant whose value matches the given constant
 *
 * Returns NULL if no matching constant can be found
 */
local_constant_t* get_f64_local_constant(symtab_function_record_t* record, double double_value){
	//Run through all of the local constants
	for(u_int16_t i = 0; i < record->local_f64_constants.current_index; i++){
		//Extract the candidate
		local_constant_t* candidate = dynamic_set_get_at(&(record->local_f64_constants), i);

		//We will be comparing the values at a byte level. We do not compare the raw values because
		//that would use FP comparison
		if(candidate->local_constant_value.float_bit_equivalent == *((u_int64_t*)&double_value)){
			return candidate;
		}
	}

	//If we get here we didn't find it
	return NULL;
}


/**
 * Part of optimizer's mark and sweep - remove any local constants
 * with a reference count of 0
 */
void sweep_local_constants(symtab_function_record_t* record){
	//An array that marks given constants for deletion
	dynamic_array_t marked_for_deletion = dynamic_array_alloc();

	//Run through every string constant
	for(u_int16_t i = 0; i < record->local_string_constants.current_index; i++){
		//Grab the constant out
		local_constant_t* constant = dynamic_set_get_at(&(record->local_string_constants), i);

		//If we have no references, then this is marked for deletion
		if(constant->reference_count == 0){
			dynamic_array_add(&marked_for_deletion, constant);
		}
	}

	//Now run through the marked for deletion array, deleting as we go
	while(dynamic_array_is_empty(&marked_for_deletion) == FALSE){
		//Grab one to delete from the back
		local_constant_t* to_be_deleted = dynamic_array_delete_from_back(&marked_for_deletion);

		//Knock it out
		dynamic_set_delete(&(record->local_string_constants), to_be_deleted);
	}

	//Now do the exact same thing for f32's. We can reuse the same array
	for(u_int16_t i = 0; i < record->local_f32_constants.current_index; i++){
		//Grab the constant out
		local_constant_t* constant = dynamic_set_get_at(&(record->local_f32_constants), i);

		//If we have no references, then this is marked for deletion
		if(constant->reference_count == 0){
			dynamic_array_add(&marked_for_deletion, constant);
		}
	}

	//Now run through the marked for deletion array, deleting as we go
	while(dynamic_array_is_empty(&marked_for_deletion) == FALSE){
		//Grab one to delete from the back
		local_constant_t* to_be_deleted = dynamic_array_delete_from_back(&marked_for_deletion);

		//Knock it out
		dynamic_set_delete(&(record->local_f32_constants), to_be_deleted);
	}

	//Now do the exact same thing for f64's. We can reuse the same array
	for(u_int16_t i = 0; i < record->local_f64_constants.current_index; i++){
		//Grab the constant out
		local_constant_t* constant = dynamic_set_get_at(&(record->local_f64_constants), i);

		//If we have no references, then this is marked for deletion
		if(constant->reference_count == 0){
			dynamic_array_add(&marked_for_deletion, constant);
		}
	}

	//Now run through the marked for deletion array, deleting as we go
	while(dynamic_array_is_empty(&marked_for_deletion) == FALSE){
		//Grab one to delete from the back
		local_constant_t* to_be_deleted = dynamic_array_delete_from_back(&marked_for_deletion);

		//Knock it out
		dynamic_set_delete(&(record->local_f64_constants), to_be_deleted);
	}

	//Scrap this now that we're done with it
	dynamic_array_dealloc(&marked_for_deletion);
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
	for(u_int8_t i = 0; i < record->number_of_params; i++){
		//Print if it's mutable
		if(record->func_params[i]->type_defined_as->mutability == MUTABLE){
			printf("mut ");
		}

		printf("%s : %s", record->func_params[i]->var_name.string, record->func_params[i]->type_defined_as->type_name.string);
		//Comma if needed
		if(i < record->number_of_params-1){
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
 * A helper method for constant name printing
 * Intended for use by error messages
 */
void print_constant_name(symtab_constant_record_t* record){
	//First the record
	printf("\n---> %d | replace %s with ", record->line_number, record->name.string);
	
	generic_ast_node_t* const_node = record->constant_node;

	//We'll now switch based on what kind of constant that we have
	switch (const_node->constant_type) {
		case INT_CONST:
			printf("%d\n", const_node->constant_value.signed_int_value);
			break;
		case INT_CONST_FORCE_U:
			printf("%ud\n", const_node->constant_value.unsigned_int_value);
			break;
		case LONG_CONST_FORCE_U:
			printf("%ld\n", const_node->constant_value.unsigned_long_value);
			break;
		case LONG_CONST:
			printf("%ld\n", const_node->constant_value.signed_long_value);
			break;
		case CHAR_CONST:
			printf("%d\n", const_node->constant_value.char_value);
			break;
		case STR_CONST:
			printf("%s", const_node->string_value.string);
			break;
		case FLOAT_CONST:
			printf("%f\n", const_node->constant_value.float_value);
			break;
		case DOUBLE_CONST:
			printf("%fd\n", const_node->constant_value.float_value);
			break;
		//We should never get here
		default:
			break;
	}

	//Now print out the semicolon
	printf(";\n");
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
 * Print the call graph's adjacency matrix out for debugging
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

	fprintf(fl, "=============== Function Call Graph ========================\n");
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

			//Deallocation for local constants
			if(temp->local_string_constants.internal_array != NULL){
				//Deallocate each local constant
				for(u_int16_t i = 0; i < temp->local_string_constants.current_index; i++){
					local_constant_dealloc(dynamic_set_get_at(&(temp->local_string_constants), i));
				}

				//Then destroy the whole set 
				dynamic_set_dealloc(&(temp->local_string_constants));
			}

			//Deallocation for local float constants
			if(temp->local_f32_constants.internal_array != NULL){
				//Deallocate each local constant
				for(u_int16_t i = 0; i < temp->local_f32_constants.current_index; i++){
					local_constant_dealloc(dynamic_set_get_at(&(temp->local_f32_constants), i));
				}

				//Then destroy the whole set 
				dynamic_set_dealloc(&(temp->local_f32_constants));
			}

			//Deallocation for local float constants
			if(temp->local_f64_constants.internal_array != NULL){
				//Deallocate each local constant
				for(u_int16_t i = 0; i < temp->local_f64_constants.current_index; i++){
					local_constant_dealloc(dynamic_set_get_at(&(temp->local_f64_constants), i));
				}

				//Then destroy the whole array
				dynamic_set_dealloc(&(temp->local_f64_constants));
			}

			//Destroy the call graph infrastructure
			dynamic_set_dealloc(&(temp->called_functions));

			//Dealloate the function type
			type_dealloc(temp->signature);

			//Deallocate the data area itself
			stack_data_area_dealloc(&(temp->data_area));

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
static void variable_dealloc(symtab_variable_record_t* variable){
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
 * Destroy a constants symtab
 */
void constants_symtab_dealloc(constants_symtab_t* symtab){
	//Create a temp record and cursor for ourselves
	symtab_constant_record_t* cursor = NULL;
	symtab_constant_record_t* temp;

	//Run through every single record. If it isn't null, we free it
	for(u_int16_t i = 0; i < CONSTANT_KEYSPACE; i++){
		//Grab the record here
		cursor = symtab->records[i];

		//If this isn't NULL, we need to traverse the potential
		//linked list and free everything
		while(cursor != NULL){
			//Hold onto it
			temp = cursor;
			//Advance it up
			cursor = cursor->next;
			//Free the temp
			free(temp);
		}
	}

	//Once, we're done, free the overall thing
	free(symtab);
}


/**
 * Destroy a local constant
 */
void local_constant_dealloc(local_constant_t* constant){
	//Go based on the type
	switch(constant->local_constant_type){
		case LOCAL_CONSTANT_TYPE_STRING:
			//First we'll deallocate the dynamic string
			dynamic_string_dealloc(&(constant->local_constant_value.string_value));
			break;

		//If it's not a string then there's nothing to free
		default:
			break;
	}

	//Then we'll free the entire thing
	free(constant);
}
