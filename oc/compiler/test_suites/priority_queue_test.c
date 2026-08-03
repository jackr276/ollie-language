/**
 * Author: Jack Robbins
 * CI test coverage for the queue, min & max priority queue implementations
*/

//Include all queue types
#include "../utils/queue/max_priority_queue.h"
#include "../utils/queue/min_priority_queue.h"
#include "../utils/queue/heap_queue.h"
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
 * Do everything to test the normal heap queue. There is no priority to test here
 */
static void test_queue(){
	//Allocate the heap queue
	heap_queue_t queue = heap_queue_alloc();

	//Run through and add them all
	for(int32_t i = 0; i < 900; i++){
		int* to_be_added = calloc(1, sizeof(int));

		//Give this the index value
		*to_be_added = i;

		enqueue(&queue, to_be_added);
	}

	//It should not be empty
	assert(queue_is_empty(&queue) == FALSE);

	//Enqueue the last int here
	int* last_int = calloc(1, sizeof(int));
	*last_int = 900;

	//Verify that the last int is not in there
	assert(heap_queue_contains(&queue, last_int) == FALSE);
	
	//Enqueue it
	enqueue(&queue, last_int);

	//Verify that the last int is in there now
	assert(heap_queue_contains(&queue, last_int) == TRUE);

	//Now run through and verify that they all come out
	for(int32_t i = 0; i < 900; i++){
		int* dequeued = dequeue(&queue);

		//Verify that they equal the right thing
		assert(*dequeued == i);
	}

	//One final dequeue to get the last int out
	dequeue(&queue);

	//Now we shouldn't have the last int in there
	assert(heap_queue_contains(&queue, last_int) == FALSE);

	//It should be empty
	assert(queue_is_empty(&queue) == TRUE);

	//When we're done deallocate
	heap_queue_dealloc(&queue);
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

	//It's not empty, so verify that the empty call works
	assert(min_priority_queue_is_empty(&min_queue) == FALSE);

	//Let's dequeue half of them
	for(int16_t i = 0; i < 250; i++){
		//Dequeue it
		priority_queue_test_node_t* node = min_priority_queue_dequeue(&min_queue);

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

		//Asser that this is the case, the priority should be highest to lowest here
		assert(node->priority == i);

		//Dequeue it again(we had duplicates)
		priority_queue_test_node_t* duplicate_node = min_priority_queue_dequeue(&min_queue);

		//Asser that this is the case, the priority should be highest to lowest here
		assert(duplicate_node->priority == i);
	}

	//Now let's dequeue the rest
	for(int16_t i = 785; i < 835; i++){
		//Dequeue it
		priority_queue_test_node_t* node = min_priority_queue_dequeue(&min_queue);

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

	//500 test items generated. These will be inserted in backwards, so the min-heap
	//operation will have to work every time
	for(int16_t i = 500; i >= 0; i--){
		//Create a test node with our given priority
		priority_queue_test_node_t* node = create_test_node(i);

		//Insert it - this is a worst case type deal
		max_priority_queue_enqueue(&max_queue, node, node->priority);
	}

	//Let's dequeue half of them and see if our order worked
	for(int16_t i = 500; i >= 250; i--){
		//Dequeue it
		priority_queue_test_node_t* node = max_priority_queue_dequeue(&max_queue);

		//Asser that this is the case, the priority should be highest to lowest here
		assert(node->priority == i);
	}

	//It's not empty, so verify that the empty call works
	assert(max_priority_queue_is_empty(&max_queue) == FALSE);

	//Now let's randomly insert some nodes with lower priorities and see where they fall
	for(int16_t i = 835; i >= 785; i--){
		//Create a test node with our given priority
		priority_queue_test_node_t* node = create_test_node(i);

		//Insert it - this is a worst case type deal
		max_priority_queue_enqueue(&max_queue, node, node->priority);
	}

	//Let's now verify that these are the ones which come off first
	//Let's dequeue half of them and see if our order worked
	for(int16_t i = 835; i >= 785; i--){
		//Dequeue it
		priority_queue_test_node_t* node = max_priority_queue_dequeue(&max_queue);

		//Asser that this is the case, the priority should be highest to lowest here
		assert(node->priority == i);
	}

	//Now the minimum node is now 249, so let's enqueue some stuff higher than it in
	//We'll also do duplicate priorities here to see how it's handled
	for(u_int16_t i = 380; i < 390; i++){
		//Create a test node with our given priority
		priority_queue_test_node_t* node = create_test_node(i);

		//Insert it - this is a worst case type deal
		max_priority_queue_enqueue(&max_queue, node, node->priority);

		//Create a test node with our given priority
		priority_queue_test_node_t* duplicate_node = create_test_node(i);

		//Insert it - this is a worst case type deal
		max_priority_queue_enqueue(&max_queue, duplicate_node, duplicate_node->priority);
	}

	//We should now be able to dequeue all of these
	for(int16_t i = 389; i >= 380; i--){
		//Dequeue it
		priority_queue_test_node_t* node = max_priority_queue_dequeue(&max_queue);

		//Asser that this is the case, the priority should be highest to lowest here
		assert(node->priority == i);

		//Dequeue it again(we had duplicates)
		priority_queue_test_node_t* duplicate_node = max_priority_queue_dequeue(&max_queue);

		//Asser that this is the case, the priority should be highest to lowest here
		assert(duplicate_node->priority == i);
	}

	//Let's now dequeue the rest of it
	for(int16_t i = 249; i >= 0; i--){
		//Dequeue it
		priority_queue_test_node_t* node = max_priority_queue_dequeue(&max_queue);

		//Asser that this is the case, the priority should be highest to lowest here
		assert(node->priority == i);
	}

	//It's now empty so let's verify that this works
	assert(max_priority_queue_is_empty(&max_queue) == TRUE);

	//Now deallocate it
	max_priority_queue_dealloc(&max_queue);
}


/**
 * Just a single test runner here with some asserts
*/
int main() {
	printf("===================== Testing regular queue =======================\n");
	test_queue();
	printf("SUCCESS\n");

	printf("===================== Testing min priority queue =======================\n");
	test_min_priority_queue();
	printf("SUCCESS\n");

	printf("===================== Testing max priority queue =======================\n");
	test_max_priority_queue();
	printf("SUCCESS\n");

	return 0;
}
