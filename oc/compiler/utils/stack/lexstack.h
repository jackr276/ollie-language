/**
 * Author: Jack Robbins
 * An API for a heap allocated stack implementation. Fully integrated for all stack
 * operations like push, pop and peek, and provides cleanup support as well
 *
 * This stack is, for efficiency reasons, specifically only for use with lexer items. A generic
 * stack is provided in heapstack
 */

#ifndef LEX_STACK_H 
#define LEX_STACK_H

#include <sys/types.h>
#include "../../lexer/lexer.h"

//Allows us to use lex_node_t as a type
typedef struct lex_node_t lex_node_t;

/**
 * The current status of the lexer stack
 */
typedef enum{
	LEX_STACK_EMPTY,
	LEX_STACK_NOT_EMPTY
} lex_stack_status_t;

/**
 * Nodes for our stack
 */
struct lex_node_t {
	lex_node_t* next;
	lexitem_t l;
};


/**
 * A reference to the the stack object that allows us to
 * have more than one stack
 */
typedef struct {
	lex_node_t* top;
	u_int16_t num_nodes;
} lex_stack_t;


/**
 * Initialize a stack
 */
lex_stack_t* lex_stack_alloc();

/**
 * Push a pointer onto the top of the stack
 */
void push_token(lex_stack_t* stack, lexitem_t l);

/**
 * Is the stack empty or not
 */
lex_stack_status_t lex_stack_is_empty(lex_stack_t* lex_stack);

/**
 * Remove the top value of the stack
 */
lexitem_t pop_token(lex_stack_t* stack);

/**
 * Return the top value of the stack, but do not
 * remove it
 */
lexitem_t peek_token(lex_stack_t* stack);

/**
 * Destroy the stack with a proper cleanup
 */
void lex_stack_dealloc(lex_stack_t** stack);

#endif /* LEX_STACK_H */
