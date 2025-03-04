/**
 * The implementation of the symbol table
*/

#include "symtab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "../ast/ast.h"

#define LARGE_PRIME 611593
static void print_generic_type(generic_type_t* type);

/**
 * Print a generic warning for the type system. This is used when variables/functions are 
 * defined and not used
 */
static void print_warning(char* info, char* file_token, u_int16_t line_number){
	fprintf(stderr, "\n[FILE: %s] --> [LINE %d: COMPILER WARNING]: %s\n", file_token, line_number, info);
}


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
	symtab->current_lexical_scope = 0;
	//Nothing has been initialized yet
	symtab->current = NULL;
	symtab->next_index = 0;

	return symtab;
}


/**
 * Dynamically allocate a type symtab
 */
type_symtab_t* type_symtab_alloc(){
	type_symtab_t* symtab = (type_symtab_t*)calloc(1, sizeof(type_symtab_t));
	symtab->next_index = 0;
	symtab->current_lexical_scope = 0;
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
	symtab_variable_sheaf_t* current = (symtab_variable_sheaf_t*)calloc(1, sizeof(symtab_variable_sheaf_t));
	//Store it in here for later
	symtab->sheafs[symtab->next_index] = current;
	symtab->next_index++;

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
	//Store it in here for later
	symtab->sheafs[symtab->next_index] = current;
	symtab->next_index++;

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
 * Universal hashing algorithm:
 * 	Start with an initial small prime
 * 	key <- small_prime
 *
 * 	for each hashable value:
 * 		key <- (key * prime) ^ (value * other prime)
 * 		
 * 	key % keyspace
 *
 * 	return key
*/
static u_int16_t hash(char* name){
	u_int32_t key = 37;
	
	char* cursor = name;
	//Two primes(this should be good enough for us)
	u_int32_t a = 54059;
	u_int32_t b = 76963;

	//Iterate through the cursor here
	for(; *cursor != '\0'; cursor++){
		//Sum this up for our key
		key = (key * a) ^ (*cursor * b);
	}

	//Cut it down to our keyspace
	return key % KEYSPACE;
}


/**
 * Dynamically allocate a variable record
*/
symtab_variable_record_t* create_variable_record(char* name, STORAGE_CLASS_T storage_class){
	//Allocate it
	symtab_variable_record_t* record = (symtab_variable_record_t*)calloc(1, sizeof(symtab_variable_record_t));

	//Store the name
	strcpy(record->var_name, name);
	//Hash it and store it to avoid to repeated hashing
	record->hash = hash(name);
	//Store the storage class
	record->storage_class = storage_class;

	return record;
}


/**
 * Dynamically allocate a function record
*/
symtab_function_record_t* create_function_record(char* name, STORAGE_CLASS_T storage_class){
	//Allocate it
	symtab_function_record_t* record = (symtab_function_record_t*)calloc(1, sizeof(symtab_function_record_t));

	//Store the name
	strcpy(record->func_name, name);
	//Hash it and store it to avoid to repeated hashing
	record->hash = hash(name);
	//Store the storage class
	record->storage_class = storage_class;
	//Was it ever called?
	record->called = 0;

	return record;
}


/**
 * Dynamically allocate and create a type record
 */
symtab_type_record_t* create_type_record(generic_type_t* type){
	//Allocate it
	symtab_type_record_t* record = (symtab_type_record_t*)calloc(1, sizeof(symtab_type_record_t));

	//Hash the type name and store it
	record->hash = hash(type->type_name);
	//Assign the type
	record->type = type;

	return record;
}


/**
 * Dynamically allocate and create a constant record
 * NOTE: we just need the name here to make the hash
 */
symtab_constant_record_t* create_constant_record(char* name){
	//Allocate it
	symtab_constant_record_t* record = calloc(1, sizeof(symtab_constant_record_t));
	
	//Hash the name and store it
	record->hash = hash(name);
	//Store the name
	strcpy(record->name, name);
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
	//While we're at it store this
	record->lexical_level = symtab->current_lexical_scope;
	
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
 */
void add_all_basic_types(type_symtab_t* symtab){
	generic_type_t* type;

	//Add in void type
	type = create_basic_type("void", VOID);
	insert_type(symtab, create_type_record(type));

	//s_int8 type
	type = create_basic_type("i8", S_INT8);
	insert_type(symtab,  create_type_record(type));

	//u_int8 type
	type = create_basic_type("u8", U_INT8);
	insert_type(symtab,  create_type_record(type));

	//char type
	type = create_basic_type("char", CHAR);
	insert_type(symtab,  create_type_record(type));
	
	//u_int16 type
	type = create_basic_type("u16", U_INT16);
	insert_type(symtab,  create_type_record(type));
		
	//s_int16 type
	type = create_basic_type("i16", S_INT16);
	insert_type(symtab,  create_type_record(type));
	
	//s_int32 type
	type = create_basic_type("i32", S_INT32);
	insert_type(symtab,  create_type_record(type));
	
	//u_int32 type
	type = create_basic_type("u32", U_INT32);
	insert_type(symtab,  create_type_record(type));
	
	//u_int64 type
	type = create_basic_type("u64", U_INT64);
	insert_type(symtab,  create_type_record(type));
	
	//s_int64 type
	type = create_basic_type("i64", S_INT64);
	insert_type(symtab,  create_type_record(type));

	//float32 type
	type = create_basic_type("f32", FLOAT32);
	insert_type(symtab,  create_type_record(type));
	
	//float64 type
	type = create_basic_type("f64", FLOAT64);
	insert_type(symtab,  create_type_record(type));

	//label type
	type = create_basic_type("label", LABEL_IDENT);
	insert_type(symtab,  create_type_record(type));

}


/**
 * Lookup the record in the symtab that corresponds to the following name.
 * 
 * There is only one lexical scope for functions, so this symtab is quite simple
 */
symtab_function_record_t* lookup_function(function_symtab_t* symtab, char* name){
	//Let's grab it's hash
	u_int16_t h = hash(name); 

	//Grab whatever record is at that hash
	symtab_function_record_t* record_cursor = symtab->records[h];
		
	//We could have had collisions so we'll have to hunt here
	while(record_cursor != NULL){
		//If we find the right one, then we can get out
		if(strcmp(record_cursor->func_name, name) == 0){
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
	u_int16_t h = hash(name);

	//Grab whatever record is at that hash
	symtab_constant_record_t* cursor = symtab->records[h];

	//So long as this isn't null, we'll do string comparisons to find our match. Recall
	//that it is possible to have collisions, so two records having the same hash is not 
	//always enough to know for sure
	while(cursor != NULL){
		if(strcmp(cursor->name, name) == 0){
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
	u_int16_t h = hash(name);

	//Define the cursor so we don't mess with the original reference
	symtab_variable_sheaf_t* cursor = symtab->current;
	symtab_variable_record_t* records_cursor;

	while(cursor != NULL){
		//As long as the previous level is not null
		records_cursor = cursor->records[h];
		
		//We could have had collisions so we'll have to hunt here
		while(records_cursor != NULL){
			//If we find the right one, then we can get out
			if(strcmp(records_cursor->var_name, name) == 0){
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
	u_int16_t h = hash(name);

	//A cursor for records iterating
	symtab_variable_record_t* records_cursor;

	//We only deal with the current level
	records_cursor = symtab->current->records[h];
	
	//We could have had collisions so we'll have to hunt here
	while(records_cursor != NULL){
		//If we find the right one, then we can get out
		if(strcmp(records_cursor->var_name, name) == 0){
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
symtab_variable_record_t* lookup_variable_lower_scope(variable_symtab_t* symtab, symtab_function_record_t* function_declared_in, char* name){
	//Grab the hash
	u_int16_t h = hash(name);

	//Define the cursor so we don't mess with the original reference
	symtab_variable_sheaf_t* cursor;
	//A cursor for records iterating
	symtab_variable_record_t* records_cursor;

	//So long as the cursor is not null
	for(u_int16_t i = 0; i < symtab->next_index; i++){
		//Grab the current sheaf
		cursor = symtab->sheafs[i];

		//Grab a records cursor
		records_cursor = cursor->records[h];

		//We could have had collisions so we'll hunt here
		while(records_cursor != NULL){
		
			//If we find the right one, then we can get out
			if(strcmp(records_cursor->var_name, name) == 0){
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
 * Lookup the record in the symtab that corresponds to the following name.
 * 
 * We are ALWAYS biased to the most local(in scope) version of the name. If we
 * do not find it in the local scope, we then search the outer scope, until there are
 * no more outer scopes to search
 */
symtab_type_record_t* lookup_type(type_symtab_t* symtab, char* name){
	//Grab the hash
	u_int16_t h = hash(name);

	//Define the cursor so we don't mess with the original reference
	symtab_type_sheaf_t* cursor = symtab->current;
	symtab_type_record_t* records_cursor;

	while(cursor != NULL){
		//As long as the previous level is not null
		records_cursor = cursor->records[h];
		
		//We could have had collisions so we'll have to hunt here
		while(records_cursor != NULL){
			//If we find the right one, then we can get out
			if(strcmp(records_cursor->type->type_name, name) == 0){
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
	printf("Name: %s,\n", record->func_name);
	printf("Hash: %d,\n", record->hash);
	printf("Lexical Level: %d,\n", record->lexical_level);
	printf("Offset: %p\n", (void*)(record->offset));
	printf("}\n");
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
	printf("Name: %s,\n", record->var_name);
	printf("Hash: %d,\n", record->hash);
	printf("Lexical Level: %d,\n", record->lexical_level);
	printf("Offset: %p\n", (void*)(record->offset));
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
	printf("Name: %s,\n", record->type->type_name);
	printf("Hash: %d,\n", record->hash);
	printf("Lexical Level: %d,\n", record->lexical_level);
	printf("}\n");
}

/**
 * Print a function name out in a stylised way
 */
void print_function_name(symtab_function_record_t* record){
	printf("In file with token %s:\n", record->module_defined_in);
	//If it's static we'll add the keyword in
	if(record->storage_class == STORAGE_CLASS_STATIC){
		printf("\t---> %d | fn:static %s(", record->line_number, record->func_name);
	} else {
		printf("\t---> %d | fn %s(", record->line_number, record->func_name);
	}

	//Print out the params
	for(u_int8_t i = 0; i < record->number_of_params; i++){
		//Print if it's mutable
		if(record->func_params[i].associate_var->is_mutable == 1){
			printf("mut ");
		}

		printf("%s : %s", record->func_params[i].associate_var->var_name, record->func_params[i].associate_var->type->type_name);
		//Comma if needed
		if(i < record->number_of_params-1){
			printf(", ");
		}
	}

	//Final closing paren and return type
	printf(") -> %s", record->return_type->type_name);

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
	printf("In file with token %s:\n", record->module_defined_in);
	//If it's part of a function we'll just print that
	if(record->is_function_paramater == 1){
		print_function_name(record->parent_function);
		return;
	} else if (record->is_label == 1){
		printf("\n---> %d | %s:\n", record->line_number, record->var_name);
		return;
	} else if(record->is_enumeration_member || record->is_construct_member){
		//The var name
		printf("{\n\t\t...\n\t\t...\t\t\n---> %d |\t %s : %s", record->line_number, record->var_name, record->type->type_name);
	} else {
		//Line num
		printf("\n---> %d | ", record->line_number);

		//Declare or let
		record->declare_or_let == 0 ? printf("declare ") : printf("let ");

		//If it's mutable print that
		if(record->is_mutable == 1){
			printf(" mut ");
		}

		//The var name
		printf("%s : ", record->var_name);

		//The type name
		printf("%s ", record->type->type_name);
		
		//We'll print out some abbreviated stuff with the let record
		if(record->declare_or_let == 1){
			printf(" := <initializer>;\n\n");
		} else {
			printf(";\n");
		}
	}
}


/**
 * A helper method for constant name printing
 * Intended for use by error messages
 */
void print_constant_name(symtab_constant_record_t* record){
	//First the record
	printf("\n---> %d | replace %s with ", record->line_number, record->name);
	
	//Grab the constant node out for convenience
	constant_ast_node_t* const_node = (constant_ast_node_t*)(((generic_ast_node_t*)record->constant_node)->node);

	//We'll now switch based on what kind of constant that we have
	switch (const_node->constant_type) {
		case INT_CONST:
		case INT_CONST_FORCE_U:
			printf("%d", const_node->int_val);
			break;
		case LONG_CONST_FORCE_U:
		case LONG_CONST:
			printf("%ld", const_node->long_val);
			break;
		case CHAR_CONST:
			printf("%d", const_node->char_val);
			break;
		case STR_CONST:
			printf("%s", const_node->string_val);
			break;
		case FLOAT_CONST:
			printf("%f", const_node->float_val);
			break;
		default:
			printf("FATAL INTERNAL COMPILER ERROR\n");
			exit(1);
	}

	//Now print out the semicolon
	printf(";\n");
}


/**
 * Print a type name. Intended for error messages
 */
static void print_generic_type(generic_type_t* type){
	//Print out where it was declared
	if(type->type_class == TYPE_CLASS_BASIC){
		printf("---> BASIC TYPE | ");
	} else {
		printf("---> %d | ", type->line_number);
	}

	//Then print out the name
	printf("%s", type->type_name);
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

	//Then print out the name
	printf("%s\n\n", record->type->type_name);
}


/**
 * Crawl the symtab and check for any unused functions. We generate some hopefully helpful
 * warnings here for the user
 */
void check_for_unused_functions(function_symtab_t* symtab, u_int16_t* num_warnings){
	//For any/all error printing
	char info[1000];
	//For temporary holding
	symtab_function_record_t* record;

	//Run through all keyspace records
	for(u_int16_t i = 0; i < KEYSPACE; i++){
		record = symtab->records[i];

		//We could have chaining here, so run through just in case
		while(record != NULL){
			if(record->called == 0 && record->defined == 0){
				//Generate a warning here
				(*num_warnings)++;

				sprintf(info, "Function \"%s\" is never defined and never called. First defined here:", record->func_name);
				print_warning(info, record->module_defined_in, record->line_number);
				//Also print where the function was defined
				print_function_name(record);
			} else if(record->called == 0 && record->defined == 1){
				//Generate a warning here
				(*num_warnings)++;

				sprintf(info, "Function \"%s\" is defined but never called. First defined here:", record->func_name);
				print_warning(info, record->module_defined_in, record->line_number);
				//Also print where the function was defined
				print_function_name(record);

			} else if(record->called == 1 && record->defined == 0){
				//Generate a warning here
				(*num_warnings)++;

				sprintf(info, "Function \"%s\" is called but never explicitly defined. First declared here:", record->func_name);
				print_warning(info, record->module_defined_in, record->line_number);
				//Also print where the function was defined
				print_function_name(record);

			}

			//Advance record up
			record = record->next;
		}
	}
}


/**
 * If a variable is declared as "mut"(mutable) but is never assigned to throughout it's
 * entire lifetime, that mut keyword is not needed
 */
void check_for_var_errors(variable_symtab_t* symtab, u_int16_t* num_warnings){
	//For any/all error printing
	char info[1000];
	//For record holding
	symtab_variable_record_t* record;

	//Sheaf cursor
	symtab_variable_sheaf_t** sheaf_cursor = symtab->sheafs;
	
	//So long as we have a sheaf
	while(*sheaf_cursor != NULL){
		//Grab the actual sheaf out
		symtab_variable_sheaf_t* sheaf = *sheaf_cursor;

		//Now we'll run through every variable in here
		for(u_int32_t i = 0; i < KEYSPACE; i++){
			record = sheaf->records[i];

			//This will happen alot
			if(record == NULL){
				continue;
			}

			//If it's a label, don't bother with it
			if(record->is_label == 1){
				continue;;
			}

			//Let's now analyze this record
			
			//We have a non initialized variable
			if(record->initialized == 0){
				sprintf(info, "Variable \"%s\" is never initialized. First defined here:", record->var_name);
				print_warning(info, record->module_defined_in, record->line_number);
				print_variable_name(record);
				(*num_warnings)++;
				//Go to the next iteration
				continue;
			}

			//If it's mutable but never mutated
			if(record->is_mutable == 1 && record->assigned_to == 0){
				sprintf(info, "Variable \"%s\" is declared as mutable but never mutated. Consider removing the \"mut\" keyword. First defined here:", record->var_name);
				print_warning(info, record->module_defined_in, record->line_number);
				print_variable_name(record);
				(*num_warnings)++;
			}

		}

		//Advance the sheaf cursor
		sheaf_cursor++;
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
	for(u_int16_t i = 0; i < KEYSPACE; i++){
		record = symtab->records[i];

		//We could have chaining here, so run through just in case
		while(record != NULL){
			temp = record;
			record = record->next;

			//If the call graph node has been defined, we will free that too
			if(temp->call_graph_node != NULL){
				free(temp->call_graph_node);
			}

			free(temp);
		}
	}

	//Free the entire symtab at the very end
	free(symtab);
}


/**
 * Provide a function that will destroy the variable symtab completely
 */
void variable_symtab_dealloc(variable_symtab_t* symtab){
	symtab_variable_sheaf_t* cursor;
	symtab_variable_record_t* record;
	symtab_variable_record_t* temp;

	//Run through all of the sheafs
	for	(u_int16_t i = 0; i < symtab->next_index; i++){
		cursor = symtab->sheafs[i];

		//Now we'll free all non-null records
		for(u_int16_t j = 0; j < KEYSPACE; j++){
			record = cursor->records[j];

			//We could have chaining here, so run through just in case
			while(record != NULL){
				temp = record;
				record = record->next;
				free(temp);
			}
		}
		//Free the sheaf
		free(cursor);
	}

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
	for	(u_int16_t i = 0; i < symtab->next_index; i++){
		cursor = symtab->sheafs[i];

		//Now we'll free all non-null records
		for(u_int16_t j = 0; j < KEYSPACE; j++){
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
	for(u_int16_t i = 0; i < KEYSPACE; i++){
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


