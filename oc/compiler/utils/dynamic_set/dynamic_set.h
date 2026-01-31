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
dynamic_set_t clone_dynamic_set(dynamic_set_t* set);

/**
 * Does the dynamic set contain this pointer?
 * 
 * RETURNS: the index if true, -1 if not
*/
int16_t dynamic_set_contains(dynamic_set_t* set, void* ptr);

/**
 * Is the dynamic set empty?
*/
u_int8_t dynamic_set_is_empty(dynamic_set_t* set);

/**
 * Add an item into the dynamic set 
 */
void dynamic_set_add(dynamic_set_t* set, void* ptr);

/**
 * Clear a dynamic set entirely - keeps the size unchanged, but
 * sets the entire internal array to 0
 */
void clear_dynamic_set(dynamic_set_t* set);

/**
 * Get an element at a specified index. Do not remove the element
 */
void* dynamic_set_get_at(dynamic_set_t* set, u_int16_t index);

/**
 * Remove an element from the back of the dynamic set - O(1) removal
 */
void* dynamic_set_delete_from_back(dynamic_set_t* set);

/**
 * Delete an element from the dynamic set at a given index. Returns
 * the element at said index
 */
void* dynamic_set_delete_at(dynamic_set_t* set, u_int16_t index);

/**
 * Delete the pointer itself from the dynamic set
 *
 * Will not complain if it cannot be found - it simply won't be deleted
 */
void dynamic_set_delete(dynamic_set_t* array, void* ptr);

/**
 * Are two dynamic sets completely equal? A "deep equals" 
 * will ensure that every single element in one set is also inside of the
 * other, and that no elements in one set are different
 */
u_int8_t dynamic_sets_equal(dynamic_set_t* a, dynamic_set_t* b);

/**
 * Deallocate an entire dynamic set. 
 *
 * NOTE: This will not touch/free any pointers in the set itself,
 * just the overall structure
*/
void dynamic_set_dealloc(dynamic_set_t* set);

#endif /* DYNAMIC_SET_H */
