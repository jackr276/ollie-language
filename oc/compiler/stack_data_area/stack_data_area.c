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
	//If we already have the variable in here then leave
	if(does_stack_contain_variable(area, variable) == TRUE){
		return;
	}

	//We already know it's one of these
	three_addr_var_t* var = variable;

	/**
	 * Special case: this is the very first thing that we've added onto 
	 * the stack. If this is the case, we can add it, update the size,
	 * and leave
	 */
	if(area->variables->current_index == 0){
		//Add it on
		dynamic_array_add(area->variables, var);

		//Stack offset for this one is 0
		var->stack_offset = 0;

		//Add the overall size to the stack
		area->total_size += var->type->type_size;

		//And we're done so leave out
		return;
	}

	/**
	 * To align new variables that are added onto the stack, we will pad
	 * their starting addresses as needed to ensure that the starting
	 * address of the variable is a multiple of alignable type size
	 */

	//Get the type that we need to align by for the new var
	generic_type_t* base_alignment = get_base_alignment_type(var->type);

	//Get the alignment size
	u_int32_t alignable_size = base_alignment->type_size;

	//What will the starting address of the new variable be?
	u_int32_t new_variable_offset = area->total_size;

	//How much padding do we need? Initially we assume none
	u_int32_t needed_padding = 0;

	//We can just use the overall data area size for this
	if(area->total_size % alignable_size != 0){
		//Grab the needed padding
		needed_padding = area->total_size % alignable_size;
	}

	//This one's stack offset is the original total size plus whatever padding we need
	var->stack_offset = area->total_size + needed_padding;
	
	//Update the total size of the stack too. The new size is the original size
	//with the needed padding and the new type's size added onto it
	area->total_size = area->total_size + needed_padding + var->type->type_size;

	//Finally add this to the array
	dynamic_array_add(area->variables, var);
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
