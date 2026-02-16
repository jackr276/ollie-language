/**
 * Author: Jack Robbins
 * A specified dynamic array that is to be used specifically for Ollie tokens. This is entirely
 * separate from a normal dynamic array because we are specifically storing tokens, not pointers
 * to tokens. Separately allocating every single token would be a performance nightmare
*/

#ifndef OLLIE_TOKEN_ARRAY_H 
#define OLLIE_TOKEN_ARRAY_H 

#include <sys/types.h>
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
	u_int32_t current_max_size;
	//The current index that we're on - it also happens to be
	//how many nodes we have
	u_int32_t current_index;
};


/**
 * Heap allocate a token array. This allows us
 * to use something like an array of parameters, for
 * instance
 */
ollie_token_array_t* token_array_heap_alloc();

/**
 * Initialize a token array to be all blank
 */
ollie_token_array_t initialize_blank_token_array();

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
 * Is the token array empty?
*/
u_int8_t token_array_is_empty(ollie_token_array_t* array);

/**
 * Add an item into the array. Note that we pass by copy for convenience, but we are
 * not storing pointers in the array
 */
void token_array_add(ollie_token_array_t* array, lexitem_t* lexitem);

/**
 * Clear a token array entirely - keeps the size unchanged, but
 * sets the entire internal array to 0
 */
void clear_token_array(ollie_token_array_t* array);

/**
 * Get an element at a specified index. Do not remove the element
 *
 * Returns a copy of the specified element
 */
lexitem_t token_array_get_at(ollie_token_array_t* array, u_int32_t index);

/**
 * Get a pointer to an element at a given index. Do not remove the element
 */
lexitem_t* token_array_get_pointer_at(ollie_token_array_t* array, u_int32_t index);

/**
 * Set an element at a specified index. No check will be performed
 * to see if the element is already there. Dynamic resize
 * will be in effect here
 */
void token_array_set_at(ollie_token_array_t* array, lexitem_t* lexitem, u_int32_t index);

/**
 * Delete an element from the token array at a given index. Returns
 * the element at said index
 */
lexitem_t token_array_delete_at(ollie_token_array_t* array, u_int32_t index);

/**
 * Delete the pointer itself from the dynamic array
 *
 * Will not complain if it cannot be found - it simply won't be deleted
 */
void token_array_delete(ollie_token_array_t* array, lexitem_t* lexitem);

/**
 * Deallocate an entire token array. 
 */
void token_array_dealloc(ollie_token_array_t* array);

/**
 * Deallocate a token array on the heap
 */
void token_array_heap_dealloc(ollie_token_array_t* array);

#endif /* OLLIE_TOKEN_ARRAY_H */
