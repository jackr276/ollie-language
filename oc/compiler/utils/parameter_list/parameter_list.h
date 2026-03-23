/**
 * Author: Jack Robbins
 * This API defines the dynamic parameter result list that is used mainly by the function
 * call emitters in the CFG. This datastructure allows us to store tagged unions that may 
 * contain either three_addr_var_t objects or three_addr_const_t objects
 */

#ifndef PARAMETER_LIST_H
#define PARAMETER_LIST_H

//Link to the instruction library
#include "../../instruction/instruction.h"

/**
 * Is our result type a constant or a parameter
 */
typedef enum {
	PARAM_RESULT_TYPE_CONST,
	PARAM_RESULT_TYPE_VAR,
} param_result_type_t;


/**
 * Maintain a tagged union type that allows us to
 * store either constants or variables. This is 
 * used for function calls
 */
typedef struct {
	//The actual result type storage
	param_result_type_t result_type;

	/**
	 * We can store either a constant or a variable - very useful for our
	 * function calls in avoiding extra assignments
	 */
	union {
		three_addr_const_t* constant_result;
		three_addr_var_t* variable_result;
	} param_result;

} function_parameter_result_t;




#endif /* PARAMETER_LIST_H */
