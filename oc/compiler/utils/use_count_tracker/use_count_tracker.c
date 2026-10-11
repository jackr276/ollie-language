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
 * Retrieve the appropriate pointer for a variable ID. If the page table is too small
 * to support the theoretical page that this ID woudl be in, we will reallocate it. Additionally,
 * if the page for said ID does not exist, we will allocate it. This helper will always return a valid
 * pointer to a page
 */
use_count_tracker_page_t* get_or_create_use_count_page_for_id(use_count_tracker_t* tracker, int32_t requested_id){
	//Get the page index that this ID belongs to
	int32_t page_index = requested_id / USE_COUNT_PAGE_SIZE;

	/**
	 * Case 1 - we do not have enough space to hold the page that this would
	 * in theory belong to. If this is the case we'll need to reallocate the
	 * page table. Since we rely on NULL pointers for pages that need to be
	 * allocated, we will need to 0 out the newly reallocated portion of
	 * the page table
	 */
	if(page_index >= tracker->page_count){
		//Hang onto this for later
		int32_t old_page_count = tracker->page_count;
		
		//Always double this to hopefully avoid more reallocations
		int32_t new_page_count = page_index * 2;

		//Store the new page count and reallocate our page table
		tracker->page_count = new_page_count;
		tracker->page_table = realloc(tracker->page_table, new_page_count * sizeof(use_count_tracker_page_t*));

		//0 out everything new that we've added in the reallocation
		memset(tracker->page_table + old_page_count, 0, sizeof(use_count_tracker_page_t*) * (new_page_count - old_page_count));
	}


	/**
	 * Case 2 - we know that there is enough space for the page table that holds this ID, but now we
	 * need to make sure that the page itself is allocated
	 */
	if(tracker->page_table[page_index] == NULL){
		tracker->page_table[page_index] = calloc(1, sizeof(use_count_tracker_page_t));
	}

	//Give back a valid pointer to this page
	return tracker->page_table[page_index];
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
