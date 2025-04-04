/**
 * Author: Jack Robbins
 * This stack is designed to be as lightweight as possible due to it's use
 * in SSA variable renaming. It was designed solely with this purpose in mind
 *
 * The lightstack ONLY stores 16-bit unsigned integers for it's values. It
 * does not store pointers, so do NOT use it for anything general purpose
*/

#include <sys/types.h>

#ifndef LIGHT_STACK_H
#define LIGHT_STACK_H

//The overall type
typedef struct lightstack_t lightstack_t;

//The lightstack structure
struct lightstack_t{
	//The index of the top
	//NOTE: the actual current top is 1 below this(top_index - 1)
	u_int16_t top_index;
	//The current size
	u_int16_t current_size;
	//The actual stack array
	u_int16_t* stack;
};

//Due to the way a lightstack works, there is NO dedicated initialization. Any/all initialization
//happens on the first push

/**
 * Push the "value" onto the stack
*/
void lightstack_push(lightstack_t* stack, u_int16_t value);

/**	
 * Pop and return a value off of the stack
*/
u_int16_t lightstack_pop(lightstack_t* stack);

/**
 * Deallocate the lightstack
*/
void lightstack_dealloc(lightstack_t* stack);

/**
 * Determine if the stack is empty
*/
u_int8_t lightstack_is_empty(lightstack_t* stack);

/**
 * Determine if the stack is empty
*/
u_int8_t lightstack_is_empty(lightstack_t* stack);

/**
 * Grab the top of the stack without removing it
*/
u_int16_t lightstack_peek(lightstack_t* stack);

#endif /* LIGHT_STACK_H */
