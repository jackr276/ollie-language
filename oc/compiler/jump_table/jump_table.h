/**
 * Author: Jack Robbins
 *
 * This module defines a type for a switch statement jumpt table. Ollie enforces the use of switch statements
 * that are able to be done using these kinds of jumps. This means that the case space must form a compact or reasonably
 * compact set(no more than a gap of 2 or 3 from the top)
*/

//Include guards
#ifndef JUMP_TABLE_H
#define JUMP_TABLE_H

#include <sys/types.h>
#include <stdio.h>
#include "../utils/dynamic_array/dynamic_array.h"

//Jump table structure
typedef struct jump_table_t jump_table_t;

/**
 * A jump table is a simple ordered array of values. We will require the user
 * to declare the range of values for the jump
*/
struct jump_table_t {
	//The list of all nodes. This is internally a dynamic array
	dynamic_array_t nodes;
	//The number of nodes
	int32_t num_nodes;
	//The ID of the jump table. Jump tables get IDs just like blocks, although
	//these tables are distinct
	int32_t jump_table_id;
};

/**
 * Allocate the jump table
 */
jump_table_t* jump_table_alloc(int32_t size);

/**
 * Completely clone a jump table using the "maps_to" entries inside
 * of the basic block struct. This is intended to be used only for
 * function inlining
 */
jump_table_t* clone_jump_table(jump_table_t* target);

/**
 * Insert an entry into the jump table. This will be used
 * for adding values from case statements in
 */
void add_jump_table_entry(jump_table_t* table, int32_t index, void* entry);

/**
 * Print a jump table in a stylized fashion
 */
void print_jump_table(FILE* fl, jump_table_t* table);

/**
 * Deallocate the jump table
 */
void jump_table_dealloc(jump_table_t* table);

/**
 * A simple utility function that prints the search table out. It is important to note
*/

#endif /* JUMP_TABLE_H */
