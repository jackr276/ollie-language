/**
 * A simple tester program that tests our parsing ability
*/

//Link to the parser
#include "../parser/parser.h"
#include "../utils/constants.h"
#include "../preprocessor/preprocessor.h"
#include "../build_system/build_system.h"
#include "../utils/option_parser/option_parser.h"
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>

/**
 * Simply prints a parse message in a nice formatted way
*/
static inline void print_console_message(error_message_type_t message_type, char* info, u_int32_t line_num){
	//Now print it
	const char* type[] = {"WARNING", "ERROR", "INFO", "DEBUG"};

	//Print this out on a single line
	fprintf(stdout, "\n[LINE %d | COMPILER %s]: %s\n", line_num, type[message_type], info);
}


/**
 * Very simple test runner program
 */
int main(int argc, char** argv){
	u_int32_t num_warnings = 0;
	u_int32_t num_errors = 0;

	//Grab the options
	compiler_options_t* options = parse_and_store_options(argc, argv, &num_warnings, &num_errors);

	//Run the build system to generate one big token stream with all dependencies
	build_system_results_t build_results = construct_build_order(options, FALSE);

	//If this fails, we need to leave
	if(build_results.status == BUILD_SYSTEM_STATUS_FAILURE){
		print_console_message(MESSAGE_TYPE_ERROR, "BUILD SYSTEM FAILED", 0);
		//0 for test runs
		exit(0);
	}

	//Store it and invoke the parser
	options->build_order = build_results.compilation_order;

	//We now need to preprocess
	preprocessor_results_t results = preprocess(options);

	//If we failed then bail out
	if(results.status == PREPROCESSOR_FAILURE){
		print_console_message(MESSAGE_TYPE_ERROR, "PREPROCESSING FAILED", 0);
		//0 for test runs
		exit(0);
	}
	
	//Parse the file
	parse(options);
}
