/**
 * Author: Jack Robbins
 * This file contains the implementations for the APIs defined in parameter_list.h
 */

#include "parameter_result_array.h"
#include <stdio.h>

/**
 * Default initial size is 6, this is usually more 
 * than most will put in their function signatures
 */
#define DEFAULT_INITIAL_SIZE 6

/**
 * Allocate a parameter results array with the default initial size. This is good
 * for when we have elaborative params and do not know how many results we will have
 */
parameter_results_array_t parameter_results_array_alloc_default_size(){
	//Stack allocate at first
	parameter_results_array_t results_array = {NULL, 0, 0};

	//Allocate the internal array with the default initial size
	results_array.parameter_results = calloc(sizeof(parameter_result_t), DEFAULT_INITIAL_SIZE);

	//Set this for later
	results_array.max_index = DEFAULT_INITIAL_SIZE;

	//Give back a copy
	return results_array;
}


/**
 * Allocate a parameter results array with a given initial size
 */
parameter_results_array_t parameter_results_array_alloc(u_int32_t initial_size){
	//Stack allocate at first
	parameter_results_array_t results_array = {NULL, 0, 0};

	//Allocate the internal array with the initial size provided
	results_array.parameter_results = calloc(sizeof(parameter_result_t), initial_size);

	//Set this for later
	results_array.max_index = initial_size;

	//Give back a copy
	return results_array;
}


/**
 * Add a parameter to the results array. We will be relying on the caller to provide us an accurate result
 * type here. The pointer is generic for this reason, we never need to actually access this memory, just
 * store the pointer
 */
void add_parameter_result_to_results_array(parameter_results_array_t* array, void* result, parameter_result_type_t result_type){
	//Dynamic resize ability
	if(array->current_index == array->max_index){
		//Double it
		array->max_index *= 2;

		//Realloc the internal array
		array->parameter_results = realloc(array->parameter_results, sizeof(parameter_result_t) * array->max_index);
	}

	array->parameter_results[array->current_index].result_type = result_type;
	
	/**
	 * Yes - we do not need this because it's a union and we're just storing memory.
	 * I prefer the expressiveness of this though as it shows the intent of the code
	 */
	switch(result_type){
		case PARAM_RESULT_TYPE_CONST:
			array->parameter_results[array->current_index].param_result.constant_result = result;
			break;

		case PARAM_RESULT_TYPE_VAR:
			array->parameter_results[array->current_index].param_result.variable_result = result;
			break;
	}

	//Current index needs to be upped for the next go around
	array->current_index++;
}


/**
 * Retrieve a parameter from the array
 */
parameter_result_t* get_result_at_index(parameter_results_array_t* array, u_int32_t index){
	//Guard here to make future debugging easier
	if(array->current_index >= index){
		sprintf(stderr, "Fatal internal compiler error: attempt to access index %d in an array of size %d", index, array->current_index);
		exit(1);
	}

	return &(array->parameter_results[index]);
}


/**
 * Deallocate a parameter results array
 */
void parameter_results_array_dealloc(parameter_results_array_t* array){
	//Just to be safe..
	if(array->parameter_results == NULL){
		return;
	}

	//Otherwise we just deallocate the internal array and we are good
	free(array->parameter_results);
}
