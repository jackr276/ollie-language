/**
 * A generic queue that stores heap pointers. This will be used for breadth-first traversal of
 * CFG graph nodes
*/

//Include guards
#ifndef HEAP_QUEUE_H
#define HEAP_QUEUE_H

#include <sys/types.h>

//The overall structure itself
typedef struct heap_queue_t heap_queue_t;
//A struct for each heapqueue node
typedef struct heap_queue_node_t heap_queue_node_t;

//The overall heap struct
struct heap_queue_t{
	//The head and tail
	heap_queue_node_t* head;
	heap_queue_node_t* tail;
};

//Heap queue node struct
struct heap_queue_node_t{
	//The next node
	heap_queue_node_t* next;
	//The data that we store
	void* data;
};


/**
 * Allocate a heap queue structure
 */
heap_queue_t* heap_queue_alloc();

/**
 * Deallocate an entire heap queue structure
 */
void heap_queue_dealloc(heap_queue_t* heap_queue);

/**
 * Enqueue a node into the queue
 */
void enqueue(heap_queue_t* heap_queue, void* data);

/**
 * Dequeue a node from the queue
 */
void* dequeue(heap_queue_t* heap_queue);

/**
 * Determine if the heap is empty
 */
u_int8_t heap_is_empty(heap_queue_t* heap_queue);

#endif /* HEAP_QUEUE_H */
