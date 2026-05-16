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
//Max file name size on linux
#define LINUX_MAX_FILE_NAME_LENGTH 300

//We'll need a mutex for all of the files that we wish to operate on
pthread_mutex_t output_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lexer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t compiler_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * A generic array containing *all* of our test files that will be worked over
 * by the threads. We then also maintain a list of all files that are found
 * to be in error. This is done for the final summary
 */
static dynamic_array_t test_files;
static dynamic_array_t error_files;
static dynamic_array_t failed_to_compile_files;

//Keep track of the total number of files and the total error files
static u_int32_t number_of_test_files = 0;
static u_int32_t number_of_failed_to_compile = 0;
static u_int32_t number_of_error_files = 0;

//Hold onto the overall directory path
static char* test_file_dir;

/**
 * Each thread has a unique start and end index that 
 * they need to iterate over. This struct contains
 * that and will be passed along to each worker
 * thread as a parameter
 */
typedef struct thread_parameters_t thread_parameters_t;
struct thread_parameters_t {
	//Start is inclusive
	u_int32_t start_index;
	//End is exclusive
	u_int32_t end_index;
	//Thread number
	u_int8_t thread_number;
};

/**
 * For OUNIT compatibility, we have 3 possible
 * scenarios:
 * 	1.) Not at all compatible
 * 	2.) Compatible
 * 	3.) Compatible but incorrect syntax
 * This enumeration represents all possible states
 */
typedef enum {
	OUNIT_NOT_COMPATIBLE,
	OUNIT_COMPATBILE,
	OUNIT_COMPATIBLE_BUT_INVALID
} ounit_compatibility_status_t;


/**
 * Parser the OUNIT test command. If the command is found to be invalid, we return a state 
 * that represents said invalidity. Otherwise, we will store the result that we expect(must
 * be an integer) inside of the passed parameter pointer
 *
 * NOTE: By the time we get here we've already seen and advanced the pointer past the OUNIT 
 * token
 *
 * Example OUNIT command: OUNIT: [console = 5]
 *
 * This tells us that the final console return value(echo $?) of the test should be 5
 *
 * This is currently the only test type that OUNIT supports. More may be added later on
 * if it is determined that other scenarios should be tested
 */
static inline u_int8_t parse_OUNIT_test_command(ollie_token_array_t* tokens, int32_t index, int32_t* expected_result){
	//Generic pointer for our lexitem
	lexitem_t* lexitem = token_array_get_pointer_at(tokens, index);

	/**
	 * We need to see a colon here. If we don't then we fail 
	 * out now
	 */
	if(lexitem->tok != COLON){
		pthread_mutex_lock(&output_mutex);
		fprintf(stdout, "Expected \":\" but got \"%s\" instead\n", lexitem_to_string(lexitem));
		pthread_mutex_unlock(&output_mutex);

		return OUNIT_COMPATIBLE_BUT_INVALID;
	}

	//Otherwise bump the index up
	index++;
	lexitem = token_array_get_pointer_at(tokens, index);

	/**
	 * Again another fail case here, we need to see an L_BRACKET
	 */
	if(lexitem->tok != L_BRACKET){
		pthread_mutex_lock(&output_mutex);
		fprintf(stdout, "Expected \"[\" but got \"%s\" instead\n", lexitem_to_string(lexitem));
		pthread_mutex_unlock(&output_mutex);

		return OUNIT_COMPATIBLE_BUT_INVALID;
	}

	index++;
	lexitem = token_array_get_pointer_at(tokens, index);

	/**
	 * We now need to see an identifier that says "console". If we don't
	 * then we are done with this
	 */
	if((lexitem->tok != IDENT) || (strcmp(lexitem->lexeme.string, "console") != 0)){
		pthread_mutex_lock(&output_mutex);
		fprintf(stdout, "Expected \"console\" but got \"%s\" instead\n", lexitem_to_string(lexitem));
		pthread_mutex_unlock(&output_mutex);

		return OUNIT_COMPATIBLE_BUT_INVALID;
	}





	//TODO FIXME
	return OUNIT_COMPATIBLE_BUT_INVALID;
}


/**
 * Is the given test compatible with OUNIT? We will determine
 * this by scanning for the OUNIT token inside of the tests
 * token array. If the test is determined to be OUNIT compatible, we will
 * also piggy-back off of this function to parse and get out what we
 * expect the actual result of the test to be
 */
static ounit_compatibility_status_t is_test_OUNIT_compatible(ollie_token_stream_t* stream, int32_t* expected_result){
	//Run through and see if we can find the OUNIT token
	for(u_int32_t i = 0; i < stream->token_stream.current_index; i++){
		//Extract the token pointer
		lexitem_t* lexitem = token_array_get_pointer_at(&(stream->token_stream), i);

		//If we see the OUNIT token then we will let the helper determine its compatibilty
		if(lexitem->tok == OUNIT){
			return parse_OUNIT_test_command(stream, i + 1, expected_result);
		}
	}

	//If we made it all the way down here then it is not OUNIT compatible
	return OUNIT_NOT_COMPATIBLE;
}

/**
 * A worker thread operates over a given range of files before it completes. The thread
 * parameters(passed in via reference through the void pointer) include the inclusive
 * start index in the test file array and the exclusive end index in the test file array
 */
void* worker(void* thread_parameters) {
	//The output file name that we will be using
	char output_file_name[1000];
	//A path for the fully qualified file
	char fully_qualified_file_name[1000];
	//The command to use OC to compile
	char compilation_command[3000];

	//Implicit case it over
	thread_parameters_t* parameters = thread_parameters;

	//Extract the two indices that we'll be using
	u_int32_t start_index = parameters->start_index;
	u_int32_t end_index = parameters->end_index;
	u_int32_t thread_id = parameters->thread_number;

	//Keep track of how many errors this exact thread has seen
	u_int32_t errors_per_thread = 0;
	u_int32_t failed_to_compile_per_thread = 0;

	//Display for debug info
	pthread_mutex_lock(&output_mutex);
	fprintf(stdout, "[Thread %d]: Thread has been assigned to validate files with indices in range [%d, %d) and will now start working\n\n", thread_id, start_index, end_index);
	pthread_mutex_unlock(&output_mutex);

	/**
	 * Now run through every file that is within this threads assignment.
	 * All of the work for each file within this given range will be handled
	 * by this thread
	 */
	for(u_int32_t i = start_index; i < end_index; i++){
		//Get the file that we're after
		char* file_name = dynamic_array_get_at(&test_files, i);

		//Generate the *.test file name for the compiled file
		sprintf(output_file_name, "%s.test", file_name);

		//Construct the fully qualified file name
		sprintf(fully_qualified_file_name, "%s%s", test_file_dir, file_name);

		/**
		 * We need to tokenize the file to get to any OUNIT directives
		 *
		 * NOTE: the lexer is NOT thread safe!!!! We need to do this in a lock
		 * to avoid bizarre errors
		 */
		pthread_mutex_lock(&lexer_mutex);
		ollie_token_stream_t token_stream = tokenize(fully_qualified_file_name);
		pthread_mutex_unlock(&lexer_mutex);

		/**
		 * If it failed(we have stuff that fails) then we need
		 * to skip this one. This is not a big deal for it to fail
		 */
		if(token_stream.status == STREAM_STATUS_FAILURE){
			pthread_mutex_lock(&output_mutex);
			fprintf(stdout, "[Thread %d]: File %s has failed to tokenize\n", thread_id, file_name);
			pthread_mutex_unlock(&output_mutex);
			continue;
		}

		/**
		 * The helper will parse the OUNIT test command(if one exists) and populate
		 * the expected result for later validations
		 */
		int32_t expected_result;
		ounit_compatibility_status_t compatibility = is_test_OUNIT_compatible(&token_stream, &expected_result);

		switch(compatibility){
			/**
			 * Easiest case just skip the whole thing
			 */
			case OUNIT_NOT_COMPATIBLE:
				continue;

			/**
			 * Another easy case. The only thing that we want to validate is that the
			 * expected result is actually a valid value. Remember that on UNIX, 
			 * the highest return value from a shell is 255
			 */
			case OUNIT_COMPATBILE:
				/**
				 * We'll have this fail. We don't want to let the developer waste
				 * time confused as to why this isn't working
				 */
				if(expected_result >= 255){
					pthread_mutex_lock(&output_mutex);
					fprintf(stdout, "[Thread %d]: File \"%s\" - The OUNIT expected return value of %d is higher than the maximum UNIX return value of %d. Please remediate your test\n", thread_id, file_name, expected_result, 255);

					//Add to the error array
					dynamic_array_add(&error_files, file_name);
					number_of_error_files++;
					errors_per_thread++;
					pthread_mutex_unlock(&output_mutex);

					//Onto the next file
					continue;
				}

				break;

			/**
			 * This means that the developer tried to make their test OUNIT compatible
			 * but they messed it up somehow
			 */
			case OUNIT_COMPATIBLE_BUT_INVALID:
				pthread_mutex_lock(&output_mutex);
				fprintf(stdout, "[Thread %d]: The file \"%s\" has an incorrect OUNIT configuration and will not be processed\n", thread_id, file_name);

				//Add to the error array
				dynamic_array_add(&error_files, file_name);
				number_of_error_files++;
				errors_per_thread++;
				pthread_mutex_unlock(&output_mutex);

				//Onto the next file don't bother compiling
				continue;
		}

		/**
		 * Otherwise it is compatible so we will begin our testing
		 * here by first compiling the actual item
		 */
		sprintf(compilation_command, "./oc/out/oc -f %s%s -o %s > /dev/null 2>&1", test_file_dir, file_name, output_file_name);

		/**
		 * Run the compilation command. The compiler relies on a shared temporary output file, so we 
		 * need to lock here to make this all work
		 */
		pthread_mutex_lock(&compiler_mutex);
		int32_t compilation_result = system(compilation_command);
		pthread_mutex_unlock(&compiler_mutex);

		/**
		 * If for any reason we have a failure here, then
		 * we will note a compilation failure and move on
		 */
		if(compilation_result != 0){
			pthread_mutex_lock(&output_mutex);
			fprintf(stdout, "[Thread %d]: Ran compilation command: %s\n", thread_id, compilation_command);
			fprintf(stdout, "[Thread %d]: The OUNIT configured test %s failed to compile with exit code %d. Developer attention is required\n\n", thread_id, file_name, compilation_result);

			//Store this in the list of files that failed to compile when they should have
			dynamic_array_add(&failed_to_compile_files, file_name);
			number_of_failed_to_compile++;
			failed_to_compile_per_thread++;
			pthread_mutex_unlock(&output_mutex);

			//Onto the next one
			continue;
		}

		/**
		 * Display our output to the console as to what we ran
		 * and what we got out of it
		 */
		pthread_mutex_lock(&output_mutex);
		pthread_mutex_unlock(&output_mutex);

		/**
		 * Delete the output file now that we are done with it. Output file
		 * names should be unique so in theory we should not have to lock this
		 */
		sprintf(compilation_command, "rm %s", output_file_name);
		int32_t deletion_result = system(compilation_command);

		/**
		 * If somehow this didn't work we should flag it
		 */
		if(deletion_result != 0){
			pthread_mutex_lock(&output_mutex);
			fprintf(stdout, "[Thread %d]: Failed to delete output file %s\n", thread_id, output_file_name);
		   	pthread_mutex_unlock(&output_mutex);
		}
	}


	//Print our final debug info and then get out
	pthread_mutex_lock(&output_mutex);
	fprintf(stdout, "\n-----------------------------------------------\n");
	fprintf(stdout, "[Thread %d]: Work Finished\n", thread_id);
	fprintf(stdout, "Files Processed: %d\n", end_index - start_index);
	fprintf(stdout, "Failed to compile: %d\n", failed_to_compile_per_thread);
	fprintf(stdout, "Ran but errored: %d\n", errors_per_thread);
	fprintf(stdout, "-----------------------------------------------\n");
	pthread_mutex_unlock(&output_mutex);

	//We have nothing to give back
	return NULL;
}


/**
 * Entry point. This will perform our setup and call into
 * our threads. We expect that the directory to be used
 * will be provided as a command line argument along with the
 * number of threads to use
 */
int main(int argc, char** argv) {
	/**
	 * Find the test file directory. It will have been passed in as a command line argument. If 
	 * it wasn't fail out
	 */
	if(argc < 3){
		fprintf(stdout, "Fatal error: please pass in an executable and a test directory as a command line argument\n");
		exit(1);
	}

	//Get the thread count - very rough - I'm not really concerned about user-friendliness with this
	int32_t thread_count = atoi(argv[1]);

	//Extract it and open it
	test_file_dir = argv[2];
	DIR* directory = opendir(test_file_dir);

	//Check that we got it
	if(directory == NULL){
		fprintf(stdout, "Fatal error: failed to open directory %s\n", test_file_dir);
		exit(1);
	}

	//Create our two dynamic arrays with initial sizes
	test_files = dynamic_array_alloc_initial_size(DEFAULT_ARRAY_SIZE);
	failed_to_compile_files = dynamic_array_alloc_initial_size(DEFAULT_ARRAY_SIZE);
	error_files = dynamic_array_alloc_initial_size(DEFAULT_ARRAY_SIZE);

	//Start the clock as we begin our run
	clock_t start_time = clock();

	//Directory entry
	struct dirent* directory_entry;

	//So long as we have directory entries to read
	while((directory_entry = readdir(directory)) != NULL){
		//If we see "." or ".." we leave
		if(directory_entry->d_name[0] == '.'){
			continue;
		}

		//Otherwise let's allocate the string for this
		char* test_file = calloc(LINUX_MAX_FILE_NAME_LENGTH, sizeof(char));

		//Copy the directory name over to this
		strncpy(test_file, directory_entry->d_name, LINUX_MAX_FILE_NAME_LENGTH * sizeof(char));
		
		//Add this to the array of all test files
		dynamic_array_add(&test_files, test_file);

		//One more test file seen
		number_of_test_files++;
	}

	/**
	 * Now that we have gathered everything that we are needing to validate, we will divide
	 * up the dynamic array into equal(mostly) sized chunks of things to validate. The last
	 * thread may have slightly less or slightly more work to do depending on how the division
	 * works out but that is not a big deal
	 */
	u_int32_t files_per_thread = number_of_test_files / thread_count;

	fprintf(stdout, "\n\n================================= Run Setup =================================\n");
	fprintf(stdout, "%d threads requested to validate %d test files. Each thread will validate %d files\n", thread_count, number_of_test_files, files_per_thread);
	fprintf(stdout, "================================= Run Setup =================================\n");

	//Inclusive start index for our current thread
	u_int32_t current_thread_file_index = 0;

	//==================== Thread Setup =========================
	/**
	 * Reserve heap space for our thread items and our parameters
	 */
	pthread_t* threads = calloc(thread_count, sizeof(pthread_t));
	thread_parameters_t* parameters = calloc(thread_count, sizeof(thread_parameters_t));

	/**
	 * Spawn every thread with the appropriate start(inclusive)
	 * index and end(exclusive) index for the array
	 */
	for(u_int32_t i = 0; i < thread_count; i++){
		//This is the start index that we maintain
		parameters[i].start_index = current_thread_file_index;

		/**
		 * If we are not the very last thread, we will increment
		 * normally. If this is the very last thread, we'll
		 * need to handle a bit more or a bit less because
		 * odds are the division of files by threadcount is not
		 * even
		 */
		if(i != thread_count - 1){
			current_thread_file_index += files_per_thread;
		} else {
			current_thread_file_index = number_of_test_files;
		}

		//Whatever it ended up being give it to the parameter array
		parameters[i].end_index = current_thread_file_index;

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
	for(u_int32_t i = 0; i < thread_count; i++){
		pthread_join(threads[i], NULL);
	}

	//Now that the work has been done these are useless
	free(threads);
	free(parameters);

	//Destroy the three mutices
	pthread_mutex_destroy(&output_mutex);
	pthread_mutex_destroy(&lexer_mutex);
	pthread_mutex_destroy(&compiler_mutex);
}
