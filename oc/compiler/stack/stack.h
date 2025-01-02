/**
 * Author: Jack Robbins
 * An API for a heap allocated stack implementation. Fully integrated for all stack
 * operations like push, pop and peek, and provides cleanup support as well
 */

#ifndef STACK_H
#define STACK_H

#include <sys/types.h>
#include "../lexer/lexer.h"

//Allows us to use stack_node_t as a type
typedef struct stack_node_t stack_node_t;

/**
 * Nodes for our stack
 */
struct stack_node_t {
	stack_node_t* next;
	Lexer_item l;
};


/**
 * A reference to the the stack object that allows us to
 * have more than one stack
 */
typedef struct {
	struct stack_node_t* top;
	u_int16_t num_nodes;
} stack_t;


/**
 * Initialize a stack
 */
stack_t* create_stack();

/**
 * Push a pointer onto the top of the stack
 */
void push(stack_t* stack, Lexer_item l);

/**
 * Remove the top value of the stack
 */
Lexer_item pop(stack_t* stack);

/**
 * Return the top value of the stack, but do not
 * remove it
 */
Lexer_item peek(stack_t* stack);

/**
 * Destroy the stack with a proper cleanup
 * FULL_CLEANUP: free all of the void* pointers on the stack
 * STATES_ONLY: all pointers are left alone. This may lead to memory leaks
 */
void destroy_stack(stack_t* stack);

#endif
