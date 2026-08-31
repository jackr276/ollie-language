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
#include "../utils/stack/heapstack.h"
#include "../utils/constants.h"

//The starting offset basis for FNV-1a64
#define OFFSET_BASIS 14695981039346656037ULL

//The FNV prime for 64 bit hashes
#define FNV_PRIME 1099511628211ULL

//The finalizer constants for the avalance finalizer
#define FINALIZER_CONSTANT_1 0xff51afd7ed558ccdULL
#define FINALIZER_CONSTANT_2 0xc4ceb9fe1a85ec53ULL

/**
 * This dynamic string will be used for temporary array type
 * lookups. This is done to avoid repeated allocations. It will
 * be allocated/deallocated with the type symtab
 */
static dynamic_string_t temporary_array_name;

//Maintain both variable and type lexical scoping IDs
static u_int32_t variable_lexical_scope_id = 0;
static u_int32_t type_lexical_scope_id = 0;


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
 * Increment and return the current error id for the type symtab. This is done
 * so we're always able to differentiate errors when it comes time to handle
 * them at a function call site
 *
 * NOTE: error_id of 0 means no error, error_id of 1 means "error" so the generic error
 */
static inline u_int32_t increment_and_get_error_id(type_symtab_t* symtab){
	//Extract
	u_int32_t error_id = symtab->error_id;

	//Increment
	symtab->error_id++;

	//Return
	return error_id;
}


/**
 * Increment and get the current lexical scope for the variable
 */
static inline u_int32_t increment_and_get_variable_lexical_scope(){
	return variable_lexical_scope_id++;
}


/**
 * Increment and get the current lexical scope for the type 
 */
static inline u_int32_t increment_and_get_type_lexical_scope(){
	return type_lexical_scope_id++;
}


/**
 * Create a label table for us to use. These, unlike the other types of 
 * symbol tables, are created on-demand on a per-function basis
 */
label_symtab_t* label_symtab_alloc(){
	//As easy as allocating and returning
	label_symtab_t* symtab = calloc(1, sizeof(label_symtab_t));
	return symtab;
}


/**
 * Dynamically allocate a function symtab. Note that this allocation
 * automatically creates the default namespace
 */
function_symtab_t* function_symtab_alloc(){
	//First create the symtab
	function_symtab_t* symtab = (function_symtab_t*)calloc(1, sizeof(function_symtab_t));

	//Now the namespaces array
	symtab->namespaces = dynamic_array_alloc();

	//Allocate our id to function map
	symtab->id_to_function_mapping = dynamic_array_alloc();

	//Now let's create the very first sheaf
	function_namespace_t* default_namespace = calloc(1, sizeof(function_namespace_t));

	//This is the default sheaf
	default_namespace->is_default = TRUE;

	//Allcoate the namespace name
	default_namespace->namespace_name = dynamic_string_alloc();

	//We'll make the name "$default", something the user couldn't enter
	dynamic_string_set(&(default_namespace->namespace_name), "$default");

	//Add this into our namespaces 
	dynamic_array_add(&(symtab->namespaces), default_namespace);

	//The current value is this
	symtab->current = default_namespace;

	return symtab;
}


/**
 * Dynamically allocate a variable symtab
 */
variable_symtab_t* variable_symtab_alloc(){
	variable_symtab_t* symtab = (variable_symtab_t*)calloc(1, sizeof(variable_symtab_t));
	//We also need to allocate the sheafs array
	symtab->sheafs = dynamic_array_alloc();

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

	//Nothing has been initialized yet
	symtab->current = NULL;

	//The initial error id starts at 1. This is because 0 is reserved for NO_ERRORS,
	symtab->error_id = 1;

	//Allocate the array storage for our temporary lookups
	temporary_array_name = dynamic_string_alloc();

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
 * Initialize a symbol table for build system modules
 */
module_symtab_t* module_symtab_alloc(){
	module_symtab_t* symtab = calloc(1, sizeof(module_symtab_t));
	return symtab;
}


/**
 * Initialize the variable symbol table scope. It is possible that the function
 * we are contained in would be NULL for the global variable scope. We also
 * store the current namespace that the scope is in. Again it is completely
 * possible for that to be NULL if we don't have any namespaces
 */
void initialize_variable_scope(variable_symtab_t* symtab, symtab_function_record_t* function_contained_in, function_namespace_t* namespace_contained_in){
	//Allocate the current sheaf
	symtab_variable_sheaf_t* current = (symtab_variable_sheaf_t*)calloc(1, sizeof(symtab_variable_sheaf_t));

	//Add it to the array
	dynamic_array_add(&(symtab->sheafs), current);

	//Get the unique ID for this lexical scpoe
	current->lexical_scope_id = increment_and_get_variable_lexical_scope();

	//What function are we in for this sheaf?
	current->function_contained_in = function_contained_in;

	//Store the namespace that we're in
	current->namespace_contained_in = namespace_contained_in;

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

	//Get the unique ID for this lexical scpoe
	current->lexical_scope_id = increment_and_get_type_lexical_scope();

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
}


/**
 * Finalize the scope, for the purposes of this project, finalizing the scope just means going
 * up by one level
 */
void finalize_type_scope(type_symtab_t* symtab){
	//Back out of this one as it's finalized
	symtab->current = symtab->current->previous_level;
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
	return hash & (MODULE_KEYSPACE - 1);
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
static inline u_int64_t hash_module_name(char* name){
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
static inline u_int64_t hash_label_name(char* name){
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
	return hash & (USER_DEFINED_LABELED_BLOCK_KEYSPACE - 1);
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
symtab_variable_record_t* create_variable_record(dynamic_string_t* name, symtab_function_record_t* function_declared_in, dependency_graph_node_t* node_defined_in, u_int32_t line_number, u_int32_t token_index){
	//Allocate it
	symtab_variable_record_t* record = calloc(1, sizeof(symtab_variable_record_t));

	//Store the name
	record->var_name = *name;
	//Hash it and store it to avoid to repeated hashing
	record->hash = hash_variable(name->string);

	//This is just a regular variable(for now)
	record->membership = NO_MEMBERSHIP;

	//Store all of this information for eventual error printing
	record->node_defined_in = node_defined_in;
	record->line_number = line_number;
	record->token_index_of_definition = token_index;

	/**
	 * IMPORTANT: since we've never made a three address variable
	 * for this, we're going to set the variable ID's to -1
	 * as a flag that they've never been set
	 */
	record->associated_three_addr_var_ids.variable_id = NEVER_SET;
	record->associated_three_addr_var_ids.memory_address_variable_id = NEVER_SET;

	/**
	 * Very Important: it is mandatory that we know what function this variable is in. If it
	 * is a global variable/type variable then use NULL
	 */
	record->function_declared_in = function_declared_in;

	//These are user defined
	record->is_user_defined = TRUE;

	//For eventual SSA generation
	record->counter_stack.stack = NULL;
	record->counter_stack.top_index = 0;
	record->counter_stack.current_size = 0;

	return record;
}


/**
 * Dealias the given variable record until we cannot dealias it anymore. Most of the time
 * variable records are not aliased, this is just as insurance for the function parameter
 * case where we do have it
 */
static inline symtab_variable_record_t* dealias_variable(symtab_variable_record_t* record){
	//Holder for our current level
	symtab_variable_record_t* current = record;

	//Work our way up the aliasing tree until we hit a root
	while(current->aliases != NULL){
		current = current->aliases;
	}

	//Give back whatever we got
	return current;
}


/**
 * Create a global variable record
 */
symtab_variable_record_t* create_global_variable_record(dynamic_string_t* name, dependency_graph_node_t* node_defined_in, u_int32_t line_number, u_int32_t token_index, visibilty_type_t visibility){
	//Allocate it
	symtab_variable_record_t* record = calloc(1, sizeof(symtab_variable_record_t));

	//Store the name
	record->var_name = *name;
	//Hash it and store it to avoid to repeated hashing
	record->hash = hash_variable(name->string);

	//Flag that this is a global variable
	record->membership = GLOBAL_VARIABLE;

	//Store all of this information for eventual error printing
	record->node_defined_in = node_defined_in;
	record->line_number = line_number;
	record->token_index_of_definition = token_index;

	/**
	 * IMPORTANT: since we've never made a three address variable
	 * for this, we're going to set the variable ID's to -1
	 * as a flag that they've never been set
	 */
	record->associated_three_addr_var_ids.variable_id = NEVER_SET;
	record->associated_three_addr_var_ids.memory_address_variable_id = NEVER_SET;

	//Store the visibility level
	record->visibility = visibility;

	//These are user defined
	record->is_user_defined = TRUE;

	//For eventual SSA generation
	record->counter_stack.stack = NULL;
	record->counter_stack.top_index = 0;
	record->counter_stack.current_size = 0;

	return record;
}


/**
 * Get the true variable name - this goes around any aliasing
 * to ensure that we are always grabbing the actual underlying
 * variable name
 */
char* get_true_variable_name(symtab_variable_record_t* variable){
	//First dealias it
	variable = dealias_variable(variable);

	//Then give back a pointer to the name
	return variable->var_name.string;
}


/**
 * Create a static variable record. These variables are really global vars
 */
symtab_variable_record_t* create_static_variable_record(dynamic_string_t* name, symtab_function_record_t* function_declared_in, dependency_graph_node_t* node_defined_in, u_int32_t line_number, u_int32_t token_index){
	//Allocate it
	symtab_variable_record_t* record = calloc(1, sizeof(symtab_variable_record_t));

	//Store the name
	record->var_name = *name;
	//Hash it and store it to avoid to repeated hashing
	record->hash = hash_variable(name->string);

	/**
	 * This may change - but for right now we'll have a static variable mangler of 0. This
	 * will be used by the CFG in case we have a bunch
	 */
	record->static_variable_mangler = 0;

	/**
	 * IMPORTANT: since we've never made a three address variable
	 * for this, we're going to set the variable ID's to -1
	 * as a flag that they've never been set
	 */
	record->associated_three_addr_var_ids.variable_id = NEVER_SET;
	record->associated_three_addr_var_ids.memory_address_variable_id = NEVER_SET;

	//Store all of this information for eventual error printing
	record->node_defined_in = node_defined_in;
	record->line_number = line_number;
	record->token_index_of_definition = token_index;

	//Flag this as static
	record->membership = STATIC_VARIABLE;

	//These are always private
	record->visibility = VISIBILITY_TYPE_PRIVATE;

	//These are user defined
	record->is_user_defined = TRUE;

	//Store the function that we were declared in
	record->function_declared_in = function_declared_in;

	//For eventual SSA generation
	record->counter_stack.stack = NULL;
	record->counter_stack.top_index = 0;
	record->counter_stack.current_size = 0;

	return record;
}


/**
 * Create a variable for a memory address that is not from an actual var
 */
symtab_variable_record_t* create_temp_memory_address_variable(symtab_function_record_t* function, generic_type_t* type, variable_symtab_t* variable_symtab, stack_region_t* stack_region, u_int32_t temp_id){
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
	symtab_variable_record_t* record = create_variable_record(&string, function, NULL, 0, 0);
	//Store the type here
	record->type_defined_as = type;

	//Store the stack region too
	record->stack_region = stack_region;

	//These are not user defined
	record->is_user_defined = FALSE;
	
	//Insert this into the variable symtab
	insert_variable(variable_symtab, record);

	//For eventual SSA generation
	record->counter_stack.stack = NULL;
	record->counter_stack.top_index = 0;
	record->counter_stack.current_size = 0;

	//And give it back
	return record;
}


/**
 * Create and return a ternary variable(we also say
 * these are SSA compatible temp vars to be generic). A ternary variable is halfway
 * between a temp and a full fledged non-temp variable. It will have a 
 * symtab record, and as such will be picked up by the phi function
 * inserted. It will also not be declared as temp
 */
symtab_variable_record_t* create_ssa_compatible_temp_var(symtab_function_record_t* function, generic_type_t* type, variable_symtab_t* variable_symtab, u_int32_t temp_id){
	/**
	 * And here is the special part - we'll need to make a symtab record
	 * for this variable and add it in
	 */
	char variable_name[100];

	/**
	 * Grab a new temp var number from here. We use the
	 * ^ because it is illegal for variables typed in by the
	 * user to have that, so we will not have collisions
	 */
	sprintf(variable_name, "^t%d", temp_id);

	//Create and set the name here
	dynamic_string_t string = dynamic_string_alloc();
	dynamic_string_set(&string, variable_name);

	//Now create and add the symtab record for this variable
	symtab_variable_record_t* record = create_variable_record(&string, function, NULL, 0, 0);
	//Store the type here
	record->type_defined_as = type;

	//Insert this into the variable symtab
	insert_variable(variable_symtab, record);

	//These are not user defined
	record->is_user_defined = FALSE;

	//For eventual SSA generation
	record->counter_stack.stack = NULL;
	record->counter_stack.top_index = 0;
	record->counter_stack.current_size = 0;

	//And give it back
	return record;
}


/**
 * Create a return by copy variable record. This will simply be a variable that is a temporary variable with a return by copy
 * membership
 */
symtab_variable_record_t* create_return_by_copy_variable(symtab_function_record_t* function, generic_type_t* type, variable_symtab_t* variable_symtab, u_int32_t temp_id){
	/**
	 * And here is the special part - we'll need to make a symtab record
	 * for this variable and add it in
	 */
	char variable_name[100];

	/**
	 * Grab a new temp var number from here. We use the
	 * ^ because it is illegal for variables typed in by the
	 * user to have that, so we will not have collisions
	 */
	sprintf(variable_name, "^t%d", temp_id);

	//Create and set the name here
	dynamic_string_t string = dynamic_string_alloc();
	dynamic_string_set(&string, variable_name);

	//Now create and add the symtab record for this variable
	symtab_variable_record_t* record = create_variable_record(&string, function, NULL, 0, 0);
	//Store the type here
	record->type_defined_as = type;

	//Insert this into the variable symtab
	insert_variable(variable_symtab, record);

	//These are not user defined
	record->is_user_defined = FALSE;

	//The membership is a return by copy var - special kind
	record->membership = RETURN_BY_COPY_PARAMETER;

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
symtab_variable_record_t* create_parameter_alias_variable(symtab_function_record_t* function, symtab_variable_record_t* aliases, variable_symtab_t* variable_symtab, u_int32_t temp_id){
	/**
	 * Grab a new temp var number from here. We use the
	 * ^ because it is illegal for variables typed in by the
	 * user to have that, so we will not have collisions
	 */
	char variable_name[100];
	sprintf(variable_name, "^t%d", temp_id);

	//Create and set the name here
	dynamic_string_t string = dynamic_string_alloc();
	dynamic_string_set(&string, variable_name);

	//Now create and add the symtab record for this variable
	symtab_variable_record_t* record = create_variable_record(&string, function, aliases->node_defined_in, aliases->line_number, aliases->token_index_of_definition);
	//Store the type here
	record->type_defined_as = aliases->type_defined_as;

	//Copy over the stack info as well - this is important for references
	record->stack_region = aliases->stack_region;
	record->stack_variable = aliases->stack_variable;

	/**
	 * We are either aliasing function parameters or the specialized
	 * return by copy parameter, flag this variable as either or
	 * depending on which is appropriate
	 */
	if(aliases->membership == FUNCTION_PARAMETER){
		record->membership = FUNCTION_PARAMETER_ALIAS;
	} else {
		record->membership = RETURN_BY_COPY_PARAMETER_ALIAS;
	}

	//These are user defined in a way
	record->is_user_defined = TRUE;

	/**
	 * Record that this record aliases the given variable so
	 * that, in the future, if we need to drill down and get
	 * it we will be able to
	 */
	record->aliases = aliases;

	//Insert this into the variable symtab
	insert_variable(variable_symtab, record);

	//For eventual SSA generation
	record->counter_stack.stack = NULL;
	record->counter_stack.top_index = 0;
	record->counter_stack.current_size = 0;

	//And give it back
	return record;
}


/**
 * Does the given value need to be passed by the stack or not? This is 
 * going to depend largely on if it's a floating point value or not *and*
 * if the current number of parameters is above the amount that are allowed. This
 * value is different for float/gp
 *
 * We also by default pass all structs and unions by copy, so if we see that
 * this variable is one of them we'll also need to make space for it
 */
static inline u_int8_t is_parameter_stack_passed(symtab_variable_record_t* variable){
	switch(variable->type_defined_as->type_class){
		case TYPE_CLASS_UNION:
		case TYPE_CLASS_STRUCT:	
			return TRUE;
		default:
			//Non-float our max is 6 register
			if(IS_FLOATING_POINT(variable->type_defined_as) == FALSE){
				return variable->class_relative_function_parameter_order > MAX_GP_REGISTER_PASSED_PARAMS ? TRUE : FALSE;
			//If it is a float then our max is 8 registers
			} else {
				return variable->class_relative_function_parameter_order > MAX_SSE_REGISTER_PASSED_PARAMS ? TRUE : FALSE;
			}
	}
}


/**
 * Helper functio to create a stack region for a given function parameter
 *
 * NOTE: This assumes that the stack data area has already been properly allocated
 */
static inline void setup_stack_region_for_function_parameter(stack_data_area_t* parameter_data_area, symtab_variable_record_t* parameter){
	//Equivalent pointer type for arrays
	generic_type_t* equivalent_pointer_type;

	//Special adjustments based on the types we have
	switch(parameter->type_defined_as->type_class){
		/**
		 * Array types are always passed by reference. We need to make sure that
		 * we represent this accurately inside of the stack region. We can use an
		 * equivalent pointer type from the conversion helper to do this
		 */
		case TYPE_CLASS_ARRAY:
			//Create the equivalent pointer type
			equivalent_pointer_type = convert_array_type_to_equivalent_pointer(parameter->type_defined_as);

			//Add this type into said stack region
			parameter->stack_region = create_stack_region_for_type(parameter_data_area, equivalent_pointer_type);
			break;
		
		default:
			//Add this type into said stack region
			parameter->stack_region = create_stack_region_for_type(parameter_data_area, parameter->type_defined_as);
			break;
	}

	//This is a stack variable, we need to note it as such
	parameter->stack_variable = TRUE;

	//Flag that this is passed via the stack
	parameter->passed_by_stack = TRUE;
}


/**
 * Add a parameter to a function and perform all internal bookkeeping needed
 *
 * *Stack Parameters*
 * Every function internally maintains a stack structure *separate* from the local stack that is used for
 * passing function parameters via the stack. If we notice that we are adding a function parameter that
 * is more than the max per class register passing value, we will add that into the specialized stack
 * data area
 *
 * NOTE: In the event that we have an elaborative stack param, we will need to account for a different stack every
 * time. The only real constant here is the bottom 4 bytes which are effectively our "count" for the number of parameters
 */
void add_function_parameter(symtab_function_record_t* function_record, symtab_variable_record_t* variable_record){
	//Store it in the function's parameters
	dynamic_array_add(&(function_record->function_parameters), variable_record);

	//Extract the signature for ease of use
	function_type_t* function_signature = function_record->signature->internal_types.function_type;
	
	//Store what function this came from
	variable_record->function_declared_in = function_record;

	/**
	 * If we have an elaborative param type, this requires special handling on
	 * our part to get right, including the creation of/conversion to a 
	 * dynamic stack type
	 */
	if(variable_record->type_defined_as->type_class == TYPE_CLASS_ELABORATIVE){
		//If we don't have a stack, let's allocate it
		if(function_record->stack_passed_parameters.stack_regions.internal_array == NULL){
			//This is specifically a parameter passing stack region. We must be sure to mention that
			stack_data_area_alloc(&(function_record->stack_passed_parameters), STACK_TYPE_PARAMETER_PASSING, STACK_DATA_AREA_SIZE_TYPE_DYNAMIC);

		//If it's not NULL, we need to convert the size type to dynamic because this is a dynamic stack now
		} else {
			function_record->stack_passed_parameters.size_type = STACK_DATA_AREA_SIZE_TYPE_DYNAMIC;
		}

		//Create the special stack region for our elaborative param type
		variable_record->stack_region = create_stack_region_for_type(&(function_record->stack_passed_parameters), variable_record->type_defined_as);

		//This is passed via the stack
		variable_record->passed_by_stack = TRUE;

		//This function does contain stack variables
		function_signature->contains_stack_params = TRUE;

		//Flag that this contains the special elaborative stack param
		function_signature->contains_elaborative_stack_param = TRUE;

	//Do we need to pass via stack? If so add it here
	} else if(is_parameter_stack_passed(variable_record) == TRUE){
		//Allocate it if need be
		if(function_record->stack_passed_parameters.stack_regions.internal_array == NULL){
			//This is specifically a parameter passing stack region. We must be sure to mention that
			stack_data_area_alloc(&(function_record->stack_passed_parameters), STACK_TYPE_PARAMETER_PASSING, STACK_DATA_AREA_SIZE_TYPE_STATIC);
		}

		//Let the helper deal with the setup
		setup_stack_region_for_function_parameter(&(function_record->stack_passed_parameters), variable_record);

		//Flag that this function contains stack params
		function_signature->contains_stack_params = TRUE;
	}
}


/**
 * Since a returned-by-copy value will *always* have the memory address to copy to
 * passed into the function via %rdi, it is essential that we go through and update
 * the symtab_function_record here as well as all of the parameters. Edge case that
 * we are looking out for: if we had 6 GP params, now we have 7, and the last one
 * is pushed over the edge to be a stack param. We need to make the adjustment for all
 * of them, as well as for their function_parameter_order
 */
void remediate_return_by_copy_gp_parameters(symtab_function_record_t* record, function_type_t* signature){
	for(int32_t i = 0; i < record->function_parameters.current_index; i++){
		//Grab the parameter out
		symtab_variable_record_t* parameter = dynamic_array_get_at(&(record->function_parameters), i);

		//Stack passed parameters do not effect the count so skip it
		if(is_parameter_stack_passed(parameter) == TRUE){
			continue;
		}

		//Floating point params also do not impact it
		if(IS_FLOATING_POINT(parameter->type_defined_as) == TRUE){
			continue;
		}

		/**
		 * If we have a parameter that is at the limit(6 for GP registers), we are now going to
		 * need to push it over the edge to make room for the return address passed in via
		 * %rdi to copy to
		 */
		if(parameter->class_relative_function_parameter_order == MAX_GP_REGISTER_PASSED_PARAMS){
			//Flag that the function itself now contains stack parameters
			signature->contains_stack_params = TRUE;

			/**
			 * If we don't yet have a stack data area then we need to allocate
			 * one now
			 */
			if(record->stack_passed_parameters.stack_regions.internal_array == NULL){
				stack_data_area_alloc(&(record->stack_passed_parameters), STACK_TYPE_PARAMETER_PASSING, STACK_DATA_AREA_SIZE_TYPE_STATIC);
			}

			setup_stack_region_for_function_parameter(&(record->stack_passed_parameters), parameter);
		}

		//Regardless of what happened, bump the class relative function parameter order
		parameter->class_relative_function_parameter_order++;
	}
}


/**
 * Dynamically allocate a function record
*/
symtab_function_record_t* create_function_record(dynamic_string_t* name, dependency_graph_node_t* dependency_contained_in, visibilty_type_t visibility, u_int8_t is_inlined, u_int8_t raises_errors, u_int32_t line_number, u_int32_t token_index){
	//Allocate it
	symtab_function_record_t* record = calloc(1, sizeof(symtab_function_record_t));

	//Allocate the data area internally
	stack_data_area_alloc(&(record->local_stack), STACK_TYPE_FUNCTION_LOCAL, STACK_DATA_AREA_SIZE_TYPE_STATIC);

	//Allocate the array for all function blocks
	record->function_blocks = dynamic_array_alloc();

	//Allocate space for the function parameter
	record->function_parameters = dynamic_array_alloc();

	//Copy the name over
	record->func_name = *name;
	//Hash it and store it to avoid to repeated hashing
	record->hash = hash_function(name->string);

	//Throw in whether or not it's public or private
	record->visibility = visibility;

	//Store the line number
	record->line_number = line_number;

	//Allocate the list of all functions that this calls
	record->called_functions = dynamic_set_alloc();

	//Store what dependency this comes from
	record->dependency_graph_node = dependency_contained_in;

	//We know that we need to create this immediately
	record->signature = create_function_pointer_type(visibility, is_inlined, line_number, raises_errors, NOT_MUTABLE);

	/**
	 * IMPOTANT - for error printing, we will store the function's token index of definition here
	 */
	record->token_index_of_definition = token_index;

	//And give it back
	return record;
}

/**
 * Create a namespace record and add it into the symtab. This will create the new namespace as a
 * child of the current one
 */
function_namespace_t* create_namespace_record(function_symtab_t* symtab, char* name){
	//Allocate it
	function_namespace_t* namespace = calloc(1, sizeof(function_namespace_t));

	//It's not the default
	namespace->is_default = FALSE;

	//Allocate the name
	namespace->namespace_name = dynamic_string_alloc();

	//Set the name in
	dynamic_string_set(&(namespace->namespace_name), name);

	//If this needs to be allocated then we will do so
	if(symtab->current->child_namespaces.internal_array == NULL){
		symtab->current->child_namespaces = dynamic_array_alloc();
	}

	//This namespace is going to be added as the child of the current one
	dynamic_array_add(&(symtab->current->child_namespaces), namespace);

	//Flag the current namespace as this one's parent
	namespace->parent_namespace = symtab->current;

	//Also add this to our overall array of all namespaces
	dynamic_array_add(&(symtab->namespaces), namespace);
	
	//Now give it back
	return namespace;
}


/**
 * Enter into a namespace. This namespace is now the current namespace until exit_namespace()
 * is called, in which case it goes back to its parent namespace
 */
void enter_namespace(function_symtab_t* symtab, function_namespace_t* new_namespace){
	symtab->current = new_namespace;
}


/**
 * Exit out of the current namespace by going to its parent
 */
void exit_namespace(function_symtab_t* symtab){
	//This is bad if we're trying to do it
	if(symtab->current->parent_namespace == NULL){
		fprintf(stderr, "Fatal internal compiler error: attempt to exit the parent namespace\n");
		exit(1);
	}

	//Go back up the chain into this one's parent namespace
	symtab->current = symtab->current->parent_namespace;
}


/**
 * Set the current namespace to be a given record. This should be used when we need to jump
 * multiple namespaces at a time
 */
void set_current_namespace(function_symtab_t* symtab, function_namespace_t* new_current_namespace){
	//Just to be safe here
	if(new_current_namespace == NULL){
		fprintf(stderr, "Fatal internal compiler error: attempt to enter a null namespace\n");
		exit(1);
	}

	//Just set current to this
	symtab->current = new_current_namespace;
}


/**
 * Set the current lexical scope be a given record. This should be used when we need to jump
 * multiple scopes at a time
 */
void set_current_lexical_scope(variable_symtab_t* symtab, symtab_variable_sheaf_t* new_lexical_scope){
	//Just to be safe here
	if(new_lexical_scope == NULL){
		fprintf(stderr, "Fatal internal compiler error: attempt to enter a null lexical scope\n");
		exit(1);
	}

	//This now is the current scope
	symtab->current = new_lexical_scope;
}


/**
 * Set the current type scope be a given record. This should be used when we need to jump
 * multiple scopes at a time
 */
void set_current_type_scope(type_symtab_t* symtab, symtab_type_sheaf_t* new_type_scope){
	//Just to be safe
	if(new_type_scope == NULL){
		fprintf(stderr, "Fatal internal compiler error: attempt to enter a null type scope\n");
		exit(1);
	}

	//Set this as the current scope
	symtab->current = new_type_scope;
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
 * Create a module record for the module table
 *
 * The module itself will contain a dependency graph node that corresponds
 * to the file name. The file name itself is stored once inside of the
 * dependency graph node already
 *
 * NOTE: this creation process will always create a clone of the given file name
 */
symtab_module_record_t* create_module_record(dependency_graph_node_t* dependency_graph_node){
	symtab_module_record_t* record = calloc(1, sizeof(symtab_module_record_t));

	//Get the hash from the module name in the dependency graph
	record->hash = hash_module_name(dependency_graph_node->module_name.string);

	//Store the dependency graph node
	record->dependency_graph_node = dependency_graph_node;

	record->next = NULL;
	return record;
}


/**
 * Create a label record for the label symtab
 *
 * NOTE: The label symtab assumes ownership of the name dynamic string
 */
symtab_label_record_t* create_label_record(dynamic_string_t* name, u_int32_t line_number){
	//Allocate the needed space
	symtab_label_record_t* label_record = calloc(1, sizeof(symtab_label_record_t));

	//Hash the label name - it is assumed that the creation always does this
	label_record->hash = hash_label_name(name->string);

	/**
	 * IMPORTANT: we assume complete ownership of the name here
	 */
	label_record->name = *name;

	//Line number for any/all error reporting
	label_record->line_number = line_number;

	//And that's about all - very simple record type
	return label_record;
}


/**
 * Insert a record into the function symbol table. This assumes that the user
 * has already checked to see if this record exists in the table
 *
 * RETURNS 0 if no collision, 1 if collision
 */
u_int8_t insert_function(function_symtab_t* symtab, symtab_function_record_t* record){
	/**
	 * Assign this a unique identifier. Once we've assigned the unique ID, bump the
	 * overall function ID for the next go around
	 */
	record->function_id = symtab->current_function_id;
	(symtab->current_function_id)++;

	/**
	 * We maintain a one-to-one mapping of index as function ID to function record
	 * pointer. This allows for quick lookups when we have to use the adjacency
	 * matrix to determine things
	 */
	dynamic_array_set_at(&(symtab->id_to_function_mapping), record, record->function_id);

	/**
	 * If this is an inlined function, we need to bump the inlined function count
	 * inside of the parent symtab struct
	 */
	if(record->signature->internal_types.function_type->is_inlined == TRUE){
		(symtab->inlined_function_count)++;
	}

	//Grab the current namespace
	function_namespace_t* current = symtab->current;

	//Store that this function is in this current namespace
	record->namespace_contained_in = current;

	//Get the record(or lack of one) at this hash
	symtab_function_record_t* cursor = current->records[record->hash];

	//If there's no collision
	if(cursor == NULL){
		//Store it and get out
		current->records[record->hash] = record;

		//No collision
		return 0;
	}

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
	record->next = NULL;

	//We did indeed have a collision here
	return 1;
}


/**
 * Insert a module into the symtab
 */
u_int8_t insert_module(module_symtab_t* symtab, symtab_module_record_t* record){
	//Grab a cursor to whatever is in the hash's spot
	symtab_module_record_t* cursor = symtab->records[record->hash];

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
	record->next = NULL;

	//We did indeed have a collision here
	return 1;
}


/**
 * Insert a label into the symtab
 *
 * NOTE: we assume that the hash has already been computed as part of record creation
 */
u_int8_t insert_label(label_symtab_t* label_symtab, symtab_label_record_t* label_record){
	//Grab the record(or NULL value) at this hash value
	symtab_label_record_t* cursor = label_symtab->records[label_record->hash];

	/**
	 * Option 1: we have no collision. If this is the case then we will just insert
	 * the label here and return 0 to indicate that nothing collided
	 */
	if(cursor == NULL){
		label_symtab->records[label_record->hash] = label_record;
		return 0;
	}

	/**
	 * Option 2: there is a record here, so we need to advance down the
	 * chain until there isn't one. Once we get to the bottom, we attach
	 * the next record to the very end
	 */
	while(cursor->next != NULL){
		cursor = cursor->next;
	}

	//Add it in
	cursor->next = label_record;
	//Just to be extra safe
	label_record->next = NULL;

	//1 signifies that there was a collision
	return 1;
}


/**
 * Inserts a variable record into the symtab. This assumes that the user has already checked to see if
 * this record exists in the table
 */
u_int8_t insert_variable(variable_symtab_t* symtab, symtab_variable_record_t* record){
	//Grab the record(or lack of one) at the hash
	symtab_variable_record_t* cursor = symtab->current->records[record->hash];

	//Store the lexical scope where it's in
	record->lexical_scope_id = symtab->current->lexical_scope_id;

	//No collision here, just store and get out
	if(cursor == NULL){
		//Store this and get out
		symtab->current->records[record->hash] = record;
		//0 = success, no collision
		return 0;
	}

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
	//Grab the record(or lack of one) at the current hash
	symtab_type_record_t* cursor = symtab->current->records[record->hash];

	//Store the lexical scope it
	record->lexical_scope_id =  symtab->current->lexical_scope_id;

	/**
	 * If we have an error type, we need to keep track of what the error id for this
	 * type is. This is done so we can uniquely identify errors down the road
	 * via number
	 */
	if(record->type->type_class == TYPE_CLASS_ERROR){
		//Update it internally here
		record->type->internal_types.error_type_id = increment_and_get_error_id(symtab);
	}

	//No collision here, just store and get out
	if(cursor == NULL){
		//Store this and get out
		symtab->current->records[record->hash] = record;
		//0 = success, no collision
		return 0;
	}

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

	//Save these for size type creation
	generic_type_t* immut_u32;
	generic_type_t* mut_u32;

	generic_type_t* type;

	//Add in void type
	type = create_basic_type("void", VOID, NOT_MUTABLE);
	num_collisions += insert_type(symtab, create_type_record(type));

	//Create the immutable void*
	type = create_pointer_type(type, 0, NOT_MUTABLE);
	num_collisions += insert_type(symtab, create_type_record(type));

	//Add in void type
	type = create_basic_type("void", VOID, MUTABLE);
	num_collisions += insert_type(symtab, create_type_record(type));

	//Create the mutable void*
	type = create_pointer_type(type, 0, NOT_MUTABLE);
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
	immut_u32 = type;
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

	//size type
	type = create_size_type(immut_u32, NOT_MUTABLE);
	num_collisions += insert_type(symtab, create_type_record(type));
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
	mut_u32 = type;
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

	//size type
	type = create_size_type(mut_u32, MUTABLE);
	num_collisions += insert_type(symtab, create_type_record(type));
	// ================================ Mutable versions of our primitive types ==============================
	
	// ================================ Specialized internal-only types ======================================
	// F128 DOUBLE_QUAD_WORD type for memory copying - mangle the name so that a user could never get this
	type = create_basic_type("&double_quad_word", F128, NOT_MUTABLE);
	num_collisions += insert_type(symtab, create_type_record(type));
	
	// ================================ Specialized internal-only types ======================================

	/**
	 * For any/all generic error handling, we need the ability to have a generic error type. In practice
	 * this is represented by the "error" lexitem, but we need a type anyway. As such we'll have a purely internal
	 * type here that has that name. Since the error id starts counting at one and this is the first thing that will
	 * hit it, we will be good here
	 */
	type = create_error_type("error", 0);
	num_collisions += insert_type(symtab, create_type_record(type));

	/**
	 * This is for observability in the test suites - if we have 
	 * more than 1 or 2 collisions here, then we have a serious problem
	 */
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

	//Stack pointer has no current function
	symtab_variable_record_t* stack_pointer = create_variable_record(&variable_name, NULL, NULL, 0, 0);
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

	//Instruction pointer has no given function
	symtab_variable_record_t* instruction_pointer = create_variable_record(&variable_name, NULL, NULL, 0, 0);
	//Set this type as a label(address)
	instruction_pointer->type_defined_as = lookup_type_name_only(types, "u64", NOT_MUTABLE)->type;

	//Give it back
	return instruction_pointer;
}


/**
 * Lookup the record in the symtab that corresponds to the following name.
 *
 * Our lookup is always biased to the most local sheaf first, and then up the
 * chain as we go
 */
symtab_function_record_t* lookup_function(function_symtab_t* symtab, char* name){
	//Let's grab it's hash
	u_int64_t h = hash_function(name); 

	//Get a cursor for the namespace
	function_namespace_t* namespace_cursor = symtab->current;

	//Keep crawling our way up until we find it
	do {
		//Grab whatever record is at that hash
		symtab_function_record_t* record_cursor = namespace_cursor->records[h];

		//We could have had collisions so we'll have to hunt here
		while(record_cursor != NULL){
			//If we find the right one, then we can get out
			if(strncmp(record_cursor->func_name.string, name, record_cursor->func_name.current_length) == 0){
				return record_cursor;
			}
			//Advance it if we didn't have the right name
			record_cursor = record_cursor->next;
		}

		//If we didn't find it then we'll go up the chain by one
		namespace_cursor = namespace_cursor->parent_namespace;

	//Keep going so long as we aren't NULL
	} while(namespace_cursor != NULL);

	//When we make it down here, we found nothing so
	return NULL;
}


/**
 * Lookup a function that needs to be in the given namespace. This will
 * not do the normal logic where we can crawl up to see if it's in a parent
 * namespace
 */
symtab_function_record_t* lookup_function_in_namespace(function_namespace_t* namespace_to_search, char* name){
	//Let's grab it's hash
	u_int64_t h = hash_function(name); 

	//Grab whatever record is at that hash
	symtab_function_record_t* record_cursor = namespace_to_search->records[h];

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
 * Use the ID to function mapping to lookup a function for a given ID. This will return
 * NULL if the function cannot be found
 */
symtab_function_record_t* get_function_by_id(function_symtab_t* symtab, int32_t id){
	/**
	 * Just to be sure we aren't looking for something crazy
	 */
	if(id >= symtab->id_to_function_mapping.current_max_size){
		fprintf(stderr, "Fatal internal compiler error: attempt to find function with id %d when highest id is %d", id, symtab->current_function_id);
	}

	//This can be NULL
	return dynamic_array_get_at(&(symtab->id_to_function_mapping), id);
}

/**
 * Lookup a global variable that needs to be in the given namespace. This will
 * not do the normal logic where we can crawl up to see if it's in a parent
 * namespace
 */
symtab_variable_record_t* lookup_variable_in_namespace(function_namespace_t* namespace_to_search, char* name){
	//Get the variable hash
	u_int64_t h = hash_variable(name);

	//Extract the related sheaf and then get a cursor using the hash
	symtab_variable_sheaf_t* sheaf_to_search = namespace_to_search->related_variable_sheaf;
	symtab_variable_record_t* cursor = sheaf_to_search->records[h];

	//So long as we haven't hit the end and don't have a match
	while(cursor != NULL){
		//If it's an exact match we're good
		if(strncmp(cursor->var_name.string, name, cursor->var_name.current_length) == 0){
			return cursor;
		}

		//Advance to the next record
		cursor = cursor->next;
	}

	//If we made it down here then we found nothing
	return NULL;
}


/**
 * Lookup a namespace inside of the symtab. Unlike searching for a function there
 * is no hashing to do here, just string comparison
 *
 * Yes this is an expensive lookup, but we don't expect to be doing it that
 * frequently. If it does become too expensive to do, then we will begin to use
 * hashing
 */
function_namespace_t* lookup_namespace(function_symtab_t* symtab, char* name){
	//Run through every namespace 
	for(int32_t i = 0; i < symtab->namespaces.current_index; i++){
		//Extract it
		function_namespace_t* namespace = dynamic_array_get_at(&(symtab->namespaces), i);

		//Not possible to lookup the default sheaf
		if(namespace->is_default == TRUE){
			continue;
		}

		//Names match then we're a go
		if(strcmp(namespace->namespace_name.string, name) == 0){
			return namespace;
		}
	}

	//If we make it all of the way down here, that means that we've found nothing
	return NULL;
}


/**
 * Does a namespace exist one level underneath the parent? This is done if we're looking
 * to add a new namespace
 */
function_namespace_t* lookup_namespace_under_current(function_symtab_t* symtab, char* name){
	//Run through the children of the current parent
	for(int32_t i = 0; i < symtab->current->child_namespaces.current_index; i++){
		//Extract the namespace
		function_namespace_t* namespace = dynamic_array_get_at(&(symtab->current->child_namespaces), i);

		//If they're a match then we're out
		if(strcmp(namespace->namespace_name.string, name) == 0){
			return namespace;
		}
	}

	//If we got to here then we found nothing
	return NULL;
}


/**
 * Does a namespace exist one level underneath the given parent? This is usually used for searching
 * up namespaces that were given in qualified names
 */
function_namespace_t* lookup_namespace_under_parent(function_namespace_t* parent_namespace, char* name){
	//Run through the children of the current parent
	for(int32_t i = 0; i < parent_namespace->child_namespaces.current_index; i++){
		//Extract the namespace
		function_namespace_t* namespace = dynamic_array_get_at(&(parent_namespace->child_namespaces), i);

		//If they're a match then we're out
		if(strcmp(namespace->namespace_name.string, name) == 0){
			return namespace;
		}
	}

	//If we got to here then we found nothing
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
 * Lookup a module in the symtab. There is only one lexical scope to lookup
 * here
 */
symtab_module_record_t* lookup_module(module_symtab_t* symtab, dynamic_string_t* module_name){
	//Obtain the hash
	u_int64_t hash = hash_module_name(module_name->string);

	//Get the starting record - remember this may not be the actual match
	symtab_module_record_t* cursor = symtab->records[hash];

	//Crawl through the records that are conjoined
	while(cursor != NULL){
		//Only an exact match is accepted
		if(dynamic_strings_equal(&(cursor->dependency_graph_node->module_name), module_name) == TRUE){
			return cursor;
		}

		cursor = cursor->next;
	}

	//If we made it to here then we found nothing
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
 * Print out the initialization state array for a variable. This is a debugging
 * only function
 */
void print_initialization_states_for_ssa_variable(symtab_variable_record_t* variable){
	printf("[");

	for(int32_t i = 0; i < variable->ssa_counter; i++){
		variable_initialization_state_t state = variable->initialization_state_map[i];

		switch(state){
			case VARIABLE_STATE_DEFINITELY_INITIALIZED:
				printf("DEFINITELY_INITIALIZED");
				break;
			case VARIABLE_STATE_UNINITIALIZED:
				printf("UNINITIALIZED");
				break;
			case VARIABLE_STATE_MAYBE_INITIALIZED:
				printf("MAYBE_INITIALIZED");
				break;
		}

		if(i != variable->ssa_counter - 1){
			printf(", ");
		}
	}

	printf("]\n");
}


/**
 * Lookup a label in the symtab
 */
symtab_label_record_t* lookup_label(label_symtab_t* label_symtab, char* name){
	//Grab the hash first
	u_int64_t h = hash_label_name(name);

	//First just try to get this here
	symtab_label_record_t* cursor = label_symtab->records[h];

	//Crawl through and string compare
	while(cursor != NULL){
		//They have to be an exact match for this to work
		if(strncmp(name, cursor->name.string, cursor->name.current_length) == 0){
			return cursor;
		}

		//Otherwise bump it up to the next one
		cursor = cursor->next;
	}

	//Either i
	return cursor;
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
 * Determine where we need to insert the bounds inside of the array type names itself
 * 
 * Some examples:
 *  Array of 5 i32's -> i32[5]
 *  Array of 3 i32[4] -> i32[3][4](most common case for us)
 * 	Array of 5 i32* -> i32*[5]
 * 	Array of 7 i32*[5] -> i32*[7][5]
 * 	Array of 55 array pointers i32[5]* -> i32[5]*[55]
 */
static inline void insert_bounds_into_array_type_name(dynamic_string_t* type_name, generic_type_t* member_type, char* bounds_buffer){
	//For use in our hunt
	int32_t insertion_index;
	u_int8_t in_brackets;

	//Go based on what the member type is
	switch(member_type->type_class){
		/**
		 * These types are all easy - we just need to insert our bounds
		 * buffer at the very back of the string
		 */
		case TYPE_CLASS_BASIC:
		case TYPE_CLASS_STRUCT:
		case TYPE_CLASS_ENUMERATED:
		case TYPE_CLASS_UNION:
		case TYPE_CLASS_POINTER:
			dynamic_string_insert_string_at_index(type_name, bounds_buffer, type_name->current_length);
			break;

		/**
		 * For an array type, we are now creating an array of arrays. So, we need to run through
		 * here and figure out where the very last chunk of array indices are and insert our bounds
		 * right before there
		 *
		 * Example(this is a very forced one to illustrate a point)
		 * 	 Array of 6 i32[5]*[4]
		 * 	 				   ^
		 * 	 				   |
		 * 	 Final Result: i32[5]*[6][4]
		 */
		case TYPE_CLASS_ARRAY:
			//By default we aren't in brackets
			in_brackets = FALSE;

			/**
			 * Strategy: Run through the string backwards until we find 
			 * a "*", or until we find a character that is not inside of 
			 * brackets itself
			 */
			for(insertion_index = type_name->current_length - 1; insertion_index >= 0; insertion_index--){
				switch(type_name->string[insertion_index]){
					case ']':
						in_brackets = TRUE;
						break;

					case '[':
						in_brackets = FALSE;
						break;

					/**
					 * If we're in brackets then this is something totally ordinary, but if
					 * we're not, we've found what we need
					 */
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
					case '8':
					case '9':
						//Escape
						if(in_brackets == FALSE){
							//Bump this up so we don't corrupt the actual type
							insertion_index++;
							goto insertion_step;
						}

						break;

					default:
						//Bump this up so we don't corrupt the actual type
						insertion_index++;
						goto insertion_step;
				}

			}

	insertion_step:
			dynamic_string_insert_string_at_index(type_name, bounds_buffer, insertion_index);
			break;

		//Our default strategy is just to put it in the back
		default:
			dynamic_string_insert_string_at_index(type_name, bounds_buffer, type_name->current_length);
			break;
	}
}


/**
 * Specifically look for an array type with the given type as a member in the symtab
 */
symtab_type_record_t* lookup_array_type(type_symtab_t* symtab, generic_type_t* member_type, u_int32_t num_members, mutability_type_t mutability){
	//For holding our array bounds
	char bounds_buffer[1000];

	//Clear the temporary buffer name
	clear_dynamic_string(&temporary_array_name);

	//Set it to be the type name here
	dynamic_string_set(&temporary_array_name, member_type->type_name.string);

	//Print the bounds into here
	sprintf(bounds_buffer, "[%d]", num_members);

	//Let the helper go through and add the bounds to the proper place
	insert_bounds_into_array_type_name(&temporary_array_name, member_type, bounds_buffer);

	//Now get the hash. We need to be using a special helper for this
	u_int64_t hash = hash_array_type_name(temporary_array_name.string, num_members, mutability);

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
			if(strncmp(record_cursor->type->type_name.string, temporary_array_name.string, record_cursor->type->type_name.current_length) == 0
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
	/**
	 * Add it into the list of functions called by the source. Since we use a set here, we are
	 * guaranteed to never add the function in more than once even if the source function calls
	 * it multiple times in the body
	 */
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
	printf("Lexical Level: %d,\n", record->lexical_scope_id);
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
	printf("Lexical Level: %d,\n", record->lexical_scope_id);

	//If we have an error type print the error type ID
	if(record->type->type_class == TYPE_CLASS_ERROR){
		printf("Error ID: %d\n", record->type->internal_types.error_type_id);
	}

	printf("}\n");
}


/**
 * Print a function name into a string buffer. We will be concatenating to this
 * buffer that we assume is preallocated
 */
void print_function_name_to_buffer(char* buffer, symtab_function_record_t* record){
	//Internal buffer for printing
	char internal_buffer[1000];

	//Get out the original token stream
	ollie_token_stream_t* original_token_stream = &(record->dependency_graph_node->token_stream);

	//First print out the line number and attach to the internal buffer
	sprintf(internal_buffer, "\n\t---> %d |", record->line_number);
	strcat(buffer, internal_buffer);

	//Now run through and print the tokens out that correspond to this function's name
	for(int32_t i = record->token_index_of_definition; i < original_token_stream->token_stream.current_index; i++){
		//Extract the token
		lexitem_t* token = token_array_get_pointer_at(&(original_token_stream->token_stream), i);

		//Print with added spaces and concatenate to our buffer
		sprintf(internal_buffer, " %s", lexitem_to_string(token));
		strcat(buffer, internal_buffer);

		//End case - if we have one of these it means that we're at the end and should leave
		if(token->tok == SEMICOLON || token->tok == L_CURLY){
			break;
		}
	}
}


/**
 * Print a variable name out in a stylized way. This is intended for error messages
 */
void print_variable_name_to_buffer(char* buffer, symtab_variable_record_t* record){
	//Internal buffer for printing
	char internal_buffer[1000];

	//Dealias first if need be
	record = dealias_variable(record);

	switch(record->membership){
		/**
		 * For function parameters we just print out 
		 * the function declaration
		 */
		case FUNCTION_PARAMETER:
			print_function_name_to_buffer(buffer, record->function_declared_in);
			break;

		/**
		 * For everything else we will generate the string using the
		 * token index that is stored inside of the record itself
		 */
		default:
			//First print out the line number and attach to the internal buffer
			sprintf(internal_buffer, "\n\t---> %d |", record->line_number);
			strcat(buffer, internal_buffer);

			//Get out the original token stream
			ollie_token_stream_t* original_token_stream = &(record->node_defined_in->token_stream);

			for(int32_t i = record->token_index_of_definition; i < original_token_stream->token_stream.current_index; i++){
				lexitem_t* token = token_array_get_pointer_at(&(original_token_stream->token_stream), i);

				//Print with added spaces and concatenate to our buffer
				sprintf(internal_buffer, " %s", lexitem_to_string(token));
				strcat(buffer, internal_buffer);

				//Generic fail cases
				if(token->tok == SEMICOLON || token->tok == L_CURLY){
					break;
				}

				//For enum members we'll need to look for the comma
				if(record->membership == ENUM_MEMBER && token->tok == COMMA){
					break;
				}
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
 * Generate the fully qualified namespace for a given namespace and return it inside of
 * a freshly allocated dynamic string
 *
 * If we are trying to get the fully qualified name on the default namespace, a null dynamic string
 * is returned
 */
dynamic_string_t generate_fully_qualified_namespace_name(function_namespace_t* namespace_record){
	//Initially it's null
	dynamic_string_t namespace_name = INITIALIZE_DYNAMIC_STRING;

	//If this is the default then get out
	if(namespace_record->is_default == TRUE){
		return namespace_name;
	}

	//Fully allocate the namespace name
	namespace_name = dynamic_string_alloc();

	//We're going to push everything up onto a stack in backwards order
	heap_stack_t stack = heap_stack_alloc();

	//Grab a cursor for the namespace record
	function_namespace_t* cursor = namespace_record;

	//So long as we haven't hit the default
	while(cursor->is_default == FALSE){
		//Hold onto this pointer
		function_namespace_t* temp = cursor;

		//Go up the chain
		cursor = cursor->parent_namespace;

		//Put temp inside of the stack
		push(&stack, temp);
	}

	//Now we'll go through the stack and generate our name that way
	while(heap_stack_is_empty(&stack) == FALSE){
		//Pop it off of the stack
		function_namespace_t* record = pop(&stack);

		//Concantenate this to our name
		dynamic_string_concatenate(&namespace_name, record->namespace_name.string);

		//If we have more to go, add the separators
		if(peek(&stack) != NULL){
			dynamic_string_concatenate(&namespace_name, "::");
		}
	}

	//Destroy the stack
	heap_stack_dealloc(&stack);

	return namespace_name;
}


/**
 * Generate the fully qualified function name for a given function and return it inside of
 * a freshly allocated dynamic string
 */
dynamic_string_t generate_fully_qualified_function_name(symtab_function_record_t* function){
	//What namespace are we in
	function_namespace_t* namespace_contained_in = function->namespace_contained_in;

	//If the function is in the default namespace there's nothing for us to do
	if(namespace_contained_in->is_default == TRUE){
		return clone_dynamic_string(&(function->func_name));
	}

	//Otherwise we'll need a fresh name here
	dynamic_string_t qualified_name = dynamic_string_alloc();

	//We're going to push everything up onto a stack in backwards order
	heap_stack_t stack = heap_stack_alloc();

	//Grab a cursor for the namespace record
	function_namespace_t* cursor = namespace_contained_in;

	//So long as we haven't hit the default
	while(cursor->is_default == FALSE){
		//Hold onto this pointer
		function_namespace_t* temp = cursor;

		//Go up the chain
		cursor = cursor->parent_namespace;

		//Put temp inside of the stack
		push(&stack, temp);
	}

	//Now we'll go through the stack and generate our name that way
	while(heap_stack_is_empty(&stack) == FALSE){
		//Pop it off of the stack
		function_namespace_t* record = pop(&stack);

		//Concantenate this to our name
		dynamic_string_concatenate(&qualified_name, record->namespace_name.string);

		//We need a separator no matter what here
		dynamic_string_concatenate(&qualified_name, "::");
	}

	//Destroy the stack
	heap_stack_dealloc(&stack);

	//Finally we can tack the function name on
	dynamic_string_concatenate(&qualified_name, function->func_name.string);
	
	return qualified_name;
}


/**
 * Generate the fully qualified variable name for a given variable and return it inside of
 * a freshly allocated dynamic string
 */
dynamic_string_t generate_fully_qualified_variable_name(symtab_variable_record_t* variable, function_namespace_t* var_namespace){
	//If the function is in the default namespace there's nothing for us to do
	if(var_namespace->is_default == TRUE){
		return clone_dynamic_string(&(variable->var_name));
	}

	//Otherwise we'll need a fresh name here
	dynamic_string_t qualified_name = dynamic_string_alloc();

	//We're going to push everything up onto a stack in backwards order
	heap_stack_t stack = heap_stack_alloc();

	//Grab a cursor for the namespace record
	function_namespace_t* cursor = var_namespace;

	//So long as we haven't hit the default
	while(cursor->is_default == FALSE){
		//Hold onto this pointer
		function_namespace_t* temp = cursor;

		//Go up the chain
		cursor = cursor->parent_namespace;

		//Put temp inside of the stack
		push(&stack, temp);
	}

	//Now we'll go through the stack and generate our name that way
	while(heap_stack_is_empty(&stack) == FALSE){
		//Pop it off of the stack
		function_namespace_t* record = pop(&stack);

		//Concantenate this to our name
		dynamic_string_concatenate(&qualified_name, record->namespace_name.string);

		//We need a separator no matter what here
		dynamic_string_concatenate(&qualified_name, "::");
	}

	//Destroy the stack
	heap_stack_dealloc(&stack);

	//Finally we can tack the variable name on
	dynamic_string_concatenate(&qualified_name, variable->var_name.string);
	
	return qualified_name;
}


/**
 * Print the call graph's adjacency matrix/transitive closure out for debugging
 */
void print_call_graph_adjacency_matrix(FILE* fl, function_symtab_t* function_symtab){
	fprintf(fl, "=============== Function Call Graph ========================\n");
	
	//We need a min priority queue for this
	min_priority_queue_t min_priority_queue = min_priority_queue_alloc();

	//Run through all of the namespaces 
	for(int32_t _ = 0; _ < function_symtab->namespaces.current_index; _++){
		function_namespace_t* sheaf = dynamic_array_get_at(&(function_symtab->namespaces), _);

		//Run through and print all of these out first
		for(int32_t i = 0; i < FUNCTION_KEYSPACE; i++){
			//Skip ahead
			if(sheaf->records[i] == NULL){
				continue;
			}

			//Otherwise grab it out
			symtab_function_record_t* cursor = sheaf->records[i];

			//Crawl the whole thing
			while(cursor != NULL){
				//Use the min priority queue to insert based on the function ID
				min_priority_queue_enqueue(&min_priority_queue, cursor, cursor->function_id);

				//Bump it up
				cursor = cursor->next;
			}
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
	int32_t function_count = function_symtab->current_function_id;

	fprintf(fl, "============= Adjacency Matrix ==============\n");

	//Run through each row
	for(int32_t i = 0; i < function_count; i++){
		//Print out the row number
		fprintf(fl, "[%2d]: ", i);

		//Now print out the columns
		for(int32_t j = 0; j < function_count; j++){
			//Will be 1(connected) or 0
			fprintf(fl, "%d ", function_symtab->call_graph_matrix[i * function_count + j]);
		}

		//Final newline
		fprintf(fl, "\n");
	}

	fprintf(fl, "============= Adjacency Matrix ==============\n");

	fprintf(fl, "============= Inline Matrix =================\n");

	for(int32_t i = 0; i < function_count; i++){
		//Print out the row(caller) number
		fprintf(fl, "[%2d]: ", i);

		//Now all of the columns(callees)
		for(int32_t j = 0; j  < function_count; j++){
			fprintf(fl, "%d ", function_symtab->inline_call_graph_matrix[i * function_count + j]);
		}

		fprintf(fl, "\n");
	}

	fprintf(fl, "============= Inline Matrix =================\n");

	fprintf(fl, "============= Transitive Closure ==============\n");

	//Run through each row
	for(int32_t i = 0; i < function_count; i++){
		//Print out the row number
		fprintf(fl, "[%2d]: ", i);

		//Now print out the columns
		for(int32_t j = 0; j < function_count; j++){
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
 * Construct the call graph adjacency matrices. This includes the regular adjacency
 * matrix and the specialized inline adjacency matrix. We are able to compute them
 * both in one go
 */
static inline void construct_call_graph_adjacency_matrices(function_symtab_t* symtab){
	//Extract the number of functions
	u_int32_t number_of_functions = symtab->current_function_id;

	/**
	 * Now that we have all of the possible functions added in, we need to create the
	 * overall adjacency matrix for all of these functions
	 *
	 * We will also maintain a separate call graph that exclusively deals with inlined functions.
	 * We know that, by our definition, this graph must be acyclic. This graph will be reverse
	 * topologically sorted to get the inline order when we do perform inlining
	 */
	symtab->call_graph_matrix = calloc(number_of_functions * number_of_functions, sizeof(u_int8_t));
	symtab->inline_call_graph_matrix = calloc(number_of_functions * number_of_functions, sizeof(u_int8_t));

	/**
	 * To populate the adjacency matrix, we'll need to run through literally ever function namespace 
	 */
	for(int32_t _ = 0; _ < symtab->namespaces.current_index; _++){
		function_namespace_t* current_namespace = dynamic_array_get_at(&(symtab->namespaces), _);

		for(int32_t i = 0; i < FUNCTION_KEYSPACE; i++){
			//Totally possible for this to happen
			if(current_namespace->records[i] == NULL){
				continue;
			}

			/**
			 * Otherwise, we actually have a space that is populated so we need to
			 * populate here. Remember, every record is a linked list so we need
			 * to explore all of the nodes
			 */
			symtab_function_record_t* cursor = current_namespace->records[i];

			//So long as the cursor is not NULL
			while(cursor != NULL){
				//Grab the cursor's unique function ID
				u_int32_t cursor_id = cursor->function_id;

				//Run through all of the functions that this function itself calls
				for(int32_t j = 0; j < cursor->called_functions.current_index; j++){
					//Extract the called function and it's internal function type
					symtab_function_record_t* called_function = dynamic_set_get_at(&(cursor->called_functions), j);
					function_type_t* called_function_type = called_function->signature->internal_types.function_type;

					//Now let's get his ID
					u_int32_t called_function_id = called_function->function_id;

					//Insert this call into the adjacency matrix
					symtab->call_graph_matrix[cursor_id * number_of_functions + called_function_id] = TRUE;

					/**
					 * If this called function is an inline function, we'll need to note
					 * this done inside of the inlined fucntion call graph as well
					 */
					if(called_function_type->is_inlined == TRUE){
						symtab->inline_call_graph_matrix[cursor_id * number_of_functions + called_function_id] = TRUE;
					}
				}

				//Bump it up to the next one
				cursor = cursor->next;
			}
		}
	}
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
	int32_t number_of_functions = symtab->current_function_id;

	//Allocate the transitive closure
	symtab->call_graph_transitive_closure = calloc(number_of_functions * number_of_functions, sizeof(u_int8_t));

	//Copy over the regular adjacency matrix to this
	memcpy(symtab->call_graph_transitive_closure, symtab->call_graph_matrix, number_of_functions * number_of_functions * sizeof(u_int8_t));

	//Now that we've made the copy, we can start on the actual transitive closure
	
	//For each node i(intermediate node)
	for(int32_t i = 0; i < number_of_functions; i++){
		for(int32_t j = 0; j < number_of_functions; j++){
			for(int32_t k = 0; k < number_of_functions; k++){
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
	//Construct the regular and inline adjacency matrices
	construct_call_graph_adjacency_matrices(symtab);

	//Now that we have the regular call graph created, we will create the transitive closure
	compute_call_graph_transitive_closure(symtab);
}


/**
 * Provide a function that will destroy the function symtab completely
 */
void function_symtab_dealloc(function_symtab_t* symtab){
	//For temporary holding
	function_namespace_t* sheaf;
	symtab_function_record_t* record;
	symtab_function_record_t* temp;

	//Run through all of the namespaces 
	for(int32_t _ = 0; _ < symtab->namespaces.current_index; _++){
		//Get the sheaf out
		sheaf = dynamic_array_get_at(&(symtab->namespaces), _);

		//Now go through all records
		for(int32_t i = 0; i < FUNCTION_KEYSPACE; i++){
			record = sheaf->records[i];

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

				//Destroy the label symtab if it exists
				label_symtab_dealloc(temp->user_defined_labels);

				//Finally free the function
				free(temp);
			}
		}

		//Destroy the name
		dynamic_string_dealloc(&(sheaf->namespace_name));

		//Destroy the array of children
		dynamic_array_dealloc(&(sheaf->child_namespaces));

		//Free the sheaf itself
		free(sheaf);
	}

	//Deallocate the namespace array
	dynamic_array_dealloc(&(symtab->namespaces));

	//Deallocate the id to function map
	dynamic_array_dealloc(&(symtab->id_to_function_mapping));

	//Free the adjacency matrix
	free(symtab->call_graph_matrix);

	//And the inlined function matrix
	free(symtab->inline_call_graph_matrix);

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
		for(int32_t j = 0; j < VARIABLE_KEYSPACE; j++){
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
		for(int32_t j = 0; j < TYPE_KEYSPACE; j++){
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

	//Destroy the temporary string storage
	dynamic_string_dealloc(&temporary_array_name);

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
	for(int32_t i = 0; i < MACRO_KEYSPACE; i++){
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


/**
 * Destroy a module symtab
 */
void module_symtab_dealloc(module_symtab_t* symtab){
	//Create temp/cursor for traversal
	symtab_module_record_t* cursor = NULL;
	symtab_module_record_t* temp;

	//Run through every single macro record
	for(int32_t i = 0; i < MODULE_KEYSPACE; i++){
		//Extract it
		cursor = symtab->records[i];

		//Run through any collision records
		while(cursor != NULL){
			//Reassign
			temp = cursor;

			//Let the helper deallocate the dependency graph node
			dependency_graph_node_dealloc(temp->dependency_graph_node);

			//Advance it up
			cursor = cursor->next;

			//Dealloc
			free(temp);
		}
	}

	//At the very end free the overall control structure
	free(symtab);
}


/**
 * Destroy a label table. We assume that a lot of time
 * when this function is called we'll actually have a nonexistent
 * table so we will account for that here
 */
void label_symtab_dealloc(label_symtab_t* symtab){
	//Totally valid and normal
	if(symtab == NULL){
		return;
	}

	//Run through every record
	for(int32_t i = 0; i < USER_DEFINED_LABELED_BLOCK_KEYSPACE; i++){
		//Extract the record
		symtab_label_record_t* label_record_cursor = symtab->records[i];

		//Remember that it's a hashtable, could be nested from collisions
		while(label_record_cursor != NULL){
			//Grab a holder
			symtab_label_record_t* temp = label_record_cursor;

			//Bump this up to the next one
			label_record_cursor = label_record_cursor->next;

			//Release the node
			free(temp);
		}
	}

	//At the very end destroy the whole thing
	free(symtab);
}
