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
#include "../utils/basic_block.h"
#include "../utils/dynamic_array/dynamic_array.h"

//If at any point a block has an ID of (-1), that means that it is in error and can be dealt with as such
static int32_t current_jump_block_id = 0;

/**
 * Increment and get the ID for the jump table
 */
static inline int32_t increment_and_get_id(){
	current_jump_block_id++;
	return current_jump_block_id;
}


/**
 * Allocate the jump table
 */
jump_table_t* jump_table_alloc(int32_t size){
	//Heap allocate
	jump_table_t* table = calloc(1, sizeof(jump_table_t));

	//Grab the ID for the table
	table->jump_table_id = increment_and_get_id();

	//Set the number of nodes
	table->num_nodes = size;

	//And initialize the dynamic array
	table->nodes = dynamic_array_alloc_initial_size(size);

	//And return a copy of this stack data
	return table;
}


/**
 * Completely clone a jump table using the "maps_to" entries inside
 * of the basic block struct. This is intended to be used only for
 * function inlining
 */
jump_table_t* clone_jump_table(jump_table_t* target){
	//Heap allocate the jump table
	jump_table_t* new_table = calloc(1, sizeof(jump_table_t));

	//Get it a fresh ID
	new_table->jump_table_id = increment_and_get_id();
	
	//Copy over the size
	new_table->num_nodes = target->num_nodes;

	//Allocate the actual jump table
	new_table->nodes = dynamic_array_alloc_initial_size(new_table->num_nodes);

	/**
	 * Now we will run through the target jump table in order and populate
	 * the new one using each block's "maps_to" field
	 */
	for(int32_t i = 0; i < new_table->num_nodes; i++){
		//Extract the target block
		basic_block_t* target_block = dynamic_array_get_at(&(target->nodes), i);

		//Populate the new jump table with the equivalent "maps_to" block
		dynamic_array_add(&(new_table->nodes), target_block->mapping_info.maps_to);
	}

	/**
	 * Finally populate the default block. It is possible for this
	 * to be NULL so we'll need to account for that
	 */
	basic_block_t* old_default_block = target->default_block;
	if(old_default_block != NULL){
		new_table->default_block = old_default_block->mapping_info.maps_to;
	}
	
	return new_table;
}


/**
 * Add a value into the jump table
 */
void add_jump_table_entry(jump_table_t* table, int32_t index, void* entry){
	//Throw an error for the programmer if this happens - we should never reach this
	if(table->num_nodes <= index){
		fprintf(stderr, "ERROR: jump table out of bounds");
		exit(1);
	}

	//Allow the API to set this accordingly
	dynamic_array_set_at(&(table->nodes), entry, index);
}


/**
 * Print a jump table in a stylized fashion. This jump table will be printed
 * out in full assembly ready order, as no optimization takes place on it
 */
void print_jump_table(FILE* fl, jump_table_t* table){
	//First thing that we'll print is the header info
	//This is in the read only data section and we want to align by 8 bytes
	fprintf(fl, ".section .rodata\n\t.align 8\n.JT%d:\n", table->jump_table_id);

	//Now we'll run through and print out everything in the table's values
	for(u_int16_t _ = 0; _ < table->num_nodes; _++){
		//Each node is a basic block
		basic_block_t* node = dynamic_array_get_at(&(table->nodes), _);

		//Now we'll print it
		fprintf(fl, "\t.quad\t.L%d\n", node->block_id);
	}

	//For readabilitiy
	fprintf(fl, "\n");
}


/**
 * Deallocate a jump table. Really all we do here is deallocate the
 * internal array
*/
void jump_table_dealloc(jump_table_t* table){
	//Deallocate the dynamic array
	dynamic_array_dealloc(&(table->nodes));

	//And we can free this structure as well
	free(table);
}
