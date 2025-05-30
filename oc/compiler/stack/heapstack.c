/**
 * Author: Jack Robbins
 * The implementation of the stack functions defined by the API in stack.h
 */

#include "heapstack.h"
#include <stdio.h>
#include <stdlib.h>


/**
 * Create a stack
 */
heap_stack_t* heap_stack_alloc(){
	//Allocate our stack
	heap_stack_t* stack = (heap_stack_t*)malloc(sizeof(heap_stack_t));

	//Initialize these values
	stack->num_nodes = 0;
	stack->top = NULL;

	//Return the stack
	return stack;
}


/**
 * Push data to the top of the stack
 */
void push(heap_stack_t* stack, void* data){
	//Just in case
	if(stack == NULL){
		printf("ERROR: Stack was never initialized\n");
		return;
	}

	//Just in case
	if(data == NULL){
		printf("ERROR: Cannot enter null data\n");
		return;
	}

	//Allocate a new node
	stack_node_t* new = (stack_node_t*)malloc(sizeof(stack_node_t));
	//Store the data
	new->data = data;

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
void* pop(heap_stack_t* stack){
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
	void* top = stack->top->data;
	
	stack_node_t* temp = stack->top;

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
void* peek(heap_stack_t* stack){
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
 * Is the stack empty or not? Return 1 if empty
 */
heap_stack_status_t heap_stack_is_empty(heap_stack_t* stack){
	if(stack->num_nodes == 0){
		return HEAP_STACK_EMPTY;
	} else {
		return HEAP_STACK_NOT_EMPTY;
	}
}


/**
 * Completely free all memory in the stack
 *
 * NOTE: This does nothing to touch whatever void* actually is
 */
void heap_stack_dealloc(heap_stack_t* stack){
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

		//Advance the cursor
		cursor = cursor->next;

		//Free the node
		free(temp);
	}

	//Finally free the stack
	free(stack);
}
