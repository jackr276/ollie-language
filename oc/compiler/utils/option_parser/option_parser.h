/**
 * Author: Jack Robbins
 * This header file defines the APIs and data structures needed
 * to parse compiler options
 */


#include "../utility_structs.h"
#include "../error_management.h"

/**
 * Parse in and execute/store the options passed to us on the command line
 */
compiler_options_t* parse_and_store_options(int argc, char** argv);
