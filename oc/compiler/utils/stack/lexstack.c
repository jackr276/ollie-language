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
 * Create a stack. The resulting control structure
 * will be stack allocated
 */
lex_stack_t lex_stack_alloc(){
	//Allocate our stack
	lex_stack_t stack;
	
	//Current token count is 0
	stack.num_tokens = 0;
	//We start off with 10
	stack.current_max_size = DEFAULT_INITIAL_LEXSTACK_SIZE;

	//Now let's allocate our internal array
	stack.tokens = calloc(stack.current_max_size, sizeof(lexitem_t));
	
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
	stack->num_tokens++;
}


/**
 * Is the lex stack empty?
 */
u_int8_t lex_stack_is_empty(lex_stack_t* lex_stack){
	return lex_stack->num_tokens == 0 ? TRUE : FALSE;
}


/**
 * Pop the head off of the stack and return the data
 *
 * Returns the BLANK token if nothing is found
 */
lexitem_t pop_token(lex_stack_t* stack){
	//Initialize the blank token
	lexitem_t empty_token;
	empty_token.tok = BLANK;

	//Fatal error here, something went wrong if the user is trying this
	if(stack == NULL){
		printf("Fatal internal compiler error: Attempt to pop off of a null lexstack\n");
		exit(1);
	}

	//If we have no tokens then return the empty token
	if(stack->num_tokens == 0){
		return empty_token;
	}

	//Remember, the num_tokens index stores the index of the *next*
	//available index. To get the top, we need to subtract one from it
	lexitem_t top = stack->tokens[stack->num_tokens - 1];

	//Decrement one overall
	stack->num_tokens--;

	//And give it back
	return top;
}


/**
 * Peek the top of the stack without removing it
 */
lexitem_t peek_token(lex_stack_t* stack){
	//Initialize the blank token here
	lexitem_t blank_token;
	blank_token.tok = BLANK;

	//Fatal error here, something went wrong if the user is trying this
	if(stack == NULL){
		printf("Fatal internal compiler error: Attempt to peek a null lexstack\n");
		exit(1);
	}

	//If we have nothing on the stack, just return the blank
	//token
	if(stack->num_tokens == 0){
		return blank_token;
	}

	//Return 
	return stack->tokens[stack->num_tokens - 1];
}


/**
 * Completely free all memory in the stack
 */
void lex_stack_dealloc(lex_stack_t* stack){
	//Fatal error here, something went wrong if the user is trying this
	if(stack->tokens == NULL){
		printf("Fatal internal compiler error: attempt to free a null lexstack\n");
		exit(1);
	}

	//Free the internal array
	free(stack->tokens);

	//Set this to null as a warning
	stack->tokens = NULL;
}
