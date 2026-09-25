/**
 * Author: Jack Robbins
 * This header file defines all of the APIs and data structures for the 
 * three_addr_var_t use count tracker
 *
 * This header file contains API definitions that are implemented in the use_count_tracker.c
 * file but also contains several simple inlined helpers
 */

#ifndef USE_COUNT_TRACKER_H
#define USE_COUNT_TRACKER_H

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>

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

//================================ Non-Inlined Functions ======================================
/**
 * Allocate the underlying data structures in the use count tracker
 */
use_count_tracker_t use_count_tracker_alloc(u_int32_t initial_variable_count);

/**
 * Perform a dynamic resize on the use count tracker based on the ID that was requested. To
 * be safe, we will always reallocate with double what was requested
 */
void variable_map_dynamic_resize_for_id(use_count_tracker_t* tracker, u_int32_t requested_id);

/**
 * Dump the use count for every single ID that currently exists
 * in the tracker. This is purely meant for debugging
 */
void dump_use_counts(use_count_tracker_t* tracker);

/**
 * Deallocate the underlying data structures in the use count tracker
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
		variable_map_dynamic_resize_for_id(tracker, id);
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
		variable_map_dynamic_resize_for_id(tracker, id);
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
		variable_map_dynamic_resize_for_id(tracker, id);
	}

	(tracker->map[id])--;
}


/**
 * Clear out all of the use counts and start fresh
 */
static inline void reset_all_use_counts(use_count_tracker_t* tracker){
	memset(tracker->map, 0, sizeof(u_int16_t) * tracker->variable_count);
}
//================================ Inlined Utility Functions ==================================
#endif /* USE_COUNT_TRACKER_H */
