/**
 * Author: Jack Robbins
 * Implementation file for the generic dynamic array
*/

//Link to header
#include "dynamic_array.h"
#include <stdlib.h>

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
