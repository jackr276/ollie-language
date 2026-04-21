/**
 * Author: Jack Robbins
 * This file contains the implementations for the APIs laid out in hashtable.h
 */

#include "value_numbering_table.h"
#include <sys/types.h>

//The starting offset basis for FNV-1a64
#define OFFSET_BASIS 14695981039346656037ULL

//The FNV prime for 64 bit hashes
#define FNV_PRIME 1099511628211ULL

//The finalizer constants for the avalanch finalizer
#define FINALIZER_CONSTANT_1 0xff51afd7ed558ccdULL
#define FINALIZER_CONSTANT_2 0xc4ceb9fe1a85ec53ULL


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
static inline u_int64_t hash(char* textual_key, u_int32_t keyspace){
	//Char pointer for the name
	char* cursor = textual_key;

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
	return hash & (keyspace - 1);
}


/**
 * Allocate a hash table with the given keyspace. The keyspace
 * is always given by the user
 */
value_numbering_table_t value_numbering_table_alloc(u_int32_t keyspace){
	//Stack allocate it
	value_numbering_table_t table = {NULL, keyspace};

	//We'll need to allocate the actual table now
	table.table = calloc(keyspace, sizeof(value_numbering_node_t));

	return table;
}


/**
 * Add a given value into the hash table. Note that once this happens the memory for the dynamic string
 * is owned by this hash table 
 */
void add_value_number_expression(value_numbering_table_t* table, three_addr_var_t* result, dynamic_string_t* textual_key){
	//First we'll need to hash this
	u_int64_t textual_key_hash = hash(textual_key->string, table->keyspace);

	//Allocate the fresh node here and populate it
	value_numbering_node_t* new_node = calloc(1, sizeof(value_numbering_node_t));
	new_node->result_value = result;
	new_node->textual_key = *textual_key;

	//Flag that this variable was itself value named
	result->was_value_named = TRUE;

	//Grab a pointer to the current value
	value_numbering_node_t* cursor = table->table[textual_key_hash];

	//If there's no collision then we can just add as is
	if(cursor == NULL){
		table->table[textual_key_hash] = new_node;
		return;
	}

	//Otherwise we'll need to drill down here
	while(cursor->next != NULL){
		//Bump it up
		cursor = cursor->next;
	}

	//Connect this to the chain
	cursor->next = new_node;
}


/**
 * Lookup a value number expression based on the textual string. This returns the three_addr_var_t that holds the result if it
 * was found, or NULL if it was not
 */
three_addr_var_t* lookup_value_number_expression(value_numbering_table_t* table, dynamic_string_t* textual_key){
	//First we'll need to hash this
	u_int64_t textual_key_hash = hash(textual_key->string, table->keyspace);

	//Grab a cursor to the node
	value_numbering_node_t* cursor = table->table[textual_key_hash];

	//So long as we have values occupied
	while(cursor != NULL){
		//If we have an exact match then we're good
		if(strcmp(cursor->textual_key.string, textual_key->string) == 0){
			return cursor->result_value;
		}

		//Bump it 
		cursor = cursor->next;
	}

	//If we made it down here then we found nothing
	return NULL;
}


/**
 * Deallocate the internal storage for the hash table
 */
void value_numbering_table_dealloc(value_numbering_table_t* table){
	/**
	 * Because of the way that the table works with collisions, we will need to traverse the
	 * entire thing and free anything that is a second level or below node as these will
	 * have been dynamically allocated
	 */

	//Run through everything
	for(u_int32_t i = 0; i < table->keyspace; i++){
		//Extract the node
		value_numbering_node_t* node = table->table[i];

		//Since we're now in the second node, this one was dynamically allocated
		while(node != NULL){
			//Temp pointer is needed
			value_numbering_node_t* temp = node;

			//Deallocate the string if we even have one
			dynamic_string_dealloc(&(node->textual_key));

			//Bump up the cursor pointer
			node = node->next;

			//Free the temp holder
			free(temp);
		}
	}

	//Once we get down here we can deallocate the underlying table
	free(table->table);
}
