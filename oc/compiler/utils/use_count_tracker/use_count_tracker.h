/**
 * Author: Jack Robbins
 * This header file defines all of the APIs and data structures for the 
 * three_addr_var_t use count tracker
 *
 * This header file contains API definitions that are implemented in the use_count_tracker.c
 * file but also contains several simple inlined helpers
 *
 * The use count tracker, under the hood, is a paged sparse array structure with a relatively
 * large page size. The large page size means that most individual functions will fit into one
 * page, allowing us to benefit from cache locality, while being small enough to prevent massive
 * chunks of memory from being hogged
 */

#ifndef USE_COUNT_TRACKER_H
#define USE_COUNT_TRACKER_H

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>

/**
 * The use count page size will be quite large because each record is 
 * just an int16_t, so overall each page will be 8kb in size
 */
#define USE_COUNT_PAGE_SIZE 4096

/**
 * The default number of pages in the event that the program
 * is too small to even fill up a full page
 */
#define DEFAULT_NUM_USE_COUNT_PAGES 2

/**
 * The two structs that we have are a page struct that contains 
 * a bunch of records in a contiguous fixed-size array and an overall
 * use_count_tracker struct that contains the page table
 */
typedef struct use_count_tracker_page_t use_count_tracker_page_t;
typedef struct use_count_tracker_t use_count_tracker_t;


/**
 * Each page is simply a fixed size 4096 record
 * array. Use counts are keyed by variable ID so there
 * is no need to store that along with the count
 */
struct use_count_tracker_page_t {
	int16_t use_counts[USE_COUNT_PAGE_SIZE];
};


/**
 * The actual use count tracker itself contains
 * a dynamically resizeable array of pointers to
 * each page and a page count
 */
struct use_count_tracker_t {
	use_count_tracker_page_t** page_table;
	int32_t page_count;
};

//================================ Non-Inlined Functions ======================================
/**
 * Allocate the underlying data structures in the use count tracker. The initial
 * variable count is used to estimate a good number of pages to start off with
 * to avoid excessive reallocations, though reallocations of the page table
 * are always a possibility
 */
use_count_tracker_t use_count_tracker_alloc(int32_t initial_variable_count);

/**
 * Retrieve the appropriate pointer for a variable ID. If the page table is too small
 * to support the theoretical page that this ID woudl be in, we will reallocate it. Additionally,
 * if the page for said ID does not exist, we will allocate it. This helper will always return a valid
 * pointer to a page
 */
use_count_tracker_page_t* get_use_count_page_for_id(use_count_tracker_t* tracker, int32_t requested_id);

/**
 * Dump the use count for every single ID that currently exists
 * in the tracker. This is purely meant for debugging
 */
void dump_use_counts(use_count_tracker_t* tracker);

/**
 * Deallocate all memory allocated by the provided use
 * count tracker
 */
void use_count_tracker_dealloc(use_count_tracker_t* tracker);
//================================ Non-Inlined Functions ======================================
//================================ Inlined Utility Functions ==================================
/**
 * Retrieve the use count for a given ID
 */
static inline u_int32_t get_use_count_by_id(use_count_tracker_t* tracker, u_int32_t id){
	//Perform the dynamic resize if needed
	if(id >= tracker->variable_count){
		use_count_tracker_dynamic_resize_for_id(tracker, id);
	}

	//Get the ID out
	return tracker->map[id];
}


/**
 * Increment the use count for a given ID
 */
static inline void increment_use_count(use_count_tracker_t* tracker, u_int32_t id){
	//Perform the dynamic resize if needed
	if(id >= tracker->variable_count){
		use_count_tracker_dynamic_resize_for_id(tracker, id);
	}

	(tracker->map[id])++;
}


/**
 * Decrement the use count for a given ID. If the use count is already
 * at 0, we will never go negative and will stay at 0
 */
static inline void decrement_use_count(use_count_tracker_t* tracker, u_int32_t id){
	//Perform the dynamic resize if needed
	if(id >= tracker->variable_count){
		use_count_tracker_dynamic_resize_for_id(tracker, id);
	}

	/**
	 * Make sure that we never end up with a negative
	 * use count for our variables here
	 */
	if(tracker->map[id] > 0){
		(tracker->map[id])--;
	} else {
		tracker->map[id] = 0;
	}
}


/**
 * Clear out all of the use counts and start fresh
 */
static inline void reset_all_use_counts(use_count_tracker_t* tracker){
	memset(tracker->map, 0, sizeof(u_int16_t) * tracker->variable_count);
}
//================================ Inlined Utility Functions ==================================
#endif /* USE_COUNT_TRACKER_H */
