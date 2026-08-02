/**
 * Author: Jack Robbins
 * This file implements the APIs for the static analyzer that were defined in the
 * header file of the same name
 */

#include "static_analyzer.h"


/**
 * Perform all static analysis on a given CFG. The functions
 * performed herein are:
 * 	1.) Mangling static variable names
 * 	2.) Converting the CFG into SSA form
 * 	3.) Perform definite assignment analysis
 * 		- This is a potential failure point
 * 	4.) Perform variable mutation analysis
 */
cfg_construction_result_type_t perform_all_static_analysis(cfg_t* cfg, front_end_results_package_t* results){

	//TODO DUMMY FOR NOW
	return CFG_RESULT_SUCCESS;

}
