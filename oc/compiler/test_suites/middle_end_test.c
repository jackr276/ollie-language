/**
 * Author: Jack Robbins
 *
 * This program tests the front end(parser, cfg constructor) and middle end(optimizer) of the compiler
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

//Link to the parser
#include "../parser/parser.h"
//Link to cfg
#include "../cfg/cfg.h"
//Link to the ollie optimizer
#include "../optimizer/optimizer.h"
#include "../utils/constants.h"

u_int32_t num_warnings;
u_int32_t num_errors;


/**
 * We'll use this helper function to process the compiler flags and return a structure that
 * tells us what we need to do throughout the compiler
 */
static compiler_options_t* parse_and_store_options(int argc, char** argv){
	//Allocate it
	compiler_options_t* options = calloc(1, sizeof(compiler_options_t));
	
	//For storing our opt
	int opt;

	//Run through all of our options
	while((opt = getopt(argc, argv, "iatdhsf:o:?")) != -1){
		//Switch based on opt
		switch(opt){
			//Invalid option
			case '?':
				printf("Invalid option: %c\n", optopt);
				exit(0);
			//Specify that we want to print intermediate representations
			case 'i':
				options->print_irs = TRUE;
				break;
			//After we print help we exit
			case 'h':
				exit(0);
			//Time execution for performance test
			case 't':
				options->time_execution = TRUE;
				break;
			//Store the input file name
			case 'f':
				options->file_name = optarg;
				break;
			//Turn on debug printing
			case 'd':
				options->enable_debug_printing = TRUE;
				break;
			//Output to assembly only
			case 'a':
				options->go_to_assembly = TRUE;
				break;
			//Specify that we want a summary to be shown
			case 's':
				options->show_summary = TRUE;
				break;
			//Specific output file
			case 'o':
				options->output_file = optarg;
				break;
		}
	}

	//This is an error, so we'll fail out here
	if(options->file_name == NULL){
		printf("[COMPILER ERROR]: No input file name provided. Use -f <filename> to specify a .ol source file\n");
		exit(1);
	}

	//Give back the options we got in the structure
	return options;
}


/**
 * Our main and only function
*/
int main(int argc, char** argv){
	//How much time we've spent
	double time_spent;
	//Initialize these both to 0
	num_errors = 0;
	num_warnings = 0;

	printf("==================================== MIDDLE END TEST ======================================\n");

	//Parse and store the options
	compiler_options_t* options = parse_and_store_options(argc, argv);

	//Do we want to time execution or not
	u_int8_t time_execution = options->time_execution;

	//Print out what we're testing
	printf("TESTING FILE: %s\n\n", options->file_name);

	//Start the timer
	clock_t begin = clock();

	//Invoke the tokenizer
	ollie_token_stream_t stream = tokenize(options->file_name);

	//If this fails, we need to leave
	if(stream.status == STREAM_STATUS_FAILURE){
		print_parse_message(PARSE_ERROR, "Tokenizing Failed", 0);
		exit(1);
	}
	
	//Store it inside of the token stream
	options->token_stream = &stream;

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

	//Once we build the CFG, we'll pass this along to the optimizer
	cfg = optimize(cfg);

	//And once we're done - for the front end test, we'll want all of this printed
	print_all_cfg_blocks(cfg);

	//Deallocate everything at the end
	ast_dealloc();
	//Free the call graph holder
	free(parse_results->os);
	function_symtab_dealloc(parse_results->function_symtab);
	type_symtab_dealloc(parse_results->type_symtab);
	variable_symtab_dealloc(parse_results->variable_symtab);
	constants_symtab_dealloc(parse_results->constant_symtab);
	dealloc_cfg(cfg);

	//Now stop the clock - we want to test the deallocation overhead too
	//Timer end
	clock_t end = clock();

	//Calculate the final time
	time_spent = (double)(end - begin)/CLOCKS_PER_SEC;

	//Print out the summary now that we're done
	printf("\n===================== MIDDLE END TEST SUMMARY ==========================\n");
	printf("Lexer processed %d lines\n", parse_results->lines_processed);
	printf("Parsing and optimizing succeeded");
	if(time_execution == TRUE){
		printf(" in %.8f seconds", time_spent);
	}
	printf(" with %d warnings\n", num_warnings);

	printf("=======================================================================\n\n");

final_printout:
	printf("==================================== END  ================================================\n");
}
