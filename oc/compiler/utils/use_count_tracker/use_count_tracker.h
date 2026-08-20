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
	u_int32_t* map;
	u_int32_t variable_count;
};

#endif /* USE_COUNT_TRACKER_H */
