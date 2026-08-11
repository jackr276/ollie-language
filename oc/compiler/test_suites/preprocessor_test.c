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
#include "../utils/option_parser/option_parser.h"
#include "../build_system/build_system.h"
#include "../utils/constants.h"
#include "../utils/utility_structs.h"


/**
 * Our main and only function
*/
int main(int argc, char** argv){
	//How much time we've spent
	double time_spent;
	//Initialize these both to 0
	u_int32_t num_errors = 0;
	u_int32_t num_warnings = 0;

	printf("==================================== PREPROCESSOR TEST ======================================\n");

	//Grab all the options using the helper
	compiler_options_t* options = parse_and_store_options(argc, argv, &num_warnings, &num_errors);

	//Do we want to time or not
	u_int8_t time_execution = options->time_execution;

	//Print out what we're testing
	printf("TESTING FILE: %s\n\n", options->file_name);

	//Start the timer
	clock_t begin = clock();

	//Invoke the build system to get our full build order
	build_system_results_t build_results = construct_build_order(options, FALSE);
	//Store the build order in here
	options->build_order = build_results.compilation_order;

	//Tokenizing failed, error out
	if(build_results.status == BUILD_SYSTEM_STATUS_FAILURE){
		printf("TOKENIZING FAILED\n");
		printf("==================================== END  ================================================\n");
		//0 for test runs - it's fine to have this fail sometimes
		exit(0);
	}

	//Print out the pre-preprocssing token stream
	printf("============================= BEFORE PREPROCESSOR =====================================\n");

	for(int32_t i = 0; i < options->build_order.current_index; i++){
		dependency_graph_node_t* dependency = dynamic_array_get_at(&(options->build_order), i);

		for(int32_t j  = 0; j  < dependency->token_stream.token_stream.current_index; j++){
			printf("%d: %s\n", j, lexitem_to_string(token_array_get_pointer_at(&(dependency->token_stream.token_stream), j)));
		}
	}

	printf("============================= BEFORE PREPROCESSOR =====================================\n");

	//We now need to preprocess
	preprocessor_results_t results = preprocess(options);
	
	//This did not work, get out
	if(results.status == PREPROCESSOR_FAILURE){
		printf("PREPROCESSOR FAILED\n");
		printf("==================================== END  ================================================\n");
	}

	//Print out the post-preprocssing token stream
	printf("============================= AFTER PREPROCESSOR =====================================\n");

	for(int32_t i = 0; i < options->build_order.current_index; i++){
		dependency_graph_node_t* dependency = dynamic_array_get_at(&(options->build_order), i);

		for(int32_t j  = 0; j  < dependency->token_stream.token_stream.current_index; j++){
			printf("%d: %s\n", j, lexitem_to_string(token_array_get_pointer_at(&(dependency->token_stream.token_stream), j)));
		}
	}

	printf("============================= AFTER PREPROCESSOR =====================================\n");

	//Now stop the clock - we want to test the deallocation overhead too
	//Timer end
	clock_t end = clock();

	//Calculate the final time
	time_spent = (double)(end - begin)/CLOCKS_PER_SEC;

	//Print out the summary now that we're done
	printf("\n===================== PREPROCESSOR TEST SUMMARY ==========================\n");
	printf("Preprocessor test succeeded ");
	if(time_execution == TRUE){
		printf("in %.8f seconds ", time_spent);
	}
	printf("with %d warnings\n", num_warnings);
	printf("=======================================================================\n\n");
	printf("==================================== END  ================================================\n");
}
