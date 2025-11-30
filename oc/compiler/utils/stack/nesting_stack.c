/**
 * Author: Jack Robbins
 * The implementation of the stack functions defined by the API in nesting_level_stack.h
 */

#include "nesting_stack.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "../constants.h"

/**
 * Create a stack
 */
nesting_stack_t* nesting_stack_alloc(){
	//Allocate our stack
	nesting_stack_t* stack = calloc(1, sizeof(nesting_stack_t));

	//Initialize these values
	stack->num_nodes = 0;
	stack->top = NULL;

	//Return the stack
	return stack;
}


/**
 * Push data to the top of the stack
 */
void push_nesting_level(nesting_stack_t* stack, nesting_level_t level){
	//Just in case
	if(stack == NULL){
		printf("ERROR: Stack was never initialized\n");
		return;
	}

	//Allocate a new node
	nesting_stack_node_t* new = calloc(1, sizeof(nesting_stack_node_t));
	//Store the level
	new->level = level;

	//Attach to the front of the stack
	new->next = stack->top;
	//Assign the top of the stack to be the new
	stack->top = new;

	//Increment number of nodes
	stack->num_nodes++;
}


/**
 * Is the lex stack empty?
 */
u_int8_t nesting_stack_is_empty(nesting_stack_t* nesting_stack){
	return nesting_stack->num_nodes == 0 ? TRUE : FALSE;
}


/**
 * Pop the head off of the stack and return the data
 */
nesting_level_t pop_nesting_level(nesting_stack_t* stack){
	//Special case: we have an empty stack
	if(stack->top == NULL){
		return NO_NESTING_LEVEL;
	}

	//Grab the data
	nesting_level_t top = stack->top->level;
	
	nesting_stack_node_t* temp = stack->top;

	//"Delete" the node from the stack
	stack->top = stack->top->next;

	//Free the node
	free(temp);
	//Decrement number of nodes
	stack->num_nodes--;

	return top;
}


/**
 * Peek the top of the stack without removing it
 */
nesting_level_t peek_nesting_level(nesting_stack_t* stack){
	//If the top is NULL, just return NULL
	if(stack->top == NULL){
		return NO_NESTING_LEVEL;
	}

	//Return the data pointer
	return stack->top->level;
}


/**
 * Perform a scan of the nesting stack to see if a given level is contained
 */
u_int8_t nesting_stack_contains_level(nesting_stack_t* nesting_stack, nesting_level_t level){
	//Grab a cursor to the current node
	nesting_stack_node_t* current = nesting_stack->top;

	//Run through the nesting stack from top to bottom
	while(current != NULL){
		//If these are equal then we're done, we've found it
		if(current->level == level){
			return TRUE;
		}

		//Otherwise we'll advance downwards
		current = current->next;
	}

	//If we make it here, we didn't find it, so we're done
	return FALSE;
}


/**
 * Completely free all memory in the stack
 */
void nesting_stack_dealloc(nesting_stack_t** stack){
	//Just in case...
	if(stack == NULL){
		printf("ERROR: Attempt to free a null pointer\n");
		return;
	}

	//Define a cursor and a temp
	void* temp;
	nesting_stack_node_t* cursor = (*stack)->top;

	//Free every node
	while(cursor != NULL){
		//Save the cursor
		temp = cursor; 

		//Advance the cursor
		cursor = cursor->next;

		//Free the node
		free(temp);
	}

	//Finally free the stack
	free(*stack);

	//Set to NULL as a warning
	*stack = NULL;
}
