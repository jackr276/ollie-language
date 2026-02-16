/**
 * Author: Jack Robbins
 * Implementation file for the generic dynamic array
*/

//Link to header
#include "ollie_token_array.h"
#include "../constants.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>


/**
 * Perform a deep comparison of two lexitems
 */
static inline u_int8_t lexitems_equal(lexitem_t* a, lexitem_t* b) {
	//If these are not equal, then we must exit
	if(a->tok != b->tok) {
		return FALSE;
	}

	//If we have certain constants, we need to check for equality there too
	switch(a->tok){
		case LONG_CONST:
		case LONG_CONST_FORCE_U:
			if(a->constant_values.unsigned_long_value != b->constant_values.unsigned_long_value){
				return FALSE;
			}

			break;

		case INT_CONST:
		case INT_CONST_FORCE_U:
			if(a->constant_values.unsigned_int_value != b->constant_values.unsigned_int_value){
				return FALSE;
			}
			
			break;

		case SHORT_CONST:
		case SHORT_CONST_FORCE_U:
			if(a->constant_values.unsigned_short_value != b->constant_values.unsigned_short_value){
				return FALSE;
			}

			break;

		case BYTE_CONST:
		case BYTE_CONST_FORCE_U:
		case CHAR_CONST:
			if(a->constant_values.unsigned_byte_value != b->constant_values.unsigned_byte_value){
				return FALSE;
			}

			break;

		//These both carry internal lexemes that we need to compare
		case IDENT:
		case STR_CONST:
			//Let the helper do the comparison
			if(dynamic_strings_equal(&(a->lexeme), &(b->lexeme)) == FALSE){
				return FALSE;
			}

			break;

		//Do nothing by default
		default:
			break;
	}

	//If we made it all the way down here then this worked
	return TRUE;
}


/**
 * Initialize a token array. The resulting
 * control structure will be stack allocated
 */
ollie_token_array_t token_array_alloc(){
	//Stack allocate this
	ollie_token_array_t array;

	//The default token array size
	array.current_max_size = TOKEN_ARRAY_DEFAULT_SIZE;

	//Store the current index as well
	array.current_index = 0;

	//And now reserve the internal space that we need
	array.internal_array = calloc(array.current_max_size, sizeof(lexitem_t));

	//Give back a copy of the control structure
	return array;
}


/**
 * Heap allocate a token array. This allows us
 * to use something like an array of parameters, for
 * instance
 */
ollie_token_array_t* token_array_heap_alloc(){
	//Allocate on the heap
	ollie_token_array_t* array = calloc(1, sizeof(ollie_token_array_t));

	//The default token array size
	array->current_max_size = TOKEN_ARRAY_DEFAULT_SIZE;

	//Store the current index as well
	array->current_index = 0;

	//And now reserve the internal space that we need
	array->internal_array = calloc(array->current_max_size, sizeof(lexitem_t));

	//Give back a copy of the control structure
	return array;
}


/**
 * Initialize a token array with an initial
 * size. This is useful if we already know
 * the size we need
 */
ollie_token_array_t token_array_alloc_initial_size(u_int32_t initial_size){
	//Stack allocate this
	ollie_token_array_t array;

	//Use the size provided by the caller
	array.current_max_size = initial_size;

	//Store the current index as well
	array.current_index = 0;

	//And now reserve the internal space that we need
	array.internal_array = calloc(array.current_max_size, sizeof(lexitem_t));

	//Give back a copy of the control structure
	return array;
}


/**
 * Create an exact clone of the token array that we're given
 */
ollie_token_array_t clone_token_array(ollie_token_array_t* array){
	//If it's null then we just allocate
	if(array == NULL || array->current_max_size == 0){
		return token_array_alloc();
	}

	//Otherwise let's allocate a structure
	ollie_token_array_t clone;

	//Copy both of these values over
	clone.current_max_size = array->current_max_size;
	clone.current_index = array->current_index;

	//Now allocate an internal array of the exact desired size
	clone.internal_array = calloc(array->current_max_size, sizeof(lexitem_t));

	//Following this, we will duplicate the entire token array using a memcpy
	memcpy(clone.internal_array, array->internal_array, sizeof(lexitem_t) * array->current_index);

	//Finally we can return the token array
	return clone;
}


/**
 * Does the token array contain this pointer?
 * 
 * RETURNS: the index if true, -1 if not
*/
int32_t token_array_contains(ollie_token_array_t* array, lexitem_t* lexitem){
	//This by default means nothing is in there
	if(array == NULL || array->current_index == 0){
		return NOT_FOUND;
	}

	//Run through the entire array
	for(u_int32_t i = 0; i < array->current_index; i++){
		//Get a pointer to the current item
		lexitem_t* lexitem_ptr = &(array->internal_array[i]);

		//If these are equal, give back the index where we found
		//them
		if(lexitems_equal(lexitem_ptr, lexitem) == TRUE){
			return i;
		}
	}

	//If we made it all the way down here, then we've exhausted all of our
	//options in the array so this has to be false
	return FALSE;
}


/**
 * Is the token arrakj
*/
u_int8_t token_array_is_empty(ollie_token_array_t* array){
	if(array->current_index == 0){
		return TRUE;
	} else {
		return FALSE;
	}
}


/**
 * Add an item into the array. Note that we pass by copy for convenience, but we are
 * not storing pointers in the array
 *
 * This function will handle our dynamic resize
 */
void token_array_add(ollie_token_array_t* array, lexitem_t* lexitem){
	//If this is happening then something has gone wrong
	if(lexitem == NULL){
		printf("ERROR: Attempting to insert a NULL lexitem into a token array\n");
		exit(1);
	}

	//If we've hit the limit we need to reup
	if(array->current_index == array->current_max_size){
		//Double the array size
		array->current_max_size *= 2;

		//Reup the array nowj:w
		array->internal_array = realloc(array->internal_array, array->current_max_size * sizeof(lexitem_t));
	}

	//Now that we've handled any needed resize we can add in
	array->internal_array[array->current_index] = *lexitem;

	//Now bump this up for the next go around
	(array->current_index)++;
}


/**
 * Clear a token array entirely - keeps the size unchanged, but
 * sets the entire internal array to 0
 */
void clear_token_array(ollie_token_array_t* array){
	//Just so we don't end up with any nasty surprises
	if(array == NULL){
		printf("ERROR: Attempting to clear a NULL token array");
		exit(1);
	}

	//Wipe the entire thing out
	memset(array->internal_array, 0, sizeof(lexitem_t) * array->current_max_size);

	//Now go in here and reset the current index
	array->current_index = 0;
}

/**
 * Get an element at a specified index. Do not remove the element
 *
 * Returns a copy of the specified element
 */
lexitem_t token_array_get_at(ollie_token_array_t* array, u_int32_t index){
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
lexitem_t* token_array_get_pointer_at(ollie_token_array_t* array, u_int32_t index){
	if(array->current_max_size <= index){
		printf("Fatal internal compiler error: Attempt to get index %d in an array of size %d\n", index, array->current_max_size);
		exit(1);
	}

	//Give back a copy for this function
	return &(array->internal_array[index]);
}


/**
 * Set an element at a specified index. No check will be performed
 * to see if the element is already there. Dynamic resize
 * will be in effect here
 */
void token_array_set_at(ollie_token_array_t* array, lexitem_t* lexitem, u_int32_t index){
	//Just for safety's sake
	if(lexitem == NULL){
		printf("ERROR: Attempting to insert a NULL pointer into a token array\n");
		exit(1);
	}

	//If we're trying to overrun the bounds, that is bad
	if(array->current_max_size <= index){
		printf("ERROR: Attempting to insert at index %d in an array of size %d\n", array->current_max_size, index);
		exit(1);
	}

	//Otherwise we're fine to copy it in
	array->internal_array[index] = *lexitem;
}


/**
 * Delete an element from the token array at a given index. Returns a copy
 * the element at said index
 */
lexitem_t token_array_delete_at(ollie_token_array_t* array, u_int32_t index){
	//Validations here
	if(array->current_max_size <= index){
		printf("ERROR: attempting to delete an element at index %d in an array of size %d\n", index, array->current_max_size);
		exit(1);
	}

	//Grab the copy that we will be returning
	lexitem_t deleted = array->internal_array[index];
	
	//Shift everything over by the list to backfill
	for(u_int32_t i = index; i < array->current_index - 1; i++){
		array->internal_array[i] = array->internal_array[i + 1];
	}

	//Very last thing should be blanked out
	array->internal_array[array->current_index - 1].constant_values.unsigned_long_value = 0;
	array->internal_array[array->current_index - 1].line_num = 0;
	array->internal_array[array->current_index - 1].tok = BLANK;

	//Current index is now one less
	array->current_index--;
	
	//Give back the copy
	return deleted;
}

/**
 * Delete the pointer itself from the dynamic array
 *
 * Will not complain if it cannot be found - it simply won't be deleted
 */
void token_array_delete(ollie_token_array_t* array, lexitem_t* lexitem){
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


/**
 * Deallocate an entire token array. 
 */
void token_array_dealloc(ollie_token_array_t* array){
	//No point in going on here
	if(array->internal_array == NULL){
		return;
	}

	//Free the internal array
	free(array->internal_array);

	//Set everything to 0
	array->current_index = 0;
	array->current_max_size = 0;
}


/**
 * Deallocate a token array on the heap
 */
void token_array_heap_dealloc(ollie_token_array_t* array){
	//No point in going on here
	if(array->internal_array == NULL){
		return;
	}

	//Free the internal array
	free(array->internal_array);

	//Now deallocate the entire control structure
	free(array);
}
