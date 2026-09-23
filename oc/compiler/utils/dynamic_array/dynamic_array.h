/**
 * Author: Jack Robbins
 * A basic, generic, reusable dynamic array
 *
 * This header file contains API declarations for larger methods like allocators and deallocators
 * and contains inlined utility functions that are lightweight and frequently used, so inlining them
 * is advantageous
 */

#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H
#include "../constants.h"
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>

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
	int32_t current_max_size;
	//The current index that we're on - it also happens to be
	//how many nodes we have
	int32_t current_index;
};


// ======================= Non-Inlined Functions ====================================
/**
 * Dynamic array initializer macro
 */
#define INITIALIZE_DYNAMIC_ARRAY (dynamic_array_t){NULL, 0, 0}

/**
 * Initialize a dynamic array on the heap 
 * specifically
 */
dynamic_array_t* dynamic_array_heap_alloc();

/**
 * Initialize a dynamic array. The resulting
 * control structure will be stack allocated
 */
dynamic_array_t dynamic_array_alloc();

/**
 * Initialize a dynamic array with an initial
 * size. This is useful if we already know
 * the size we need
 */
dynamic_array_t dynamic_array_alloc_initial_size(int32_t initial_size);

/**
 * Create an exact clone of the dynamic array that we're given
 */
dynamic_array_t clone_dynamic_array(dynamic_array_t* array);

/**
 * Add an item into the dynamic array
 */
void dynamic_array_add(dynamic_array_t* array, void* ptr);

/**
 * Add an item into the dynamic array *IF* the dynamic array
 * itself has been allocated. This is intended for a very specific
 * use in the instruction selector
 */
void dynamic_array_add_if_allocated(dynamic_array_t* array, void* ptr);

/**
 * Set an element at a specified index. No check will be performed
 * to see if the element is already there. Dynamic resize
 * will be in effect here
 */
void dynamic_array_set_at(dynamic_array_t* array, void* ptr, int32_t index);

/**
 * Are two dynamic arrays completely equal? A "deep equals" 
 * will ensure that every single element in one array is also inside of the
 * other, and that no elements in one array are different
 */
u_int8_t dynamic_arrays_equal(dynamic_array_t* a, dynamic_array_t* b);

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

// ======================= Non-Inlined Functions ====================================
// ======================= Inlined Utility Functions ================================

/**
 * Does the dynamic array contain this pointer?
 * 
 * RETURNS: the index if true, -1 if not
*/
static inline int16_t dynamic_array_contains(dynamic_array_t* array, void* ptr){
	//If it's null just return false
	if(array == NULL || array->internal_array == NULL){
		return NOT_FOUND;
	}

	//We'll run through the entire array, comparing pointer by pointer
	for(int32_t i = 0; i < array->current_index; i++){
		//If we find an exact memory address match return true
		if(array->internal_array[i] == ptr){
			return i;
		}
	}

	//If we make it here, we found nothing so return the NOT_FOUND alias(-1)
	return NOT_FOUND;
}


/**
 * Is the dynamic array is empty?
 */
static inline u_int8_t dynamic_array_is_empty(dynamic_array_t* array){
	return array->current_index == 0 ? TRUE : FALSE;
}


/**
 * Clear a dynamic array entirely - keeps the size unchanged, but
 * sets the entire internal array to 0
 */
static inline void clear_dynamic_array(dynamic_array_t* array){
	//Just to be safe
	if(array == NULL){
		printf("ERROR: Attempting to clear a NULL dynamic array\n");
		exit(1);
	}

	//Wipe the entire thing out
	memset(array->internal_array, 0, sizeof(void*) * array->current_max_size);

	//Our current index is now 0
	array->current_index = 0;
}


/**
 * Get an element at a specified index. Do not remove the element
 */
static inline void* dynamic_array_get_at(dynamic_array_t* array, int32_t index){
	/**
	 * Return NULL here. It is the caller's responsibility to check this
	 */
	if(array->current_max_size <= index){
		printf("Fatal internal compiler error. Attempt to get index %d in an array of size %d\n", index, array->current_max_size);
		exit(1);
	}

	//Otherwise we should be good to grab. Again we do not delete here
	return array->internal_array[index];
}


/**
 * Delete an element from the dynamic array at a given index. Returns
 * the element at said index
 */
static inline void* dynamic_array_delete_at(dynamic_array_t* array, int32_t index) {
	//Again if we can't do this, we won't disrupt the program. Just return NULL
	if(array->current_index <= index){
		return NULL;
	}

	//We'll grab the element at this index first
	void* deleted = array->internal_array[index];

	//Now we'll run through everything from that index up until the end, shifting left every time
	for(int32_t i = index; i < array->current_index - 1; i++){
		array->internal_array[i] = array->internal_array[i + 1];
	}

	//Null this out
	array->internal_array[array->current_index - 1] = NULL;

	//We've seen one less of these now
	(array->current_index)--;

	//And once we've done that shifting, we're done so
	return deleted;
}


/**
 * Delete the pointer itself from the dynamic array
 *
 * Will not complain if it cannot be found - it simply won't be deleted
 */
static inline void dynamic_array_delete(dynamic_array_t* array, void* ptr){
	//If this is NULL or empty we'll just return
	if(ptr == NULL || array == NULL || array->current_index == 0){
		return;
	}

	//Otherwise we'll need to grab this index
	int16_t index = dynamic_array_contains(array, ptr);

	//If we couldn't find it - no harm, we just won't do anything
	if(index == NOT_FOUND){
		return;
	}

	//Now we'll use the index to delete
	dynamic_array_delete_at(array, index);
}


/**
 * Get the very last element in the dynamic array. Returns NULL if
 * the array is empty
 */
static inline void* dynamic_array_get_from_back(dynamic_array_t* array){
	//Already empty
	if(array->current_index == 0){
		return NULL;
	}

	//Grab off of the very end
	return array->internal_array[array->current_index - 1];
}


/**
 * Remove an element from the back of the dynamic array - O(1) removal
 */
static inline void* dynamic_array_delete_from_back(dynamic_array_t* array){
	//Already empty
	if(array->current_index == 0){
		return NULL;
	}

	//Grab off of the very end
	void* deleted = array->internal_array[array->current_index - 1];

	//Decrement the index
	(array->current_index)--;

	//Give back the pointer
	return deleted;
}
// ======================= Inlined Utility Methods ================================
#endif /* DYNAMIC_ARRAY_H */
