/**
 * Author: Jack Robbins
 * An API for a heap allocated stack implementation. Fully integrated for all stack
 * operations like push, pop and peek, and provides cleanup support as well
 *
 * This is a fully generic stack, for use in DFS mostly
 */

#ifndef HEAP_STACK_H
#define HEAP_STACK_H

#include <sys/types.h>

//Allows us to use stack_node_t as a type
typedef struct stack_node_t stack_node_t;

/**
 * Define a return type for is_empty
 * queries
 */
typedef enum {
	HEAP_STACK_EMPTY,
	HEAP_STACK_NOT_EMPTY
} heap_stack_status_t;

/**
 * Nodes for our stack
 */
struct stack_node_t {
	stack_node_t* next;
	void* data;
};


/**
 * A reference to the the stack object that allows us to
 * have more than one stack
 */
typedef struct {
	struct stack_node_t* top;
	u_int16_t num_nodes;
} heap_stack_t;


/**
 * Initialize a stack
 */
heap_stack_t* heap_stack_alloc();

/**
 * Push a pointer onto the top of the stack
 */
void push(heap_stack_t* stack, void* data);

/**
 * Remove the top value of the stack
 */
void* pop(heap_stack_t* stack);

/**
 * Is the stack empty or not
 */
heap_stack_status_t is_empty(heap_stack_t* stack);

/**
 * Return the top value of the stack, but do not
 * remove it
 */
void* peek(heap_stack_t* stack);

/**
 * Destroy the stack with a proper cleanup
 */
void heap_stack_dealloc(heap_stack_t* stack);

#endif /* HEAP_STACK_H */
