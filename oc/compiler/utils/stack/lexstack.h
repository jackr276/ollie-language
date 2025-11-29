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

//Predefine
typedef struct lex_stack_t lex_stack_t;

/**
 * Lexitem stack type itself
 */
struct lex_stack_t {
	//Internal token arrays
	lexitem_t* tokens;
	//The current maximum size
	u_int32_t current_max_size;
	//The number of tokens
	u_int32_t num_tokens;
};


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
u_int8_t lex_stack_is_empty(lex_stack_t* lex_stack);

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
