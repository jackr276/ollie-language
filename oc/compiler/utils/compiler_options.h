/**
 * Author: Jack Robbins
 * Utility file that contains the compiler option type
 */

#include <sys/types.h>

//Compiler option type
typedef struct compiler_options_t compiler_options_t;


/**
 * Define an enum that stores all compiler options.
 * This struct will be used throughout the compiler
 * to tell us what to print out
 */
struct compiler_options_t {
	//The name of the file(-f)
	char* file_name;
	//The name of the output file(-o )
	char* output_file;
	//Do we want to skip outputting
	//to assembly? 
	u_int8_t skip_output;
	//Enable all debug printing 
	u_int8_t enable_debug_printing;
	//Print out summary? 
	u_int8_t show_summary;
	//Only output assembly(no .o)
	u_int8_t go_to_assembly; 
	//Time execution for performance testing
	u_int8_t time_execution;
	//Do we want module-specific timing
	u_int8_t module_specific_timing;
	//Is this a CI run?
	u_int8_t is_test_run;
	//Print intermediate representations
	u_int8_t print_irs;
};

