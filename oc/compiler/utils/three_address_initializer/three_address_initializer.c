/**
 * Author: Jack Robbins
 * This file contains the implementation for the APIs defined in the header file
 * of the same name
 */

//Link to the header file
#include "three_address_initializer.h"
#include <stdlib.h>
#include <sys/types.h>

//Atomically increasing ID
static u_int32_t initializer_id = 0;

/**
 * Atomic increase functionality for our intializer ID
 */
static u_int32_t increment_and_get_id(){
	return initializer_id++;
}

 
/**
 * Dynamically allocate an initializer of a given type
 */
three_addr_initializer_t* three_addr_initializer_alloc(generic_type_t* type){
	three_addr_initializer_t* 

}


/**
 * Add an initializer result to the given initializer
 */
void add_intializer_result(three_addr_initializer_t* initializer, void* result, initializer_result_type_t result_type);


/**
 * Get the result of an intializer at a given index
 */
void* get_intializer_result_at_index(three_addr_initializer_t* initializer, int32_t index);


/**
 * Destroy a given initializer
 */
void three_addr_initializer_dealloc(three_addr_initializer_t* initializer){
	free(initializer->results.result_array);
	free(initializer);
}
