/**
 * Author: Jack Robbins
 *
 * The implementation for the jump table
*/

//Link to header
#include "jump_table.h"
#include <stdlib.h>


/**
 * Allocate the jump table
 */
jump_table_t jump_table_alloc(u_int32_t size){
	//Stack allocate
	jump_table_t table;

	//Now we dynamically allocate the array
	table.nodes = calloc(sizeof(void*), size);
	//Now we set the actual value
	table.num_nodes = size;

	//And return a copy of this stack data
	return table;
}


/**
 * Deallocate a jump table. Really all we do here is deallocate the
 * internal array
*/
void jump_table_dealloc(jump_table_t* table){
	//Just free the internal table, set to NULL as warnings
	free(table->nodes);
	
	//Set these for the future
	table->nodes = NULL;
	table->num_nodes = 0;
}


