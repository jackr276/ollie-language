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
 * Retrieve the use count for a given ID
 */
inline u_int32_t get_use_count_by_id(u_int32_t id){

}

/**
 * Increment the use count for a given ID
 */
inline void increment_use_count(u_int32_t id){

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
