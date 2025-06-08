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


//An overall structure that holds our stack data area
typedef struct stack_data_area_t stack_data_area_t;
//Each node represents the allocation of a certain variable of a certain size
typedef struct stack_data_area_node_t stack_data_area_node_t;


/**
 * A structure that contains an automatically organizing linked
 * list. This linked list contains all of our data
 */
struct stack_data_area_t{
	//The head node of the data area.
	//This will always be the highest(i.e. *highest* offset) node 
	stack_data_area_node_t* highest;
	//The total size of the data area
	u_int32_t total_size;
};


/**
 * Each individual node contains a reference
 * to a given three address code variable
 */
struct stack_data_area_node_t{
	//The next node
	stack_data_area_node_t* next;
	stack_data_area_node_t* previous;
	//The variable that this node references
	void* variable;
	//The size of the node(may not be the same as the var due to alignment)
	u_int32_t variable_size;
	//What is the offset(%rsp + __) for this node
	u_int32_t offset;
};

/**
 * Add a node into the stack data area
 */
void add_variable_to_stack(stack_data_area_t* area, void* variable);

/**
 * Add a spilled variable to the stack. Spilled variables
 * always go on top, no matter what
 */
void add_spilled_variable_to_stack(stack_data_area_t* area, void* variable);

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
