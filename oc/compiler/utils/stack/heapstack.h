/**
 * Author: Jack Robbins
 * An API for a heap allocated stack implementation. Fully integrated for all stack
 * operations like push, pop and peek, and provides cleanup support as well
 *
 * This is a fully generic stack, for use in DFS mostly - that stores nothing but pointers
 */

#ifndef HEAP_STACK_H
#define HEAP_STACK_H
#include <sys/types.h>

//Predeclare the type
typedef struct heap_stack_t heap_stack_t;

/**
 * The stack contains a dynamically resizable
 * array, an index, and the current maximum nod
 */
struct heap_stack_t{
	//Stack is a generic array
	void** stack;
	//Points to the next point in the array
	u_int32_t current_index;
	//Current highest size
	u_int32_t current_max_index;
}; 


/**
 * Initialize a stack. The control
 * structure itself will be on the
 * stack in the end
 */
heap_stack_t heap_stack_alloc();

/**
 * Push a pointer onto the top of the stack
 */
void push(heap_stack_t* stack, void* data);

/**
 * Remove the top value of the stack
 */
void* pop(heap_stack_t* stack);

/**
 * Completely wipe the heap stack out
 */
void reset_heap_stack(heap_stack_t* stack);

/**
 * Is the stack empty or not
 */
u_int8_t heap_stack_is_empty(heap_stack_t* stack);

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
