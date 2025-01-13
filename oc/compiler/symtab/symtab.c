/**
 * The implementation of the symbol table
*/

#include "symtab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define LARGE_PRIME 611593


/**
 * Dynamically allocate and return a symtab pointer for compiler use
 */
symtab_t* initialize_symtab(SYMTAB_RECORD_TYPE record_type){
	symtab_t* symtab = (symtab_t*)calloc(1, sizeof(symtab_t));
	//What type of symtab is it?
	symtab->type = record_type;
	//Just in case
	symtab->current_lexical_scope = -1;
	symtab->next_index = 0;

	return symtab;
}

/**
 * Initialize a new lexical scope. This involves making a new sheaf and
 * adding it in
*/
void initialize_scope(symtab_t* symtab){
	//Function symtab
	if(symtab->type == FUNCTION){
		symtab_function_sheaf_t* current = (symtab_function_sheaf_t*)calloc(1, sizeof(symtab_function_sheaf_t));
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

	//Variable symtab
	} else {
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
}


/**
 * Finalize the scope, for the purposes of this project, finalizing the scope just means going
 * up by one level
 */
void finalize_scope(symtab_t* symtab){
	//Function symtab
	if(symtab->type == FUNCTION){
		//Back out of this one as it's finalized
		symtab->current = ((symtab_function_sheaf_t*)symtab->current)->previous_level;

		//Go back up one
		symtab->current_lexical_scope--;

	//Variable symtab
	} else {
		//Back out of this one as it's finalized
		symtab->current = ((symtab_variable_sheaf_t*)symtab->current)->previous_level;

		//Go back up one
		symtab->current_lexical_scope--;
	}
}


/**
 * Hash a name before entry/search into the hash table
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

	return record;
}


/**
 * Insert a record into the symbol table. This assumes that the user 
 * has already checked to see if this record is in the symbol table
*/
u_int8_t insert(symtab_t* symtab, void* record){
	//Function symtab
	if(symtab->type == FUNCTION){
		symtab_function_record_t* func_record = (symtab_function_record_t*)record;
		//While we're at it store this
		func_record->lexical_level = symtab->current_lexical_scope;

		//No collision here, just store and get out
		if(((symtab_function_sheaf_t*)symtab->current)->records[func_record->hash] == NULL){
			//Store this and get out
			((symtab_function_sheaf_t*)symtab->current)->records[func_record->hash] = func_record;
			//0 = success, no collision
			return 0;
		}

		//Otherwise, there is a collision
		//Grab the head record
		symtab_function_record_t* cursor = ((symtab_function_sheaf_t*)symtab->current)->records[func_record->hash];
		//Store this while we're at it
		cursor->lexical_level = symtab->current_lexical_scope;

		//Get to the very last node
		while(cursor->next != NULL){
			cursor = cursor->next;
		}

		//Now that cursor points to the very last node, we can add it in
		cursor->next = record;
		//This should be null anyways, but it never hurts to double check
		func_record->next = NULL;

		//1 = success, but there was a collision
		return 1;

	//Variable symtab
	} else {
		symtab_variable_record_t* var_record = (symtab_variable_record_t*)record;
		//While we're at it store this
		var_record->lexical_level = symtab->current_lexical_scope;

		//No collision here, just store and get out
		if(((symtab_variable_sheaf_t*)symtab->current)->records[var_record->hash] == NULL){
			//Store this and get out
			((symtab_variable_sheaf_t*)symtab->current)->records[var_record->hash] = var_record;
			//0 = success, no collision
			return 0;
		}

		//Otherwise, there is a collision
		//Grab the head record
		symtab_variable_record_t* cursor = ((symtab_variable_sheaf_t*)symtab->current)->records[var_record->hash];

		//Get to the very last node
		while(cursor->next != NULL){
			cursor = cursor->next;
		}

		//Now that cursor points to the very last node, we can add it in
		cursor->next = record;
		//This should be null anyways, but it never hurts to double check
		var_record->next = NULL;

		//1 = success, but there was a collision
		return 1;
	}

}


/**
 * Lookup the record in the symtab that corresponds to the following name.
 * 
 * We are ALWAYS biased to the most local(in scope) version of the name. If we
 * do not find it in the local scope, we then search the outer scope, until there are
 * no more outer scopes to search
 */
void* lookup(symtab_t* symtab, char* name){
	//Let's grab it's hash
	u_int16_t h = hash(name); 

	//Function symtab
	if(symtab->type == FUNCTION){
		//Define the cursor so we don't mess with the original reference
		symtab_function_sheaf_t* cursor = symtab->current;
		symtab_function_record_t* records_cursor;

		while(cursor != NULL){
			//As long as the previous level is not null
			records_cursor = cursor->records[h];
		
			//We could have had collisions so we'll have to hunt here
			while(records_cursor != NULL){
				//If we find the right one, then we can get out
				if(strcmp(records_cursor->func_name, name) == 0){
					return records_cursor;
				}
				//Advance it
				records_cursor = records_cursor->next;
			}

			//Go up to a higher scope
			cursor = cursor->previous_level;
		}

	//Variable symtab
	} else {
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
 * Print a function name out in a stylised way
 */
void print_function_name(symtab_function_record_t* record){
	printf("\n---> %d | func %s(", record->line_number, record->func_name);

	//Print out the params
	for(u_int8_t i = 0; i < record->number_of_params; i++){
		printf("%s %s", record->func_params[i].associate_var->type.type_name, record->func_params[i].associate_var->var_name);
		//Comma if needed
		if(i < record->number_of_params-1){
			printf(", ");
		}
	}

	//Final closing paren and return type
	printf(") -> %s\n", record->return_type.type_name);
}

/**
 * Print a variable name out in a stylized way
 * Intended for error messages
 */
void print_variable_name(symtab_variable_record_t* record){
	//Line num
	printf("\n---> %d | ", record->line_number);

	//Declare or let
	record->declare_or_let == 0 ? printf("declare ") : printf("let ");

	//The type name
	printf("%s ", record->type.type_name);

	//The var name
	printf("%s", record->var_name);

	//We'll print out some abbreviated stuff with the let record
	if(record->declare_or_let == 1){
		printf(" := <initializer>;");
	} else {
		printf(";\n");
	}
}


/**
 * Provide a function that will destroy the symtab completely. It is important to note that 
 * we must have the root level symtab here
*/
void destroy_symtab(symtab_t* symtab){
	//Function symtab
	if(symtab->type == FUNCTION){
		symtab_function_sheaf_t* cursor;
		symtab_function_record_t* record;
		symtab_function_record_t* temp;

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
	//var symtab
	} else {
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
	}

	//Once we're all done here, destroy the global symtab
	free(symtab);
}
