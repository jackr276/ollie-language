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


#endif /* LOCAL_CONSTANT_H */
