/**
 * Author: Jack Robbins
 *
 * This file contains the implementations for the stack data area header file and
 * the APIs defined within
*/

#include "stack_data_area.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "../instruction/instruction.h"

//For standardization across all modules
#define TRUE 1
#define FALSE 0


/**
 * Allocate the internal dynamic array in the data area
 */
void stack_data_area_alloc(stack_data_area_t* area){
	//Just allocate the dynamic array
	area->variables = dynamic_array_alloc();

	//Currently the size is 0
	area->total_size = 0;
}


/**
 * Does the stack already contain this variable? This is important for types like
 * constructs and arrays
 */
static u_int8_t does_stack_contain_variable(stack_data_area_t* area, three_addr_var_t* var){
	//Run through and try to find this
	for(u_int16_t i = 0; i < area->variables->current_index; i++){
		if(dynamic_array_get_at(area->variables, i) == var){
			return TRUE;
		}
	}

	//Return false if we get here
	return FALSE;
}


/**
 * Align the stack data area size to be 16-byte aligned
 */
void align_stack_data_area(stack_data_area_t* area){
	//If it already is a perfect multiple of 16, then we're good
	if(area->total_size % 16 == 0){
		return;
	}

	//Otherwise we can align
	
	/**
	 * Example: align 258 to 16-bytes by rounding up
	 * 258 is 100000010
	 * 15 is  000001111
	 *
	 * Add them we get 273: 100010001
	 * 0XF is 1111
	 * ~0XF is 1111110000
	 *  100010001
	 * &111110000
	 * 100010000
	 *
	 * This is: 272, and it is now aligned
	 */
	area->total_size = (area->total_size + 15) & ~0xF;
}


/**
 * Add a node into the stack data area
 *
 * We'll need to guarantee that the base and ending address of each
 * variable in here is a multiple of their alignment requirement K
 */
void add_variable_to_stack(stack_data_area_t* area, void* variable){
	//We already know it's one of these
	three_addr_var_t* var = variable;

	//First, let's create what we'll use to store this 
	stack_data_area_node_t* node = calloc(1, sizeof(stack_data_area_node_t));
	//Tie this in
	node->variable = variable;

	//Let's make this a multiple of 4 by first adding three(set the 2 lsb's), and 
	//then rounding up by anding with a value with the 2 lsb's as 0
	node->variable_size = (var->type->type_size + 3) & ~0x3;

	//We can now increment the total size here
	area->total_size += node->variable_size;

	//Once we have all of this, we're able to add this into the stack. We add into the stack
	//in a priority-queue like way, where the smallest values reside at the top, and the largest
	//values reside at the bottom. We always start at the top and work our way down
	stack_data_area_node_t* current = area->highest;

	//Special case - inserting at the head
	if(current == NULL){
		//This one is the highest
		area->highest = node;
		//No offset - it's the lowest
		node->offset = 0;
		//We're done here. No need to calculate offsets because there's only one thing
		//here
		return;
	}

	//Otherwise, we'll need to do some more complex operations. We'll keep
	//searching until we get to the place where the node is larger
	//than the one before it
	while(current->next != NULL && current->variable_size <= node->variable_size){
		current = current->next;
	}

	//When we get down here, we know that our node is smaller than
	//the current node's size, so we'll want to insert our node *above* the current node

	//Once we get here, there are are three options
	//This means that we hit the tail, and we're at the very end
	if(current->next == NULL && current->variable_size <= node->variable_size){
		//Add this in here at the very end
		current->next = node;
		node->previous = current;

		//In this case, recalculate all of the offsets based on node because it's our
		//very lowest value(0 offset)
		recalculate_all_offsets(node);

	//This means that we have a new highest
	} else if(current == area->highest){
		//Tie it in
		current->previous = node;
		node->next = current;

		//Reassign this pointer
		area->highest = node;

		//Redo all the offsets based on previous
		recalculate_all_offsets(current);

	//Otherwise we aren't at the very end. We'll insert
	//the node before the previous
	} else {
		//Let's break the chain here
		current->previous->next = node;
		node->previous = current->previous;
		//Now attach it to current
		node->next = current;
		current->previous = node;

		//In this case, recalculate all of the offsets based on current
		//because current comes below node, so we'll need to use it's offset
		//as a seed value
		recalculate_all_offsets(current);
	}
}


/**
 * Add a spilled variable to the stack. Spilled variables
 * always go on top, no matter what
 */
void add_spilled_variable_to_stack(stack_data_area_t* area, void* variable){
	//We already know it's one of these
	three_addr_var_t* var = variable;

	if(does_stack_contain_variable(area, var) == TRUE){
		return;
	}

	

	//First, let's create what we'll use to store this 
	stack_data_area_node_t* node = calloc(1, sizeof(stack_data_area_node_t));
	//Tie this in
	node->variable = variable;

	//Let's make this a multiple of 4 by first adding three(set the 2 lsb's), and 
	//then rounding up by anding with a value with the 2 lsb's as 0
	node->variable_size = (var->type->type_size + 3) & ~0x3;

	//We can now increment the total size here
	area->total_size += node->variable_size;

	//Calculate this node's offset
	if(area->highest != NULL){
		//Calculate the offset of this one
		node->offset = area->highest->offset + area->highest->variable_size;
	} else {
		//Otherwise this is the highest, so it's 0
		node->offset = 0;
	}

	//Store this in the variable too
	var->stack_offset = node->offset;

	//This is the highest area
	node->next = area->highest;
	
	//This is now the highest on there
	area->highest = node;
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

	//Let's also update this variable's related offset to avoid confusion down the line
	((three_addr_var_t*)(variable))->stack_offset = 0;

	//Special case - it's the head
	if(area->highest == current){
		//Reserve this one's address
		stack_data_area_node_t* temp = area->highest;
		//Advance to the next one
		area->highest = area->highest->next;
		//Now we deallocate temp
		free(temp);

		//If this is the case we can recalculate
		if(area->highest != NULL){
			//Forget the reference to the dead node
			area->highest->previous = NULL;
			//Recalculate all of these offsets
			recalculate_all_offsets(area->highest);
		}

	//Other special case - it's the tail
	} else if(current->next == NULL){
		//Just NULL this out and we're good
		current->previous->next = NULL;

		//This offset is now nothing - it's at the bottom
		current->previous->offset = 0;

		//Update this one's variable too
		((three_addr_var_t*)(current->previous->variable))->stack_offset = 0;

		//Now redo all of our offsets
		recalculate_all_offsets(current->previous);

		//And scrap current
		free(current);

	//Otherwise it's now the head
	} else {
		//This will cut out current completely
		current->previous->next = current->next;
		current->next->previous = current->previous;

		//Use the next field to recalculate all offsets
		recalculate_all_offsets(current->next);

		//and now we scrap this one
		free(current);
	}
}


/**
 * Print the stack data area out in its entirety
 */
void print_stack_data_area(stack_data_area_t* area){
	printf("======== Stack Layout ============\n");

	//Easiest case here
	if(area->highest == NULL){
		printf("EMPTY\n");
	} else {
		//Otherwise we run through everything
		stack_data_area_node_t* current = area->highest;

		while(current != NULL){
			three_addr_var_t* current_var = current->variable;
			//We'll take the variable and the size
			if(current_var->is_temporary == FALSE){
				printf("%10s\t%8d\t%8d\n", current_var->linked_var->var_name.string, current->variable_size, current_var->stack_offset);
			} else {
				printf("temp %d\t%8d\t%8d\n", current_var->temp_var_number, current->variable_size, current_var->stack_offset);
			}

			//Advance the node
			current = current->next;
		}
	}

	printf("======== Stack Layout ============\n");
}



/**
 * Deallocate the internal linked list of the stack data area
 */
void stack_data_area_dealloc(stack_data_area_t* stack_data_area){
	//All we need to do here is deallocate the dynamic array
	dynamic_array_dealloc(stack_data_area->variables);
}
