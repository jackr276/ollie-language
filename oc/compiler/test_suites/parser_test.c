/**
 * A simple tester program that tests our parsing ability
*/

//Link to the parser
#include "../parser/parser.h"
#include "../utils/constants.h"
#include "../preprocessor/preprocessor.h"
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>


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
 * Very simple test runner program
*/
int main(int argc, char** argv){
	//Grab the options
	compiler_options_t* options = parse_and_store_options(argc, argv);

	//First things first - run the lexer
	ollie_token_stream_t token_stream = tokenize(options->file_name);

	//If this fails, we need to leave
	if(token_stream.status == STREAM_STATUS_FAILURE){
		print_parse_message(MESSAGE_TYPE_ERROR, "Tokenizing Failed", 0);
		//0 for test runs
		exit(0);
	}

	//Store it and invoke the parser
	options->token_stream = &token_stream;

	//We now need to preprocess
	preprocessor_results_t results = preprocess(options->file_name, options->token_stream);

	//If we failed then bail out
	if(results.success == FALSE){
		print_parse_message(MESSAGE_TYPE_ERROR, "Tokenizing Failed", 0);
		//0 for test runs
		exit(0);
	}
	
	//Parse the file
	parse(options);
}
