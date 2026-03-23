/**
 * Author: Jack Robbins
 * This file contains the implementations for the APIs defined in parameter_list.h
 */

#include "parameter_result_array.h"

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
 * Add a parameter to the results array
 */
void add_parameter_to_results_array(parameter_results_array_t* array, u_int32_t index){

}


/**
 * Retrieve a parameter from the array
 */
param_result_type_t* get_result_at_index(parameter_results_array_t* array, u_int32_t index){

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
