/**
 * Author: Jack Robbins
 * This file contains the implementations for the APIs defined in parameter_list.h
 */

#include "parameter_result_array.h"

/**
 * Default initial size is 6, this is usually more 
 * than most will put in their function signatures
 */
#define DEFAULT_INITIAL_SIZE 8

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
parameter_results_array_t parameter_results_array_alloc(int32_t initial_size){
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
 * Perform a dynamic resize for the parameter result array so that the proposed index will fit. In
 * the event that we do need to resize, we will always resize to double the proposed index to cut
 * down on how many of these resizes we must do
 */
void parameter_results_dynamic_resize_for_index(parameter_results_array_t* array, int32_t proposed_index){
	//Nothing to do in this case
	if(array->max_index > proposed_index){
		return;
	}

	//Like said above always reallocate to double this size
	array->max_index = proposed_index * 2;
	array->parameter_results = realloc(array->parameter_results, array->max_index * sizeof(parameter_result_t));
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
