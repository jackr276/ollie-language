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
//Max file name size on linux
#define LINUX_MAX_FILE_NAME_LENGTH 300

//We'll need a mutex for all of the files that we wish to operate on
pthread_mutex_t error_list_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t output_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lexer_mutex = PTHREAD_MUTEX_INITIALIZER;

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
 * Is the given test compatible with OUNIT? We will determine
 * this by scanning for the OUNIT token inside of the tests
 * token array
 */
static inline u_int8_t is_test_OUNIT_compatible(ollie_token_stream_t* stream){
	//Run through and see if we can find the OUNIT token
	for(u_int32_t i = 0; i < stream->token_stream.current_index; i++){
		//Extract the token pointer
		lexitem_t* lexitem = token_array_get_pointer_at(&(stream->token_stream), i);

		//We got one so we are done
		if(lexitem->tok == OUNIT){
			return TRUE;
		}
	}

	//If we made it all the way down here then it is not OUNIT compatible
	return FALSE;
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

	//Keep track of how many errors this exact thread has seen
	u_int32_t errors_per_thread = 0;

	//Display for debug info
	pthread_mutex_lock(&output_mutex);
	fprintf(stdout, "Thread %d has been assigned to validate files with indices in range [%d, %d) and will now start working\n\n", parameters->thread_number, start_index, end_index);
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
			fprintf(stdout, "File %s has failed to tokenize\n", file_name);
			pthread_mutex_unlock(&output_mutex);
			continue;
		}

		/**
		 * It is not OUNIT compatible so we skip it
		 */
		if(is_test_OUNIT_compatible(&token_stream) == FALSE){
			continue;
		}

		/**
		 * Otherwise it is compatible so we will begin our testing
		 * here by first compiling the actual item
		 */
		sprintf(compilation_command, "exit $(./oc/out/oc -f %s%s -o %s)", test_file_dir, file_name, output_file_name);

		//Run the command in the system
		int32_t compilation_result = system(compilation_command);

		/**
		 * If for any reason we have a failure here, then
		 * we will note a compilation failure and move on
		 */
		if(compilation_result != 0){
			pthread_mutex_lock(&output_mutex);
			fprintf(stdout, "Ran compilation command: %s\n", compilation_command);
			fprintf(stdout, "The OUNIT configured test %s failed to compile with exit code %d. Developer attention is required\n\n", file_name, compilation_result);

			//Store this in the list of files that failed to compile when they should have
			dynamic_array_add(&failed_to_compile_files, file_name);
			number_of_failed_to_compile++;
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
	}


	//Print our final debug info and then get out
	pthread_mutex_lock(&output_mutex);
	fprintf(stdout, "Thread %d has finished working. Validated %d files, found %d in error\n", parameters->thread_number, end_index - start_index, errors_per_thread);
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
		fprintf(stderr, "Fatal error: please pass in an executable and a test directory as a command line argument\n");
		exit(1);
	}

	//Get the thread count - very rough - I'm not really concerned about user-friendliness with this
	int32_t thread_count = atoi(argv[1]);

	//Extract it and open it
	test_file_dir = argv[2];
	DIR* directory = opendir(test_file_dir);

	//Check that we got it
	if(directory == NULL){
		fprintf(stderr, "Fatal error: failed to open directory %s\n", test_file_dir);
		exit(1);
	}

	//Create our two dynamic arrays with initial sizes
	test_files = dynamic_array_alloc_initial_size(DEFAULT_ARRAY_SIZE);
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
	pthread_mutex_destroy(&error_list_mutex);
	pthread_mutex_destroy(&output_mutex);
	pthread_mutex_destroy(&lexer_mutex);
}
