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
 * Get a string local constant whose value matches the given constant
 *
 * Returns NULL if no matching constant can be found
 */
local_constant_t* get_string_local_constant(symtab_function_record_t* record, char* string_value){
	//Run through all of the local constants
	for(u_int16_t i = 0; i < record->local_string_constants.current_index; i++){
		//Extract the candidate
		local_constant_t* candidate = dynamic_set_get_at(&(record->local_string_constants), i);

		//If we have a match then we're good here, we'll return the candidate and leave
		if(strncmp(candidate->local_constant_value.string_value.string, string_value, candidate->local_constant_value.string_value.current_length) == 0){
			return candidate;
		}
	}

	//If we get here we didn't find it
	return NULL;
}


/**
 * Get an f32 local constant whose value matches the given constant
 *
 * Returns NULL if no matching constant can be found
 */
local_constant_t* get_f32_local_constant(symtab_function_record_t* record, float float_value){
	//Run through all of the local constants
	for(u_int16_t i = 0; i < record->local_f32_constants.current_index; i++){
		//Extract the candidate
		local_constant_t* candidate = dynamic_set_get_at(&(record->local_f32_constants), i);

		//We will be comparing the values at a byte level. We do not compare the raw values because
		//that would use FP comparison
		if(candidate->local_constant_value.float_bit_equivalent == *((u_int32_t*)&float_value)){
			return candidate;
		}
	}

	//If we get here we didn't find it
	return NULL;
}


/**
 * Get an f64 local constant whose value matches the given constant
 *
 * Returns NULL if no matching constant can be found
 */
local_constant_t* get_f64_local_constant(symtab_function_record_t* record, double double_value){
	//Run through all of the local constants
	for(u_int16_t i = 0; i < record->local_f64_constants.current_index; i++){
		//Extract the candidate
		local_constant_t* candidate = dynamic_set_get_at(&(record->local_f64_constants), i);

		//We will be comparing the values at a byte level. We do not compare the raw values because
		//that would use FP comparison
		if(candidate->local_constant_value.float_bit_equivalent == *((u_int64_t*)&double_value)){
			return candidate;
		}
	}

	//If we get here we didn't find it
	return NULL;
}


/**
 * Get a 128 bit local constant whose value matches the given constant
 *
 * Returns NULL if no matching constant can be found
 */
local_constant_t* get_xmm128_local_constant(symtab_function_record_t* record, int64_t upper_64_bits, int64_t lower_64_bits){
	//Run through all of the local constants
	for(u_int16_t i = 0; i < record->local_xmm_constants.current_index; i++){
		//Extract the candidate
		local_constant_t* candidate = dynamic_set_get_at(&(record->local_xmm_constants), i);

		//We will be comparing at the byte level for both the lower and upper 64 bits
		if((candidate->local_constant_value.lower_64_bits ^ lower_64_bits) == 0
			&& (candidate->upper_64_bits ^ upper_64_bits) == 0){

			return candidate;
		}
	}

	//If we get down here it means that we found nothing
	return NULL;
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
