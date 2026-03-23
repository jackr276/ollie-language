/**
 * Author: Jack Robbins
 * This API defines the dynamic parameter result list that is used mainly by the function
 * call emitters in the CFG. This datastructure allows us to store tagged unions that may 
 * contain either three_addr_var_t objects or three_addr_const_t objects
 */

#ifndef PARAMETER_RESULT_ARRAY_H
#define PARAMETER_RESULT_ARRAY_H

//Link to the instruction library
#include "../../instruction/instruction.h"
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
	u_int32_t current_index;
	u_int32_t max_index;
};

/**
 * Allocate a parameter results array with the default initial size. This is good
 * for when we have elaborative params and do not know how many results we will have
 */
parameter_results_array_t parameter_results_array_alloc_default_size();


/**
 * Allocate a parameter results array with a given initial size
 */
parameter_results_array_t parameter_results_array_alloc(u_int32_t initial_size);


/**
 * Add a parameter to the results array. We will be relying on the caller to provide us an accurate result
 * type here. The pointer is generic for this reason, we never need to actually access this memory, just
 * store the pointer
 */
void add_parameter_result_to_results_array(parameter_results_array_t* array, void* result, parameter_result_type_t result_type);


/**
 * Retrieve a parameter from the array
 */
parameter_result_t* get_result_at_index(parameter_results_array_t* array, u_int32_t index);


/**
 * Deallocate a parameter results array
 */
void parameter_results_array_dealloc(parameter_results_array_t* array);

#endif /* PARAMETER_RESULT_ARRAY_H */
