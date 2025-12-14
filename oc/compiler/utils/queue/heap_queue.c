/**
 * Author: Jack Robbins
 *
 * Implementation file for the heap allocated queue datastructure util
*/

#include "heap_queue.h"
#include <stdlib.h>
#include <sys/types.h>
//For the TRUE and FALSE constants
#include "../constants.h"

/**
 * Allocate a heap queue structure. The overall
 * heap structure will be allocated to the stack
 */
heap_queue_t heap_queue_alloc(){
	//First we allocate it
	heap_queue_t queue;

	//We currently have nothing
	queue.num_nodes = 0;

	//These are both initially NULL
	queue.head = NULL;
	queue.tail = NULL;

	//And we're done, just return it
	return queue;
}


/**
 * Deallocate an entire heap queue structure
 *
 * NOTE: Only the nodes are freed, not the underlying data
 */
void heap_queue_dealloc(heap_queue_t* heap_queue){
	//Grab a cursor to use
	heap_queue_node_t* cursor = heap_queue->head;
	heap_queue_node_t* temp;

	//We run through the whole list freeing node by node
	while(cursor != NULL){
		temp = cursor;
		//Advance cursor
		cursor = cursor->next;
		//Free this
		free(temp);
	}
}


/**
 * Enqueue a node into the queue
 */
void enqueue(heap_queue_t* heap_queue, void* data){
	//If the data is NULL, we just don't add anything
	if(data == NULL || heap_queue == NULL){
		return;
	}

	//To enqueue, we first need a new node
	heap_queue_node_t* node = calloc(1, sizeof(heap_queue_node_t));

	//This node stores our data
	node->data = data;
	
	//Special case -- this is the very first node
	if(heap_queue->head == NULL){
		heap_queue->head = node;
		heap_queue->tail = node;
	//Otherwise, we have to add to the end
	} else {
		//Add this in
		heap_queue->tail->next = node;
		//He now is the tail
		heap_queue->tail = node;
	}

	//We have one more node, so
	heap_queue->num_nodes++;
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
 * Determine if the heap is empty
 */
u_int8_t queue_is_empty(heap_queue_t* heap_queue){
	return heap_queue->num_nodes == 0 ? TRUE : FALSE;
}
