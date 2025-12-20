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

#include <sys/types.h>
#include "../utils/dynamic_array/dynamic_array.h"
#include "../utils/stack_management_structs.h"

/**
 * Allocate the internal dynamic array in the data area
 */
void stack_data_area_alloc(stack_data_area_t* area);

/**
 * Create a stack region for the type provided. This will handle alignment and addition
 * of this stack region
 */
stack_region_t* create_stack_region_for_type(stack_data_area_t* area, generic_type_t* type);

/**
 * Mark a stack region as important. This will handle any/all variable marking as important
 * as well if applicable
 */
void mark_stack_region(stack_region_t* region);

/**
 * Remove a given region from the stack
 */
void remove_region_from_stack(stack_data_area_t* area, stack_region_t* region);

/**
 * Print the stack data area out in its entirety
 */
void print_stack_data_area(stack_data_area_t* area);

/**
 * Does the stack contain a given pointer value? This is used for avoiding redundant addresses
 * in the stack
 */
stack_region_t* does_stack_contain_pointer_to_variable(stack_data_area_t* area, void* variable);

/**
 * Align the stack data area to be 16-byte aligned
 */
void align_stack_data_area(stack_data_area_t* area);

/**
 * Deallocate the internal linked list of the stack data area
 */
void stack_data_area_dealloc(stack_data_area_t* stack_data_area);

#endif /* STACK_DATA_AREA_H */
