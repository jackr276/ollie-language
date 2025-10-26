/**
 * Author: Jack Robbins
 *
 * This contains the APIs for the stack data area. The stack data area is designed to be a "living" data
 * structure that we can manipulate as a program goes through optimization and register selection. We can
 * add/remove values as needed
 *
 * The stack data area itself will be organized as a self-sorting list where the largest values are
 * placed at the very bottom. This allows us to save overall space with our alignment
*/

//Header guards
#ifndef STACK_DATA_AREA_H
#define STACK_DATA_AREA_H

//By default we only have 5 here. The region array will dynamically resize
//if need be
#define DEFAULT_STACK_REGION_SIZE 5

#include <sys/types.h>
#include "../utils/dynamic_array/dynamic_array.h"

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
	stack_region_t* regions;
	//The total size of the data area
	u_int32_t total_size;
	//The next region size
	u_int16_t next_region;
	//The region max size
	u_int16_t region_max_size;
};

/**
 * Allocate the internal dynamic array in the data area
 */
void stack_data_area_alloc(stack_data_area_t* area);

/**
 * Add a node into the stack data area
 */
void add_variable_to_stack(stack_data_area_t* area, void* variable);

/**
 * Does a stack contain a given *symtab variable* address?
 */
u_int8_t does_stack_contain_symtab_variable(stack_data_area_t* area, void* symtab_variable);

/**
 * Remove a node from the stack if it is deemed useless
 */
void remove_variable_from_stack(stack_data_area_t* area, void* variable);

/**
 * Print the stack data area out in its entirety
 */
void print_stack_data_area(stack_data_area_t* area);

/**
 * Align the stack data area to be 16-byte aligned
 */
void align_stack_data_area(stack_data_area_t* area);

/**
 * Deallocate the internal linked list of the stack data area
 */
void stack_data_area_dealloc(stack_data_area_t* stack_data_area);

#endif /* STACK_DATA_AREA_H */
