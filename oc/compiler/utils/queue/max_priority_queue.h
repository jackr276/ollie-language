/**
 * Author: Jack Robbins
 *
 * A max priority queue, implemented as a max-heap behind the scenes
 *
 * NOTE: In this version of the priority queue, items with higher priority 
 * come off first
*/

#ifndef MAX_PRIORITY_QUEUE_H
#define MAX_PRIORITY_QUEUE_H
#include <sys/types.h>

//Overall priority queue struct
typedef struct max_priority_queue_t max_priority_queue_t;
//The nodes in our priority queue
typedef struct max_priority_queue_node_t max_priority_queue_node_t;

/**
 * This struct will usually be passed by copy -- it contains a pointer 
 * to the internal heap
*/
struct max_priority_queue_t {
	//The actual heap that exists in the priority queue
	//Internally implemented as an array
	max_priority_queue_node_t* heap;
	//The current size
	u_int16_t next_index;
	//The maximum size
	u_int16_t maximum_size;
};

/**
 * Each individual node is stored here. The priority needs to be stored 
 * along with the pointer, which is what makes these nodes necessary
 *
 * The priority is as follows -> higher is higher priority
*/
struct max_priority_queue_node_t{
	//Our priority
	int64_t priority;
	//What is actually in here - usually an AST node,
	//but this could be anything if needed
	void* ptr;
};

/**
 * Initialize the priority queue - returns a copy
 */
max_priority_queue_t max_priority_queue_alloc();

/**
 * Insert a node with a given priority into the priority queue. Return
 * an enum that dictates if we needed to reorder
*/
void max_priority_queue_enqueue(max_priority_queue_t* queue, void* ptr, int64_t priority);

/**
 * Dequeue from the priority queue
*/
void* priority_queue_dequeue(max_priority_queue_t* queue);

/**
 * Is the priority queue empty?
*/
u_int8_t priority_queue_is_empty(max_priority_queue_t* queue);

/**
 * Deallocate the memory of the priority queue
*/
void priority_queue_dealloc(max_priority_queue_t* queue);

#endif /* MAX_PRIORITY_QUEUE_H */
