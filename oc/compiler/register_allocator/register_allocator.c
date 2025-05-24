/**
 * Author: Jack Robbins
 *
 * This file contains the implementations of the APIs defined in the header file with the same name
*/

#include "register_allocator.h"
//For live rnages
#include "../dynamic_array/dynamic_array.h"

//A struct that stores all of our live ranges
typedef struct live_range_t live_range_t;

/**
 * For our live ranges, we'll really only need the name and
 * the variables
 */
struct live_range_t{
	//We'll store the name
	char live_range_name[MAX_IDENT_LENGTH];
	//and the variables that it has
	dynamic_array_t* variables;
};

/**
 * Print all live ranges that we have
 */
static void print_all_live_ranges(dynamic_array_t* live_ranges){

}


/**
 * Construct the live ranges for all variables that we'll need to concern ourselves with
 *
 * Conveniently, all code in OIR is translated into SSA form by the front end. In doing this, we're able
 * to find live ranges in one pass of the code
 *
 * We will run through the entirety of the straight-line code. We will use the disjoint-set union-find algorithm
 * to do this.
 *
 * For each instruction:
 * 	If variable is not in a live range set:
 * 		make a new live range set and add the variable to it
 * 	else:
 * 		add the variable to the corresponding live range set
 */
static dynamic_array_t* construct_all_live_ranges(cfg_t* cfg){

	//Placehold
	return NULL;
}


/**
 * Perform our register allocation algorithm on the entire cfg
 */
void allocate_all_registers(cfg_t* cfg){

}
