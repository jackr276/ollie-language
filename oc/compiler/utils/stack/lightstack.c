/**
 * Author: Jack Robbins
 * This file contains the implementation for the specialized, lightweight stack
 * defined in "lightstack"
*/

#include "lightstack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "../constants.h"

//Our default stack size is 10 numbers
#define DEFAULT_STACK_SIZE 10

/**
 * Initialize a lightstack
 */
lightstack_t lightstack_initialize(){
	//Stack allocate
	lightstack_t lighstack;

	//We just need to null all of these fields out
	lighstack.stack = NULL;
	lighstack.current_size = 0;
	lighstack.top_index = 0;

	//And give it back
	return lighstack;
}


/**
 * Push the "value" onto the stack
*/
void lightstack_push(lightstack_t* stack, u_int32_t value){
	//Let's see if the stack was initialized. If it wasn't we'll
	//do that right now
	if(stack->stack == NULL){
		//Allocate enough space for 10 16-bit integers
		stack->stack = calloc(sizeof(u_int32_t), DEFAULT_STACK_SIZE);
		//Update the current size too
		stack->current_size = 10;

	//Otherwise we could run into a case where we need to resize the
	//stack. We can do this using a simple reallocation
	} else if(stack->current_size == stack->top_index){
		//Double the current size
		stack->current_size *= 2;
		//Perform our reallocation
		stack->stack = realloc(stack->stack, stack->current_size * sizeof(u_int32_t));
	}

	//Now that any needed updates are out of the way, we can actually push
	stack->stack[stack->top_index] = value; 

	//And update the top index for the next go around
	(stack->top_index)++;
}

/**	
 * Pop and return a value off of the stack
*/
u_int32_t lightstack_pop(lightstack_t* stack){
	//We assume that the user has checked this before calling
	if(lightstack_is_empty(stack) == TRUE){
		fprintf(stderr, "ATTEMPT TO POP AN EMPTY LIGHTSTACK\n");
		exit(1);
	}

	//The most recently pushed value is always at the top index - 1
	u_int16_t value = stack->stack[stack->top_index - 1];

	//Decrement from this
	(stack->top_index)--;

	//Give the value back
	return value;
}


/**
 * Reset the entire lightstack
 */
void reset_lightstack(lightstack_t* stack){
	//Only reset here if it isn't NULL
	if(stack->stack != NULL){
		//Wipe the values
		memset(stack->stack, 0, sizeof(stack->current_size));
	}

	//Reset the index
	stack->top_index = 0;
}


/**
 * Deallocate the lightstack
*/
void lightstack_dealloc(lightstack_t* stack){
	//We only bother freeing the internal pointer to the array
	if(stack->stack != NULL){
		free(stack->stack);
	}
}


/**
 * Determine if the stack is empty
*/
u_int8_t lightstack_is_empty(lightstack_t* stack){
	//If this is the case give back true
	if(stack->top_index == 0){
		return TRUE;
	}

	//Otherwise not
	return FALSE;
}


/**
 * Peek from the top of the stack
*/
u_int32_t lightstack_peek(lightstack_t* stack){
	if(stack->top_index == 0){
		return 0;
	} else {
		return stack->stack[stack->top_index - 1];
	}
}
