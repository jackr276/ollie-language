/**
 * The implementation of the symbol table
*/

#include "symtab.h"
#include <stdlib.h>
#include <sys/types.h>

#define LARGE_PRIME 611593

/**
 * Initialize a new lexical scope
*/
symtab_t* initialize_scope(symtab_t* symtab){
	//If there is no next level
	if(symtab->next_level == NULL){
		//Let's make one
		symtab_t* next_level = (symtab_t*)calloc(1, sizeof(symtab_t));
		//Next lexical level
		next_level->lexical_level = symtab->lexical_level + 1;
		return next_level;
	} 

	//Otherwise there already exist a next level, so we'll just return that
	return symtab->next_level;
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
	if(symtab->records[record->hash] == NULL){
		//Store this and get out
		symtab->records[record->hash] = record;
		//0 = success, no collision
		return 0;
	}

	//Otherwise, there is a collision
	//Grab the head record
	symtab_record_t* cursor = symtab->records[record->hash];

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




