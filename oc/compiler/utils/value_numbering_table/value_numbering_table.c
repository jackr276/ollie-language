/**
 * Author: Jack Robbins
 * This file contains the implementations for the APIs laid out in hashtable.h
 */

#include "value_numbering_table.h"
#include <sys/types.h>

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
		value_numbering_node_t* node = &(table->table[i]);

		//Deallocate the string if we even have one
		dynamic_string_dealloc(&(node->textual_string));

		//Advance to the next pointer
		node = node->next;

		//Since we're now in the second node, this one was dynamically allocated
		while(node != NULL){
			//Temp pointer is needed
			value_numbering_node_t* temp = node;

			//Deallocate the string if we even have one
			dynamic_string_dealloc(&(node->textual_string));

			//Bump up the cursor pointer
			node = node->next;

			//Free the temp holder
			free(temp);
		}
	}

	//Once we get down here we can deallocate the underlying table
	free(table->table);
}
