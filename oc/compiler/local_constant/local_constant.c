/**
 * Author: Jack Robbins
 * This file contains the implementations for the local constant system
 * as defined in local_constant.h
*/

#include "local_constant.h"
#include <sys/types.h>

//Keep an atomically incrementing integer for the local constant ID
static u_int32_t local_constant_id = 0;

/**
 * Atomically increment and return the local constant id
 */
static inline u_int32_t increment_and_get_local_constant_id(){
	local_constant_id++;
	return local_constant_id;
	
}


