/**
 * Author: Jack Robbins
 * A three address code intializer is a kind of hybrid between a three_addr_var_t and an
 * instruction_t. It is expected that this will hold more than one variable/constant and may
 * even hold nested initializers inside of it. There are currently three kinds of initializers,
 * those being: arrays, strings and structs
 */

#ifndef THREE_ADDRESS_INITIALIZER_H
#define THREE_ADDRESS_INITIALIZER_H

//These will contain constants and variables
#include "three_address_constant.h"
#include "three_address_variable.h"


typedef struct three_addr_initializer_t three_addr_initializer_t;


//TODO FILL OUT
struct three_addr_initializer_t {
	//TODO
};

#endif /* THREE_ADDRESS_INITIALIZER_H */
