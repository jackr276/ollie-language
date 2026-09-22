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

/**
 * A three address initializer may contain constants, variables *OR*
 * nested initializers inside of it
 */
typedef struct three_addr_initializer_t three_addr_initializer_t;
struct three_addr_initializer_t {

	//TODO NEED DYNAMIC RESIZE

	//TODO
};

#endif /* THREE_ADDRESS_INITIALIZER_H */
