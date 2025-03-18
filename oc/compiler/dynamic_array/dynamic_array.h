/**
 * Author: Jack Robbins
 * A basic, generic, reusable dynamic array
*/

#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H
#include <sys/types.h>

//The default is 20 -- this can always be reupped
#define DEFAULT_SIZE 20

//The overall dynamic array structure
typedef struct dynamic_array_t dynamic_array_t;

/**
 * Allows for an automatically resizing, error-free
 * and thoughtless dynamic array. This is primarily
 * designed for the Worklists in SSA conversion, but
 * can be used anywhere
*/
struct dynamic_array_t{
	//The current maximum size
	u_int16_t current_max_size;
	//The current index that we're on - it also happens to be
	//how many nodes we have
	u_int16_t current_index;
	//The overall array - void* so it's generic
	void** internal_array;
};







#endif
