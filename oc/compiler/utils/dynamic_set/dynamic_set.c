/**
 * Author: Jack Robbins
 * Implementation file for the generic dynamic set
*/

//Link to header
#include "dynamic_set.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "../constants.h"

/**
 * Allocate an entire dynamic array. The resulting control
 * structure will be stack allocated
*/
dynamic_set_t dynamic_set_alloc(){
	//First we'll create the overall structure
 	dynamic_set_t set;

	//Set the max size using the sane default 
	set.current_max_size = DYNAMIC_ARRAY_DEFAULT_SIZE;

	//Starts off at 0
	set.current_index = 0;

	//Now we'll allocate the overall internal array
	set.internal_array = calloc(set.current_max_size, sizeof(void*));

	//Now we're all set
	return set;
} 


/**
 * Initialize a dynamic set with an initial
 * size. This is useful if we already know
 * the size we need
 */
dynamic_set_t dynamic_set_alloc_initial_size(u_int16_t initial_size){
//First we'll create the overall structure
 	dynamic_set_t set;

	//Set the max size using the sane default 
	set.current_max_size = initial_size;

	//Set the current index flag
	set.current_index = 0;

	//Now we'll allocate the overall internal array
	set.internal_array = calloc(set.current_max_size, sizeof(void*));

	//Now we're all set
	return set;
}


/**
 * Create an exact clone of the dynamic array that we're given
 */
dynamic_set_t clone_dynamic_set(dynamic_set_t* set){
	//If it's null then we'll just allocate for the user
	if(set == NULL || set->current_index == 0){
		return dynamic_set_alloc();
	}

	//First we create the overall structure
	dynamic_set_t cloned;

	//Now we'll create the array for it - of the exact same size as the original
	cloned.internal_array = calloc(set->current_max_size, sizeof(void*));

	//Now we'll perform a memory copy
	memcpy(cloned.internal_array, set->internal_array, set->current_max_size * sizeof(void*));
	
	//Finally copy over the rest of the information
	cloned.current_index = set->current_index;
	cloned.current_max_size = set->current_max_size;

	//And give back what was cloned
	return cloned;
}


/**
 * Does the dynamic set contain this pointer?
 *
 * NOTE: This will currently do a linear scan. O(n) time, should be fast
 * enough for our purposes here. If it's really slowing things down, consider
 * sorting the array and binary searching
*/
int16_t dynamic_set_contains(dynamic_set_t* set, void* ptr){
	//If it's null just return false
	if(set == NULL || set->internal_array == NULL){
		return NOT_FOUND;
	}

	//We'll run through the entire array, comparing pointer by pointer
	for(u_int16_t i = 0; i < set->current_index; i++){
		//If we find an exact memory address match return true
		if(set->internal_array[i] == ptr){
			return i;
		}
	}

	//If we make it here, we found nothing so
	return NOT_FOUND;
}


/**
 * Is the dynamic set empty?
*/
u_int8_t dynamic_set_is_empty(dynamic_set_t* set){
	//We'll just return what the next index is
	if(set->current_index == 0){
		return TRUE;
	} else {
		return FALSE;
	}
}


/**
 * Add an element into the dynamic set. This function enforces
 * uniqueness in the addition
 */
void dynamic_set_add(dynamic_set_t* set, void* ptr){
	//Let's just double check here. Hard fail if this happens
	if(ptr == NULL){
		printf("ERROR: Attempting to insert a NULL pointer into a dynamic set\n");
		exit(1);
	}

	//Let's now check and see if the dynamic set already contains this memory
	//address inside of it. If it does, then we'll just leave without adding
	//anything
	for(u_int16_t i = 0; i < set->current_index; i++){
		//Compare our pointer with the stored pointer. If we have a match,
		//then we need to exit out
		if(set->internal_array[i] == ptr){
			return;
		}
	}

	//If we make it here, then we know that we are good to add
	
	//Now we'll see if we need to reallocate this
	if(set->current_index == set->current_max_size){
		//We'll double the current max size
		set->current_max_size *= 2;

		//And we'll reallocate the array
		set->internal_array = realloc(set->internal_array, sizeof(void*) * set->current_max_size);
	}

	//Now that we're all set, we can add our element in. Elements are always added in at the very end
	set->internal_array[set->current_index] = ptr;

	//Bump this up by 1
	set->current_index++;

	//And we're all set
}


/**
 * Clear a dynamic set entirely - keeps the size unchanged, but
array * sets the entire internal array to 0
 */
void clear_dynamic_set(dynamic_set_t* set){
	//Just to be safe
	if(set == NULL || set->internal_array == NULL){
		printf("ERROR: Attempting to clear a NULL dynamic set\n");
		exit(1);
	}

	//Wipe the entire thing out
	memset(set->internal_array, 0, sizeof(void*) * set->current_max_size);

	//Our current index is now 0
	set->current_index = 0;
}


/**
 * Get an element at a specified index. Do not remove the element
 */
void* dynamic_set_get_at(dynamic_set_t* set, u_int16_t index){
	//This should never be happening
	if(set->current_max_size <= index){
		printf("Fatal internal compiler error. Attempt to get index %d in an array of size %d\n", index, set->current_index);
		exit(1);
	}

	//Otherwise we should be good to grab. Again we do not delete here
	return set->internal_array[index];
}


/**
 * Remove an element from the back of the dynamic array - O(1) removal
 */
void* dynamic_set_delete_from_back(dynamic_set_t* set){
	//Already empty
	if(set->current_index == 0){
		return NULL;
	}

	//Grab off of the very end
	void* deleted = set->internal_array[set->current_index - 1];

	//Decrement the index
	(set->current_index)--;

	//Give back the pointer
	return deleted;
}


/**
 * Delete an element from a specified index. The element itself
 * is returned, allowing this to be used as a search & delete function
 * all in one
 */
void* dynamic_set_delete_at(dynamic_set_t* set, u_int16_t index){
	//Again if we can't do this, we won't disrupt the program. Just return NULL
	if(set->current_index <= index){
		return NULL;
	}

	//We'll grab the element at this index first
	void* deleted = set->internal_array[index];

	//Now we'll run through everything from that index up until the end, 
	//shifting left every time
	for(u_int16_t i = index; i < set->current_index - 1; i++){
		//Shift left here
		set->internal_array[i] = set->internal_array[i + 1];
	}

	//Null this out
	set->internal_array[set->current_index - 1] = NULL;

	//We've seen one less of these now
	(set->current_index)--;

	//And once we've done that shifting, we're done so
	return deleted;
}


/**
 * Delete the pointer itself from the dynamic set
 *
 * Will not complain if it cannot be found - it simply won't be deleted
 */
void dynamic_set_delete(dynamic_set_t* set, void* ptr){
	//If this is NULL or empty we'll just return
	if(ptr == NULL || set == NULL || set->current_index == 0){
		return;
	}

	//Otherwise we'll need to grab this index
	int16_t index = dynamic_set_contains(set, ptr);

	//If we couldn't find it - no harm, we just won't do anything
	if(index == NOT_FOUND){
		return;
	}

	//Now we'll use the index to delete
	dynamic_set_delete_at(set, index);
}


/**
 * Are two dynamic sets completely equal? A "deep equals" 
 * will ensure that every single element in one array is also inside of the
 * other, and that no elements in one array are different
 */
u_int8_t dynamic_sets_equal(dynamic_set_t* a, dynamic_set_t* b){
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
 * Deallocate an entire dynamic array
*/
void dynamic_set_dealloc(dynamic_set_t* set){
	//Let's just make sure here...
	if(set->internal_array == NULL){
		return;
	}

	//First we'll free the internal array
	free(set->internal_array);

	//Set this to NULL as a warning
	set->internal_array = NULL;
	set->current_index = 0;
	set->current_max_size = 0;
}
