/**
 * Author: Jack Robbins
 *
 * A priority queue, implemented as a min-heap behind the scenes. Specifically used by Ollie Compiler in
 * the reordering of case statements in switch blocks
*/

#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

//Overall priority queue struct
#include <sys/types.h>
typedef struct priority_queue_t priority_queue_t;
//The nodes in our priority queue
typedef struct priority_queue_node_t priority_queue_node_t;

/**
 * The priority queue's emptiness status
 */
typedef enum{
	PRIORITY_QUEUE_EMPTY,
	PRIORITY_QUEUE_NOT_EMPTY
} priority_queue_status_t;


/**
 * This struct will usually be passed by copy -- it contains a pointer 
 * to the internal heap
*/
struct priority_queue_t{
	//The actual heap that exists in the priority queue
	priority_queue_node_t* heap;
	//The current size
	u_int16_t next_index;
	//The maximum size
	u_int16_t maximum_size;
};

/**
 * Each individual node is stored here. The priority needs to be stored 
 * along with the pointer, which is what makes these nodes necessary
*/
struct priority_queue_node_t{
	//Our priority
	int64_t priority;
	//What is actually in here - usually an AST node,
	//but this could be anything if needed
	void* ptr;
};

/**
 * Initialize the priority queue - returns a copy
 */
priority_queue_t priority_queue_alloc();

/**
 * Insert a node with a given priority into the priority queue. Return
 * an enum that dictates if we needed to reorder
*/
void priority_queue_enqueue(priority_queue_t* queue, void* ptr, int64_t priority);

/**
 * Dequeue from the priority queue
*/
void* priority_queue_dequeue(priority_queue_t* queue);

/**
 * Is the priority queue empty?
*/
priority_queue_status_t priority_queue_is_empty(priority_queue_t* queue);

/**
 * Deallocate the memory of the priority queue
*/
void priority_queue_dealloc(priority_queue_t* queue);

#endif /* PRIORITY_QUEUE_H */
