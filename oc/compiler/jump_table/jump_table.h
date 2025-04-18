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

//Jump table structure
typedef struct jump_table_t jump_table_t;

/**
 * A jump table is a simple ordered array of values. We will require the user
 * to declare the range of values for the jump
*/
struct jump_table_t{
	//A list of all nodes. This list is guaranteed to always be sorted
	void** nodes;
	//The number of nodes that we have
	u_int32_t num_nodes;
};

/**
 * A simple utility function that prints the search table out. It is important to note
*/

#endif /* JUMP_TABLE_H */
