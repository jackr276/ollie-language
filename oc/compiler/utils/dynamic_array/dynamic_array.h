/**
 * Author: Jack Robbins
 * A basic, generic, reusable dynamic array
*/

#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H
#include <sys/types.h>

#define NOT_FOUND -1


//The overall dynamic array structure
typedef struct dynamic_array_t dynamic_array_t;

/**
 * Allows for an automatically resizing, error-free
 * and thoughtless dynamic array. This is primarily
 * designed for the Worklists in SSA conversion, but
 * can be used anywhere
*/
struct dynamic_array_t{
	//The overall array - void* so it's generic
	void** internal_array;
	//The current maximum size
	u_int16_t current_max_size;
	//The current index that we're on - it also happens to be
	//how many nodes we have
	u_int16_t current_index;
};


/**
 * Initialize a dynamic array
 */
dynamic_array_t* dynamic_array_alloc();


/**
 * Initialize a dynamic array with an initial
 * size. This is useful if we already know
 * the size we need
 */
dynamic_array_t* dynamic_array_alloc_initial_size(u_int16_t initial_size);


/**
 * Create an exact clone of the dynamic array that we're given
 */
dynamic_array_t* clone_dynamic_array(dynamic_array_t* array);

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
 * Add an item into the dynamic array
 */
void dynamic_array_add(dynamic_array_t* array, void* ptr);


/**
 * Get an element at a specified index. Do not remove the element
 */
void* dynamic_array_get_at(dynamic_array_t* array, u_int16_t index);


/**
 * Set an element at a specified index. No check will be performed
 * to see if the element is already there. Dynamic resize
 * will be in effect here
 */
void dynamic_array_set_at(dynamic_array_t* array, void* ptr, u_int16_t index);


/**
 * Delete an element from the dynamic array at a given index. Returns
 * the element at said index
 */
void* dynamic_array_delete_at(dynamic_array_t* array, u_int16_t index);

/**
 * Delete the pointer itself from the dynamic array
 *
 * Will not complain if it cannot be found - it simply won't be deleted
 */
void dynamic_array_delete(dynamic_array_t* array, void* ptr);

/**
 * Remove an element from the back of the dynamic array - O(1) removal
 */
void* dynamic_array_delete_from_back(dynamic_array_t* array);

/**
 * Are two dynamic arrays completely equal? A "deep equals" 
 * will ensure that every single element in one array is also inside of the
 * other, and that no elements in one array are different
 */
u_int8_t dynamic_arrays_equal(dynamic_array_t* a, dynamic_array_t* b);

/**
 * Reset a dynamic array by wiping the contents of its memory
 */
void reset_dynamic_array(dynamic_array_t* array);

/**
 * Deallocate an entire dynamic array. 
 *
 * NOTE: This will not touch/free any pointers in the array itself,
 * just the overall structure
*/
void dynamic_array_dealloc(dynamic_array_t* array);

#endif /* DYNAMIC_ARRAY_H */
