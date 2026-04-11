/**
 * Author: Jack Robbins
 *
 * Implementation file for the heap allocated queue datastructure util
*/

#include "heap_queue.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
//For the TRUE and FALSE constants
#include "../constants.h"

/**
 * Allocate a heap queue structure. The overall
 * heap structure will be allocated to the stack
 */
heap_queue_t heap_queue_alloc(){
	//Stack allocate the queue
	heap_queue_t queue;

	//Initially no elements
	queue.num_elements = 0;

	//Use the default capacity
	queue.capacity = HEAP_QUEUE_DEFAULT_CAPACITY;

	//Dynamically allocate the underlying array
	queue.data = calloc(queue.capacity, sizeof(void*));

	//Front being -1 is a flag that we're 0
	queue.front = -1;

	return queue;
}


/**
 * Enqueue a node into the queue
 */
void enqueue(heap_queue_t* heap_queue, void* data){
	//Fail out if this happens
	if(data == NULL){
		fprintf(stderr, "Attempt to insert NULL into a heap queue");
		exit(1);
	}

	/**
	 * Dynamic resize condition - we will overflow the queue if we do this
	 * so we need to resize
	 *
	 * TODO RESIZE ISN'T so simple
	 */
	if(heap_queue->num_elements == heap_queue->capacity){
	}


}


/**
 * Dequeue from the queue(take from the head)
 *
 * Returns NULL if there was an error or empty queue
*/
void* dequeue(heap_queue_t* heap_queue){
	//Let's just check to save ourselves here
	if(heap_queue == NULL || heap_queue->head == NULL){
		return NULL;
	}

	//Grab a reference to the head
	heap_queue_node_t* head = heap_queue->head;

	//Grab the data
	void* data = head->data;
	
	//Advance this up now
	heap_queue->head = head->next;

	//Free the old head
	free(head);

	//We've one less node now
	heap_queue->num_nodes--;

	//And give back the data
	return data;
}


/**
 * Determine if the queue is empty
 */
u_int8_t queue_is_empty(heap_queue_t* heap_queue){
	return heap_queue->num_elements == 0 ? TRUE : FALSE;
}


/**
 * Deallocate the heap queue data structure
 */
void heap_queue_dealloc(heap_queue_t* heap_queue){
	//Free the data
	free(heap_queue->data);

	//Reset everything else
	heap_queue->front = -1;
	heap_queue->capacity = 0;
	heap_queue->num_elements = 0;
}
