/**
 * Author: Jack Robbins
 * This header file defines all of the APIs and data structures for the 
 * three_addr_var_t use count tracker
 */

#ifndef USE_COUNT_TRACKER_H
#define USE_COUNT_TRACKER_H

#include <sys/types.h>

//Predeclare the struct itself
typedef struct use_count_tracker_t use_count_tracker_t;

/**
 * Very simple structure. All we really need is the array
 * and then the overall count. 32 bit integers for the count
 * may be overkill but it's ok I'm not sweating it
 */
struct use_count_tracker_t {
	u_int16_t* map;
	u_int32_t variable_count;
};


/**
 * Allocate the underlying data structures in the use count tracker
 */
use_count_tracker_t use_count_tracker_alloc(u_int32_t initial_variable_count);

/**
 * Retrieve the use count for a given ID
 */
u_int32_t get_use_count_by_id(use_count_tracker_t* tracker, u_int32_t id);

/**
 * Increment the use count for a given ID
 */
void increment_use_count(use_count_tracker_t* tracker, u_int32_t id);

/**
 * Decrement the use count for a given ID. If the use count is already
 * at 0, we will never go negative and will stay at 0
 */
void decrement_use_count(use_count_tracker_t* tracker, u_int32_t id);

/**
 * Deallocate the underlying data structures in the use count tracker
 */
void use_count_tracker_dealloc(use_count_tracker_t* tracker);

#endif /* USE_COUNT_TRACKER_H */
