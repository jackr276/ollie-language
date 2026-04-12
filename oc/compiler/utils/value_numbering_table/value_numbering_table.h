/**
 * Author: Jack Robbins
 * This module contains the API definitions for a hashtable used by the global value numberer
 */

//Include guards
#ifndef VALUE_NUMBERING_TABLE_H 
#define VALUE_NUMBERING_TABLE_H

#include <sys/types.h>

//Link to instruction for the three addr var
#include "../../instruction/instruction.h"

//Predeclare the value numbering table struct
typedef struct value_numbering_table_t value_numbering_table_t;
//The individual nodes within the value numbering table
typedef struct value_numbering_node_t value_numbering_node_t;

/**
 * The hash table struct contains everything that we need to keep
 * track of our hash table, including the keyspace and actual array
 * itself
 */
struct value_numbering_table_t {
	//Array of value numbering nodes
	value_numbering_node_t** table;
	//How large is the internal array for the hash table
	u_int32_t keyspace;
};


/**
 * Value numbering node exists to store the textual string
 * and the result value. It also holds a next pointer should
 * we need to traverse
 */
struct value_numbering_node_t {
	//The textual string that we use as the key
	dynamic_string_t textual_string;
	//What variable is the result value stored in?
	three_addr_var_t* result_value;
	//If we have collisions, this is the next pointer
	value_numbering_node_t* next;
};


/**
 * Allocate a hash table with the given keyspace. The keyspace
 * is always given by the user
 */
value_numbering_table_t value_numbering_table_alloc(u_int32_t keyspace);


/**
 * Add a given value into the hash table. Note that once this happens the memory for the dynamic string
 * is owned by this hash table 
 */
void add_value_number_expression(value_numbering_table_t* table, three_addr_var_t* result, dynamic_string_t textual_string);


/**
 * Lookup a value number expression based on the textual string. This returns the three_addr_var_t that holds the result if it
 * was found, or NULL if it was not
 */
three_addr_var_t* lookup_value_number_expression(value_numbering_table_t* table, dynamic_string_t* textual_string);


/**
 * Deallocate the internal storage for the hash table
 */
void value_numbering_table_dealloc(value_numbering_table_t* table);

#endif /* VALUE_NUMBERING_TABLE_H */
