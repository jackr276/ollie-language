/**
 * Author: Jack Robbins
 * Implementation file for the dynamic integer array
*/

//Link to header
#include "dynamic_integer_array.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "../constants.h"

/**
 * Allocate an entire dynamic array. The resulting control
 * structure will be stack allocated
*/
dynamic_integer_array_t dynamic_integer_array_alloc(){
	//First we'll create the overall structure
 	dynamic_integer_array_t array;

	//Set the max size using the sane default 
	array.current_max_size = DYNAMIC_ARRAY_DEFAULT_SIZE;

	//Starts off at 0
	array.current_index = 0;

	//Now we'll allocate the overall internal array
	array.internal_array = calloc(array.current_max_size, sizeof(int32_t));

	//Now we're all set
	return array;
} 


/**
 * Initialize a dynamic array with an initial
 * size. This is useful if we already know
 * the size we need
 */
dynamic_integer_array_t dynamic_integer_array_alloc_initial_size(int32_t initial_size){
	//First we'll create the overall structure
 	dynamic_integer_array_t array;

	//Set the max size using the sane default 
	array.current_max_size = initial_size;

	//Set the current index flag
	array.current_index = 0;

	//Now we'll allocate the overall internal array
	array.internal_array = calloc(array.current_max_size, sizeof(int32_t));

	//Now we're all set
	return array;
}


/**
 * Does the dynamic array contain this pointer?
 *
 * NOTE: This will currently do a linear scan. O(n) time, should be fast
 * enough for our purposes here. If it's really slowing things down, consider
 * sorting the array and binary searching
*/
u_int8_t dynamic_integer_array_contains(dynamic_integer_array_t* array, int32_t value){
	//If it's null just return false
	if(array == NULL || array->internal_array == NULL){
		return FALSE;
	}

	//We'll run through the entire array, comparing pointer by pointer
	for(int32_t i = 0; i < array->current_index; i++){
		//If we find an exact memory address match return true
		if(array->internal_array[i] == value){
			return TRUE;
		}
	}

	//If we make it here, we found nothing so
	return FALSE;
}


/**
 * Is the dynamic array is empty?
*/
u_int8_t dynamic_integer_array_is_empty(dynamic_integer_array_t* array){
	//We'll just return what the next index is
	if(array->current_index == 0){
		return TRUE;
	} else {
		return FALSE;
	}
}


/**
 * Add an element into the dynamic array
 */
void dynamic_integer_array_add(dynamic_integer_array_t* array, int32_t value){
	//Now we'll see if we need to reallocate this
	if(array->current_index == array->current_max_size){
		//We'll double the current max size
		array->current_max_size *= 2;

		//And we'll reallocate the array
		array->internal_array = realloc(array->internal_array, sizeof(int32_t) * array->current_max_size);
	}

	//Now that we're all set, we can add our element in. Elements are always added in at the very end
	array->internal_array[array->current_index] = value;

	//Bump this up by 1
	array->current_index++;

	//And we're all set
}


/**
 * Clear a dynamic array entirely - keeps the size unchanged, but
 * sets the entire internal array to 0
 */
void clear_dynamic_integer_array(dynamic_integer_array_t* array){
	//Just to be safe
	if(array == NULL){
		printf("ERROR: Attempting to clear a NULL dynamic array\n");
		exit(1);
	}

	//Wipe the entire thing out
	memset(array->internal_array, 0, sizeof(int32_t) * array->current_max_size);

	//Our current index is now 0
	array->current_index = 0;
}


/**
 * Get an element at a specified index. Do not remove the element
 */
int32_t dynamic_integer_array_get_at(dynamic_integer_array_t* array, int32_t index){
	//Return NULL here. It is the caller's responsibility to check this
	if(array->current_max_size <= index){
		printf("Fatal internal compiler error. Attempt to get index %d in an array of size %d\n", index, array->current_max_size);
		exit(1);
	}

	//Otherwise we should be good to grab. Again we do not delete here
	return array->internal_array[index];
}


/**
 * Set an element at a specified index. No check will be performed
 * to see if the element is already there. Dynamic resize
 * will be in effect here
 */
void dynamic_integer_array_set_at(dynamic_integer_array_t* array, int32_t value, int32_t index){
	//This is not allowed
	if(array->current_max_size <= index){
		printf("ERROR: Attempting to set index %d in an array of size %d\n", index, array->current_max_size);
		exit(1);
	}

	//Now that we've taken care of all that, we'll perform the setting
	array->internal_array[index] = value;

	//NOTE: we will NOT modify the so-called "current-index" that is used for setting. If the user
	//mixes these two together, they are responsible for the consequences
}


/**
 * Insert a value into a list in sorted order(least to greatest).
 * This is meant primarily for case statement handling. It also validates
 * the uniqueness constraint of the list given in
 *
 * Returns TRUE if the insertion worked, FALSE if a duplicate was found
 */
u_int8_t sorted_dynamic_integer_array_insert_unique(dynamic_integer_array_t* array, int32_t value){
	//Handle dynamic resize - always double
	if(array->current_index == array->current_max_size){
		array->current_max_size *= 2;

		array->internal_array = realloc(array->internal_array, sizeof(int32_t) * array->current_max_size);
	}
	
	//We will need this outside of the loop's scope
	int32_t i;

	//Run through everything in the list
	for(i = 0; i < array->current_index; i++){
		//Once we've found it, we can get out
		if(value < array->internal_array[i]){
			break;
		}

		//This invalidates the uniqueness constraint so we need to fail out
		if(value == array->internal_array[i]){
			return FALSE;
		}
	}

	//Bump this up now, we're going to have one more element
	array->current_index++;

	//Shift everything in the list to the right to make room
	for(int32_t j = array->current_index - 1; j > i; j--){
		//Shift over by 1 each time
		array->internal_array[j] = array->internal_array[j - 1];
	}

	//And finally, put in our guy
	array->internal_array[i] = value;

	//This worked
	return TRUE;
}



/**
 * Delete an element from a specified index. The element itself
 * is returned, allowing this to be used as a search & delete function
 * all in one
 */
int32_t dynamic_integer_array_delete_at(dynamic_integer_array_t* array, int32_t index){
	//Again if we can't do this, we won't disrupt the program. Just return NULL
	if(array->current_index <= index){
		return -1;
	}

	//We'll grab the element at this index first
	int32_t deleted = array->internal_array[index];

	//Now we'll run through everything from that index up until the end, 
	//shifting left every time
	for(u_int16_t i = index; i < array->current_index - 1; i++){
		//Shift left here
		array->internal_array[i] = array->internal_array[i + 1];
	}

	//Null this out
	array->internal_array[array->current_index - 1] = 0;	

	//We've seen one less of these now
	(array->current_index)--;

	//And once we've done that shifting, we're done so
	return deleted;
}


/**
 * Remove an element from the back of the dynamic array - O(1) removal
 */
int32_t dynamic_integer_array_delete_from_back(dynamic_integer_array_t* array){
	//Already empty
	if(array->current_index == 0){
		return -1;
	}

	//Grab off of the very end
	int32_t deleted = array->internal_array[array->current_index - 1];

	//Decrement the index
	(array->current_index)--;

	//Give back the pointer
	return deleted;
}


/**
 * Deallocate an entire dynamic array
*/
void dynamic_integer_array_dealloc(dynamic_integer_array_t* array){
	//Let's just make sure here...
	if(array->internal_array == NULL){
		return;
	}

	//First we'll free the internal array
	free(array->internal_array);

	//Set this to NULL as a warning
	array->internal_array = NULL;
	array->current_index = 0;
	array->current_max_size = 0;
}
