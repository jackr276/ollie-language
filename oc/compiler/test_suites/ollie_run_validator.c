/**
 *  Author: Jack Robbins
 *
 *  The Ollie Run Validator has been designed to, in conjuction with the memory checker,
 *  give us automated capabilities when validating tests that are able to be validated
 *  via run. What does it mean to be validated via run? - if we compile and run the program,
 *  the *shell exit code* should match what we are expecting. Remember that the shell
 *  exit code is what main returns. Some tests have been designed to do this to validate
 *  full end-to-end runs
 *
 *
 *  Strategy is as follows: we first in the main thread read in every single test file in the
 *  given directory into a big array of all test files. Following that, the array is divided up
 *  so that every single thread is given it's own subset of test files to work with(index start(inclusive)
 *  to end(exclusive)). This should avoid the need for us to do any locks. Then each thread 
 *  is told to start. Internal to the thread, for each file, we will tokenize the file to see if
 *  we have a special tag OUNIT. If so, we will compile the file using oc, run it
 *  inside of a subshell, and compare the result with the expected result. If they match we've succeeded,
 *  if they don't then we have a failure and we put it inside of the error file list
 */

//Use our own in-house dynamic array
#include "../utils/dynamic_array/dynamic_array.h"
#include "../lexer/lexer.h"
#include "../utils/constants.h"
//We will be doing this multithreaded
#include <pthread.h>
#include <sys/types.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>

//Default file array size to avoid excessive resizing
#define DEFAULT_ARRAY_SIZE 1000
//The maximum console output value in UNIX
#define MAX_EXIT_STATUS_VALUE 255

/**
 * There are 4 potential things that we need to lock. To avoid holding
 * up other areas of the process with those locks unnecessarily, we will
 * maintain a separate mutex for each given item. The items that we need
 * to lock are
 *
 *  1.) STDOUT output
 *  2.) Lexer - not thread safe in its implementation
 *  3.) Compiler - not thread safe in its implementation
 *  4.) Error file queue
 */
pthread_mutex_t stdout_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t file_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lexer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t compiler_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t error_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * A generic array containing *all* of our test files that will be worked over
 * by the threads. We then also maintain a list of all files that are found
 * to be in error. This is done for the final summary
 */
static dynamic_array_t test_files;

//Keep track of the file counts for all different OUNIT types
static u_int32_t number_of_ounit_compatible_files = 0;

/**
 * For exit status validation, we have two ways to fail:
 * 	1.) It could fail to compile
 * 	2.) The actual output could differ from the expected
 */
static u_int32_t number_of_exit_status_validation_files = 0;
static dynamic_array_t failed_to_compile_exit_status_validation_files;
static dynamic_array_t failed_exit_status_validation_files;

/**
 * For failed to compile validation:
 * 	1.) The file could compile when we want it to fail 
 */
static u_int32_t number_of_fail_to_compile_validation_files = 0;
static dynamic_array_t compiled_when_failure_expected_files;

/**
 * Also keep track of our invalid files if there are any
 */
static dynamic_array_t invalid_ounit_configuration_files;

//Holders for our output and test file directories
static char* output_directory;

/**
 * Our current thread parameter structure only contains
 * the thread's unique ID
 */
typedef struct thread_parameters_t thread_parameters_t;
struct thread_parameters_t {
	u_int8_t thread_number;
};


/**
 * The test parameters struct is optionally filled by the OUNIT
 * parser, depending on what our test type is
 */
typedef struct test_parameters_t test_parameters_t;
struct test_parameters_t {
	int32_t expected_exit_status;
};


/**
 * For OUNIT types, we have a few possible
 * scenarios:
 * 	1.) Not at all compatible - just ignore
 * 	2.) Flagged as compatible but incorrect - fail
 * 	3.) exit_status directive - OUNIT wants the test ran and the exit statuses compared
 * 	4.) fail_to_compile - we expect that this test will not compile intentionally
 * This enumeration represents all possible states
 */
typedef enum {
	OUNIT_TYPE_NONE,
	OUNIT_TYPE_INVALID,
	OUNIT_TYPE_EXIT_STATUS_VALIDATION,
	OUNIT_TYPE_FAIL_TO_COMPILE,
} ounit_type_t;


/**
 * The exit_status OUNIT directive tells OUNIT to run the generated executable and check for a result
 * using the echo $? command. The user will pass an integer constant into OUNIT to be checked for. We will
 * do validations here to ensure that everything is proper
 *
 * OUNIT: [exit_status = <integer_constant>]
 */
static inline ounit_type_t parse_exit_status_OUNIT_directive(ollie_token_array_t* tokens, int32_t* index, test_parameters_t* parameters){
	lexitem_t* lexitem;

	//Advance up to the next token in the stream
	(*index)++;
	lexitem = token_array_get_pointer_at(tokens, *index);

	/**
	 * Again another fail case here, we need to see an =
	 */
	if(lexitem->tok != EQUALS){
		pthread_mutex_lock(&stdout_mutex);
		fprintf(stdout, "Expected \"=\" but got \"%s\" instead\n", lexitem_to_string(lexitem));
		pthread_mutex_unlock(&stdout_mutex);

		return OUNIT_TYPE_INVALID;
	}

	//Advance to the next token
	(*index)++;
	lexitem = token_array_get_pointer_at(tokens, *index);

	/**
	 * We now need to see any kind of integer equivalent constant. This means
	 * that chars, shorts, longs, etc are fine. Floats and strings are not
	 */
	switch(lexitem->tok){
		case SHORT_CONST:
			parameters->expected_exit_status = lexitem->constant_values.signed_short_value;
			break;

		case SHORT_CONST_FORCE_U:
			parameters->expected_exit_status = lexitem->constant_values.unsigned_short_value;
			break;

		case INT_CONST:
			parameters->expected_exit_status = lexitem->constant_values.signed_int_value;
			break;

		case INT_CONST_FORCE_U:
			parameters->expected_exit_status = lexitem->constant_values.unsigned_int_value;
			break;

		case LONG_CONST:
			parameters->expected_exit_status = lexitem->constant_values.signed_long_value;
			break;

		case LONG_CONST_FORCE_U:
			parameters->expected_exit_status = lexitem->constant_values.unsigned_long_value;
			break;

		case BYTE_CONST:
			parameters->expected_exit_status = lexitem->constant_values.signed_byte_value;
			break;

		case BYTE_CONST_FORCE_U:
			parameters->expected_exit_status = lexitem->constant_values.unsigned_byte_value;
			break;

		case CHAR_CONST:
			parameters->expected_exit_status = lexitem->constant_values.char_value;
			break;

		case TRUE_CONST:
			parameters->expected_exit_status = TRUE;
			break;
			
		case FALSE_CONST:
			parameters->expected_exit_status = FALSE;
			break;

		/**
		 * Anything not listed above is an automatic fail
		 * case. We will display the issue too
		 */
		default:
			pthread_mutex_lock(&stdout_mutex);
			fprintf(stdout, "An integer adjacent constant was expected after the =, instead saw \"%s\"\n", lexitem_to_string(lexitem));
			pthread_mutex_unlock(&stdout_mutex);

			return OUNIT_TYPE_INVALID;
	}

	/**
	 * We'll have this fail. We don't want to let the developer waste
	 * time confused as to why this isn't working
	 */
	if(parameters->expected_exit_status >= MAX_EXIT_STATUS_VALUE){
		pthread_mutex_lock(&stdout_mutex);
		fprintf(stdout, "The OUNIT expected return value of %d is higher than the maximum UNIX return value of %d. Please remediate your test\n",
		  		parameters->expected_exit_status,
		  		MAX_EXIT_STATUS_VALUE);
		pthread_mutex_unlock(&stdout_mutex);

		return OUNIT_TYPE_INVALID;
	}

	//If we make it to here then we're good, and we're of this given type
	return OUNIT_TYPE_EXIT_STATUS_VALIDATION;
}


/**
 * Parser the OUNIT test command. If the command is found to be invalid, we return a state 
 * that represents said invalidity. Otherwise, we will store the result that we expect(must
 * be an integer) inside of the passed parameter pointer
 *
 * NOTE: By the time we get here we've already seen and advanced the pointer past the OUNIT 
 * token
 *
 * Example OUNIT command: OUNIT: [exit_status = 5]
 *
 * This tells us that the final exit_status return value(echo $?) of the test should be 5
 *
 * The example above is just one of the test types that OUNIT supports
 */
static inline ounit_type_t parse_OUNIT_test_command(ollie_token_array_t* tokens, int32_t index, test_parameters_t* parameters){
	//By default assume we're invalid
	ounit_type_t ounit_type = OUNIT_TYPE_INVALID;

	//Generic pointer for our lexitem
	lexitem_t* lexitem = token_array_get_pointer_at(tokens, index);

	/**
	 * We need to see a colon here. If we don't then we fail 
	 * out now
	 */
	if(lexitem->tok != COLON){
		pthread_mutex_lock(&stdout_mutex);
		fprintf(stdout, "Expected \":\" but got \"%s\" instead\n", lexitem_to_string(lexitem));
		pthread_mutex_unlock(&stdout_mutex);

		return OUNIT_TYPE_INVALID;
	}

	//Otherwise bump the index up
	index++;
	lexitem = token_array_get_pointer_at(tokens, index);

	/**
	 * Again another fail case here, we need to see an L_BRACKET
	 */
	if(lexitem->tok != L_BRACKET){
		pthread_mutex_lock(&stdout_mutex);
		fprintf(stdout, "Expected \"[\" but got \"%s\" instead\n", lexitem_to_string(lexitem));
		pthread_mutex_unlock(&stdout_mutex);

		return OUNIT_TYPE_INVALID;
	}

	index++;
	lexitem = token_array_get_pointer_at(tokens, index);

	/**
	 * What we are requesting to do with OUNIT here depends on the token in
	 * this area. An unrecognized token will produce an error that will be flagged 
	 * to the user
	 */
	switch(lexitem->tok){
		case EXIT_STATUS:
			ounit_type = parse_exit_status_OUNIT_directive(tokens, &index, parameters);
			break;
			
		case FAIL_TO_COMPILE:
			ounit_type = OUNIT_TYPE_FAIL_TO_COMPILE;
			break;

		default:
			pthread_mutex_lock(&stdout_mutex);
			fprintf(stdout, "Invalid OUNIT directive \"%s\", please review the directive list\n", lexitem_to_string(lexitem));
			pthread_mutex_unlock(&stdout_mutex);

			return OUNIT_TYPE_INVALID;
	}

	//Bump it up one last time
	index++;
	lexitem = token_array_get_pointer_at(tokens, index);

	/**
	 * Again another fail case here, we need to see an ]
	 */
	if(lexitem->tok != R_BRACKET){
		pthread_mutex_lock(&stdout_mutex);
		fprintf(stdout, "Expected \"]\" but got \"%s\" instead\n", lexitem_to_string(lexitem));
		pthread_mutex_unlock(&stdout_mutex);

		return OUNIT_TYPE_INVALID;
	}

	//Give back whatever OUNIT type we ended up with 
	return ounit_type;
}


/**
 * Is the given test compatible with OUNIT? We will determine
 * this by scanning for the OUNIT token inside of the tests
 * token array. If the test is determined to be OUNIT compatible, we will
 * also piggy-back off of this function to parse and get out what we
 * expect the actual result of the test to be
 */
static ounit_type_t is_test_OUNIT_compatible(ollie_token_stream_t* stream, test_parameters_t* parameters){
	//Run through and see if we can find the OUNIT token
	for(int32_t i = 0; i < stream->token_stream.current_index; i++){
		//Extract the token pointer
		lexitem_t* lexitem = token_array_get_pointer_at(&(stream->token_stream), i);

		//If we see the OUNIT token then we will let the helper determine its compatibilty
		if(lexitem->tok == OUNIT){
			number_of_ounit_compatible_files++;
			return parse_OUNIT_test_command(&(stream->token_stream), i + 1, parameters);
		}
	}

	//If we made it all the way down here then it is not OUNIT compatible
	return OUNIT_TYPE_NONE;
}


/**
 * We will construct the output file name from the fully qualified file name
 * by replacing every single "/" with a "_". This guarantees a unique file name 
 * every time. This helper assumes that the output file name buffer has already
 * been allocated
 */
static inline void construct_output_file_name_from_full_path(char* output_file_name, const char* file_name){
	//Wipe the whole thing out
	memset(output_file_name, 0, FILENAME_MAX);

	//Start copying from 0
	int32_t source_index = 0;
	int32_t dest_index = 0;

	/**
	 * Skip over a leading ./ so that we don't include it in the final product
	 */
	if(file_name[source_index] == '.' && file_name[source_index + 1] == '/'){
		source_index += 2;
	}

	//Keep going so long as it's true
	while(TRUE){
		//What character are we copying
		char to_copy = file_name[source_index];

		switch(to_copy){
			//This should not happen
			case '\0':
				fprintf(stderr, "Fatal internal compiler error: end of string hit without seeing .ol for file %s", file_name);
				exit(1);

			//Replace all slashes with _
			case '/':
				output_file_name[dest_index] = '_';
				break;

			/**
			 * A dot is likely the filename. If it is a .ol file name, then we'll concatenate
			 * the .test to it and leave. Otherwise this is an error
			 */
			case '.':
				if(file_name[source_index + 1] == 'o' && file_name[source_index + 2] == 'l'){
					strcat(output_file_name, ".test");
					return;

				} else {
					fprintf(stderr, "Fatal internal compiler error: . detected in string %s before the ending .ol", file_name);
					exit(1);
				}

			//Default is just to copy this character over
			default:
				output_file_name[dest_index] = to_copy;
				break;
		}

		//Bump these both up
		source_index++;
		dest_index++;
	}
}


/**
 * Exit status validation requires both the compilation and execution of a given program. This 
 * helper does those steps in that order. This is a thread safe helper, locking is used to
 * maintain thread safety around the compiler as it is not inherently thread safe
 */
static inline void handle_exit_status_validation(u_int32_t thread_id, char* file_name, u_int32_t* thread_error_count, test_parameters_t* parameters){
	//All needed string buffers
	char output_file_name[FILENAME_MAX];
	char command_buffer[10000];
	char run_command_buffer[10000];

	//The output file name itself will come from the fully qualified file name
	construct_output_file_name_from_full_path(output_file_name, file_name);

	//Save that this was eligible to be run
	pthread_mutex_lock(&stdout_mutex);
	number_of_exit_status_validation_files++;
	pthread_mutex_unlock(&stdout_mutex);

	/**
	 * Otherwise it is compatible so we will begin our testing
	 * here by first compiling the actual item
	 */
	sprintf(command_buffer, "%s/oc -f %s -o %s > /dev/null 2>&1", output_directory, file_name, output_file_name);

	/**
	 * Run the compilation command. The compiler relies on a shared temporary output file, so we 
	 * need to lock here to make this all work
	 */
	pthread_mutex_lock(&compiler_mutex);
	int32_t compilation_result = system(command_buffer);
	pthread_mutex_unlock(&compiler_mutex);

	/**
	 * If for any reason we have a failure here, then
	 * we will note a compilation failure and move on
	 */
	if(compilation_result != 0){
		pthread_mutex_lock(&stdout_mutex);
		fprintf(stdout, "[Thread %d]: Ran compilation command: %s\n", thread_id, command_buffer);
		fprintf(stdout, "[Thread %d]: The OUNIT configured test %s failed to compile with exit code %d. Developer attention is required\n\n", thread_id, file_name, compilation_result);
		pthread_mutex_unlock(&stdout_mutex);

		//Add to the array and bump the count
		pthread_mutex_lock(&error_queue_mutex);
		dynamic_array_add(&failed_to_compile_exit_status_validation_files, file_name);
		pthread_mutex_unlock(&error_queue_mutex);

		//Bump up the per-thread result
		(*thread_error_count)++;

		//Exit out
		return;
	}

	/**
	 * Create the actual run command and get a result out
	 */
	sprintf(run_command_buffer, "./%s", output_file_name);
	int32_t runtime_result = WEXITSTATUS(system(run_command_buffer));

	/**
	 * If the results match then we are all set here. If they
	 * do not then we will need to display an error
	 */
	if(runtime_result == parameters->expected_exit_status){
		pthread_mutex_lock(&stdout_mutex);
		fprintf(stdout, "Compilation command ran: %s\n", command_buffer);
		fprintf(stdout, "Execution command ran: %s\n", run_command_buffer);
		fprintf(stdout, "Expected execution result [%d] matched the expected result [%d]. Test was a success.\n", runtime_result, parameters->expected_exit_status);
		pthread_mutex_unlock(&stdout_mutex);

	} else {
		pthread_mutex_lock(&stdout_mutex);
		fprintf(stdout, "Compilation command ran: %s\n", command_buffer);
		fprintf(stdout, "Execution command ran: %s\n", run_command_buffer);
		fprintf(stdout, "Expected execution result was [%d], but got [%d] instead\n", parameters->expected_exit_status, runtime_result);
		pthread_mutex_unlock(&stdout_mutex);

		//Add to the list failing the actual exit status validation
		pthread_mutex_lock(&error_queue_mutex);
		dynamic_array_add(&failed_exit_status_validation_files, file_name);
		pthread_mutex_unlock(&error_queue_mutex);
		
		//Count it as one more error
		(*thread_error_count)++;
	}

	/**
	 * Delete the output file now that we are done with it. Output file
	 * names should be unique so in theory we should not have to lock this
	 */
	sprintf(command_buffer, "rm %s", output_file_name);
	int32_t deletion_result = system(command_buffer);

	/**
	 * If somehow this didn't work we should flag it
	 */
	if(deletion_result != 0){
		pthread_mutex_lock(&stdout_mutex);
		fprintf(stdout, "[Thread %d]: Failed to delete output file %s\n", thread_id, output_file_name);
		(*thread_error_count)++;
		pthread_mutex_unlock(&stdout_mutex);
	}
}


/**
 * Handle an OUNIT test type where we explicitly want the compilation to fail
 */
static inline void handle_fail_to_compile_validation(u_int32_t thread_id, char* file_name, u_int32_t* thread_error_count){
	//All needed string buffers
	char output_file_name[FILENAME_MAX];
	char command_buffer[10000];

	//Save that this was eligible to be run
	pthread_mutex_lock(&stdout_mutex);
	number_of_fail_to_compile_validation_files++;
	pthread_mutex_unlock(&stdout_mutex);

	//The output file name itself will come from the fully qualified file name
	construct_output_file_name_from_full_path(output_file_name, file_name);

	/**
	 * Use the @ flag to avoid directing this into an output file. We should
	 * just see it fail to compile
	 */
	sprintf(command_buffer, "%s/oc -f %s -o %s > /dev/null 2>&1", output_directory, file_name, output_file_name);

	/**
	 * Run the compilation command. The compiler relies on a shared temporary output file, so we 
	 * need to lock here to make this all work
	 */
	pthread_mutex_lock(&compiler_mutex);
	int32_t compilation_result = system(command_buffer);
	pthread_mutex_unlock(&compiler_mutex);

	/**
	 * If the compilation result was *not* zero, then we have a compilation failure which is good in this case.
	 */
	if(compilation_result != 0){
		pthread_mutex_lock(&stdout_mutex);
		fprintf(stdout, "[Thread %d]: Ran compilation command: %s\n", thread_id, command_buffer);
		fprintf(stdout, "[Thread %d]: The OUNIT configured test %s failed to compile as expected with exit code %d\n\n", thread_id, file_name, compilation_result);
		pthread_mutex_unlock(&stdout_mutex);

	/**
	 * Otherwise, the compilation code was 0. Note that in this case, that is an *error*. We wanted
	 * this to fail. We'll need to record an error for this
	 */
	} else {
		pthread_mutex_lock(&stdout_mutex);
		fprintf(stdout, "[Thread %d]: Ran compilation command: %s\n", thread_id, command_buffer);
		fprintf(stdout, "[Thread %d]: The OUNIT configured test %s was expected to fail, but compiled successfully. Developer attention is required\n\n", thread_id, file_name);
		pthread_mutex_unlock(&stdout_mutex);

		//Add to the array and bump the count
		pthread_mutex_lock(&error_queue_mutex);
		dynamic_array_add(&compiled_when_failure_expected_files, file_name);
		pthread_mutex_unlock(&error_queue_mutex);

		//Bump up the per-thread result
		(*thread_error_count)++;

		/**
		 * Since the file compiled we're going to need to clean this up
		 */
		sprintf(command_buffer, "rm %s", output_file_name);
		int32_t deletion_result = system(command_buffer);

		/**
		 * If somehow this didn't work we should flag it
		 */
		if(deletion_result != 0){
			pthread_mutex_lock(&stdout_mutex);
			fprintf(stdout, "[Thread %d]: Failed to delete output file %s\n", thread_id, output_file_name);
			(*thread_error_count)++;
			pthread_mutex_unlock(&stdout_mutex);
		}
	}
}


/**
 * Our worker thread operates by polling the file list(trying to delete from the back), 
 * and attempting to validate it if it is OUNIT compatible. Once validated it will update
 * the appropriate metrics and loop back around. Once it finds that it cannot grab anything off
 * of the queue it will exit
 */
void* worker(void* thread_parameters) {
	//Test parameters that certain test types require
	test_parameters_t test_parameters;

	//Extract our thread ID
	u_int32_t thread_id = ((thread_parameters_t*)(thread_parameters))->thread_number;

	//Keep track of how many errors this exact thread has seen
	u_int32_t errors_per_thread = 0;

	//Display for debug info
	pthread_mutex_lock(&stdout_mutex);
	fprintf(stdout, "[Thread %d]: Thread has been created and will now start working\n\n", thread_id);
	pthread_mutex_unlock(&stdout_mutex);

	//Loop forever until we hit our exit condition
	while(TRUE){
		char* file_name = NULL;

		/**
		 * Lock the file queue mutex and see if we have anything to validate. Remember that
		 * the delete_from_back returns NULL if the array is empty
		 */
		pthread_mutex_lock(&file_queue_mutex);
		file_name = dynamic_array_delete_from_back(&test_files);
		pthread_mutex_unlock(&file_queue_mutex);

		/**
		 * Exit condition - we no longer have files to validate so we are done. Display for logging
		 * and then get out
		 */
		if(file_name == NULL){
			pthread_mutex_lock(&stdout_mutex);
			fprintf(stdout, "[Thread %d]: Thread has no files left to validate. Thread will now exit\n\n", thread_id);
			pthread_mutex_unlock(&stdout_mutex);
			break;
		}

		/**
		 * We need to tokenize the file to get to any OUNIT directives
		 *
		 * NOTE: the lexer is NOT thread safe!!!! We need to do this in a lock
		 * to avoid bizarre errors
		 *
		 * We have silent mode turned on for this
		 */
		pthread_mutex_lock(&lexer_mutex);
		ollie_token_stream_t token_stream = tokenize(file_name, TRUE);
		pthread_mutex_unlock(&lexer_mutex);

		/**
		 * If it failed(we have stuff that fails) then we need
		 * to skip this one. This is not a big deal for it to fail
		 */
		if(token_stream.status == STREAM_STATUS_FAILURE){
			pthread_mutex_lock(&stdout_mutex);
			fprintf(stdout, "[Thread %d]: File %s has failed to tokenize. This file will not be tested\n", thread_id, file_name);
			pthread_mutex_unlock(&stdout_mutex);
			continue;
		}

		/**
		 * The helper will parse the OUNIT test command(if one exists) and populate
		 * the expected result for later validations
		 */
		ounit_type_t test_type = is_test_OUNIT_compatible(&token_stream, &test_parameters);

		switch(test_type){
			/**
			 * Easiest case just skip the whole thing
			 */
			case OUNIT_TYPE_NONE:
				break;

			/**
			 * Let the helper handle the case where the tester is requesting to run and validate the exit status
			 * for a test
			 */
			case OUNIT_TYPE_EXIT_STATUS_VALIDATION:
				handle_exit_status_validation(thread_id, file_name, &errors_per_thread, &test_parameters);
				break;

			/**
			 * Let the helper handle the case where the tester is requesting that a test should fail to compile
			 */
			case OUNIT_TYPE_FAIL_TO_COMPILE:
				handle_fail_to_compile_validation(thread_id, file_name, &errors_per_thread);
				break;

			/**
			 * This means that the developer tried to make their test OUNIT compatible
			 * but they messed it up somehow
			 */
			case OUNIT_TYPE_INVALID:
				pthread_mutex_lock(&stdout_mutex);
				fprintf(stdout, "[Thread %d]: The file \"%s\" has an incorrect OUNIT configuration and will not be processed\n", thread_id, file_name);
				pthread_mutex_unlock(&stdout_mutex);

				pthread_mutex_lock(&error_queue_mutex);
				dynamic_array_add(&invalid_ounit_configuration_files, file_name);
				pthread_mutex_unlock(&error_queue_mutex);

				//Per-thread tracking
				errors_per_thread++;

				//Onto the next file don't bother compiling
				break;
		}
	}

	//Print our final debug info and then get out
	pthread_mutex_lock(&stdout_mutex);
	fprintf(stdout, "\n-----------------------------------------------\n");
	fprintf(stdout, "[Thread %d]: Work Finished\n", thread_id);
	fprintf(stdout, "Ran but errored: %d\n", errors_per_thread);
	fprintf(stdout, "-----------------------------------------------\n");
	pthread_mutex_unlock(&stdout_mutex);

	//We have nothing to give back
	return NULL;
}


/**
 * Print out a list of all invalid OUNIT configured files, if any exist
 */
static inline void print_invalid_ounit_configuration_summary(){
	/**
	 * If we have any files that were setup incorrectly we should print that now
	 */
	if(invalid_ounit_configuration_files.current_index != 0){
		printf("\n===============================================\n");
		printf("INVALID OUNIT CONFIGURATION DETECTED IN THE FOLLOWING FILES:\n");
		for(int32_t i = 0; i < invalid_ounit_configuration_files.current_index; i++){
			char* file_name = dynamic_array_get_at(&invalid_ounit_configuration_files, i);
			printf("%d) %s\n", i + 1, file_name);
		}
		printf("\n===============================================\n");
	}
}


/**
 * Wrapper that helps us print all exit status OUNIT type statistics
 */
static inline void print_exit_status_validation_summary(){
	printf("\n===============================================\n");
	printf("EXIT STATUS VALIDATION:\n");
	printf("FILES FAILING EXIT STATUS VALIDATION: %d\n", failed_exit_status_validation_files.current_index);
	printf("FILES FAILING TO COMPILE: %d\n", failed_to_compile_exit_status_validation_files.current_index);

	//Only print out if we need to
	if(failed_exit_status_validation_files.current_index > 0){
		printf("\nFILES FAILING EXIT STATUS VALIDATION:\n");

		//Print out all of them
		for(int32_t i = 0; i < failed_exit_status_validation_files.current_index; i++){
			//Get the error file out
			char* error_file_name = dynamic_array_get_at(&failed_exit_status_validation_files, i);
			printf("%d) %s\n", i + 1, error_file_name);
		}
	}

	//Only print out if we need to
	if(failed_to_compile_exit_status_validation_files.current_index > 0){
		printf("\nFAILING TO COMPILE FOR EXIT STATUS VALIDATION:\n");

		//Print out all of them
		for(int32_t i = 0; i < failed_to_compile_exit_status_validation_files.current_index; i++){
			//Get the error file out
			char* failed_to_compile_file = dynamic_array_get_at(&failed_to_compile_exit_status_validation_files, i);
			printf("%d) %s\n", i + 1, failed_to_compile_file);
		}
	}
	printf("===============================================\n");
}


/**
 * Wrapper that helps us print all fail to compile OUNIT type statistics
 */
static inline void print_fail_to_compile_validation_summary(){
	printf("\n===============================================\n");
	printf("COMPILATION FAILURE VALIDATION:\n");
	printf("FILES COMPILING WHEN FAILURE WAS EXPECTED: %d\n", compiled_when_failure_expected_files.current_index);

	//Only print out if we need to
	if(compiled_when_failure_expected_files.current_index > 0){
		printf("\nFILES COMPILING WHEN FAILURE WAS EXPECTED:\n");

		//Print out all of them
		for(int32_t i = 0; i < compiled_when_failure_expected_files.current_index; i++){
			//Get the error file out
			char* failed_to_compile_file = dynamic_array_get_at(&compiled_when_failure_expected_files, i);
			printf("%d) %s\n", i + 1, failed_to_compile_file);
		}
	}
	printf("===============================================\n");
}


/**
 * This helper will run the OUNIT tester for all of our single file tests in
 * the single file test directory. We will be storing fully qualified names
 * in here
 *
 * NOTE: this helper will close the directory when done
 */
static inline void get_all_single_file_tests(char* directory_name){
	//Try to open this and verify that it does in open
	DIR* single_file_tests_directory = opendir(directory_name);
	if(single_file_tests_directory == NULL){
		fprintf(stdout, "Fatal error: failed to open the provided single file test directory %s\n", directory_name);
		exit(1);
	}

	//Directory entry
	struct dirent* directory_entry;

	//So long as we have directory entries to read
	while((directory_entry = readdir(single_file_tests_directory)) != NULL){
		/**
		 * If it's not a regular file we do not want to try and
		 * compile it
		 */
		if(directory_entry->d_type != DT_REG){
			continue;
		}

		//Otherwise let's allocate the string for this
		char* test_file = calloc(FILENAME_MAX, sizeof(char));

		//Print the fully qualified name into here
		snprintf(test_file, FILENAME_MAX, "%s%s", directory_name, directory_entry->d_name);
		
		//Add this to the array of all test files
		dynamic_array_add(&test_files, test_file);
	}

	//Close this out once we're done
	closedir(single_file_tests_directory);
}


/**
 * This helper will run the OUNIT tester for all of our multi file tests
 * in the multi-file test directory. We will be storing fully qualified
 * names in here to maintain distinction, since all files that we are after
 * are called "main.ol"
 *
 * The structure of these is as follows:
 * 	multifile_test_directory/<subdirectory>/main.ol
 *
 * We *only* ever compile files that are called main. This keeps things
 * simple from the end of this grabber
 *
 * NOTE: this helper will close the directory when done
 */
static inline void get_all_multi_file_tests(char* directory_name){
	//Next try to open this and verify that it does open
	DIR* multi_file_tests_directory = opendir(directory_name);
	if(multi_file_tests_directory == NULL){
		fprintf(stdout, "Fatal error: failed to open the provided multi-file test directory %s\n", directory_name);
		exit(1);
	}

	//Holders for directory entries
	struct dirent* directory_entry;
	struct dirent* subdirectory_entry;

	/**
	 * Run through all of the directories in the higher level parent 
	 * directory for multi file tests
	 */
	while((directory_entry = readdir(multi_file_tests_directory)) != NULL){
		/**
		 * For the higher level, we are looking for
		 * nested subdirectories
		 */
		if(directory_entry->d_type != DT_DIR){
			continue;
		}

		//Save the name here
		char* subdirectory = directory_entry->d_name;

		//Open the subdirectory up for searching
		DIR* subdir = opendir(subdirectory);

		//Did we find the main file for this subdirectory or not
		u_int8_t found_main_file_for_subdir = FALSE;

		/**
		 * Run through everything in the subdirectory seeing
		 * if we can find the main file
		 */
		while((subdirectory_entry = readdir(subdir)) != NULL){
			//Only after regular files here
			if(subdirectory_entry->d_type != DT_REG){
				continue;
			}

			/**
			 * If we find the main file, we will flag it and get out of
			 * this loop, there is no point in looking any further
			 */
			if(strcmp(subdirectory_entry->d_name, "main.ol") == 0){
				//Allocate and populate the test file string
				char* test_file = calloc(FILENAME_MAX, sizeof(char));
				snprintf(test_file, FILENAME_MAX, "%s%s/%s", directory_name, subdirectory, directory_entry->d_name);

				//Add this to the array of all test files
				dynamic_array_add(&test_files, test_file);

				found_main_file_for_subdir = TRUE;
				break;
			}
		}

		/**
		 * If we could not find it for this subdirectory, we are going to count
		 * this as an invalid OUNIT configuration
		 */
		if(found_main_file_for_subdir == FALSE){
			//Allocate and populate the test file string
			char* test_file = calloc(FILENAME_MAX, sizeof(char));
			snprintf(test_file, FILENAME_MAX, "%s%s", directory_name, subdirectory);

			//Flag that this directory has an invalid OUNIT config
			dynamic_array_add(&invalid_ounit_configuration_files, test_file);
		}

		//And then close it out
		closedir(subdir);
	}

	//Once done close this out
	closedir(multi_file_tests_directory);
}




/**
 * Entry point. This will perform our setup and call into
 * our threads. We expect that the directory to be used
 * will be provided as a command line argument along with the
 * number of threads to use
 */
int main(int argc, char** argv) {
	u_int32_t return_value = 0;
	u_int32_t test_file_count = 0;

	/**
	 * Find the test file directory. It will have been passed in as a command line argument. If 
	 * it wasn't fail out
	 */
	if(argc < 5){
		fprintf(stdout, "Fatal error: please pass in an executable, a test directory for singular tests, a test directory for multifile tests, and an output directory as a command line argument\n");
		exit(1);
	}

	/**
	 * Step 1: Extract all of our given parameters. They are as follows:
	 * argv[1] = thread count
	 * argv[2] = single file tests directory
	 * argv[3] = multi file tests directory
	 * argv[4] = output directory
	 */
	int32_t thread_count = atoi(argv[1]);
	char* single_file_tests_dir = argv[2];
	char* multi_file_tests_dir = argv[3];
	output_directory = argv[4];

	/**
	 * Step 2: now we can prepare all of the structures that we'll need to do this. These arrays will
	 * be reused for the single & multi-file tests
	 */
	test_files = dynamic_array_alloc_initial_size(DEFAULT_ARRAY_SIZE);
	failed_exit_status_validation_files = dynamic_array_alloc_initial_size(DEFAULT_ARRAY_SIZE);
	failed_to_compile_exit_status_validation_files = dynamic_array_alloc_initial_size(DEFAULT_ARRAY_SIZE);	
	compiled_when_failure_expected_files = dynamic_array_alloc_initial_size(DEFAULT_ARRAY_SIZE);
	invalid_ounit_configuration_files = dynamic_array_alloc_initial_size(DEFAULT_ARRAY_SIZE);

	//Start the clock as we begin our run
	clock_t start_time = clock();

	//Extract all of the tsets
	get_all_single_file_tests(single_file_tests_dir);
	get_all_multi_file_tests(multi_file_tests_dir);

	//Extract this for result printing
	test_file_count = test_files.current_index;

	//==================== Thread Setup =========================
	/**
	 * Reserve heap space for our thread items and our parameters
	 */
	pthread_t* threads = calloc(thread_count, sizeof(pthread_t));
	thread_parameters_t* parameters = calloc(thread_count, sizeof(thread_parameters_t));

	/**
	 * Create our thread ID's and spawn the pthread workers
	 */
	for(int32_t i = 0; i < thread_count; i++){
		//Thread number is just for debugging but we do store it
		parameters[i].thread_number = i;

		//Now that we have done everything here, it is time to spawn the thread
		pthread_create(&(threads[i]), NULL, worker, &(parameters[i]));
	}

	/**
	 * Now that we have spawned all of our threads
	 * we wait in this blocking loop for them
	 * to finish
	 */
	for(int32_t i = 0; i < thread_count; i++){
		pthread_join(threads[i], NULL);
	}

	//Now that the work has been done these are useless
	free(threads);
	free(parameters);

	//Record the final time
	clock_t stop_time = clock();
	double time_taken = (double)(stop_time - start_time) / CLOCKS_PER_SEC;

	//Record the total number of errors
	u_int32_t total_error_count = compiled_when_failure_expected_files.current_index 
								+ failed_to_compile_exit_status_validation_files.current_index 
								+ failed_exit_status_validation_files.current_index
								+ invalid_ounit_configuration_files.current_index;

	printf("\n\n\n\n\n\n================================ Ollie Run Validation Summary =================================== \n");
	printf("FILES CONSIDERED: %d\n", test_file_count);
	printf("FILES ELIGIBLE FOR EXIT STATUS VALIDATION: %d\n", number_of_exit_status_validation_files);
	printf("FILES ELIGIBLE FOR COMPILATION FAILURE VALIDATION: %d\n", number_of_fail_to_compile_validation_files);
	printf("TOTAL ELIGIBLE FILE COUNT: %d\n", number_of_ounit_compatible_files);
	printf("CPU TIME ELAPSED: %.4f seconds\n", time_taken);

	//Use the helpers to print out all of our summaries
	print_invalid_ounit_configuration_summary();
	print_exit_status_validation_summary();
	print_fail_to_compile_validation_summary();

	//Flag that the developer needs to look at this
	if(total_error_count != 0){
		printf("\n\nFAILURES DETECTED: DEVELOPER ATTENTION IS REQUIRED\n");

		//1 for error
		return_value = 1;
	}

	printf("\n================================ Ollie Run Validation Summary =================================== \n");

	//Destroy all of our mutices
	pthread_mutex_destroy(&file_queue_mutex);
	pthread_mutex_destroy(&stdout_mutex);
	pthread_mutex_destroy(&lexer_mutex);
	pthread_mutex_destroy(&compiler_mutex);
	pthread_mutex_destroy(&error_queue_mutex);

	return return_value;
}
