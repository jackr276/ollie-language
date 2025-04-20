/**
 * Author: Jack Robbins
 *
 * The implementation for the jump table
*/

//Link to header
#include "jump_table.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include "../cfg/cfg.h"

//If at any point a block has an ID of (-1), that means that it is in error and can be dealt with as such
static int32_t current_jump_block_id = 0;

/**
 * Increment and get the ID for the jump table
 */
static int32_t increment_and_get_id(){
	current_jump_block_id++;
	return current_jump_block_id;
}


/**
 * Allocate the jump table
 */
jump_table_t jump_table_alloc(u_int16_t size){
	//Stack allocate
	jump_table_t table;

	//Grab the ID for the table
	table.jump_table_id = increment_and_get_id();

	//Now we dynamically allocate the array
	table.nodes = calloc(sizeof(void*), size);
	//Now we set the actual value
	table.num_nodes = size;

	//And return a copy of this stack data
	return table;
}


/**
 * Add a value into the jump table
 */
void add_jump_table_entry(jump_table_t* table, u_int16_t index, void* entry){
	//Throw an error for the programmer if this happens - we should never reach this
	if(table->num_nodes <= index){
		fprintf(stderr, "ERROR: jump table out of bounds");
		exit(1);
	}

	//We simply add in like this
	table->nodes[index] = entry;
}

/**
 * Print a jump table in a stylized fashion
 */
void print_jump_table(jump_table_t* table){

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


