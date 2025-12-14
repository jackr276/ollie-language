/**
 * Author: Jack Robbins
 * Implementation file for the generic dynamic array
*/

//Link to header
#include "dynamic_array.h"
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
dynamic_array_t dynamic_array_alloc(){
	//First we'll create the overall structure
 	dynamic_array_t array;

	//Set the max size using the sane default 
	array.current_max_size = DYNAMIC_ARRAY_DEFAULT_SIZE;

	//Starts off at 0
	array.current_index = 0;

	//Now we'll allocate the overall internal array
	array.internal_array = calloc(array.current_max_size, sizeof(void*));

	//Now we're all set
	return array;
} 


/**
 * Initialize a dynamic array on the heap 
 * specifically. This should only be used
 * when you absolutely need it
 */
dynamic_array_t* dynamic_array_heap_alloc(){
	//First we'll create the overall structure
 	dynamic_array_t* array = calloc(1, sizeof(dynamic_array_t));

	//Set the max size using the sane default 
	array->current_max_size = DYNAMIC_ARRAY_DEFAULT_SIZE;

	//Starts off at 0
	array->current_index = 0;

	//Now we'll allocate the overall internal array
	array->internal_array = calloc(array->current_max_size, sizeof(void*));

	//Now we're all set
	return array;
}


/**
 * Initialize a dynamic array with an initial
 * size. This is useful if we already know
 * the size we need
 */
dynamic_array_t dynamic_array_alloc_initial_size(u_int16_t initial_size){
//First we'll create the overall structure
 	dynamic_array_t array;

	//Set the max size using the sane default 
	array.current_max_size = initial_size;

	//Set the current index flag
	array.current_index = 0;

	//Now we'll allocate the overall internal array
	array.internal_array = calloc(array.current_max_size, sizeof(void*));

	//Now we're all set
	return array;
}


/**
 * Create an exact clone of the dynamic array that we're given
 */
dynamic_array_t clone_dynamic_array(dynamic_array_t* array){
	//If it's null then we'll just allocate for the user
	if(array == NULL || array->current_index == 0){
		return dynamic_array_alloc();
	}

	//First we create the overall structure
	dynamic_array_t cloned;

	//Now we'll create the array for it - of the exact same size as the original
	cloned.internal_array = calloc(array->current_max_size, sizeof(void*));

	//Now we'll perform a memory copy
	memcpy(cloned.internal_array, array->internal_array, array->current_max_size * sizeof(void*));
	
	//Finally copy over the rest of the information
	cloned.current_index = array->current_index;
	cloned.current_max_size = array->current_max_size;

	//And return this pointer
	return cloned;
}


/**
 * Does the dynamic array contain this pointer?
 *
 * NOTE: This will currently do a linear scan. O(n) time, should be fast
 * enough for our purposes here. If it's really slowing things down, consider
 * sorting the array and binary searching
*/
int16_t dynamic_array_contains(dynamic_array_t* array, void* ptr){
	//If it's null just return false
	if(array == NULL || array->internal_array == NULL){
		return NOT_FOUND;
	}

	//We'll run through the entire array, comparing pointer by pointer
	for(u_int16_t i = 0; i < array->current_index; i++){
		//If we find an exact memory address match return true
		if(array->internal_array[i] == ptr){
			return i;
		}
	}

	//If we make it here, we found nothing so
	return NOT_FOUND;
}


/**
 * Is the dynamic array is empty?
*/
u_int8_t dynamic_array_is_empty(dynamic_array_t* array){
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
void dynamic_array_add(dynamic_array_t* array, void* ptr){
	//Let's just double check here. Hard fail if this happens
	if(ptr == NULL){
		printf("ERROR: Attempting to insert a NULL pointer into a dynamic array\n");
		exit(1);
	}
	
	//Now we'll see if we need to reallocate this
	if(array->current_index == array->current_max_size){
		//We'll double the current max size
		array->current_max_size *= 2;

		//And we'll reallocate the array
		array->internal_array = realloc(array->internal_array, sizeof(void*) * array->current_max_size);
	}

	//Now that we're all set, we can add our element in. Elements are always added in at the very end
	array->internal_array[array->current_index] = ptr;

	//Bump this up by 1
	array->current_index++;

	//And we're all set
}


/**
 * Clear a dynamic array entirely - keeps the size unchanged, but
 * sets the entire internal array to 0
 */
void clear_dynamic_array(dynamic_array_t* array){
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
void* dynamic_array_get_at(dynamic_array_t* array, u_int16_t index){
	//Return NULL here. It is the caller's responsibility
	//to check this
	if(array->current_max_size <= index){
		printf("Fatal internal compiler error. Attempt to get index %d in an array of size %d\n", index, array->current_index);
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
void dynamic_array_set_at(dynamic_array_t* array, void* ptr, u_int16_t index){
	//Let's just double check here
	if(ptr == NULL){
		printf("ERROR: Attempting to set index %d to a NULL pointer ino a dynamic array\n", index);
		exit(1);
	}

	//There is always a chance that we'll need to resize here. If so, we'll resize
	//enough to comfortably fix the new index
	if(array->current_max_size <= index){
		//The current max size is now double the index
		array->current_max_size = index * 2;

		//We'll now want to realloc
		array->internal_array = realloc(array->internal_array, sizeof(void*) * array->current_max_size);
	}

	//Now that we've taken care of all that, we'll perform the setting
	array->internal_array[index] = ptr;

	//NOTE: we will NOT modify the so-called "current-index" that is used for setting. If the user
	//mixes these two together, they are responsible for the consequences
}


/**
 * Delete an element from a specified index. The element itself
 * is returned, allowing this to be used as a search & delete function
 * all in one
 */
void* dynamic_array_delete_at(dynamic_array_t* array, u_int16_t index){
	//Again if we can't do this, we won't disrupt the program. Just return NULL
	if(array->current_index <= index){
		return NULL;
	}

	//We'll grab the element at this index first
	void* deleted = array->internal_array[index];

	//Now we'll run through everything from that index up until the end, 
	//shifting left every time
	for(u_int16_t i = index; i < array->current_index - 1; i++){
		//Shift left here
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
void dynamic_array_delete(dynamic_array_t* array, void* ptr){
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

	//And we're done
}


/**
 * Remove an element from the back of the dynamic array - O(1) removal
 */
void* dynamic_array_delete_from_back(dynamic_array_t* array){
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


/**
 * Are two dynamic arrays completely equal? A "deep equals" 
 * will ensure that every single element in one array is also inside of the
 * other, and that no elements in one array are different
 */
u_int8_t dynamic_arrays_equal(dynamic_array_t* a, dynamic_array_t* b){
	//Safety check here 
	if(a == NULL || b == NULL){
		return FALSE;
	}

	//Do they have the same number of elements? If not - they can't
	//possibly be equal
	if(a->current_index != b->current_index){
		return FALSE;
	}
	
	//If we get here, we know that they have the same number of elements.
	//Now we'll have to check if every single element matches. An important
	//note is that order does not matter here. In fact, most of the time
	//arrays that are the same have different orders
	
	//Did we find the a_ptr?
	u_int8_t found_a;
	
	//For every node in the "a" array
	for(u_int16_t i = 0; i < a->current_index; i++){
		//Let's grab out this pointer
		void* a_ptr = a->internal_array[i];

		//Assume by default we can't find it
		found_a = FALSE;

		//Now we must find this a_ptr in b. If we can't find
		//it, the whole thing is over
		for(u_int16_t j = 0; j < b->current_index; j++){
			//If we have a match, set the flag to
			//true and get out
			if(a_ptr == b->internal_array[j]){
				found_a = TRUE;
				break;
			}
			//Otherwise we keep chugging along
		}

		//If we get out here AND we did not find A, we
		//have a difference. As such, we're done here
		if(found_a == FALSE){
			return FALSE;
		}

		//Otherwise we did find a_ptr, so we'll go onto the next one
	}

	//If we made it all the way down here, then they're the same
	return TRUE;
}


/**
 * Reset a dynamic array by wiping the contents of its memory
 */
void reset_dynamic_array(dynamic_array_t* array){
	//If this is the case then just bail
	if(array == NULL || array->internal_array == NULL){
		return;
	}

	//Otherwise we'll memset by wiping everything
	memset(array->internal_array, 0, array->current_max_size * sizeof(void**));

	//The current index is now 0
	array->current_index = 0;
}


/**
 * Deallocate an entire dynamic array
*/
void dynamic_array_dealloc(dynamic_array_t* array){
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


/**
 * Deallocate a dynamic array that was on the heap
 */
void dynamic_array_heap_dealloc(dynamic_array_t** array){
	//Let's just make sure here...
	if((*array)->internal_array == NULL){
		return;
	}

	//First we'll free the internal array
	free((*array)->internal_array);

	//Free the overall structure too
	free(*array);

	//Set this as a warning
	*array = NULL;
}
