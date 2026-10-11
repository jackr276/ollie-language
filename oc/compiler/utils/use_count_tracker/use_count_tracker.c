/**
 * Author: Jack Robbins
 * This file implements the APIs defined in the header file of the same name for the
 * variable use count tracker
 *
 * The use count tracker only comes into play inside of the instruction selector. It will
 * only be included in builds that require the instruction selector
 */

#include "use_count_tracker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>


/**
 * Allocate the underlying data structures in the use count tracker. The initial
 * variable count is used to estimate a good number of pages to start off with
 * to avoid excessive reallocations, though reallocations of the page table
 * are always a possibility
 */
use_count_tracker_t use_count_tracker_alloc(int32_t initial_variable_count){
	//Stack allocate
	use_count_tracker_t tracker;

	/**
	 * Get the estimated page count by dividing the initial variable count
	 * by the page size. If the program is too small to even justify a full page, we
	 * will default to a set number of pages to be safe
	 */
	int32_t estimated_page_count = initial_variable_count / USE_COUNT_PAGE_SIZE;
	int32_t initial_page_count = estimated_page_count > 0 ? estimated_page_count : DEFAULT_NUM_USE_COUNT_PAGES;

	//Store the initial page count in the tracker and allocate the page table
	tracker.page_count = initial_page_count;
	tracker.page_table = calloc(initial_page_count, sizeof(use_count_tracker_page_t*));
	return tracker;
}


/**
 * Perform a dynamic resize on the use count tracker based on the ID that was requested. To
 * be safe, we will always reallocate with double what was requested
 *
 * It is smart to call this before populating use counts on any new function, just to make sure 
 * you have enough space
 */
void use_count_tracker_dynamic_resize_for_id(use_count_tracker_t* tracker, u_int32_t requested_id){
	//Nothing to worry about here
	if(tracker->variable_count > requested_id){
		return;
	}

	//Cache the old variable count and reup it
	u_int32_t old_variable_count = tracker->variable_count;
	tracker->variable_count = requested_id * 2;

	//Reallocate our map with the new count
	tracker->map = realloc(tracker->map, sizeof(u_int16_t) * tracker->variable_count);

	/**
	 * We need to make sure that this is all set to 0's. So, grab a pointer to the new 
	 * region by skipping over all of the variables that already exist, and set all of
	 * that memory to be 0's
	 *
	 * Do a memset where we bump the pointer up to past the old region, and wipe the entire
	 * thing out starting after the already populated region by setting all the bytes to
	 * be 0
	 */
	memset(tracker->map + old_variable_count, 0, sizeof(u_int16_t) * (tracker->variable_count - old_variable_count));
}


/**
 * Dump the use count for every single ID that currently exists
 * in the tracker. This is purely meant for debugging
 */
void dump_use_counts(use_count_tracker_t* tracker){
	printf("============ Use count by variable ID ============\n");
	for(int32_t i = 0; i < tracker->page_count; i++){
		use_count_tracker_page_t* page = tracker->page_table[i];

		if(page != NULL){
			printf("PAGE %d:\n", i);

			for(int32_t j = 0; j < USE_COUNT_PAGE_SIZE; j++){
				printf("ID %d: Use count %d\n", j, page->use_counts[j]);
			}

		} else {
			printf("PAGE %d: NULL\n", i);
		}
	}

	printf("============ Use count by variable ID ============\n");
}


/**
 * Deallocate all memory allocated by the provided use
 * count tracker
 */
void use_count_tracker_dealloc(use_count_tracker_t* tracker){
	//Free all of the pages in the page table
	for(int32_t i = 0; i < tracker->page_count; i++){
		free(tracker->page_table[i]);
	}

	//Now free the page table and 0 out the count
	free(tracker->page_table);
	tracker->page_count = 0;
}
