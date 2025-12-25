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
//For our worker queue
#include "../utils/queue/heap_queue.h"
//For any needed constants
#include "../utils/constants.h"

//Generic amount of test files
#define TEST_FILES 500

//Maximum size of a given file in linux
#define MAX_FILE_SIZE 300

//Mutices for shared states
pthread_mutex_t file_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t result_mutex = PTHREAD_MUTEX_INITIALIZER;

//The file queue
heap_queue_t file_queue;
//Total number of errors we have
u_int32_t total_errors = 0;
//Number of files in error
u_int32_t number_of_error_files = 0;
//The number of files in error
char files_in_error[TEST_FILES][MAX_FILE_SIZE];


/**
 * Worker thread:
 *
 * While(true){
 * 		Locks the file queue
 * 		if(queue is empty) -> exit
 * 		Polls the queue for a file(deletes from the queue)
 * 		Unlocks the file queue
 *
 * 		Does the work
 *
 * 		Locks the result mutex
 *		Updates the results
 *		Unlocks the result mutex
 * }
 */
void* worker(){
	//For creating our commands
	char command[3000];
	
	//For holding our file name
	char* file_name;

	//The return code for our command
	int32_t return_code;
	
	//The number of errors for each given file
	int32_t num_errors;

	//Forever loop while there is work to do
	while(TRUE){
		//Lock the queue mutex
		pthread_mutex_lock(&file_queue_mutex);

		//If we find that the queue is empty, we leave this
		//thread. We need to make sure to unlock the mutex
		//before leaving
		if(queue_is_empty(&file_queue) == TRUE){
			//Unlock the file queue mutex
			pthread_mutex_unlock(&file_queue_mutex);

			//Nothing of meaning to return here
			return NULL;
		}

		//Get our file out of the queue
		file_name = dequeue(&file_queue);

		//Unlock the file queue mutex
		pthread_mutex_unlock(&file_queue_mutex);

		//Our command. We use 2>&1 to write all errors to stdout so that we can grep it
		sprintf(command, "exit $(valgrind ./oc/out/ocd -ditsa@ -f ./oc/test_files/%s 2>&1 | grep \"SUMMARY\" | sed -n 's/.*ERROR SUMMARY: \\([0-9]\\+\\).*/\\1/p')", file_name);

		//Run the command in the system
		return_code = system(command);
		

		//Lock the result mutex. This also doubles as a mutex for stdout. We delay
		//printing anything until we're in here to keep results consistent
		pthread_mutex_lock(&result_mutex);

		printf("\n=========== Checking %s =================\n", file_name);
		printf("Running test command: %s\n\n", command);

		//If we exited, get the exit code
		if(WIFEXITED(return_code)){
			//Get the actual exit code(this will be the error number)
			num_errors = WEXITSTATUS(return_code);
			
		//If we got a bad signal, get out
		} else if (WIFSIGNALED((return_code))){
			printf("ERROR: command terminated with signal %d\n", WTERMSIG(return_code));
		}

		//Bookkeeping for final printing
		if(num_errors > 0){
			//Get this into the list
			strncpy(files_in_error[number_of_error_files], file_name, 300);
			//Bump up the count
			number_of_error_files++;
		}

		//Update the total error count
		total_errors += num_errors;

		//Get the error count out
		printf("\nTEST FILE: %s -> %d ERRORS\n", file_name, num_errors);

		printf("\n=========================================\n");
		//Unlock the result mutex
		pthread_mutex_unlock(&result_mutex);
	}
}

/**
* Hook in and run via the main function. We will be relying
* on make to verify that certain rules are precompiled for us
*/
int main(int argc, char** argv){
	//Initialize the heap queue
	file_queue = heap_queue_alloc();

	//Do we even have valgrind - if this returns 1 we don't so
	//get out
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

	//Get the thread count
	int thread_count = atoi(argv[1]);

	//Extract it and open it
	char* directory_path = argv[2];
	DIR* directory = opendir(directory_path);

	//Check that we got it
	if(directory == NULL){
		printf("Fatal error: failed to open directory %s\n", directory_path);
		exit(1);
	}

	//Run through everything in the given directory
	struct dirent* directory_entry;

	//So long as we can keep reading from the directory, we will stuff the
	//queue with what we need
	while((directory_entry = readdir(directory)) != NULL){
		//If se see '.' or '..' bail out
		if(directory_entry->d_name[0] == '.'){
			continue;
		}

		//Create some space for it
		char* file_name = calloc(sizeof(char), MAX_FILE_SIZE);

		//Copy it over
		strncpy(file_name, directory_entry->d_name, MAX_FILE_SIZE);

		//Add it into the queue
		enqueue(&file_queue, file_name);
	}

	//Close the directory
	closedir(directory);

	//Display for the user
	printf("\n===================================== SETUP ================================\n");
	printf("THREADS: %d\n", thread_count);
	printf("DIRECTOR: %s\n", directory_path);
	printf("\n===================================== SETUP ================================\n\n");

	//=========================== Setup for threads ============================
	
	//Space for our threads
	pthread_t* threads = calloc(thread_count, sizeof(pthread_t));

	//Spawn a worker for each thread
	for(u_int16_t i = 0; i < thread_count; i++){
		pthread_create(&(threads[i]), NULL, worker, NULL);
	}

	//Wait for them all to join
	for(u_int16_t i = 0; i < thread_count; i++){
		pthread_join(threads[i], NULL);
	}

	printf("================================ Ollie Memory Check Summary =================================== \n");
	printf("TOTAL ERRORS: %d\n", total_errors);

	//Only print out if we need to
	if(total_errors > 0){
		printf("FILES IN ERROR:\n");

		//Print out all of them
		for(u_int32_t i = 0; i < number_of_error_files; i++){
			printf("%d) %s\n", i, files_in_error[i]);
		}

		//One final error for emphasis
		printf("\n\nMEMORY CHECK FAILURE: DEVELOPER ATTENTION IS REQUIRED\n\n");
	}

	printf("================================ Ollie Memory Check Summary =================================== \n");
	
	//All went well
	return total_errors;
}
