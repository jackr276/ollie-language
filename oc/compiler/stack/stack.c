/**
 * Author: Jack Robbins
 * The implementation of the stack functions defined by the API in stack.h
 */

#include "stack.h"
#include <stdio.h>
#include <stdlib.h>


/**
 * Create a stack
 */
stack_t* create_stack(){
	//Allocate our stack
	stack_t* stack = (stack_t*)malloc(sizeof(stack_t));

	//Initialize these values
	stack->num_nodes = 0;
	stack->top = NULL;

	//Return the stack
	return stack;
}


/**
 * Push data to the top of the stack
 */
void push(stack_t* stack, void* element){
	//Just in case
	if(stack == NULL){
		printf("ERROR: Stack was never initialized\n");
		return;
	}

	//Allocate a new node
	stack_node_t* new = (stack_node_t*)malloc(sizeof(stack_node_t));
	//Store the data
	new->data = element;

	//Attach to the front of the stack
	new->next = stack->top;
	//Assign the top of the stack to be the new
	stack->top = new;

	//Increment number of nodes
	stack->num_nodes++;
}


/**
 * Pop the head off of the stack and return the data
 */
void* pop(stack_t* stack){
	//Just in case
	if(stack == NULL){
		printf("ERROR: Stack was never initialized\n");
		return NULL;
	}

	//Special case: we have an empty stack
	if(stack->top == NULL){
		return NULL;
	}

	//If there are no nodes return 0
	if(stack->num_nodes == 0){
		return NULL;
	}

	//Grab the data
	void* ptr = stack->top->data;
	
	stack_node_t* temp = stack->top;

	//"Delete" the node from the stack
	stack->top = stack->top->next;

	//Free the node
	free(temp);
	//Decrement number of nodes
	stack->num_nodes--;

	return ptr;
}


/**
 * Peek the top of the stack without removing it
 */
void* peek(stack_t* stack){
	//Just in case
	if(stack == NULL){
		printf("ERROR: Stack was never initialized\n");
		return NULL;
	}

	//If the top is NULL, just return NULL
	if(stack->top == NULL){
		return NULL;
	}

	//If there are no nodes return 0
	if(stack->num_nodes == 0){
		return NULL;
	}

	//Return the data pointer
	return stack->top->data;
}


/**
 * Completely free all memory in the stack
 */
void destroy_stack(stack_t* stack, stack_cleanup_mode_t mode){
	//Just in case...
	if(stack == NULL){
		printf("ERROR: Attempt to free a null pointer\n");
		return;
	}

	//Define a cursor and a temp
	void* temp;
	stack_node_t* cursor = stack->top;

	//Free every node
	while(cursor != NULL){
		//Save the cursor
		temp = cursor; 

		//If we are in full cleanup mode, free the data too
		if(mode == FULL_CLEANUP){
			free(cursor->data);
		}

		//Advance the cursor
		cursor = cursor->next;

		//Free the node
		free(temp);
	}

	//Finally free the stack
	free(stack);
}
