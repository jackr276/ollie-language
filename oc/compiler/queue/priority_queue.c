/**
 * Author: Jack Robbins
 *
 * This is the implementation file for the associated priority queue header file
*/

#include "priority_queue.h"
#include <stdlib.h>

// Initially the queue size is 50. This is usually enough for most switch statements. Of course
// if a user writes more than 50 cases, it will be accomodated
#define INITIAL_QUEUE_SIZE 50

/**
 * Initialize the priority queue with the default size
*/
priority_queue_t initialize_priority_queue(){
	//Stack allocated
	priority_queue_t queue;

	//We need to reserve the initial space
	queue.heap = calloc(INITIAL_QUEUE_SIZE, sizeof(priority_queue_node_t));

	//Set these values too
	queue.maximum_size = INITIAL_QUEUE_SIZE;
	queue.next_index = 0;

	//And give back a copy
	return queue;
}

/**
 * Insert a node into the priority queue
 *
 * IMPORTANT: Since we calloc'd the entire thing, all of our priorities are set to 
 * 0. As such, we can never actually have a priority of 0(which could happen) as a node
 * priority. This would confuse the system and would be a confusing edge case. As such, every
 * priority has 1 added to it, that way even if it is 0 passed in, it won't be in the system
 */
insertion_status_t priority_queue_enqueue(priority_queue_t* queue, void *ptr, u_int16_t priority){

}




/**
 * Deallocate the priority queue
*/
void deallocate_priority_queue(priority_queue_t* queue){
	//We need to deallocate the heap only here
	free(queue->heap);
	//And we're done
}
