/**
 * Author: Jack Robbins
 *
 * This is the implementation file for the associated priority queue header file
*/

#include "max_priority_queue.h"
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>

//For access to TRUE and FALSE
#include "../constants.h"

// Initially the queue size is 10. This is usually enough for most switch statements. Of course
// if a user writes more than 10 cases, it will be accomodated
#define INITIAL_QUEUE_SIZE 10

/**
 * Initialize the priority queue with the default size
*/
max_priority_queue_t max_priority_queue_alloc(){
	//Stack allocated
	max_priority_queue_t queue;

	//We need to reserve the initial space
	queue.heap = calloc(INITIAL_QUEUE_SIZE, sizeof(max_priority_queue_node_t));

	//Set these values too
	queue.maximum_size = INITIAL_QUEUE_SIZE;
	queue.next_index = 0;

	//And give back a copy
	return queue;
}


/**
 * Get the parent index of something in the heap. This is
 * always half of one less than the index
 */
static u_int16_t get_parent_index(u_int16_t index){
	return (index - 1 ) / 2;
}


/**
 * Swap the two given indices. Remember that this heap isn't a heap of
 * pointers, so it's not as simple as swapping pointers
 */
static void swap(max_priority_queue_t* queue, u_int16_t index1, u_int16_t index2){
	//Grab out index 1
	max_priority_queue_node_t temp = queue->heap[index1];

	//Put index 2 in 1's spot
	queue->heap[index1] = queue->heap[index2];

	//Then put 1 back in 2's spot
	queue->heap[index2] = temp;
}


/**
 * Generic max-heapify operation. This will recursively
 * "up-heapify" because we start at the front and go forwards
 *
 * When we are done, the root will be the highest value
 */
static void max_heapify(max_priority_queue_t* queue, u_int16_t index){
	//Initially set largest to be what we're given
	u_int16_t largest_index = index;

	//Get the left and right children here
	u_int16_t left_child_index = index * 2 + 1;
	u_int16_t right_child_index = index * 2 + 2;

	/**
	 * If the left child is actually there(first condition) and it's priority
	 * is greater than the "largest" index, it is our new largest 
	 */
	if(left_child_index < queue->next_index && 
		queue->heap[left_child_index].priority > queue->heap[largest_index].priority){
		largest_index = left_child_index;
	}

	/**
	 * If the right child is actually there(first condition) and it's priority
	 * is greater than the "largest" index, it is our new largest 
	 */
	if(right_child_index < queue->next_index && 
		queue->heap[right_child_index].priority > queue->heap[largest_index].priority){
		largest_index = right_child_index;
	}

	//If we found something larger than the index, we must swap
	if(largest_index != index){
		//Swap the index and smallest index
		swap(queue, index, largest_index);

		//Recursively max-heapify with the new 
		//largest index
		max_heapify(queue, largest_index);
	}
}


/**
 * Insert a node into the priority queue
 *
 * IMPORTANT: Since we calloc'd the entire thing, all of our priorities are set to 
 * 0. As such, we can never actually have a priority of 0(which could happen) as a node
 * priority. This would confuse the system and would be a confusing edge case. As such, every
 * priority has 1 added to it, that way even if it is 0 passed in, it won't be in the system
 */
void max_priority_queue_enqueue(max_priority_queue_t* queue, void *ptr, int64_t priority){
	//Automatic resize if needed
	if(queue->next_index == queue->maximum_size){
		//Double it
		queue->maximum_size *= 2;
		//Realloc
		queue->heap = realloc(queue->heap, sizeof(max_priority_queue_node_t) * queue->maximum_size);
	}

	//See the top explanation for why we do this
	u_int16_t adjusted_priority = priority + 1;

	//Insert the value at the very end
	queue->heap[queue->next_index].priority = adjusted_priority;
	queue->heap[queue->next_index].ptr = ptr;

	//We'll need a reference to this to min-heapify
	u_int32_t current_index = queue->next_index;

	//Increment this for the next go around
	queue->next_index++;

	//So long as we're in valid bounds and the child/parent are backwards
	while(current_index > 0 && 
		queue->heap[get_parent_index(current_index)].priority < queue->heap[current_index].priority){
		//Swap the values
		swap(queue, get_parent_index(current_index), current_index);

		//Update this index to be it's parent
		current_index = get_parent_index(current_index);
	}
}

/**
 * Dequeue from the priority queue
 */
void* max_priority_queue_dequeue(max_priority_queue_t* queue){
	//Save the pointer
	void* dequeued = queue->heap[0].ptr;

	//Put the last element in the front to prime the heap
	queue->heap[0] = queue->heap[queue->next_index - 1];
 
	//Decrement the next index
	queue->next_index--;

	//Minheapify with 0 as the seed to maintain the maxheap property
	max_heapify(queue, 0);

	//Give this pointer back
	return dequeued;
}


/**
 * Simply return if the next index is 0
 */
u_int8_t max_priority_queue_is_empty(max_priority_queue_t* queue){
	return queue->next_index == 0 ? TRUE : FALSE;
}


/**
 * Deallocate the priority queue
*/
void max_priority_queue_dealloc(max_priority_queue_t* queue){
	//We need to deallocate the heap only here
	free(queue->heap);
	//And we're done
}
