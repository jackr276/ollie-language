/**
 * Author: Jack Robbins
 * This header file defines the API for the static analyzer. The static analyzer will
 * convert the CFG into SSA form, perform uninitialized variable detection, mutation
 * checking, and other static analysis functions
 */


#ifndef STATIC_ANALYZER_H
#define STATIC_ANALYZER_H

//Link to CFG structure
#include "../parser/parser.h"
#include "../cfg/cfg.h"
#include <sys/types.h>

/**
 * Perform all static analysis on a given CFG. The functions
 * performed herein are:
 * 	1.) Mangling static variable names
 * 	2.) Converting the CFG into SSA form
 * 	3.) Perform definite assignment analysis
 * 		- This is a potential failure point
 * 	4.) Perform variable mutation analysis
 */
cfg_construction_result_type_t perform_all_static_analysis(cfg_t* cfg, front_end_results_package_t* results, u_int32_t* num_errors, u_int32_t* num_warnings);

#endif /* STATIC_ANALYZER_H */
