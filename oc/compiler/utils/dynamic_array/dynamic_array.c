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
 * Dynamic resize utility - this should only ever be called by other
 * functions within this module and is therefore not exposed via
 * the API
 */
static inline void dynamic_resize_if_needed(dynamic_array_t* array, int32_t proposed_index){
	/**
	 * Less than the current maximum, this is not needed. We can just
	 * get out
	 */
	if(proposed_index < array->current_max_size){
		return;
	}

	/**
	 * Otherwise it's needed, so we'll need to resize by doubling the
	 * proposed index
	 */
	array->current_max_size = proposed_index * 2;
	array->internal_array = realloc(array->internal_array, sizeof(void*) * array->current_max_size);
}


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
dynamic_array_t dynamic_array_alloc_initial_size(int32_t initial_size){
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

	//Now we'll perform a memory copy. Only clone up to the current index
	memcpy(cloned.internal_array, array->internal_array, array->current_index * sizeof(void*));
	
	//Finally copy over the rest of the information
	cloned.current_index = array->current_index;
	cloned.current_max_size = array->current_max_size;

	//And return this pointer
	return cloned;
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

	/**
	 * Pass along the current index(what we'd be adding to)
	 * to determine if a resize is needed
	 */
	dynamic_resize_if_needed(array, array->current_index);

	//Now that we're all set, we can add our element in. Elements are always added in at the very end
	array->internal_array[array->current_index] = ptr;

	//Bump this up by 1
	array->current_index++;
}


/**
 * Add an item into the dynamic array *IF* the dynamic array
 * itself has been allocated. This is intended for a very specific
 * use in the instruction selector
 */
void dynamic_array_add_if_allocated(dynamic_array_t* array, void* ptr){
	/**
	 * The dynamic array has not been allocated so we just move on. If it
	 * has been then we'll fall into the regular add logic
	 */
	if(array->internal_array == NULL){
		return;
	}

	//Let's just double check here. Hard fail if this happens
	if(ptr == NULL){
		printf("ERROR: Attempting to insert a NULL pointer into a dynamic array\n");
		exit(1);
	}

	/**
	 * Resize if needed so that we can place something at the current index
	 */
	dynamic_resize_if_needed(array, array->current_index);

	//Now that we're all set, we can add our element in. Elements are always added in at the very end
	array->internal_array[array->current_index] = ptr;

	//Bump this up by 1
	array->current_index++;
}


/**
 * Set an element at a specified index. No check will be performed
 * to see if the element is already there. Dynamic resize
 * will be in effect here
 *
 * NOTE: we will NOT modify the so-called "current-index" that is used for setting. If the user
 * mixes these two together, they are responsible for the consequences
 */
void dynamic_array_set_at(dynamic_array_t* array, void* ptr, int32_t index){
	//Let's just double check here
	if(ptr == NULL){
		printf("ERROR: Attempting to set index %d to a NULL pointer ino a dynamic array\n", index);
		exit(1);
	}

	/**
	 * If the array's max size is smaller than the index
	 * that we want to insert at, we will dynamically resize
	 */
	dynamic_resize_if_needed(array, index);

	//Now that we've taken care of all that, we'll perform the setting
	array->internal_array[index] = ptr;
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

	//Do they have the same number of elements? If not - they can't possibly be equal
	//
	//TODO IS THIS STILL TRUE WITH SET AT LOGIC?
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
	for(int32_t i = 0; i < a->current_index; i++){
		//Let's grab out this pointer
		void* a_ptr = a->internal_array[i];

		//Assume by default we can't find it
		found_a = FALSE;

		//Now we must find this a_ptr in b. If we can't find
		//it, the whole thing is over
		for(int32_t j = 0; j < b->current_index; j++){
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
