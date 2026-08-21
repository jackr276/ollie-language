/**
 * Author: Jack Robbins
 * This file implements the APIs defined in the header file of the same name for the
 * variable use count tracker
 *
 * The use count tracker only comes into play inside of the instruction selector. It will
 * only be included in builds that require the instruction selector
 */

#include "use_count_tracker.h"
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/**
 * Allocate the underlying data structures in the use count tracker
 */
use_count_tracker_t use_count_tracker_alloc(u_int32_t initial_variable_count){
	use_count_tracker_t tracker;

	//Populate with our starting variable account
	tracker.variable_count = initial_variable_count;

	/**
	 * Allocate our underlying array. This array is indexed
	 * by the variable IDs and has the use counts as its values
	 */
	tracker.map = calloc(sizeof(u_int16_t), initial_variable_count);

	return tracker;
}


/**
 * Perform a dynamic resize on the use count tracker based on the ID that was requested. To
 * be safe, we will always reallocate with double what was requested
 */
static inline void perform_dynamic_resize(use_count_tracker_t* tracker, u_int32_t requested_id){
	//Nothing to worry about here
	if(tracker->variable_count < requested_id){
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
	 */
	memset(tracker->map + old_variable_count, 0, sizeof(u_int16_t) * tracker->variable_count - old_variable_count);
}


/**
 * Retrieve the use count for a given ID
 */
inline u_int32_t get_use_count_by_id(use_count_tracker_t* tracker, u_int32_t id){

}


/**
 * Increment the use count for a given ID
 */
inline void increment_use_count(use_count_tracker_t* tracker, u_int32_t id){

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
