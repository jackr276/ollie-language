/**
 * Author: Jack Robbins
 *
 * This file contains the implementations for the stack data area header file and
 * the APIs defined within
*/

#include "stack_data_area.h"
#include <stdio.h>
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
 * Does the stack already contain this variable? This is important for types like
 * constructs and arrays
 */
u_int8_t does_stack_contain_symtab_variable(stack_data_area_t* area, void* symtab_variable){
	//Run through and try to find this
	for(u_int16_t i = 0; i < area->variables->current_index; i++){
		//Grab it out
		three_addr_var_t* ir_variable = dynamic_array_get_at(area->variables, i);

		//This is our success case
		if(ir_variable->linked_var != NULL && ir_variable->linked_var == symtab_variable){
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
	 * To align new variables that are added onto the stack, we will pad
	 * their starting addresses as needed to ensure that the starting
	 * address of the variable is a multiple of alignable type size
	 */

	//Get the type that we need to align by for the new var
	generic_type_t* base_alignment = get_base_alignment_type(var->type);

	//Get the alignment size
	u_int32_t alignable_size = base_alignment->type_size;

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
 * Completely realign every piece of data in the stack data
 * area. This is only done after a deletion takes place
 */
static void realign_data_area(stack_data_area_t* area){
	//We're completely restarting here, so set this to 0
	area->total_size = 0;

	//Run through every single variable
	for(u_int16_t i = 0; i < area->variables->current_index; i++){
		//Grab it out
		three_addr_var_t* variable = dynamic_array_get_at(area->variables, i);

		/**
		 * To align new variables that are added onto the stack, we will pad
		 * their starting addresses as needed to ensure that the starting
		 * address of the variable is a multiple of alignable type size
		 */

		//Get the type that we need to align by for the new var
		generic_type_t* base_alignment = get_base_alignment_type(variable->type);

		//Get the alignment size
		u_int32_t alignable_size = base_alignment->type_size;

		//How much padding do we need? Initially we assume none
		u_int32_t needed_padding = 0;

		//We can just use the overall data area size for this
		if(area->total_size % alignable_size != 0){
			//Grab the needed padding
			needed_padding = area->total_size % alignable_size;
		}

		//This one's stack offset is the original total size plus whatever padding we need
		variable->stack_offset = area->total_size + needed_padding;
		
		//Update the total size of the stack too. The new size is the original size
		//with the needed padding and the new type's size added onto it
		area->total_size = area->total_size + needed_padding + variable->type->type_size;
	}
}


/**
 * Remove a node from the stack if it is deemed useless
 *
 * Once we're done removing, we'll need to completely redo the data area's alignment
 *
 */
void remove_variable_from_stack(stack_data_area_t* area, void* variable){
	//Delete this variable
	dynamic_array_delete(area->variables, variable);

	//Realign the whole thing
	realign_data_area(area);
}


/**
 * Print the stack data area out in its entirety
 */
void print_stack_data_area(stack_data_area_t* area){
	printf("======== Stack Layout ============\n");

	//If it's empty we'll leave
	if(area->variables->current_index == 0){
		printf("EMPTY\n");
		printf("======== Stack Layout ============\n");
		return;
	}

	//Otherwise run through everything backwards and print
	for(int16_t i = area->variables->current_index - 1; i >= 0; i--){
		//Grab the variable out
		three_addr_var_t* variable = dynamic_array_get_at(area->variables, i);

		//We'll take the variable and the size
		if(variable->is_temporary == FALSE){
			printf("%10s\t%8d\t%8d\n", variable->linked_var->var_name.string, variable->type->type_size, variable->stack_offset);
		} else {
			printf("temp %d\t%8d\t%8d\n", variable->temp_var_number, variable->type->type_size, variable->stack_offset);
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
