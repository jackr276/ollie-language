/**
 * Author: Jack Robbins
 *
 * This program tests the front end of the compiler only
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>

#include "../build_system/build_system.h"
#include "../parser/parser.h"
#include "../preprocessor/preprocessor.h"
#include "../cfg/cfg.h"
#include "../utils/constants.h"
#include "../utils/option_parser/option_parser.h"

/**
 * Simply prints a parse message in a nice formatted way
*/
static void print_console_message(error_message_type_t message_type, char* info, u_int32_t line_num){
	//Now print it
	const char* type[] = {"WARNING", "ERROR", "INFO", "DEBUG"};

	//Print this out on a single line
	fprintf(stdout, "\n[LINE %d | COMPILER %s]: %s\n", line_num, type[message_type], info);
}


/**
 * Our main and only function
*/
int main(int argc, char** argv){
	//How much time we've spent
	double time_spent;
	//Initialize these both to 0
	u_int32_t num_errors = 0;
	u_int32_t num_warnings = 0;

	printf("==================================== FRONT END TEST ======================================\n");

	//Grab all the options using the helper
	compiler_options_t* options = parse_and_store_options(argc, argv, &num_warnings, &num_errors);

	//Do we want to time or not
	u_int8_t time_execution = options->time_execution;

	//Print out what we're testing
	printf("TESTING FILE: %s\n\n", options->file_name);

	//Start the timer
	clock_t begin = clock();

	//Let the build system construct our token stream
	build_system_results_t build_system_results = construct_build_order(options, FALSE);

	//If this fails, we need to leave
	if(build_system_results.status == BUILD_SYSTEM_STATUS_FAILURE){
		print_console_message(MESSAGE_TYPE_ERROR, "Tokenizing Failed", 0);
		//0 for test runs
		exit(0);
	}
	
	//Store it inside of the token stream
	options->build_order = build_system_results.compilation_order;

	//We now need to preprocess
	preprocessor_results_t results = preprocess(options);

	//If we failed then bail out
	if(results.status == PREPROCESSOR_FAILURE){
		print_console_message(MESSAGE_TYPE_ERROR, "Preprocessing Failed", 0);
		//0 for test runs
		exit(0);
	}

	//Now that we can actually open the file, we'll parse
	front_end_results_package_t* parse_results = parse(options);

	//Let's see what kind of results we got
	if(parse_results->root->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		//Timer end
		clock_t end = clock();

		//Calculate the final time
		time_spent = (double)(end - begin)/CLOCKS_PER_SEC;

		char info[2000];
		if(time_execution == TRUE){
			sprintf(info, "Parsing failed with %d errors and %d warnings in %.8f seconds", parse_results->num_errors, parse_results->num_warnings, time_spent);
		} else {
			sprintf(info, "Parsing failed with %d errors and %d warnings", parse_results->num_errors, parse_results->num_warnings);
		}

		printf("\n===================== Ollie Compiler Summary ==========================\n");
		printf("Lexer processed %d lines\n", parse_results->lines_processed);
		printf("%s\n", info);
		printf("=======================================================================\n\n");
		//Jump to the end, we're done here
		goto final_printout;
	}

	//The number of warnings and errors
	num_warnings += parse_results->num_warnings;
	num_errors += parse_results->num_errors;

	//Now we'll invoke the cfg builder
	cfg_t* cfg = build_cfg(parse_results, &num_errors, &num_warnings);

	/**
	 * We could not construct the CFG so we need to exit out here
	 */
	if(cfg->result == CFG_RESULT_FAILURE){
		//Timer end
		clock_t end = clock();

		//Calculate the final time
		time_spent = (double)(end - begin)/CLOCKS_PER_SEC;

		char info[2000];
		if(time_execution == TRUE){
			sprintf(info, "CFG construction failed with %d errors and %d warnings in %.8f seconds", parse_results->num_errors, parse_results->num_warnings, time_spent);
		} else {
			sprintf(info, "CFG construction with %d errors and %d warnings", parse_results->num_errors, parse_results->num_warnings);
		}

		printf("\n===================== Ollie Compiler Summary ==========================\n");
		printf("Lexer processed %d lines\n", parse_results->lines_processed);
		printf("%s\n", info);
		printf("=======================================================================\n\n");
		//Jump to the end, we're done here
		goto final_printout;
	}

	//Print out the adjacency matrix for the call graph
	print_call_graph_adjacency_matrix(stdout, parse_results->function_symtab);

	//And once we're done - for the front end test, we'll want all of this printed
	print_all_cfg_blocks(cfg);

	//Deallocate everything at the end
	ast_dealloc();
	function_symtab_dealloc(parse_results->function_symtab);
	type_symtab_dealloc(parse_results->type_symtab);
	variable_symtab_dealloc(parse_results->variable_symtab);
	dealloc_cfg(cfg);

	//Now stop the clock - we want to test the deallocation overhead too
	//Timer end
	clock_t end = clock();

	//Calculate the final time
	time_spent = (double)(end - begin)/CLOCKS_PER_SEC;

	//Print out the summary now that we're done
	printf("\n===================== FRONT END TEST SUMMARY ==========================\n");
	printf("Lexer processed %d lines\n", parse_results->lines_processed);
	printf("Parsing succeeded ");
	if(time_execution == TRUE){
		printf("in %.8f seconds ", time_spent);
	}
	printf("with %d warnings\n", num_warnings);
	printf("=======================================================================\n\n");

final_printout:
	printf("==================================== END  ================================================\n");
}
