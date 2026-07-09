/**
 * Author: Jack Robbins
 * A dynamic integer array for our general use
*/

#ifndef DYNAMIC_INTEGER_ARRAY_H
#define DYNAMIC_INTEGER_ARRAY_H
#include <sys/types.h>

//The overall dynamic integer array structure
typedef struct dynamic_integer_array_t dynamic_integer_array_t;

/**
 * Allows for an automatically resizing, error-free
 * and thoughtless dynamic array. This is primarily
 * designed for the Worklists in SSA conversion, but
 * can be used anywhere
*/
struct dynamic_integer_array_t{
	//The overall array - void* so it's generic
	int* internal_array;
	//The current maximum size
	int32_t current_max_size;
	//The current index that we're on - it also happens to be how many nodes we have
	int32_t current_index;
};

/**
 * Macro to initialize a NULL stack allocated dynamic array
 */
#define INITIALIZE_NULL_DYNAMIC_INTEGER_ARRAY(dynamic_integer_array)\
	dynamic_integer_array.internal_array = NULL;\
	dynamic_integer_array.current_max_size = 0;\
	dynamic_integer_array.current_index = 0;\

/**
 * Initialize a dynamic integer array. The resulting
 * control structure will be stack allocated
 */
dynamic_integer_array_t dynamic_integer_array_alloc();

/**
 * Initialize a dynamic array with an initial
 * size. This is useful if we already know
 * the size we need
 */
dynamic_integer_array_t dynamic_integer_array_alloc_initial_size(int32_t initial_size);

/**
 * Does the dynamic integer array contain this value?
 */
u_int8_t dynamic_integer_array_contains(dynamic_integer_array_t* array, int32_t value);

/**
 * Is the dynamic integer array empty?
*/
u_int8_t dynamic_integer_array_is_empty(dynamic_integer_array_t* array);

/**
 * Add an item into the dynamic array
 */
void dynamic_integer_array_add(dynamic_integer_array_t* array, int32_t value);

/**
 * Clear a dynamic array entirely - keeps the size unchanged, but
 * sets the entire internal array to 0
 */
void clear_dynamic_integer_array(dynamic_integer_array_t* array);

/**
 * Get an element at a specified index. Do not remove the element
 */
int32_t dynamic_integer_array_get_at(dynamic_integer_array_t* array, int32_t index);

/**
 * Set an element at a specified index. No check will be performed
 * to see if the element is already there. Dynamic resize
 * will be in effect here
 */
void dynamic_integer_array_set_at(dynamic_integer_array_t* array, int32_t value, int32_t index);

/**
 * Insert a value into a list in sorted order(least to greatest).
 * This is meant primarily for case statement handling. It also validates
 * the uniqueness constraint of the list given in
 *
 * Returns TRUE if the insertion worked, FALSE if a duplicate was found
 */
u_int8_t sorted_dynamic_integer_array_insert_unique(dynamic_integer_array_t* array, int32_t value);

/**
 * Delete an element from the dynamic array at a given index. Returns
 * the element at said index
 */
int32_t dynamic_integer_array_delete_at(dynamic_integer_array_t* array, int32_t index);

/**
 * Remove an element from the back of the dynamic array - O(1) removal
 */
int32_t dynamic_integer_array_delete_from_back(dynamic_integer_array_t* array);

/**
 * Deallocate an entire dynamic array. 
 *
 * NOTE: This will not touch/free any pointers in the array itself,
 * just the overall structure
*/
void dynamic_integer_array_dealloc(dynamic_integer_array_t* array);

#endif /* DYNAMIC_INTEGER_ARRAY_H */
