/**
 * Author: Jack Robbins
 * This file contains the implementations for the local constant system
 * as defined in local_constant.h
*/

#include "local_constant.h"
#include <sys/types.h>
#include <stdlib.h>

//Keep an atomically incrementing integer for the local constant ID
static u_int32_t local_constant_id = 0;

/**
 * Atomically increment and return the local constant id
 */
static inline u_int32_t increment_and_get_local_constant_id(){
	local_constant_id++;
	return local_constant_id;
	
}


/**
 * Create a local constant and return the pointer to it
 */
local_constant_t* string_local_constant_alloc(generic_type_t* type, dynamic_string_t* value){
	//Dynamically allocate it
	local_constant_t* local_const = calloc(1, sizeof(local_constant_t));

	//Store the type as well
	local_const->type = type;

	//Copy the dynamic string in
	local_const->local_constant_value.string_value = clone_dynamic_string(value);

	//Now we'll add the ID
	local_const->local_constant_id = increment_and_get_local_constant_id();

	//Store what type we have
	local_const->local_constant_type = LOCAL_CONSTANT_TYPE_STRING;

	//And finally we'll add it back in
	return local_const;
}


/**
 * Create an F32 local constant
 */
local_constant_t* f32_local_constant_alloc(generic_type_t* f32_type, float value){
	//Dynamically allocate it
	local_constant_t* local_const = calloc(1, sizeof(local_constant_t));

	//Store the type as well
	local_const->type = f32_type;

	//Copy the dynamic string in. We cannot print out floats directly, so we instead
	//use the bits that make up the float and cast them to an i32 *without rounding*
	local_const->local_constant_value.float_bit_equivalent = *((int32_t*)(&value));

	//Now we'll add the ID
	local_const->local_constant_id = increment_and_get_local_constant_id();

	//Store what type we have
	local_const->local_constant_type = LOCAL_CONSTANT_TYPE_F32;

	//And finally we'll add it back in
	return local_const;
}


/**
 * Create an F64 local constant
 */
local_constant_t* f64_local_constant_alloc(generic_type_t* f64_type, double value){
	//Dynamically allocate it
	local_constant_t* local_const = calloc(1, sizeof(local_constant_t));

	//Store the type as well
	local_const->type = f64_type;

	//Copy the dynamic string in. We cannot print out floats directly, so we instead
	//use the bits that make up the float and cast them to an i32 *without rounding*
	local_const->local_constant_value.float_bit_equivalent = *((int64_t*)(&value));

	//Now we'll add the ID
	local_const->local_constant_id = increment_and_get_local_constant_id();

	//Store what type we have
	local_const->local_constant_type = LOCAL_CONSTANT_TYPE_F64;

	//And finally we'll add it back in
	return local_const;
}


/**
 * Create a 128 bit local constant
 *
 * NOTE: we will use an f64 for this, although we all know that this is truly a 128 bit type
 */
local_constant_t* xmm128_local_constant_alloc(generic_type_t* f64_type, int64_t upper_64_bits, int64_t lower_64_bits){
	//Dynamically allocate it
	local_constant_t* local_const = calloc(1, sizeof(local_constant_t));

	//Store the type as well
	local_const->type = f64_type;
	
	//Store the lower and upper 64 bits for this local constant
	local_const->local_constant_value.lower_64_bits = lower_64_bits;
	local_const->upper_64_bits = upper_64_bits;

	//Now we'll add the ID
	local_const->local_constant_id = increment_and_get_local_constant_id();

	//Store what type we have
	local_const->local_constant_type = LOCAL_CONSTANT_TYPE_XMM128;

	//And finally we'll add it back in
	return local_const;
}


/**
 * Destroy a local constant
 */
void local_constant_dealloc(local_constant_t* constant){
	//Go based on the type
	switch(constant->local_constant_type){
		case LOCAL_CONSTANT_TYPE_STRING:
			//First we'll deallocate the dynamic string
			dynamic_string_dealloc(&(constant->local_constant_value.string_value));
			break;

		//If it's not a string then there's nothing to free
		default:
			break;
	}

	//Then we'll free the entire thing
	free(constant);
}
