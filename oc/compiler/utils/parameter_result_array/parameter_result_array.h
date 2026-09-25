/**
 * Author: Jack Robbins
 * This API defines the dynamic parameter result list that is used mainly by the function
 * call emitters in the CFG. This datastructure allows us to store tagged unions that may 
 * contain either three_addr_var_t objects or three_addr_const_t objects
 *
 * This header file contains declarations for APIs that are defined in the parameter_result_array.c
 * file, and contains inlined definitions for commonly used utility functions
 */

#ifndef PARAMETER_RESULT_ARRAY_H
#define PARAMETER_RESULT_ARRAY_H

#include "../three_address_constant.h"
#include "../three_address_variable.h"
#include <sys/types.h>

typedef struct parameter_result_t parameter_result_t;
typedef struct parameter_results_array_t parameter_results_array_t;

//For initializing null versions
#define NULL_PARAMETER_RESULT_ARRAY_INITIALIZER {NULL, 0, 0}


/**
 * Is our result type a constant or a parameter
 */
typedef enum {
	PARAM_RESULT_TYPE_CONST,
	PARAM_RESULT_TYPE_VAR,
} parameter_result_type_t;


/**
 * Maintain a tagged union type that allows us to
 * store either constants or variables. This is 
 * used for function calls
 */
struct parameter_result_t {
	//The actual result type storage
	parameter_result_type_t result_type;

	/**
	 * We can store either a constant or a variable - very useful for our
	 * function calls in avoiding extra assignments
	 */
	union {
		three_addr_const_t* constant_result;
		three_addr_var_t* variable_result;
	} param_result;
};


/**
 * The actual array itself is just a dynamic
 * array that contains however many results we actually
 * need. The user is going to have to provide an
 * initial size here unlike in a dynamic array
 */
struct parameter_results_array_t {
	parameter_result_t* parameter_results;
	int32_t current_index;
	int32_t max_index;
};

//================================= Non-Inlined Functions =============================================
/**
 * Allocate a parameter results array with the default initial size. This is good
 * for when we have elaborative params and do not know how many results we will have
 */
parameter_results_array_t parameter_results_array_alloc_default_size();

/**
 * Allocate a parameter results array with a given initial size
 */
parameter_results_array_t parameter_results_array_alloc(int32_t initial_size);

/**
 * Perform a dynamic resize for the parameter result array so that the proposed index will fit. In
 * the event that we do need to resize, we will always resize to double the proposed index to cut
 * down on how many of these resizes we must do
 */
void parameter_results_dynamic_resize_for_index(parameter_results_array_t* array, int32_t proposed_index);

/**
 * Deallocate a parameter results array
 */
void parameter_results_array_dealloc(parameter_results_array_t* array);
//================================= Non-Inlined Functions =============================================
//================================= Inlined Utility Functions =========================================
/**
 * Add a parameter to the results array. We will be relying on the caller to provide us an accurate result
 * type here. The pointer is generic for this reason, we never need to actually access this memory, just
 * store the pointer
 */
static inline void add_parameter_result_to_results_array(parameter_results_array_t* array, void* result, parameter_result_type_t result_type){
}


/**
 * Retrieve a parameter from the array
 *
 * We assume that the user is smart enough to do their own checks here
 */
static inline parameter_result_t* get_result_at_index(parameter_results_array_t* array, int32_t index){
	return &(array->parameter_results[index]);
}
//================================= Inlined Utility Functions =========================================

#endif /* PARAMETER_RESULT_ARRAY_H */
