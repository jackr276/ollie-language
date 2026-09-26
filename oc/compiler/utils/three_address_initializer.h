/**
 * Author: Jack Robbins
 * A three address code intializer is a kind of hybrid between a three_addr_var_t and an
 * instruction_t. It is expected that this will hold more than one variable/constant and may
 * even hold nested initializers inside of it. There are currently three kinds of initializers,
 * those being: arrays, strings and structs
 *
 * NOTE: because we require these to be tied into the variable ID system, we cannot do any allocation
 * here. We need to do all allocation in the instruction.c file to have access to that atomically
 * increasing variable ID
 */

#ifndef THREE_ADDRESS_INITIALIZER_H
#define THREE_ADDRESS_INITIALIZER_H

//These will contain constants and variables
#include "three_address_constant.h"
#include "three_address_variable.h"
#include <stdlib.h>

//Forward declarations
typedef struct three_addr_initializer_t three_addr_initializer_t;
typedef struct initializer_result_t initializer_result_t;

/**
 * Define an enum that will allow us to discern between intializer
 * types
 */
typedef enum {
	INITIALIZER_RESULT_TYPE_CONSTANT,
	INITIALIZER_RESULT_TYPE_VARIABLE,
	INITIALIZER_RESULT_TYPE_SUB_INITIALIZER,
} initializer_result_type_t;


/**
 * What kind of initializer is it? We only support struct and array. Remember
 * that string initializers under the hood are just arrays of chars
 */
typedef enum {
	INITIALIZER_TYPE_ARRAY,
	INITIALIZER_TYPE_STRUCT,
} initializer_type_t;


/**
 * An initializer result is a tagged union that could store a constant,
 * a variable, or a sub-initializer(just another initializer pointer)
 */
struct initializer_result_t {
	union {
		three_addr_const_t* constant_value;
		three_addr_var_t* variable_value;
		three_addr_initializer_t* initializer_value;
	} value;

	initializer_result_type_t result_type;
};


/**
 * A three address initializer may contain constants, variables *OR*
 * nested initializers inside of it
 */
struct three_addr_initializer_t {
	/**
	 * The initializer ID ties into the three address variable ID system. This
	 * is very important for variable mapping. It is called the variable ID to 
	 * reflect this
	 */
	int32_t variable_id;

	/**
	 * The "Initializer results" mimics dynamic array functionality
	 * almost to a T, except that instead of storing pointers, we are
	 * instead store initializer_result_t objects which are themselves like
	 * tagged unions of values
	 */
	struct {
		initializer_result_t* result_array;
		int32_t results_max_index;
		int32_t results_current_index;
	} results;

	//Less frequently accessed field - the type of the initializer
	initializer_type_t initializer_type;
	//Store the type as well
	generic_type_t* type;
};


/**
 * Add an initializer result to the given initializer. This will internally 
 */
static inline void add_intializer_result(three_addr_initializer_t* initializer, void* result, initializer_result_type_t result_type){
	/**
	 * If we've hit the limit here we'll need to dynamically resize by doubling and then reallocating
	 * the underlying result array
	 */
	if(initializer->results.results_current_index == initializer->results.results_max_index){
		initializer->results.results_max_index *= 2;
		initializer->results.result_array = (initializer_result_t*)realloc(initializer->results.result_array, sizeof(initializer_result_t) * initializer->results.results_max_index);
	}

	//Grab a pointer to where we want to add
	initializer_result_t* new_result_ptr = &(initializer->results.result_array[initializer->results.results_current_index]);

	switch(result_type){
		case INITIALIZER_RESULT_TYPE_CONSTANT: {
			new_result_ptr->result_type = INITIALIZER_RESULT_TYPE_CONSTANT;
			new_result_ptr->value.constant_value = (three_addr_const_t*)result;
			break;
		}

		case INITIALIZER_RESULT_TYPE_VARIABLE: {
			new_result_ptr->result_type = INITIALIZER_RESULT_TYPE_VARIABLE;
			new_result_ptr->value.variable_value = (three_addr_var_t*)result;
			break;
		}

		case INITIALIZER_RESULT_TYPE_SUB_INITIALIZER: {
			new_result_ptr->result_type = INITIALIZER_RESULT_TYPE_SUB_INITIALIZER;
			new_result_ptr->value.initializer_value = (three_addr_initializer_t*)result;
			break;
		}
	}

	//Bump the current index for the next go around
	(initializer->results.results_current_index)++;
}


/**
 * Get the result of an intializer at a given index
 */
static inline initializer_result_t* get_intializer_result_at_index(three_addr_initializer_t* initializer, int32_t index){
	return &(initializer->results.result_array[index]);
}


/**
 * Destroy a given initializer
 */
static inline void three_addr_initializer_dealloc(three_addr_initializer_t* initializer){
	/**
	 * We'll need to destroy any sub-initializers that we have here recursively
	 */
	for(int32_t i = 0; i < initializer->results.results_max_index; i++){
		initializer_result_t* result = &(initializer->results.result_array[i]);

		//Recursively destroy if this happens
		if(result->result_type == INITIALIZER_RESULT_TYPE_SUB_INITIALIZER){
			three_addr_initializer_dealloc(result->value.initializer_value);
		}
	}

	//Free the overall structure
	free(initializer->results.result_array);
	free(initializer);
}

#endif /* THREE_ADDRESS_INITIALIZER_H */
