/**
 * Author: Jack Robbins
 * This header file defines the APIs and data structures needed
 * to parse compiler options
 */


#include "../utility_structs.h"
#include "../error_management.h"

#ifndef OPTION_PARSER_H
#define OPTION_PARSER_H

/**
 * Parse in and execute/store the options passed to us on the command line
 */
compiler_options_t* parse_and_store_options(int argc, char** argv, u_int32_t* num_warnings, u_int32_t* num_errors);

#endif /* OPTION_PARSER_H */
