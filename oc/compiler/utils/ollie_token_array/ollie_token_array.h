/**
 * Author: Jack Robbins
 * A specified dynamic array that is to be used specifically for Ollie tokens. This is entirely
 * separate from a normal dynamic array because we are specifically storing tokens, not pointers
 * to tokens. Separately allocating every single token would be a performance nightmare, so we instead
 * store them in a flat data structure here and reference them by retrieving pointers to the flat structure
 *
 * This header file contains definitions for APIs that are defined in the C file, and also includes inlined 
 * definitions for frequently used, lightweight APIs like token_array_get_at()
 */

#ifndef OLLIE_TOKEN_ARRAY_H 
#define OLLIE_TOKEN_ARRAY_H 

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include "../constants.h"
#include <string.h>
//Link to the token/lexitem structs
#include "../token.h"

typedef struct ollie_token_array_t ollie_token_array_t;

/**
 * A simple dynamic array structure for holding ollie tokens
 *
 * This is heavily used by the lexer/preprocessor
*/
struct ollie_token_array_t{
	//The internal token array
	lexitem_t* internal_array;
	//The current maximum size
	int32_t current_max_size;
	//The current index that we're on - it also happens to be
	//how many nodes we have
	int32_t current_index;
};


/**
 * Simple initializer for a blank token array
 */
#define BLANK_TOKEN_ARRAY (ollie_token_array_t){NULL, 0, 0}

// =============================== Non-Inlined Functions ==================================================
/**
 * Heap allocate a token array. This allows us
 * to use something like an array of parameters, for
 * instance
 */
ollie_token_array_t* token_array_heap_alloc();

/**
 * Initialize a token array. The resulting
 * control structure will be stack allocated
 */
ollie_token_array_t token_array_alloc();

/**
 * Initialize a token array with an initial
 * size. This is useful if we already know
 * the size we need
 */
ollie_token_array_t token_array_alloc_initial_size(u_int32_t initial_size);

/**
 * Create an exact clone of the token array that we're given
 */
ollie_token_array_t clone_token_array(ollie_token_array_t* array);

/**
 * Does the token array contain this pointer?
 * 
 * RETURNS: the index if true, -1 if not
*/
int32_t token_array_contains(ollie_token_array_t* array, lexitem_t* lexitem);

/**
 * Add an item into the array. Note that we pass by copy for convenience, but we are
 * not storing pointers in the array
 */
void token_array_add(ollie_token_array_t* array, lexitem_t* lexitem);

/**
 * Set an element at a specified index. No check will be performed
 * to see if the element is already there. Dynamic resize
 * will be in effect here
 */
void token_array_set_at(ollie_token_array_t* array, lexitem_t* lexitem, int32_t index);

/**
 * Deallocate an entire token array. 
 */
void token_array_dealloc(ollie_token_array_t* array);

/**
 * Deallocate a token array on the heap
 */
void token_array_heap_dealloc(ollie_token_array_t* array);

// =============================== Non-Inlined Functions ==================================================
// =============================== Inlined Utility Functions ==============================================
/**
 * Get an element at a specified index. Do not remove the element
 *
 * Returns a copy of the specified element
 */
static inline lexitem_t token_array_get_at(ollie_token_array_t* array, int32_t index){
	if(array->current_max_size <= index){
		printf("Fatal internal compiler error: Attempt to get index %d in an array of size %d\n", index, array->current_max_size);
		exit(1);
	}

	//Give back a copy for this function
	return array->internal_array[index];
}


/**
 * Get a pointer to an element at a given index. Do not remove the element
 */
static inline lexitem_t* token_array_get_pointer_at(ollie_token_array_t* array, int32_t index){
	if(array->current_max_size <= index){
		printf("Fatal internal compiler error: Attempt to get index %d in an array of size %d\n", index, array->current_max_size);
		exit(1);
	}

	//Give back a copy for this function
	return &(array->internal_array[index]);
}


/**
 * Clear a token array entirely - keeps the size unchanged, but
 * sets the entire internal array to 0
 */
static inline void clear_token_array(ollie_token_array_t* array) {
	//Wipe the entire array out
	memset(array->internal_array, 0, sizeof(void*) * array->current_max_size);

	//Reset the current index
	array->current_index = 0;
}


/**
 * Is the token array empty?
 */
static inline u_int8_t token_array_is_empty(ollie_token_array_t* array) {
	return array->current_index == 0 ? TRUE : FALSE;
}


/**
 * Delete an element from the token array at a given index. Returns
 * the element at said index
 */
static inline lexitem_t token_array_delete_at(ollie_token_array_t* array, int32_t index) {
	//Validations here
	if(array->current_max_size <= index){
		printf("ERROR: attempting to delete an element at index %d in an array of size %d\n", index, array->current_max_size);
		exit(1);
	}

	//Grab the copy that we will be returning
	lexitem_t deleted = array->internal_array[index];
	
	//Shift everything over by the list to backfill
	for(int32_t i = index; i < array->current_index - 1; i++){
		array->internal_array[i] = array->internal_array[i + 1];
	}

	//The very last element should be blacked out
	array->internal_array[array->current_index - 1] = (lexitem_t){{0}, 0, BLANK};

	//Current index is now one less
	(array->current_index)--;
	
	//Give back the copy
	return deleted;
}


/**
 * Delete the pointer itself from the dynamic array
 *
 * Will not complain if it cannot be found - it simply won't be deleted
 */
static inline void token_array_delete(ollie_token_array_t* array, lexitem_t* lexitem) {
	//No point in going further here
	if(array == NULL || array->internal_array == NULL || lexitem == NULL){
		return;
	}

	//Get the index if the token array contains this
	int32_t index = token_array_contains(array, lexitem);

	//Couldn't find it, leave
	if(index == NOT_FOUND){
		return;
	}

	//Otherwise, use the helper to do the deletion
	token_array_delete_at(array, index);
}
// =============================== Inlined Utility Functions ==============================================
#endif /* OLLIE_TOKEN_ARRAY_H */
