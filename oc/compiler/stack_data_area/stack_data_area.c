/**
 * Author: Jack Robbins
 *
 * This file contains the implementations for the stack data area header file and
 * the APIs defined within
*/

#include "stack_data_area.h"
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>

//Atomically increasing stack region ID
static u_int32_t current_stack_region_id = 0;


/**
 * Returns the current region Id, then increments
 * holder
 */
static u_int32_t increment_and_get_stack_region_id(){
	//Get the old value and then increment
	return current_stack_region_id++;
}


/**
 * Allocate the internal dynamic array in the data area
 */
void stack_data_area_alloc(stack_data_area_t* area){
	//Allocate the regions array
	area->stack_regions = dynamic_array_alloc();
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
 * Create a stack region with a given size and base address in the stack 
 */
static stack_region_t* create_stack_region(u_int32_t base_address, u_int32_t size){
	//Calloc it
	stack_region_t* region = calloc(1, sizeof(stack_region_t));

	//Populate
	region->size = size;
	region->base_address = base_address;

	//Assign it a unique ID
	region->stack_region_id = increment_and_get_stack_region_id();

	//Throw back
	return region;
}


/**
 * Create a stack region for the type provided. This will handle alignment and addition
 * of this stack region
 */
stack_region_t* create_stack_region_for_type(stack_data_area_t* area, generic_type_t* type){
	/**
	 * To align new variables that are added onto the stack, we will pad
	 * their starting addresses as needed to ensure that the starting
	 * address of the variable is a multiple of alignable type size
	 */

	//First we'll need the type that we can align by
	generic_type_t* base_alignment_type = get_base_alignment_type(type);

	//Get the alignment size
	u_int32_t alignable_size = base_alignment_type->type_size;

	//How much padding do we need? Initially we assume none
	u_int32_t needed_padding = 0;

	//We can just use the overall data area size for this
	if(area->total_size % alignable_size != 0){
		//Grab the needed padding
		needed_padding = area->total_size % alignable_size;
	}

	//Create a new stack region. The base address of this stack region must be the total area plus
	//the needed padding
	stack_region_t* region = create_stack_region(area->total_size + needed_padding, type->type_size);

	//Store the type in here - we'll need it for later on
	region->type = type;

	//The new size has the needed padding and the new region's size on top of it
	area->total_size = area->total_size + needed_padding + type->type_size;

	//Add the region into the stack data area
	dynamic_array_add(area->stack_regions, region);

	//Give back the allocated region
	return region;
}


/**
 * Completely realign every piece of data in the stack data
 * area. This is only done after a deletion takes place
 */
static void realign_data_area(stack_data_area_t* area){
	//We're completely restarting here, so set this to 0
	area->total_size = 0;

	//Run through every single variable
	for(u_int16_t i = 0; i < area->stack_regions->current_index; i++){
		//Grab it out
		stack_region_t* region = dynamic_array_get_at(area->stack_regions, i);

		/**
		 * To align new regions that are added onto the stack, we will pad
		 * their starting addresses as needed to ensure that the starting
		 * address of the variable is a multiple of alignable type size
		 */

		//Get the type that we need to align by for the new var
		generic_type_t* base_alignment = get_base_alignment_type(region->type);

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
		region->base_address = area->total_size + needed_padding;
		
		//Update the total size of the stack too. The new size is the original size
		//with the needed padding and the new type's size added onto it
		area->total_size = area->total_size + needed_padding + region->type->type_size;
	}
}


/**
 * Remove a given region from the stack
 */
void remove_region_from_stack(stack_data_area_t* area, stack_region_t* region){
	//Delete this variable
	dynamic_array_delete(area->stack_regions, region);

	//Realign the entire thing now
	realign_data_area(area);
}


/**
 * Print the stack data area out in its entirety
 */
void print_stack_data_area(stack_data_area_t* area){
	printf("======== Stack Layout ============\n");

	//If it's empty we'll leave
	if(area->stack_regions->current_index == 0){
		printf("EMPTY\n");
		printf("======== Stack Layout ============\n");
		return;
	}

	//Run through all of the regions backwards and print
	for(int16_t i = area->stack_regions->current_index - 1; i >= 0; i--){
		//Extract it
		stack_region_t* region = dynamic_array_get_at(area->stack_regions, i);

		//Print it
		printf("Region #%d\t%8d\t%8d\n", region->stack_region_id, region->size, region->base_address);
	}

	printf("======== Stack Layout ============\n");
}


/**
 * Deallocate the internal linked list of the stack data area
 */
void stack_data_area_dealloc(stack_data_area_t* stack_data_area){
	//Run through all regions
	for(u_int16_t i = 0; i < stack_data_area->stack_regions->current_index; i++){
		//Grab it out
		stack_region_t* region = dynamic_array_get_at(stack_data_area->stack_regions, i);

		//Delete it
		free(region);
	}

	//Finally deallocate the region here
	dynamic_array_dealloc(stack_data_area->stack_regions);
}
