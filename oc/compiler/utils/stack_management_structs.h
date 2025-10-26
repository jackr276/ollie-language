/**
 * Author: Jack Robbins
 * This header file defines all of the
 * datatypes that we use for stack data area management
*/

//Include guards
#ifndef STACK_MANAGEMENT_STRUCTS_H
#define STACK_MANAGEMENT_STRUCTS_H

//By default we only have 5 here. The region array will dynamically resize
//if need be
#define DEFAULT_STACK_REGION_SIZE 5

#include <sys/types.h>
#include "./dynamic_array/dynamic_array.h"

//An overall structure that holds our stack data area
typedef struct stack_data_area_t stack_data_area_t;
//A stack region has a size itself. Variables/live ranges can point into the stack region
typedef struct stack_region_t stack_region_t;

/**
 * A stack region has a size and a base address on
 * the stack. Anything whose stack offset is *within*
 * that region is considered to be a part of it
 *
 * For example: an array may take up 50kb of stack space
 * starting at (relative address) 0. Anything between
 * 0 - 50 is considered within this region
 *
 * We don't really care what variables are "in" the stack region,
 * so there's no need to store those
 */
struct stack_region_t {
	//The unique ID for this region
	u_int32_t stack_region_id;
	//The base address
	u_int32_t base_address;
	//The size
	u_int32_t size;
	//The read count
	u_int32_t read_count;
};


/**
 * A structure that contains an automatically organizing linked
 * list. This linked list contains all of our data
 */
struct stack_data_area_t{
	//The array of all variables in the stack currently
	dynamic_array_t* variables;
	//Heap array for the regions
	dynamic_array_t* stack_regions;
	//The total size of the data area
	u_int32_t total_size;
};


#endif /* STACK_MANAGEMENT_STRUCTS_H */
