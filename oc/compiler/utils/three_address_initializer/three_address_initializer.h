/**
 * Author: Jack Robbins
 * A three address code intializer is a kind of hybrid between a three_addr_var_t and an
 * instruction_t. It is expected that this will hold more than one variable/constant and may
 * even hold nested initializers inside of it. There are currently three kinds of initializers,
 * those being: arrays, strings and structs
 *
 * However, due to the dynamic resize and nature of the nested elements, we are not able to
 * just have this as a header and instead will need to have it implemented and part of the build system
 */

#ifndef THREE_ADDRESS_INITIALIZER_H
#define THREE_ADDRESS_INITIALIZER_H

//These will contain constants and variables
#include "../three_address_constant.h"
#include "../three_address_variable.h"
#include <sys/types.h>

//Forward declarations
typedef struct three_addr_initializer_t three_addr_initializer_t;
typedef struct initializer_result_t initializer_result_t;


/**
 * A three address initializer may contain constants, variables *OR*
 * nested initializers inside of it
 */
struct three_addr_initializer_t {
	//Unique identifier
	u_int32_t initializer_id;
	//Store the type as well
	generic_type_t* type;
	/**
	 * The "Initializer results" mimics dynamic array functionality
	 * almost to a T, except that instead of storing pointers, we are
	 * instead store initializer_result_t objects which are themselves like
	 * tagged unions of values
	 */
	initializer_result_t* initializer_results;
	int32_t results_max_index;
	int32_t results_current_index;
};


/**
 * Dynamically allocate an initializer of a given type
 */
three_addr_initializer_t* three_addr_initializer_alloc(generic_type_t* type);


/**
 * Destroy a given initializer
 */
void three_addr_initializer_dealloc(generic_type_t* type);

#endif /* THREE_ADDRESS_INITIALIZER_H */
