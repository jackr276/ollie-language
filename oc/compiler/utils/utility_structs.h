/**
 * Author: Jack Robbins
 * Utility file that contains the compiler option type
 */

//Include guards
#ifndef UTILITY_STRUCTS_H
#define UTILITY_STRUCTS_H

#include <sys/types.h>

//Compiler option type
typedef struct compiler_options_t compiler_options_t;
//For storing times
typedef struct module_times_t module_times_t;

/**
 * Define a structure that stores all compiler options.
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
	//Print only the post allocation results
	u_int8_t print_post_allocation;
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


/**
 * Define a structure that contains all times 
 * that we could have for the compiler
*/
struct module_times_t {
	//Parser & lexer
	double parser_time;
	//Control flow graph constructor
	double cfg_time;
	//Optimizer
	double optimizer_time;
	//Instruction selector
	double selector_time;
	//Instruction scheduler 
	double scheduler_time;
	//Register allocator
	double allocator_time;
	//Overall compilation time
	double total_time;
};

#endif /* UTILITY_STRUCTS_H */
