/**
 * Author: Jack Robbins
 *
 * A priority queue, implemented as a min-heap behind the scenes
 *
 * NOTE: In this version of the priority queue, the item with the minimum priority
 * comes off first
*/

#ifndef MIN_PRIORITY_QUEUE_H
#define MIN_PRIORITY_QUEUE_H
#include <sys/types.h>

//Overall priority queue struct
typedef struct min_priority_queue_t min_priority_queue_t;
//The nodes in our priority queue
typedef struct min_priority_queue_node_t min_priority_queue_node_t;

/**
 * This struct will usually be passed by copy -- it contains a pointer 
 * to the internal heap
*/
struct min_priority_queue_t {
	//The actual heap that exists in the priority queue
	min_priority_queue_node_t* heap;
	//The current size
	u_int16_t next_index;
	//The maximum size
	u_int16_t maximum_size;
};

/**
 * Each individual node is stored here. The priority needs to be stored 
 * along with the pointer, which is what makes these nodes necessary
 *
 * The priority is as follows -> lower is higher priority
*/
struct min_priority_queue_node_t{
	//Our priority
	int64_t priority;
	//What is actually in here - usually an AST node,
	//but this could be anything if needed
	void* ptr;
};

/**
 * Initialize the priority queue - returns a copy
 */
min_priority_queue_t min_priority_queue_alloc();

/**
 * Insert a node with a given priority into the priority queue
*/
void min_priority_queue_enqueue(min_priority_queue_t* queue, void* ptr, int64_t priority);

/**
 * Dequeue from the priority queue
*/
void* min_priority_queue_dequeue(min_priority_queue_t* queue);

/**
 * Is the priority queue empty?
*/
u_int8_t min_priority_queue_is_empty(min_priority_queue_t* queue);

/**
 * Deallocate the memory of the priority queue
*/
void min_priority_queue_dealloc(min_priority_queue_t* queue);

#endif /* MIN_PRIORITY_QUEUE_H */
