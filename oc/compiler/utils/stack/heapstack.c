/**
 * Author: Jack Robbins
 * The implementation of the stack functions defined by the API in stack.h
 */

#include "heapstack.h"
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
//For the TRUE and FALSE values
#include "../constants.h"

//By default, our size is 10
#define DEFAULT_HEAP_STACK_SIZE 10

/**
 * Create a stack
 */
heap_stack_t* heap_stack_alloc(){
	//Allocate our stack
	heap_stack_t* stack = calloc(1, sizeof(heap_stack_t));

	//Now allocate the internal array
	stack->stack = calloc(DEFAULT_HEAP_STACK_SIZE, sizeof(void*));

	//Current index is 10
	stack->current_max_index = 10;

	//Return the stack
	return stack;
}


/**
 * Push data to the top of the stack. We assume that
 * the stack has already been initialized when we do
 * this
 *
 * This function will dynamically resize stack as needed
 */
void push(heap_stack_t* stack, void* data){
	//Dynamic resize - do we need to do it
	if(stack->current_max_index == stack->current_index){
		//Double it
		stack->current_max_index *= 2;

		//Perform the resize
		stack->stack = realloc(stack->stack, stack->current_max_index * sizeof(void*));
	}

	//Insert into the stack
	stack->stack[stack->current_index] = data;

	//And update the current index
	stack->current_index++;
}


/**
 * Pop the head off of the stack and return the data
 *
 * We represent the stack internally with an array, so all that popping
 * does is return the data at the last inserted index(back) of the array
 */
void* pop(heap_stack_t* stack){
	//If there are no nodes return 0
	if(stack->current_index == 0){
		return NULL;
	}

	//Decrement one from the current index. The pointer
	//will now reference the last inserted node
	stack->current_index--;

	//Grab the data at the current index
	void* top = stack->stack[stack->current_index];

	//Null it out now to be safe so that future 
	//callers don't mistake it for something else
	stack->stack[stack->current_index] = NULL;
	
	//Give this back
	return top;
}


/**
 * Peek the top of the stack without removing it
 */
void* peek(heap_stack_t* stack){
	//If there are no nodes return 0
	if(stack->current_index == 0){
		return NULL;
	}

	//Give back the value at the current index
	//minus 1(last inserted index)
	return stack->stack[stack->current_index - 1];
}


/**
 * Is the stack empty or not? Return 1 if empty
 */
u_int8_t heap_stack_is_empty(heap_stack_t* stack){
	return stack->current_index == 0 ? TRUE : FALSE;
}


/**
 * Completely wipe the heap stack out
 *
 * Note that we will not attempt to shrink the internal
 * array - if that has grown beyond the default then it will
 * stay. We're just wiping the whole thing out
 */
void reset_heap_stack(heap_stack_t* stack){
	//Zero the whole thing out
	memset(stack->stack, 0, stack->current_max_index * sizeof(void*));

	//And now reset the current index to be 0
	stack->current_index = 0;
}


/**
 * Completely free all memory in the stack
 *
 * NOTE: This does nothing to touch whatever void* actually is
 */
void heap_stack_dealloc(heap_stack_t* stack){
	//Release the stack
	free(stack->stack);

	//And release the entire struct
	free(stack);
}
