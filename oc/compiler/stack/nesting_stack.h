/**
 * Author: Jack Robbins
 * An API for a heap allocated stack implementation. Fully integrated for all stack
 * operations like push, pop and peek, and provides cleanup support as well
 *
 * This stack is, for efficiency reasons, specifically only for use with lexer items. A generic
 * stack is provided in heapstack
 */

#ifndef NESTING_STACK_H 
#define NESTING_STACK_H 

#include <sys/types.h>

typedef struct nesting_stack_node_t nesting_stack_node_t;

/**
 * All of our different possible nesting values
 */
typedef enum {
	NO_NESTING_LEVEL = 0, // Our default value
	FUNCTION,
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
} nesting_stack_status_t;

/**
 * Nodes for our stack
 */
struct nesting_stack_node_t {
	nesting_stack_node_t* next;
	nesting_level_t level;
};


/**
 * A reference to the the stack object that allows us to
 * have more than one stack
 */
typedef struct {
	nesting_stack_node_t* top;
	u_int16_t num_nodes;
} nesting_stack_t;


/**
 * Initialize a stack
 */
nesting_stack_t* nesting_stack_alloc();

/**
 * Add a new nesting level to the top of the stack
 */
void push_nesting_level(nesting_stack_t* stack, nesting_level_t level);

/**
 * Is the stack empty or not
 */
nesting_stack_status_t nesting_stack_is_empty(nesting_stack_t* nesting_stack);

/**
 * Perform a scan of the nesting stack to see if a given level is contained
 */
u_int8_t nesting_stack_contains_level(nesting_stack_t* nesting_stack, nesting_level_t level);

/**
 * Remove the top value of the stack
 */
nesting_level_t pop_nesting_level(nesting_stack_t* stack);

/**
 * Return the top value of the stack, but do not
 * remove it
 */
nesting_level_t peek_nesting_level(nesting_stack_t* stack);

/**
 * Destroy the stack with a proper cleanup
 */
void nesting_stack_dealloc(nesting_stack_t** stack);

#endif /* NESTING_LEVEL_STACK_T */
