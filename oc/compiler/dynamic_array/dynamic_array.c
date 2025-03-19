/**
 * Author: Jack Robbins
 * Implementation file for the generic dynamic array
*/

//Link to header
#include "dynamic_array.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

//The default is 20 -- this can always be reupped
#define DEFAULT_SIZE 20

//Standard booleans here
#define TRUE 1
#define FALSE 0

/**
 * Allocate an entire dynamic array
*/
dynamic_array_t* dynamic_array_alloc(){
	//First we'll create the overall structure
 	dynamic_array_t* array = calloc(sizeof(dynamic_array_t), 1);

	//Set the max size using the sane default 
	array->current_max_size = DEFAULT_SIZE;

	//Now we'll allocate the overall internal array
	array->internal_array = calloc(array->current_max_size, sizeof(void*));

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
int16_t dynamic_array_contains(dynamic_array_t* array, void* ptr){
	//We'll run through the entire array, comparing pointer by pointer
	for(u_int16_t i = 0; i < array->current_index; i++){
		//If we find an exact memory address match return true
		if(array->internal_array[i] == ptr){
			return i;
		}
	}

	//If we make it here, we found nothing so
	return -1;
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
 * Insert an element into the dynamic array
 */
void dynamic_array_insert(dynamic_array_t* array, void* ptr){
	//Let's just double check here
	if(ptr == NULL){
		fprintf(stderr, "FATAL ERROR: Attempt to insert a null pointer into a dynamic array");
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
	//And we're all set
}


/**
 * Get an element at a specified index. Do not remove the element
 */
void* dynamic_array_get_at(dynamic_array_t* array, u_int16_t index){
	//Return NULL here. It is the caller's responsibility
	//to check this
	if(array->current_index <= index){
		return NULL;
	}


	//Otherwise we should be good to grab. Again we do not delete here
	return array->internal_array[index];
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

	//SPECIAL CASE-> we deleted the very last one
	if(array->current_index - 1 == index){
		//All we need to do here is back up the index and return
		array->current_index--; 

		//And give it back
		return deleted;
	}

	//Now we'll run through everything from that index up until the end, 
	//shifting left every time
	for(u_int16_t i = index; i < array->current_index - 1; i++){
		//Shift left here
		array->internal_array[i] = array->internal_array[i + 1];
	}

	//And once we've done that shifting, we're done so
	return deleted;
}


/**
 * Deallocate an entire dynamic array
*/
void dynamic_array_dealloc(dynamic_array_t* array){
	//Let's just make sure here...
	if(array == NULL){
		return;
	}

	//First we'll free the internal array
	free(array->internal_array);

	//Then we'll free the overall structure
	free(array);
}
