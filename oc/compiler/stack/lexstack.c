/**
 * Author: Jack Robbins
 * The implementation of the stack functions defined by the API in stack.h
 */

#include "lexstack.h"
#include <stdio.h>
#include <stdlib.h>


/**
 * Create a stack
 */
lex_stack_t* create_lex_stack(){
	//Allocate our stack
	lex_stack_t* stack = (lex_stack_t*)malloc(sizeof(lex_stack_t));

	//Initialize these values
	stack->num_nodes = 0;
	stack->top = NULL;

	//Return the stack
	return stack;
}


/**
 * Push data to the top of the stack
 */
void push_token(lex_stack_t* stack, Lexer_item l){
	//Just in case
	if(stack == NULL){
		printf("ERROR: Stack was never initialized\n");
		return;
	}

	//Allocate a new node
	lex_node_t* new = (lex_node_t*)malloc(sizeof(lex_node_t));
	//Store the data
	new->l = l;

	//Attach to the front of the stack
	new->next = stack->top;
	//Assign the top of the stack to be the new
	stack->top = new;

	//Increment number of nodes
	stack->num_nodes++;
}


/**
 * Is the lex stack empty? 0 = no, 1 = yes
 */
u_int8_t lex_stack_is_empty(lex_stack_t* lex_stack){
	//If top is null, it's empty(1)
	return lex_stack->top == NULL;
}


/**
 * Pop the head off of the stack and return the data
 */
Lexer_item pop_token(lex_stack_t* stack){
	Lexer_item l;
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
	Lexer_item top = stack->top->l;
	
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
Lexer_item peek_token(lex_stack_t* stack){
	Lexer_item l;
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
void destroy_lex_stack(lex_stack_t* stack){
	//Just in case...
	if(stack == NULL){
		printf("ERROR: Attempt to free a null pointer\n");
		return;
	}

	//Define a cursor and a temp
	void* temp;
	lex_node_t* cursor = stack->top;

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
