/**
 * Author: Jack Robbins
 * A basic, reusable dynamic set. This is nearly identical to a dynamic array with the exception
 * being that it will always enforce uniqueness on its members
*/

#ifndef DYNAMIC_SET_H
#define DYNAMIC_SET_H
#include <sys/types.h>

#define NOT_FOUND -1

typedef struct dynamic_set_t dynamic_set_t;

/**
 * Allows for an automatically resizing, error-free
 * and thoughtless dynamic set
*/
struct dynamic_set_t{
	//The overall array - void* so it's generic
	void** internal_array;
	//The current maximum size
	u_int16_t current_max_size;
	//The current index that we're on - it also happens to be
	//how many nodes we have
	u_int16_t current_index;
};


/**
 * Initialize a dynamic set. The resulting
 * control structure will be stack allocated
 */
dynamic_set_t dynamic_set_alloc();

/**
 * Initialize a dynamic set with an initial
 * size. This is useful if we already know
 * the size we need
 */
dynamic_set_t dynamic_set_alloc_initial_size(u_int16_t initial_size);

/**
 * Create an exact clone of the dynamic set that we're given
 */
dynamic_set_t clone_dynamic_array(dynamic_set_t* set);

/**
 * Does the dynamic set contain this pointer?
 * 
 * RETURNS: the index if true, -1 if not
*/
int16_t dynamic_set_contains(dynamic_set_t* set, void* ptr);

/**
 * Is the dynamic array is empty?
*/
u_int8_t dynamic_set_is_empty(dynamic_set_t* set);

/**
 * Add an item into the dynamic set 
 */
void dynamic_set_add(dynamic_set_t* set, void* ptr);

/**
 * Clear a dynamic array entirely - keeps the size unchanged, but
 * sets the entire internal array to 0
 */
void clear_dynamic_array(dynamic_array_t* array);

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

/**
 * Deallocate a dynamic array that was on the heap
 */
void dynamic_array_heap_dealloc(dynamic_array_t** array);

#endif /* DYNAMIC_SET_H */
