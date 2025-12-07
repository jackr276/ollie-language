/**
 * Author: Jack Robbins
 * The implementation of the stack functions defined by the API in nesting_level_stack.h
 */

#include "nesting_stack.h"
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include "../constants.h"

//By default we'll be 10 levels deep - but this module dynamically reallocates
#define DEFAULT_NESTING_STACK_SIZE 10

/**
 * Create a stack
 */
nesting_stack_t* nesting_stack_alloc(){
	//Allocate our stack
	nesting_stack_t* stack = calloc(1, sizeof(nesting_stack_t));

	//Allocate the internal array
	stack->stack = calloc(DEFAULT_NESTING_STACK_SIZE, sizeof(nesting_level_t));

	//Store the current max index
	stack->current_max_index = DEFAULT_NESTING_STACK_SIZE;

	//Return the stack
	return stack;
}


/**
 * Push data to the top of the stack
 *
 * This function handles our dynamic resize if need be
 */
void push_nesting_level(nesting_stack_t* stack, nesting_level_t level){
	//Handle dynamic resize case
	if(stack->current_index == stack->current_max_index){
		//Double it
		stack->current_max_index *= 2;

		//Realloc the array
		stack->stack = realloc(stack->stack, stack->current_max_index * sizeof(nesting_level_t));
	}

	//Insert the value at the current index
	stack->stack[stack->current_index] = level;

	//Increment the current index
	stack->current_index++;
}


/**
 * Is the lex stack empty?
 */
u_int8_t nesting_stack_is_empty(nesting_stack_t* nesting_stack){
	return nesting_stack->current_index == 0 ? TRUE : FALSE;
}


/**
 * Get the estimated execution frequency of something given a nesting level using
 * our custom rules
 *
 * Algorithm:
 * 	for each level in the stack from bottom to top
 * 		if level == if:
 * 			estimated_execution_frequency *= 1/2 
 * 		if level == loop
 * 			estimated_execution_frequency *= 10
 * 		if level == case_statement
 * 			estimated_execution_frequency *= 1/5
 */
u_int32_t get_estimated_execution_frequency_from_nesting_stack(nesting_stack_t* stack){
	//Initialize our frequency to be 1 initially
	u_int32_t estimated_execution_frequency = 1;

	//Iterate over the entire nesting stack from bottom-to-top
	for(int32_t i = stack->current_index - 1; i >= 0; i--){
		//Extract it
		nesting_level_t level = stack->stack[i];

		//We update based on it
		switch(level){
			case NESTING_IF_STATEMENT:

			//We assume that each loop executes 10 times
			case NESTING_LOOP_STATEMENT:
				estimated_execution_frequency *= 10;
				break;

			//We know for a fact that a defer statement
			//will only ever execute once - so if we get
			//here, we do a reset
			case NESTING_DEFER_STATEMENT:
				estimated_execution_frequency = 1;
				break;

			//By default do nothing - this just means that our
			//estimated execution count remains the same
			default:
				break;
		}
	}

	//Give back whatever we thought
	return estimated_execution_frequency;
}


/**
 * Pop the head off of the stack and return the data
 */
nesting_level_t pop_nesting_level(nesting_stack_t* stack){
	//Handle the empty stack case
	if(stack->current_index == 0){
		return NO_NESTING_LEVEL;
	}

	//Decrement the current index
	stack->current_index--;

	//Return the stack value at the decremented index
	nesting_level_t data = stack->stack[stack->current_index];

	//NULL this out just for safety
	stack->stack[stack->current_index] = NO_NESTING_LEVEL;

	//Give back the data
	return data;
}


/**
 * Peek the top of the stack without removing it
 */
nesting_level_t peek_nesting_level(nesting_stack_t* stack){
	//If there's nothing on this then just give back our
	//version of null
	if(stack->current_index == 0){
		return NO_NESTING_LEVEL;
	}

	//Return the data at the value before the current index
	return stack->stack[stack->current_index - 1];
}


/**
 * Perform a scan of the nesting stack to see if a given level is contained
 */
u_int8_t nesting_stack_contains_level(nesting_stack_t* nesting_stack, nesting_level_t level){
	//We can simply crawl through the array to find this out. Remember
	//that the top of the stack is the back of the array, so we go through
	//backwards
	for(int32_t i = nesting_stack->current_index - 1; i >= 0; i--){
		//We do contain it
		if(nesting_stack->stack[i] == level){
			return TRUE;
		}
	}

	//If we do a full scan of the array and still get down here, we didn't make it
	return FALSE;
}


/**
 * Completely free all memory in the stack
 */
void nesting_stack_dealloc(nesting_stack_t** stack){
	//Free the internal array
	free((*stack)->stack);

	//Free the overall struct
	free(*stack);

	//Set it to NULL as a warning
	*stack = NULL;
}
