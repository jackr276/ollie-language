/**
 * Author: Jack Robbins
 * CI test coverage for the min & max priority queue implementations
*/

//Include both queue types
#include "../utils/queue/max_priority_queue.h"
#include "../utils/queue/min_priority_queue.h"
#include <sys/types.h>

/**
 * Do everything to test the minimum priority queue
 */
static void test_min_priority_queue(){
	//Create it
	min_priority_queue_t min_queue = min_priority_queue_alloc();


	//500 test items generated
	for(u_int16_t i = 0; i < 500; i++){

	}


	//Now deallocate it
	min_priority_queue_dealloc(&min_queue);
}


/**
 * Do everything to test the maximum priority queue
 */
static void test_max_priority_queue(){
	//Create it
	max_priority_queue_t max_queue = max_priority_queue_alloc();


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
