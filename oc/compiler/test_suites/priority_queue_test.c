/**
 * Author: Jack Robbins
 * CI test coverage for the min & max priority queue implementations
*/

//Include both queue types
#include "../utils/queue/max_priority_queue.h"
#include "../utils/queue/min_priority_queue.h"
#include "../utils/constants.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>

static u_int32_t current_node_id;

//A node for testing the priority queue
typedef struct priority_queue_test_node_t priority_queue_test_node_t;

//Just holds an ID and a priority
struct priority_queue_test_node_t{
	int64_t priority;
	u_int32_t node_id;
};


/**
 * Create a dummy test node with a given priority
 */
static priority_queue_test_node_t* create_test_node(int64_t priority){
	//Allocate it
	priority_queue_test_node_t* node = calloc(1, sizeof(priority_queue_test_node_t));

	//Grab the id and increment
	node->node_id = current_node_id;
	current_node_id++;
	
	//Priority is whatever we gave it
	node->priority = priority;

	return node;
}


/**
 * Do everything to test the minimum priority queue
 */
static void test_min_priority_queue(){
	//Create it
	min_priority_queue_t min_queue = min_priority_queue_alloc();

	//Wipe the current node id
	current_node_id = 0;

	//500 test items generated. These will be inserted in backwards, so the min-heap
	//operation will have to work every time
	for(u_int16_t i = 0; i < 500; i++){
		//Create a test node with our given priority
		priority_queue_test_node_t* node = create_test_node(i);

		//Insert it - this is a worst case type deal
		min_priority_queue_enqueue(&min_queue, node, node->priority);
	}

	//Let's dequeue half of them
	for(int16_t i = 0; i < 250; i++){
		//Dequeue it
		priority_queue_test_node_t* node = min_priority_queue_dequeue(&min_queue);

		//Assert that 
		printf("Dequeued node with priority %ld\n", node->priority);

		//Asser that this is the case, the priority should be highest to lowest here
		assert(node->priority == i);
	}

	//Now let's randomly insert some nodes with higher priorities and see where they fall
	for(u_int16_t i = 785; i < 835; i++){
		//Create a test node with our given priority
		priority_queue_test_node_t* node = create_test_node(i);

		//Insert it - this is a worst case type deal
		min_priority_queue_enqueue(&min_queue, node, node->priority);
	}

	//Now let's dequeue the other 250 nodes
	for(u_int16_t i = 250; i < 500; i++){
		//Dequeue it
		priority_queue_test_node_t* node = min_priority_queue_dequeue(&min_queue);

		//Assert that 
		printf("Dequeued node with priority %ld\n", node->priority);

		//Asser that this is the case, the priority should be highest to lowest here
		assert(node->priority == i);
	}

	//Now the minimum node is 785, so let's enqueue some stuff smaller than it
	//Now let's randomly insert some nodes with higher priorities and see where they fall
	//We'll also do duplicate priorities here to see how it's handled
	for(u_int16_t i = 0; i < 10; i++){
		//Create a test node with our given priority
		priority_queue_test_node_t* node = create_test_node(i);

		//Insert it - this is a worst case type deal
		min_priority_queue_enqueue(&min_queue, node, node->priority);

		//Create a test node with our given priority
		priority_queue_test_node_t* duplicate_node = create_test_node(i);

		//Insert it - this is a worst case type deal
		min_priority_queue_enqueue(&min_queue, duplicate_node, duplicate_node->priority);
	}

	//We should now be able to dequeue all of these
	for(u_int16_t i = 0; i < 10; i++){
		//Dequeue it
		priority_queue_test_node_t* node = min_priority_queue_dequeue(&min_queue);

		//Assert that 
		printf("Dequeued node with priority %ld\n", node->priority);

		//Asser that this is the case, the priority should be highest to lowest here
		assert(node->priority == i);

		//Dequeue it again(we had duplicates)
		priority_queue_test_node_t* duplicate_node = min_priority_queue_dequeue(&min_queue);

		//Assert that 
		printf("Dequeued node with priority %ld\n", duplicate_node->priority);

		//Asser that this is the case, the priority should be highest to lowest here
		assert(duplicate_node->priority == i);
	}

	//Now let's dequeue the rest
	for(int16_t i = 785; i < 835; i++){
		//Dequeue it
		priority_queue_test_node_t* node = min_priority_queue_dequeue(&min_queue);

		//Assert that 
		printf("Dequeued node with priority %ld\n", node->priority);

		//Asser that this is the case, the priority should be highest to lowest here
		assert(node->priority == i);
	}

	//It's now empty, so verify that the empty call works
	assert(min_priority_queue_is_empty(&min_queue) == TRUE);
	
	//Now deallocate it
	min_priority_queue_dealloc(&min_queue);
}


/**
 * Do everything to test the maximum priority queue
 */
static void test_max_priority_queue(){
	//Create it
	max_priority_queue_t max_queue = max_priority_queue_alloc();

	//Wipe the current node id
	current_node_id = 0;

	//500 test items generated
	for(u_int16_t i = 0; i < 500; i++){

	}

	//Now deallocate it
	max_priority_queue_dealloc(&max_queue);
}


/**
 * Just a single test runner here with some asserts
*/
int main() {
	//Invoke the min tester first
	test_min_priority_queue();

	//Then the max tester
	test_max_priority_queue();

	return 0;
}
