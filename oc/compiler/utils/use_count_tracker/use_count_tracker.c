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
 * Allocate the underlying data structures in the use count tracker
 */
use_count_tracker_t use_count_tracker_alloc(u_int32_t initial_variable_count){
	//Stack allocate it
	use_count_tracker_t tracker;

	//Populate with our starting variable account
	tracker.variable_count = initial_variable_count;

	/**
	 * Allocate our underlying array. This array is indexed
	 * by the variable IDs and has the use counts as its values
	 */
	tracker.map = calloc(initial_variable_count, sizeof(u_int16_t));

	return tracker;
}


/**
 * Perform a dynamic resize on the use count tracker based on the ID that was requested. To
 * be safe, we will always reallocate with double what was requested
 */
static inline void perform_dynamic_resize(use_count_tracker_t* tracker, u_int32_t requested_id){
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
 * Retrieve the use count for a given ID
 */
u_int32_t get_use_count_by_id(use_count_tracker_t* tracker, u_int32_t id){
	//Do this if needed
	perform_dynamic_resize(tracker, id);

	//Give back whatever the use count it
	return tracker->map[id];
}


/**
 * Increment the use count for a given ID
 */
void increment_use_count(use_count_tracker_t* tracker, u_int32_t id){
	//Do this if needed
	perform_dynamic_resize(tracker, id);

	//Bump the use count for our given id
	(tracker->map[id])++;
}


/**
 * Decrement the use count for a given ID. If the use count is already
 * at 0, we will never go negative and will stay at 0
 */
void decrement_use_count(use_count_tracker_t* tracker, u_int32_t id){
	//Do this if needed
	perform_dynamic_resize(tracker, id);

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
 * Clear out all of the use counts and start fresh. This is really
 * just a simple memory wipe
 */
void reset_all_use_counts(use_count_tracker_t* tracker){
	memset(tracker->map, 0, tracker->variable_count * sizeof(u_int16_t));
}


/**
 * Dump the use count for every single ID that currently exists
 * in the tracker. This is purely meant for debugging
 */
void dump_use_counts(use_count_tracker_t* tracker){
	printf("============ Use count by variable ID ============\n");
	for(u_int32_t i = 0; i < tracker->variable_count; i++){
		printf("ID %d: Use count %d\n", i, tracker->map[i]);
	}

	printf("============ Use count by variable ID ============\n");
}


/**
 * Deallocate the underlying data structures in the use count tracker
 */
void use_count_tracker_dealloc(use_count_tracker_t* tracker){
	//Deallocate the underlying array
	free(tracker->map);

	//Set this to 0 as a warning
	tracker->variable_count = 0;
}
