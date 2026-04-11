/**
 * Author: Jack Robbins
 *
 * Implementation file for the heap allocated queue data structure. We use a circular queue for this.
 * Conceptually, a circular queue can wrap around itself to avoid the need to shift. We maintain the
 * index of the front and we can always calculate the rear by doing rear = (front + num_elements) % size
 *
 * This queue will dynamically resize as needed, but only upwards. We will never downsize the queue
 * as this is just wasteful. A couple dozen extra bytes allocated is fine so long as it saves us the trouble
 * of reallocating
 */

#include "heap_queue.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
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

	//Front is just the front of the array
	queue.front = 0;

	return queue;
}


/**
 * Dynamically resize the heap queue as needed. This is not
 * as simple as many other data structures because we need to copy the
 * data in the correct order
 *
 * Procedure resize:
 * 	new_capacity = old_capacity * 2
 * 	new_data = allocate new data
 *
 * 	for i in range num_elements:
 *  	int index = (front + i) % old_capacity
 *  	new_data[i] = old_data[index]
 *
 * 	front = 0
 *  capacity = new_capacity
 *  data = new_data
 */
static inline void resize(heap_queue_t* queue){
	//New capacity is double the old one(maintain powers of 2)
	u_int32_t new_capacity = queue->capacity * 2;
	//Allocate a fresh array
	void** new_data = calloc(new_capacity, sizeof(void*));

	/**
	 * Copy over each element in the correct order starting at the
	 * front index of the old queue
	 */
	for(u_int32_t i = 0; i < queue->num_elements; i++){
	 	//Get the index of the "i"th element the old way
	 	int32_t index = (queue->front + i) % queue->capacity;

		//Now new_data[i] = this old data. It will be in the right order
		new_data[i] = queue->data[index];
	}

	//Destroy the old buffer
	free(queue->data);

	//Front is at 0 after our copy
	queue->front = 0;
	//Capacity is new
	queue->capacity = new_capacity;
	//Data is new now too
	queue->data = new_data;
}


/**
 * Enqueue a node into the queue.
 * Algorithm for circular queue enqueue:
 *
 * 	if capacity == num_elements:
 * 		resize
 *
 * 	int rear = (front + num_elements) % capacity
 * 	queue->data[rear] = new_data
 * 	num_elements++
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
	 */
	if(heap_queue->num_elements == heap_queue->capacity){
		resize(heap_queue);
	}

	/**
	 * Calculate the rear index by doing (front + element_count) % capacity. This will wrap
	 * us around the front of the array by doing %capacity which is where the circular name
	 * comes from
	 */
	int32_t rear_index = (heap_queue->front + heap_queue->num_elements) % heap_queue->capacity;

	//Store the pointer at the new index
	heap_queue->data[rear_index] = data;

	//Bump the size for the next go around
	heap_queue->num_elements++;
}


/**
 * Dequeue from the queue(take from the head)
 *
 * Algorithm for a circular queue:
 * 
 * data = queue->underlying[front_index]
 * front_index = (front_index + 1) % capacity
 * num_elements--
 */
void* dequeue(heap_queue_t* heap_queue){
	//Grab the data out first
	void* data = heap_queue->data[heap_queue->front];

	/**
	 * Recompute the front by adding 1 and then using the capacity to perform
	 * our wrap around procedure. This is important to ensure that we aren't
	 * overrunning the bounds of our buffer and is how this queue gets the
	 * circular name
	 */
	heap_queue->front = (heap_queue->front + 1) % heap_queue->capacity;

	//Size went done
	heap_queue->num_elements--;

	//And give back their data
	return data;
}


/**
 * Completely wipe the existing memory of the heap queue. This is
 * done if we wish to reuse it
 */
void heap_queue_clear(heap_queue_t* heap_queue){
	//Wipe out these two values
	heap_queue->front = 0;
	heap_queue->num_elements = 0;

	//Wipe out the entire pointer array
	memset(heap_queue->data, 0, heap_queue->capacity * sizeof(void*));
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
