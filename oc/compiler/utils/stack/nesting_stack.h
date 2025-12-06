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
	NESTING_FUNCTION,
	NESTING_CASE_STATEMENT,
	NESTING_C_STYLE_CASE_STATEMENT, // This one allows breaks
	NESTING_LOOP_STATEMENT,
	NESTING_IF_STATEMENT,
	NESTING_DEFER_STATEMENT,
} nesting_level_t;


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
u_int8_t nesting_stack_is_empty(nesting_stack_t* nesting_stack);

/**
 * Perform a scan of the nesting stack to see if a given level is contained
 */
u_int8_t nesting_stack_contains_level(nesting_stack_t* nesting_stack, nesting_level_t level);

/**
 * Get the estimated execution frequency of something given a nesting level using
 * our custom rules
 */
u_int32_t get_estimated_execution_frequency_from_nesting_stack(nesting_stack_t* stack);

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
