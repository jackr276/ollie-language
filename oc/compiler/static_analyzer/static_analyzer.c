/**
 * Author: Jack Robbins
 * This file implements the APIs for the static analyzer that were defined in the
 * header file of the same name
 */

#include "static_analyzer.h"


/**
 * Since static variables also count for us as global variables, we need to
 * be able to handle a case where say, for instance, that two separate
 * functions have a static variable called "x". If we just left it as is,
 * we would have an ambiguous reference and the assembler woudl fail. To fix
 * this, we will mangle those names such that we now get "x.0" and "x.1" instead
 * of two "x"'s
 */
static void mangle_static_variable_names(dynamic_array_t* global_variables){
	//We'll keep a running id to mangle things
	u_int32_t static_var_mangler = 0;
	char mangler[100];

	/**
	 * Run through all of our global variables here
	 */
	for(int32_t i = 0; i < global_variables->current_index; i++){
		//Extract our current candidate
		global_variable_t* candidate = dynamic_array_get_at(global_variables, i);
		
		/**
		 * Global variable name collision is already enforced by the symtab in the
		 * parser so we can skip this for efficiency's sake
		 */
		if(candidate->variable->membership == GLOBAL_VARIABLE){
			continue;
		}

		//Print this into the buffer
		snprintf(mangler, 100, ".%d", static_var_mangler);
		
		//Now concatenate it to our variable name
		dynamic_string_concatenate(&(candidate->variable->var_name), mangler);

		//Bump it up for the next go around
		static_var_mangler++;
	}
}


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
	/**
	 * 1.) Mangle all static variable names with a unique number identifier at the very end
	 * to avoid name collisions
	 */
	mangle_static_variable_names(&(cfg->global_variables));

	//TODO DUMMY FOR NOW
	return CFG_RESULT_SUCCESS;

}
