/**
 * Author: Jack Robbins
 * This submodule holds everything that we need for the local constant(.LC) subsystem
*/

#ifndef LOCAL_CONSTANT_H
#define LOCAL_CONSTANT_H

#include <sys/types.h>

//Necessary ollie specific dependencies
#include "../utils/dynamic_string/dynamic_string.h"
#include "../type_system/type_system.h"

//The definition of a local constant(.LCx)
typedef struct local_constant_t local_constant_t;

/**
 * What kind of local constant do we have? Local constants
 * can be strings or floating point numbers, which are represented
 * by ".long"
 */
typedef enum {
	LOCAL_CONSTANT_TYPE_STRING,
	LOCAL_CONSTANT_TYPE_F32,
	LOCAL_CONSTANT_TYPE_F64,
	LOCAL_CONSTANT_TYPE_XMM128 //Special case where a full 128 bit lane of xmm is needed
} local_constant_type_t;


/**
 * A local constant(.LCx) is a value like a string that is intended to 
 * be used by a function. We define them separately because they have many less
 * fields than an actual basic block
 */
struct local_constant_t{
	//What is the type of the local constant?
	generic_type_t* type;
	//Holds the actual value
	union {
		//Local constants can be strings
		dynamic_string_t string_value;
		//In the case where we have f32/f64, we store the *bit equivalent*
		//i32/i64 value inside of here and print that out
		u_int64_t float_bit_equivalent;
		//For the 128 bit section - we need to store the 2 64 bit sections
		//separately
		u_int64_t lower_64_bits;
	} local_constant_value;
	//Unfortunately we can't hold it all in the union
	u_int64_t upper_64_bits;
	//And the ID of it
	u_int16_t local_constant_id;
	//The reference count of the local constant
	u_int16_t reference_count;
	//What is the type of it
	local_constant_type_t local_constant_type;
};


/**
 * Create a string local constant
 */
local_constant_t* string_local_constant_alloc(generic_type_t* type, dynamic_string_t* value);

/**
 * Create an f32 local constant
 */
local_constant_t* f32_local_constant_alloc(generic_type_t* f32_type, float value);

/**
 * Create an f64 local constant
 */
local_constant_t* f64_local_constant_alloc(generic_type_t* f32_type, double value);

/**
 * Create a 128 bit local constant
 *
 * NOTE: we will use an f64 for this, although we all know that this is truly a 128 bit type
 */
local_constant_t* xmm128_local_constant_alloc(generic_type_t* f64_type, int64_t upper_64_bits, int64_t lower_64_bits);

/**
 * Get an f32 local constant whose value matches the given constant
 *
 * Returns NULL if no matching constant can be found
 */
local_constant_t* get_f32_local_constant(dynamic_array_t* records, float constant_value);

/**
 * Get an f64 local constant whose value matches the given constant
 *
 * Returns NULL if no matching constant can be found
 */
local_constant_t* get_f64_local_constant(dynamic_array_t* records, double constant_value);

/**
 * Get a 128 bit local constant whose value matches the given constant
 *
 * Returns NULL if no matching constant can be found
 */
local_constant_t* get_xmm128_local_constant(dynamic_array_t* records, int64_t upper_64_bits, int64_t lower_64_bits);

/**
 * Get a string local constant whose value matches the given constant
 *
 * Returns NULL if no matching constant can be found
 */
local_constant_t* get_string_local_constant(dynamic_array_t* records, char* string_value);

/**
 * Destroy a local constant
 */
void local_constant_dealloc(local_constant_t* constant);

#endif /* LOCAL_CONSTANT_H */
