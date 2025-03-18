/**
 * Author: Jack Robbins
 * A basic, generic, reusable dynamic array
*/

#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H
#include <sys/types.h>

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


/**
 * Initialize a dynamic array
 */
dynamic_array_t* dynamic_array_alloc();


/**
 * Does the dynamic array contain this pointer?
 * 
 * RETURNS: the index if true, -1 if not
*/
int16_t dynamic_array_contains(dynamic_array_t* array, void* ptr);


/**
 * Is the dynamic array is empty?
*/
u_int8_t dynamic_array_is_empty(dynamic_array_t* array);


/**
 * Insert an element into the dynamic array
 */
void dynamic_array_insert(dynamic_array_t* array, void* ptr);


/**
 * Deallocate an entire dynamic array. 
 *
 * NOTE: This will not touch/free any pointers in the array itself,
 * just the overall structure
*/
void dynamic_array_dealloc(dynamic_array_t* array);

#endif /* DYNAMIC_ARRAY_H */
