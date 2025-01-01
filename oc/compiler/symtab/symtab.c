/**
 * The implementation of the symbol table
*/

#include "symtab.h"
#include <sys/types.h>

#define LARGE_PRIME 611593


/**
 * Dynamically allocate and return a symtab pointer for compiler use
 */
symtab_t* initialize_symtab(){
	symtab_t* symtab = (symtab_t*)calloc(1, sizeof(symtab_t));
	//Just in case
	symtab->current_lexical_scope = 0;
	symtab->next_index = 0;

	return symtab;
}

/**
 * Initialize a new lexical scope. This involves making a new sheaf and
 * adding it in
*/
void initialize_scope(symtab_t* symtab){
	//Dynamically allocate a new one
	symtab_sheaf_t* current = (symtab_sheaf_t*)calloc(1, sizeof(symtab_sheaf_t));

	//Store it in here for later
	symtab->sheafs[symtab->next_index++] = current;

	//Increment if it isn't 0
	if(symtab->current_lexical_scope != 0){
		symtab->current_lexical_scope++;
	}

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
void finalize_scope(symtab_t* symtab){
	//Back out of this one as it's finalized
	symtab->current = symtab->current->previous_level;

	//Back up one
	if(symtab->current_lexical_scope != 0){
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
 * Dynamically allocate a record
*/
symtab_record_t* create_record(char* name, u_int16_t lexical_level, u_int64_t offset){
	//Allocate it
	symtab_record_t* record = (symtab_record_t*)calloc(1, sizeof(symtab_record_t));

	//Store the name
	record->name = name;
	//Hash it and store it to avoid to repeated hashing
	record->hash = hash(name);
	record->lexical_level = lexical_level;
	//This here is not used currently
	record->offset = offset;

	return record;
}


/**
 * Insert a record into the symbol table. This assumes that the user 
 * has already checked to see if this record is in the symbol table
*/
u_int8_t insert(symtab_t* symtab, symtab_record_t* record){
	//No collision here, just store and get out
	if(symtab->current->records[record->hash] == NULL){
		//Store this and get out
		symtab->current->records[record->hash] = record;
		//0 = success, no collision
		return 0;
	}

	//Otherwise, there is a collision
	//Grab the head record
	symtab_record_t* cursor = symtab->current->records[record->hash];

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
 * Lookup the record in the symtab that corresponds to the following name.
 * 
 * We are ALWAYS biased to the most local(in scope) version of the name. If we
 * do not find it in the local scope, we then search the outer scope, until there are
 * no more outer scopes to search
 */
symtab_record_t* lookup(symtab_t* symtab, char* name){
	//Let's grab it's hash
	u_int16_t h = hash(name); 

	//Define the cursor so we don't mess with the original reference
	symtab_sheaf_t* cursor = symtab->current;
	symtab_record_t* records_cursor;

	//As long as the previous level is not null
	while(cursor->previous_level != NULL){
		records_cursor = cursor->records[h];
		
		//If we actually have something in here
		if(records_cursor != NULL){
			//We could have had collisions so we'll have to hunt here
			while(records_cursor != NULL){
				//If we find the right one, then we can get out
				if(strcmp(records_cursor->name, name) == 0){
					return records_cursor;
				}
				//Advance it
				records_cursor = records_cursor->next;
			}
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
void print_record(symtab_record_t* record){
	//Safety check
	if(record == NULL){
		printf("NULL RECORD\n");
		return;
	}

	printf("Record: {\n");
	printf("Name: %s,\n", record->name);
	printf("Lexical Level: %d,\n", record->lexical_level);
	printf("Offset: %p\n", (void*)(record->offset));
	printf("}\n");
}


/**
 * Provide a function that will destroy the symtab completely. It is important to note that 
 * we must have the root level symtab here
*/
void destroy_symtab(symtab_t* symtab){
	symtab_sheaf_t* cursor;
	symtab_record_t* record;
	symtab_record_t* temp;

	//Run through all of the sheafs
	for(u_int16_t i = 0; i < symtab->next_index; i++){
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
	}

	//Once we're all done here, destroy the global symtab
	free(symtab);
}
