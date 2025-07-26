/**
 * Author: Jack Robbins
 * An API for a heap allocated stack implementation. Fully integrated for all stack
 * operations like push, pop and peek, and provides cleanup support as well
 *
 * This stack is, for efficiency reasons, specifically only for use with lexer items. A generic
 * stack is provided in heapstack
 */

#ifndef NESTING_LEVEL_STACK_H 
#define NESTING_LEVEL_STACK_H 

#include <sys/types.h>

/**
 * All of our different possible nesting values
 */
typedef enum {
	FUNCTION,
	COMPOUND_STATEMENT,
	CASE_STATEMENT,
	LOOP_STATEMENT,
	IF_STATEMENT,
	DEFER_STATEMENT,
	//TODO more probably needed
} nesting_level_t;

/**
 * The current status of the lexer stack
 */
typedef enum{
	NESTING_STACK_EMPTY,
	NESTING_STACK_NOT_EMPTY,
} nesting_level_stack_status_t;

/**
 * Nodes for our stack
 */
struct nesting_level_node_t {
	nesting_level_node_t* next;
	nesting_level_t level;
};


/**
 * A reference to the the stack object that allows us to
 * have more than one stack
 */
typedef struct {
	nesting_level_node_t* top;
	u_int16_t num_nodes;
} nesting_level_stack_t;


/**
 * Initialize a stack
 */
nesting_level_stack_t* nesting_stack_alloc();

/**
 * Add a new nesting level to the top of the stack
 */
void push_nesting_level(nesting_level_stack_t* stack, nesting_level_t level);

/**
 * Is the stack empty or not
 */
nesting_level_stack_status_t nesting_stack_is_empty(nesting_level_stack_t* nesting_stack);

/**
 * Remove the top value of the stack
 */
nesting_level_t pop_level(nesting_level_stack_t* stack);

/**
 * Return the top value of the stack, but do not
 * remove it
 */
nesting_level_t peek_token(nesting_level_stack_t* stack);

/**
 * Destroy the stack with a proper cleanup
 */
void nesting_stack_dealloc(nesting_level_stack_t** stack);

#endif /* NESTING_LEVEL_STACK_T */
