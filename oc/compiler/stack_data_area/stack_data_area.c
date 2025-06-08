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


/**
 * Whenever we delete or insert something, we'll need to recompute all of the offsets
 * of anything that is above that node. This function will recompute the offsets
 * of anything above the value passed in as "node"
 */
static void recalculate_all_offsets(stack_data_area_t* area, stack_data_area_node_t* node){
	//Hang onto our current offset. This will initially be whatever this node's offset is plus
	//it's variable size. So, the *next* node that we encounter will have an offset of current
	//offset
	u_int32_t current_offset = node->offset + node->variable_size;

	//Grab the one above this in the stack
	stack_data_area_node_t* current = node->previous;

	//Hold onto the current var
	three_addr_var_t* current_var;

	//So long as we don't hit the end here
	while(current != NULL){
		//Grab this for convenience
		current_var = current->variable;

		//Assign the offset
		current->offset = current_offset;

		//This needs to be recomputed too
		current_var->stack_offset = current_offset;

		//Now recompute the overall offset
		current_offset = current_offset + current->variable_size;

		//Now we advance the pointer upwards
		current = current->previous;
	}

	//By the time we get out down here, we should have reassessed all of the offsets
}


/**
 * Align the stack data area size to be 16-byte aligned
 */
void align_stack_data_area(stack_data_area_t* area){
	//This means there's nothing in it
	if(area->total_size == 0){
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
 * NOTE: We guarantee that each address in the stack will be at least
 * 4-byte aligned. As such, the size of the variable on the stack may not 
 * match the size of the variable in the stack data area
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
		recalculate_all_offsets(area, node);

	//This means that we have a new highest
	} else if(current == area->highest){
		//Tie it in
		current->previous = node;
		node->next = current;

		//Reassign this pointer
		area->highest = node;

		//Redo all the offsets based on previous
		recalculate_all_offsets(area, current);

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
		recalculate_all_offsets(area, current);
	}
}


/**
 * Add a spilled variable to the stack. Spilled variables
 * always go on top, no matter what
 */
void add_spilled_variable_to_stack(stack_data_area_t* area, void* variable){
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

	//Calculate this node's offset
	if(area->highest != NULL){
		//Calculate the offset of this one
		node->offset = area->highest->offset + area->highest->variable_size;
	} else {
		//Otherwise this is the highest, so it's 0
		node->offset = 0;
	}

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
			recalculate_all_offsets(area, area->highest);
		}

	//Other special case - it's the tail
	} else if(current->next == NULL){
		//Just NULL this out and we're good
		current->previous->next = NULL;

		//This offset is now nothing - it's at the bottom
		current->previous->offset = 0;

		//Now redo all of our offsets
		recalculate_all_offsets(area, current->previous);

		//And scrap current
		free(current);

	//Otherwise it's now the head
	} else {
		//This will cut out current completely
		current->previous->next = current->next;
		current->next->previous = current->previous;

		//Use the next field to recalculate all offsets
		recalculate_all_offsets(area, current->next);

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
			printf("%10s\t%8d\t%8d\n", current_var->linked_var->var_name, current->variable_size, current_var->stack_offset);

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

