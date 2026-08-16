/**
 */

#ifndef GLOBAL_VARIABLE_H
#define GLOBAL_VARIABLE_H

#include "three_address_constant.h"

//The definition of a global variable container
typedef struct global_variable_t global_variable_t;

/**
 * This enumeration will tell the printer in the final
 * compiler steps what kind of global variable initializer
 * we have
 */
typedef enum {
	GLOBAL_VAR_INITIALIZER_NONE = 0, //Most common case, we have nothing
	GLOBAL_VAR_INITIALIZER_CONSTANT, //Just a singular constant
	GLOBAL_VAR_INITIALIZER_ARRAY, //An array of constants
	GLOBAL_VAR_INITIALIZER_STRING, //A string constant
	GLOBAL_VAR_INITIALIZER_STRUCT, //A struct constant
} global_variable_initializer_type_t;


/**
 * A global variable stores the variable itself
 * and it stores the value, if it has one
 */
struct global_variable_t{
	//The variable itself - stores the name
	symtab_variable_record_t* variable;

	//Could be a constant or an array of constants
	union {
		//The value - if given - of the variable
		three_addr_const_t* constant_value;
		//A dynamic array of constants, if we have that
		dynamic_array_t array_initializer_values;
		//A dynamic array of constants for our struct
		dynamic_array_t struct_initializer_values;
	} initializer_value;

	//Store the type as well
	generic_type_t* variable_type;
	//What is this variable's reference count?
	u_int16_t reference_count;
	//Is this a relative global variable(used for char*)
	u_int8_t is_relative;
	//Store the initializer type
	global_variable_initializer_type_t initializer_type;
};


#endif /* GLOBAL_VARIABLE_H */
