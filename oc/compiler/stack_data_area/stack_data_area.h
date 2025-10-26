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
