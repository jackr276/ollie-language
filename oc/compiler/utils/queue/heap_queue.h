/**
 * A generic queue that stores heap pointers. This will be used for breadth-first traversal of
 * CFG graph nodes
*/

//Include guards
#ifndef HEAP_QUEUE_H
#define HEAP_QUEUE_H

#include <sys/types.h>

/**
 * The capacity starts off at 16 and is doubled every time, meaning
 * that we've always got a power of 2 as our queue size
 */
#define HEAP_QUEUE_DEFAULT_CAPACITY 16

//The overall structure itself
typedef struct heap_queue_t heap_queue_t;


/**
 * The heap queue uses a circular queue data structure. To maintain this
 * we'll need the data itself, a front index value, a capacity and a size
 */
struct heap_queue_t{
	//Data array
	void* data;
	//Rear index
	int32_t front;
	//How many elements are there in the queue
	u_int32_t num_elements;
	//Maximum capacity(this is what is resized)
	u_int32_t capacity;
};

/**
 * Allocate a heap queue structure. The overall
 * control structure will be allocated to the stack
 */
heap_queue_t heap_queue_alloc();

/**
 * Deallocate an entire heap queue structure
 *
 * NOTE: Only the nodes are freed, not the underlying data
 */
void heap_queue_dealloc(heap_queue_t* heap_queue);

/**
 * Enqueue a node into the queue
 */
void enqueue(heap_queue_t* heap_queue, void* data);

/**
 * Dequeue a node from the queue
 *
 * Returns NULL if there was an error or empty queue
 */
void* dequeue(heap_queue_t* heap_queue);

/**
 * Determine if the heap is empty. Returns 1 if empty, 0 if not
 */
u_int8_t queue_is_empty(heap_queue_t* heap_queue);

#endif /* HEAP_QUEUE_H */
