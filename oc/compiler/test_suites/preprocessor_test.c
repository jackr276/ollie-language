/**
 * Author: Jack Robbins
 *
 * This test is meant to exclusively test the preprocessor before any parsing has
 * taken place. It will serve as a canary for any issues that come up
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>

//Link to the preprocessor
#include "../preprocessor/preprocessor.h"
#include "../utils/constants.h"
#include "../utils/utility_structs.h"


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

	printf("==================================== PREPROCESSOR TEST ======================================\n");

	//Grab all the options using the helper
	compiler_options_t* options = parse_and_store_options(argc, argv);

	//Do we want to time or not
	u_int8_t time_execution = options->time_execution;

	//Print out what we're testing
	printf("TESTING FILE: %s\n\n", options->file_name);

	//Start the timer
	clock_t begin = clock();

	//Invoke the tokenizer
	ollie_token_stream_t stream = tokenize(options->file_name);

	//Tokenizing failed, error out
	if(stream.status == STREAM_STATUS_FAILURE){
		printf("TOKENIZING FAILED\n");
		printf("==================================== END  ================================================\n");
	}

	//Store it inside of the token stream
	options->token_stream = &stream;

	//Print out the pre-preprocssing token stream
	printf("============================= BEFORE PREPROCESSOR =====================================\n");

	for(u_int32_t i = 0; i < stream.token_stream.current_index; i++){
		printf("%d: %s\n", i, lexitem_to_string(token_array_get_pointer_at(&(stream.token_stream), i)));
	}

	printf("============================= BEFORE PREPROCESSOR =====================================\n");

	//We now need to preprocess
	preprocessor_results_t results = preprocess(options->file_name, options->token_stream);
	
	//This did not work, get out
	if(results.success == FALSE){
		printf("PREPROCESSOR FAILED\n");
		printf("==================================== END  ================================================\n");
	}

	//Now stop the clock - we want to test the deallocation overhead too
	//Timer end
	clock_t end = clock();

	//Calculate the final time
	time_spent = (double)(end - begin)/CLOCKS_PER_SEC;

	//Print out the summary now that we're done
	printf("\n===================== PREPROCESSOR TEST SUMMARY ==========================\n");
	if(time_execution == TRUE){
		printf("in %.8f seconds ", time_spent);
	}
	printf("with %d warnings\n", num_warnings);
	printf("=======================================================================\n\n");
	printf("==================================== END  ================================================\n");
}
