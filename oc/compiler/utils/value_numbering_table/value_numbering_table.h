/**
 * Author: Jack Robbins
 * This module contains the API definitions for a hashtable used by the global value numberer
 */

//Include guards
#ifndef VALUE_NUMBERING_TABLE_H 
#define VALUE_NUMBERING_TABLE_H

#include <sys/types.h>

//Predeclare the value numbering table struct
typedef struct value_numbering_table_t value_numbering_table_t;

/**
 * The hash table struct contains everything that we need to keep
 * track of our hash table, including the keyspace and actual array
 * itself
 */
struct value_numbering_table_t {
	void** internal_array;
	//How large is the internal array for the hash table
	u_int32_t keyspace;
};


/**
 * Allocate a hash table with the given keyspace. The keyspace
 * is always given by the user
 */
value_numbering_table_t value_numbering_table_alloc(u_int32_t keyspace);


/**
 * Add a given value into the hash table
 */
void add_value_number_expression(value_numbering_table_t* table, void* value, char* textual_string);


/**
 * Deallocate the internal storage for the hash table
 */
void value_numbering_table_dealloc(value_numbering_table_t* table);

#endif /* VALUE_NUMBERING_TABLE_H */
