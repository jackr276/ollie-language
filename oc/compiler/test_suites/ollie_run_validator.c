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
 *  we have a special tag TODO WHAT IS THE TAG. If so, we will compile the file using oc, run it
 *  inside of a subshell, and compare the result with the expected result. If they match we've succeeded,
 *  if they don't then we have a failure and we put it inside of the error file list
 */

//Use our own in-house dynamic array
#include "../utils/dynamic_array/dynamic_array.h"
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

/**
 * A generic array containing *all* of our test files that will be worked over
 * by the threads. We then also maintain a list of all files that are found
 * to be in error. This is done for the final summary
 */
dynamic_array_t test_files;
dynamic_array_t error_files;

//Keep track of the total number of files and the total error files
u_int32_t number_of_test_files = 0;
u_int32_t number_of_error_files = 0;

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
};


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
	char* directory_path = argv[2];
	DIR* directory = opendir(directory_path);

	//Check that we got it
	if(directory == NULL){
		fprintf(stderr, "Fatal error: failed to open directory %s\n", directory_path);
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
	fprintf(stdout, "%d threads requested for validate %d test files. Each thread will valid %d files\n", thread_count, number_of_test_files, files_per_thread);
	fprintf(stdout, "================================= Run Setup =================================\n");

	//Inclusive start index for our current thread
	u_int32_t current_thread_file_index = 0;

	/**
	 * Spawn every thread with the appropriate start(inclusive)
	 * index and end(exclusive) index for the array
	 */
	for(u_int32_t i = 0; i < thread_count; i++){

	}

	//Destroy the two mutices
	pthread_mutex_destroy(&error_list_mutex);
	pthread_mutex_destroy(&output_mutex);
}
