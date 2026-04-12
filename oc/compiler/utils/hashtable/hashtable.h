/**
 * Author: Jack Robbins
 * This module contains API definitions for a basic hash table used by the compiler. The hash table is most
 * often used for global value numbering optimizations
 */

//Include guards
#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <sys/types.h>

//Predeclare the hash table struct
typedef struct hash_table_t hash_table_t;

/**
 * The hash table struct contains everything that we need to keep
 * track of our hash table, including the keyspace and actual array
 * itself
 */
struct hash_table_t {
	void** internal_array;
	//How large is the internal array for the hash table
	u_int32_t keyspace;
};


/**
 * Allocate a hash table with the given keyspace. The keyspace
 * is always given by the user
 */
hash_table_t hash_table_alloc(u_int32_t keyspace);


/**
 * Deallocate the internal storage for the hash table
 */
void hash_table_dealloc(hash_table_t* table);


#endif /* HASH_TABLE_H */

