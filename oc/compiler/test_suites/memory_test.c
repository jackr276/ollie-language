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
#include "../utils/constants.h"
#include "../utils/dynamic_array/dynamic_array.h"

//By default we'll allocate 1000 slots for our test file array
#define DEFAULT_ARRAY_SIZE 1000

//Need two mutices - one for the results and one for our file list
pthread_mutex_t result_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t file_list_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * What is the desired output directory for us(./oc/out, $$RUNNER_TEMP, etc)
 */
static char* output_directory; 

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
 * All that our thread parameter does is pass along the thread number
 */
typedef struct thread_parameters_t thread_parameters_t;
struct thread_parameters_t {
	u_int8_t thread_number;
};


/**
 * Each worker thread will poll the file list and see if there is a test file to work on. If there
 * is, we will run the test on it. If not, then we will exit
 *
 * Worker thread:
 *
 * while true:
 * 		if worklist is empty then:
 * 			thread exit
 *
 * 		file = delete file from worklist
 * 		run test on file
 */
void* worker(void* thread_parameters){
	//For creating our commands
	char command[3000];

	//Extract the thread number
	u_int8_t thread_number = ((thread_parameters_t*)(thread_parameters))->thread_number;

	//How many errors have we specifically seen in this thread
	u_int32_t num_errors_for_thread = 0;

	/**
	 * Display for the user what files we are working on inside of this specific thread
	 */
	pthread_mutex_lock(&result_mutex);
	fprintf(stdout, "\n\nThread %d has been created and will begin validations\n\n\n", thread_number);
	pthread_mutex_unlock(&result_mutex);

	//Keep looping so long as we have files to validate
	while(TRUE){
		//Our file name
		char* file_name = NULL;

		/**
		 * See if we are able to pop a file off the back. If the list
		 * is empty, we will get a NULL pointer which is how we know to exit
		 */
		pthread_mutex_lock(&file_list_mutex);
		file_name = dynamic_array_delete_from_back(&test_files);
		pthread_mutex_unlock(&file_list_mutex);

		/**
		 * No file left - this is our exit condition for the thread
		 */
		if(file_name == NULL){
			pthread_mutex_lock(&result_mutex);
			fprintf(stdout, "\n\nThread %d has no files left to validate and will now exit\n\n\n", thread_number);
			pthread_mutex_unlock(&result_mutex);

			return NULL;
		}

		/**
		 * Otherwise, we have a file that we need to validate so we will go ahead and do that now
		 * First generate our command. We use 2>&1 to write all errors to stdout so that we can grep it
		 */
		sprintf(command, "exit $(valgrind %s/ocd -ditsa@ -f %s%s 2>&1 | grep \"SUMMARY\" | sed -n 's/.*ERROR SUMMARY: \\([0-9]\\+\\).*/\\1/p')",
		  				is_ci_run == 0 ? local_output_path : ci_output_path,
		  				test_directory_path,
		  				file_name);

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
			fprintf(stdout, "ERROR: command terminated with signal %d\n", WTERMSIG(command_return_code));
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
		fprintf(stdout, "\nTEST FILE: %s -> %d ERRORS\n", file_name, num_errors_for_file);
		fprintf(stdout, "\n=========================================\n");

		//Unlock the result mutex
		pthread_mutex_unlock(&result_mutex);

		//Update the number of errors that this thread has
		num_errors_for_thread += num_errors_for_file;
	}
}


/**
 * This helper will extract all of the valid single file tests from our
 * single file test directory
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
	//The fully qualified name buffer for subdirectories
	char fully_qualified_name[FILENAME_MAX];

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

		/**
		 * We don't want to look at the .. or . directories
		 * so we'll skip past those
		 */
		if(directory_entry->d_name[0] == '.'){
			continue;
		}

		//Generate the fully qualified name and use that to open the directory
		snprintf(fully_qualified_name, FILENAME_MAX, "%s%s", directory_name, directory_entry->d_name);
		DIR* subdir = opendir(fully_qualified_name);

		//If it's NULL then fail out
		if(subdir == NULL){
			fprintf(stdout, "Fatal error: failed to open the nested multifile test directory %s\n", fully_qualified_name);
			exit(1);
		}

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
				char* test_file = calloc(FILENAME_MAX * 2, sizeof(char));
				snprintf(test_file, FILENAME_MAX * 2, "%s/%s", fully_qualified_name, subdirectory_entry->d_name);

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
			snprintf(test_file, FILENAME_MAX, "%s", fully_qualified_name);

			//Fail out for this and let the user deal with it
			fprintf(stderr, "Invalid multifile test example detected for file %s\n", test_file);
			exit(1);
		}

		//And then close it out
		closedir(subdir);
	}

	//Once done close this out
	closedir(multi_file_tests_directory);
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
		fprintf(stdout, "Fatal error: Valgrind is not installed on this system. Please install valgrind before proceeding\n");
		exit(1);
	}

	/**
	 * We must have the following passed into us:
	 * argv[1] - thread_count
	 * argv[2] - single file test directory
	 * argv[3] - multi file test directory
	 * argv[4] - output directory
	 */
	if(argc < 5){
		fprintf(stdout, "Fatal error: please pass in an executable, a test directory for singular tests, a test directory for multifile tests, and an output directory as a command line argument\n");
		exit(1);
	}
	int32_t thread_count = atoi(argv[1]);
	char* single_file_tests_dir = argv[2];
	char* multi_file_tests_dir = argv[3];
	output_directory = argv[4];

	//We can now start the clock
	clock_t start_time = clock();

	//Create our two dynamic arrays with initial sizes
	test_files = dynamic_array_alloc_initial_size(DEFAULT_ARRAY_SIZE);
	files_in_error = dynamic_array_alloc_initial_size(DEFAULT_ARRAY_SIZE);

	//Extract all of the tests that we want first
	get_all_single_file_tests(single_file_tests_dir);
	get_all_multi_file_tests(multi_file_tests_dir);

	//Display for the user
	fprintf(stdout, "\n===================================== SETUP ================================\n");
	fprintf(stdout, "THREADS: %d\n", thread_count);
	fprintf(stdout, "SINGLE FILE TEST DIRECTORY: %s\n", single_file_tests_dir);
	fprintf(stdout, "MULTI FILE TEST DIRECTORY: %s\n", multi_file_tests_dir);
	fprintf(stdout, "Memory Checker will spawn %d threads\n", thread_count);
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
	for(int32_t i = 0; i < thread_count; i++){
		//Unique thread id
		parameters[i].thread_number = i;

		//Create the thread and pass along the parameters that we've just made
		pthread_create(&(threads[i]), NULL, worker, &(parameters[i]));
	}

	//Wait for them all to join
	for(int32_t i = 0; i < thread_count; i++){
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
	pthread_mutex_destroy(&file_list_mutex);

	//And we can deallocate these too
	dynamic_array_dealloc(&test_files);
	dynamic_array_dealloc(&files_in_error);
	
	//All went well
	return total_errors;
}
