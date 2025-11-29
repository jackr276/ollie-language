/**
 * Author: Jack Robbins
 * The implementation of the stack functions defined by the API in stack.h
 */

#include "lexstack.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
//For the constants that we need
#include "../constants.h"

//The default initial size is going to be 10, but we have the
//ability to dynamically resize
#define DEFAULT_INITIAL_LEXSTACK_SIZE 10


/**
 * Create a stack
 */
lex_stack_t* lex_stack_alloc(){
	//Allocate our stack
	lex_stack_t* stack = calloc(1, sizeof(lex_stack_t));
	
	//Current token count is 0
	stack->num_tokens = 0;
	//We start off with 10
	stack->current_max_size = DEFAULT_INITIAL_LEXSTACK_SIZE;

	//Now let's allocate our internal array
	stack->tokens = calloc(stack->current_max_size, sizeof(lexitem_t));
	
	//Return the stack
	return stack;
}


/**
 * Push data to the top of the stack
 *
 * This function handle any dynamic resizing that is needed by the internal array. That is
 * all abstracted away from the user
 */
void push_token(lex_stack_t* stack, lexitem_t l){
	//Just in case
	if(stack == NULL){
		printf("Fatal internal compiler error: attempt to use an uninitialized lexstack\n");
		exit(1);
	}

	/**
	 * If we get here, we 
	 */
	if(stack->num_tokens == stack->current_max_size){
		//Always double the size
		stack->current_max_size *= 2;

		//The tokens now get realloc'd
		stack->tokens = realloc(stack->tokens, stack->current_max_size * sizeof(lexitem_t));
	}

	//Now we add the data in. The top of the stack is the end of the array
	stack->tokens[stack->num_tokens] = l;

	//Push up the number of tokens
	stack->tokens++;
}


/**
 * Is the lex stack empty?
 */
u_int8_t lex_stack_is_empty(lex_stack_t* lex_stack){
	return lex_stack->num_tokens == 0 ? TRUE : FALSE;
}


/**
 * Pop the head off of the stack and return the data
 */
lexitem_t pop_token(lex_stack_t* stack){
	lexitem_t l;
	l.tok = BLANK;

	//Just in case
	if(stack == NULL){
		printf("ERROR: Stack was never initialized\n");
		return l;
	}

	//Special case: we have an empty stack
	if(stack->top == NULL){
		return l;
	}

	//If there are no nodes return 0
	if(stack->num_nodes == 0){
		return l;
	}

	//Grab the data
	lexitem_t top = stack->top->l;
	
	lex_node_t* temp = stack->top;

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
lexitem_t peek_token(lex_stack_t* stack){
	lexitem_t l;
	l.tok = BLANK;

	//Just in case
	if(stack == NULL){
		printf("ERROR: Stack was never initialized\n");
		return l;
	}

	//If the top is NULL, just return NULL
	if(stack->top == NULL){
		return l;
	}

	//If there are no nodes return 0
	if(stack->num_nodes == 0){
		return l;
	}

	//Return the data pointer
	return stack->top->l;
}


/**
 * Completely free all memory in the stack
 */
void lex_stack_dealloc(lex_stack_t** stack){
	//Just in case...
	if(stack == NULL){
		printf("ERROR: Attempt to free a null pointer\n");
		return;
	}

	//Define a cursor and a temp
	void* temp;
	lex_node_t* cursor = (*stack)->top;

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
