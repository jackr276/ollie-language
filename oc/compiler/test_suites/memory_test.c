/**
* Author: Jack Robbins
* This specialized test suite will use the "valgrind" command
* to run through every single test suite and check to see what kind of
* memory errors that we have, if any at all
*/

#include <linux/limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
//For multithreading
#include <pthread.h>
#include <time.h>
//For any needed constants
#include "../utils/dynamic_array/dynamic_array.h"

//Maximum size of a given file name in linux
#define MAX_FILE_NAME_SIZE 300

//By default we'll allocate 1000 slots for our test file array
#define DEFAULT_ARRAY_SIZE 1000

//Mutex for stdout/error file queue
pthread_mutex_t result_mutex = PTHREAD_MUTEX_INITIALIZER;

//Total number of errors we have
u_int32_t total_errors = 0;
//Number of files in error
u_int32_t number_of_error_files = 0;
//The current test file index
u_int32_t current_test_file_index;
//The total number of actual test files
u_int32_t total_test_files = 0;

//Dynamic arrays to hold the test files and the files in error
dynamic_array_t test_files;
dynamic_array_t files_in_error;

/**
 * The thread parameters will pass along the inclusive start index
 * and exclusive end index to tell the thread where to focus its
 * energy
 */
typedef struct thread_parameters_t thread_parameters_t;
struct thread_parameters_t {
	//Array start(inclusive)
	u_int32_t start_index;
	//Array end(exclusive)
	u_int32_t end_index;
	//Thread ID
	u_int8_t thread_number;
};


/**
 * Each worker thread is assigned its own range of files to work on. Since these ranges
 * are unique, we do not need to worry about locking the file queue which is a big benefit
 * to this approach
 *
 * Worker thread:
 *
 * for i in range [start, end)
 * 		Get file at i
 *
 * 		Does the work
 *
 * 		Locks the result mutex
 *		Updates the results
 *		Unlocks the result mutex
 * }
 *
 * Thread exit
 */
void* worker(void* thread_parameters){
	//For creating our commands
	char command[3000];

	//Extract/cast our actual thread parameters
	thread_parameters_t* parameters = thread_parameters;

	//For ease of use extract the start and end indices
	u_int32_t start_index = parameters->start_index;
	u_int32_t end_index = parameters->end_index;

	//How many errors have we specifically seen in this thread
	u_int32_t num_errors_for_thread = 0;

	/**
	 * Display for the user what files we are working on inside of this specific thread
	 */
	pthread_mutex_lock(&result_mutex);
	fprintf(stdout, "Thread %d was assigned to work on files in range [%d, %d) and will now start working\n\n", parameters->thread_number, start_index, end_index);
	pthread_mutex_unlock(&result_mutex);

	/**
	 * Run through every file that was assigned to use
	 * in the file array
	 */
	for(u_int32_t i = start_index; i < end_index; i++){
		//Extract the file at this index
		char* file_name = dynamic_array_get_at(&test_files, i);

		//Our command. We use 2>&1 to write all errors to stdout so that we can grep it
		sprintf(command, "exit $(valgrind ./oc/out/ocd -ditsa@ -f ./oc/test_files/%s 2>&1 | grep \"SUMMARY\" | sed -n 's/.*ERROR SUMMARY: \\([0-9]\\+\\).*/\\1/p')", file_name);

		//Run the command in the system
		int32_t command_return_code = system(command);

		/**
		 * Lock the result mutex. This also doubles as a mutex for stdout. We delay
		 * printing anything until we're in here to keep results consistent
		 */
		pthread_mutex_lock(&result_mutex);

		printf("\n=========== Checking %s =================\n", file_name);
		printf("Running test command: %s\n\n", command);

		//Assume we have no errors
		int32_t num_errors_for_file= 0;

		//If we exited, get the exit code
		if(WIFEXITED(command_return_code)){
			//Get the actual exit code(this will be the error number)
			num_errors_for_file = WEXITSTATUS(command_return_code);
			
		//If we got a bad signal, get out
		} else if (WIFSIGNALED((command_return_code))){
			printf("ERROR: command terminated with signal %d\n", WTERMSIG(command_return_code));
		}

		//Bookkeeping for final printing
		if(num_errors_for_file > 0){
			//Add the pointer for this into the list - we do no memory management so this is fine
			dynamic_array_add(&files_in_error, file_name);

			//Bump up the count
			number_of_error_files++;
		}

		//Update the total error count
		total_errors += num_errors_for_file;

		//Get the error count out
		printf("\nTEST FILE: %s -> %d ERRORS\n", file_name, num_errors_for_file);
		printf("\n=========================================\n");

		//Unlock the result mutex
		pthread_mutex_unlock(&result_mutex);

		//Update the number of errors that this thread has
		num_errors_for_thread += num_errors_for_file;
	}

	/**
	 * Display visually that the thread has completed it's work and will now exit
	 */
	pthread_mutex_lock(&result_mutex);
	fprintf(stdout, "Thread %d has validated files in range [%d, %d] and found %d errors", parameters->thread_number, start_index, end_index, num_errors_for_thread);
	pthread_mutex_unlock(&result_mutex);

	//Return value is not important
	return NULL;
}


/**
* Hook in and run via the main function. We will be relying
* on make to verify that certain rules are precompiled for us
*/
int main(int argc, char** argv){
	//Do we even have valgrind - if this returns 1 we don't so get out
	int valgrind_found = system("which valgrind");

	//We can't go any further
	if(valgrind_found != 0){
		printf("Fatal error: Valgrind is not installed on this system. Please install valgrind before proceeding\n");
		exit(1);
	}

	//Find the test file directory. It will have been passed in as a command line argument. If 
	//it wasn't fail out
	if(argc < 3){
		printf("Fatal error: please pass in an executable and a test directory as a command line argument\n");
		exit(1);
	}

	//Get the thread count - very rough - I'm not really concerned about user-friendliness with this
	int thread_count = atoi(argv[1]);

	//Extract it and open it
	char* directory_path = argv[2];
	DIR* directory = opendir(directory_path);

	//Check that we got it
	if(directory == NULL){
		printf("Fatal error: failed to open directory %s\n", directory_path);
		exit(1);
	}

	//Create our two dynamic arrays with initial sizes
	test_files = dynamic_array_alloc_initial_size(DEFAULT_ARRAY_SIZE);
	files_in_error = dynamic_array_alloc_initial_size(DEFAULT_ARRAY_SIZE);

	//Start the time
	clock_t start_time = clock();

	//Run through everything in the given directory
	struct dirent* directory_entry;

	//Initialize the total test file counts
	total_test_files = 0;

	/**
	 * So long as we can keep reading from the directory, we will stuff the
	 * queue with what we need
	 */
	while((directory_entry = readdir(directory)) != NULL){
		//If we see '.' or '..' bail out
		if(directory_entry->d_name[0] == '.'){
			continue;
		}

		//Allocate space for the file name
		char* file_name = calloc(MAX_FILE_NAME_SIZE, sizeof(char));

		//Copy it over
		strncpy(file_name, directory_entry->d_name, MAX_FILE_NAME_SIZE * sizeof(char));

		//Add this into the dynamic array
		dynamic_array_add(&test_files, file_name);

		//Increment the test file number
		total_test_files++;
	}

	//Close the directory
	closedir(directory);

	/**
	 * Each thread will have an equal amount of files, with the exception for the last
	 * thread, which may have more or less because the division may be uneven
	 */
	u_int32_t files_per_thread = total_test_files / thread_count;

	//Display for the user
	fprintf(stdout, "\n===================================== SETUP ================================\n");
	fprintf(stdout, "THREADS: %d\n", thread_count);
	fprintf(stdout, "DIRECTORY: %s\n", directory_path);
	fprintf(stdout, "Memory Checker will spawn %d threads to work on %d files each\n", thread_count, files_per_thread);
	fprintf(stdout, "\n===================================== SETUP ================================\n\n");

	//=========================== Setup for threads ============================
	
	//Space for our threads
	pthread_t* threads = calloc(thread_count, sizeof(pthread_t));
	thread_parameters_t* parameters = calloc(thread_count, sizeof(thread_parameters_t));

	/**
	 * For each thread we will give a range of indices [start, end) to
	 * work on. For the last thread, we will need to give whatever
	 * is left to ensure that we actually get all of the files
	 */
	u_int32_t current_start_index = 0;
	for(u_int32_t i = 0; i < thread_count; i++){
		//Unique thread id
		parameters[i].thread_number = i;
		parameters[i].start_index = current_start_index;

		//Special handling is needed for the very last thread
		if(i != thread_count - 1){
			current_start_index += files_per_thread;
			parameters[i].end_index = current_start_index;

		} else {
			parameters[i].end_index = total_test_files;
		}

		//Create the thread and pass along the parameters that we've just made
		pthread_create(&(threads[i]), NULL, worker, &(parameters[i]));
	}

	//Wait for them all to join
	for(u_int32_t i = 0; i < thread_count; i++){
		pthread_join(threads[i], NULL);
	}

	//Get rid of all these threads
	free(threads);
	free(parameters);

	//Record the final time
	clock_t stop_time = clock();
	double time_taken = (double)(stop_time - start_time) / CLOCKS_PER_SEC;

	printf("\n\n\n\n\n\n================================ Ollie Memory Check Summary =================================== \n");
	printf("FILES TESTED: %d\n", total_test_files);
	printf("CPU TIME ELAPSED: %.4f seconds\n", time_taken);
	printf("TOTAL ERRORS: %d\n", total_errors);
	printf("TOTAL ERROR FILES: %d\n", number_of_error_files);

	//Only print out if we need to
	if(total_errors > 0){
		printf("FILES IN ERROR:\n");

		//Print out all of them
		for(u_int32_t i = 0; i < number_of_error_files; i++){
			//Get the error file out
			char* error_file_name = dynamic_array_get_at(&files_in_error, i);

			printf("%d) %s\n", i, error_file_name);
		}

		//One final error for emphasis
		printf("\n\nMEMORY CHECK FAILURE: DEVELOPER ATTENTION IS REQUIRED\n\n");

	} else {
		printf("ALL TEST CASES CLEAN\n");
	}

	printf("================================ Ollie Memory Check Summary =================================== \n\n\n\n\n\n");

	//Destroy the mutices
	pthread_mutex_destroy(&result_mutex);

	dynamic_array_dealloc(&test_files);
	dynamic_array_dealloc(&files_in_error);
	
	//All went well
	return total_errors;
}
