/**
 * Author: Jack Robbins
 *
 * This file contains the implementations for the stack data area header file and
 * the APIs defined within
*/

#include "stack_data_area.h"
#include <stdio.h>
#include <stdlib.h>
#include "../instruction/instruction.h"

/**
 * Add a node into the stack data area
 */
void add_variable_to_stack(stack_data_area_t* area, void* variable){
	//We already know it's one of these
	three_addr_var_t* var = variable;
	//TODO
	
}

/**
 * Remove a node from the stack if it is deemed useless
 *
 * We'll need to traverse the linked list and ultimately delete this
 * value from it. We shouldn't have to worry about realigning because everything
 * will already be aligned properly
 */
void remove_variable_from_stack(stack_data_area_t* area, void* variable){
	//The node where we hopefully find this value
	stack_data_area_node_t* current = area->highest;

	//Keep going until we either find it or run off of the list
	while(current != NULL && current->variable != variable){
		current = current->next;
	}

	//This should never happen, but if it does, we'll need to notify the developer in an obvious way
	if(current == NULL){
		printf("[FATAL COMPILER ERROR]: Attempt to remove a nonexistent variable from stack_data_area\n");
		exit(1);
	}

	//Otherwise we found it. Now to remove it, we'll need to sever the connections in an appropriate way
	
	//First remove the space
	area->total_size -= current->variable_size;
	
	//Special case - it's the head
	if(area->highest == current){
		//Reserve this one's address
		stack_data_area_node_t* temp = area->highest;
		//Advance to the next one
		area->highest = area->highest->next;
		//Forget the reference to the dead node
		area->highest->previous = NULL;
		//Now we deallocate temp
		free(temp);
	//Otherwise it's now the head
	} else {
		//This will cut out current completely
		current->previous->next = current->next;
		current->next->previous = current->previous;
		free(current);
	}
}


/**
 * Print the stack data area out in its entirety
 */
void print_stack_data_area(stack_data_area_t* area){
	//TODO

}



/**
 * Deallocate the internal linked list of the stack data area
 */
void stack_data_area_dealloc(stack_data_area_t* stack_data_area){
	//Grab the current node
	stack_data_area_node_t* current = stack_data_area->highest;
	//We'll need a temporary holder variable here
	stack_data_area_node_t* temp;

	//Traverse the linked list and deallocate as we go
	while(current != NULL){
		//Hold onto the reference
		temp = current;
		//Advance current
		current = current->next;
		//Free the temp holder
		free(temp);
	}

	//We don't need to deallocate the overall structure because it 
	//itself is a part of a function record
}

