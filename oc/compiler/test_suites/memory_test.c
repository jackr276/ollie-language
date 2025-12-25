/**
* Author: Jack Robbins
* This specialized test suite will use the "valgrind" command
* to run through every single test suite and check to see what kind of
* memory errors that we have, if any at all
*/

#include <linux/limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
//For multithreading
#include <pthread.h>

//Generic amount of test files
#define TEST_FILES 500

//Maximum size of a given file in linux
#define MAX_FILE_SIZE 300

//Mutices for shared states
pthread_mutex_t file_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t result_mutex = PTHREAD_MUTEX_INITIALIZER;

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
 * 		Locks the 
 *
 * }
 *
 * Runs the command
 * 
 */
void* worker(void* parameters){

}

/**
* Hook in and run via the main function. We will be relying
* on make to verify that certain rules are precompiled for us
*/
int main(int argc, char** argv){
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

	//For creating our commands
	char command[3000];

	//Total number of errors we have
	int total_errors = 0;

	//Number of files in error
	int number_of_error_files = 0;

	//The number of files in error
	char files_in_error[TEST_FILES][MAX_FILE_SIZE];

	//So long as we can keep reading from the directory
	while((directory_entry = readdir(directory)) != NULL){
		printf("=========== Checking %s =================\n", directory_entry->d_name);

		//Our command. We use 2>&1 to write all errors to stdout so that we can grep it
		sprintf(command, "exit $(valgrind ./oc/out/ocd -ditsa@ -f ./oc/test_files/%s 2>&1 | grep \"SUMMARY\" | sed -n 's/.*ERROR SUMMARY: \\([0-9]\\+\\).*/\\1/p')", directory_entry->d_name);
		printf("Running test command: %s\n\n", command);

		//Get the return code
		int return_code = system(command);

		//Number of errors
		int num_errors = 0;

		//If we exited, get the exit code
		if(WIFEXITED(return_code)){
			//Get the actual exit code(this will be the error number)
			num_errors = WEXITSTATUS(return_code);
			
		//If we got a bad signal, get out
		} else if (WIFSIGNALED((return_code))){
			printf("ERROR: command terminated with signal %d\n", WTERMSIG(return_code));
			exit(1);
		}

		//Get the error count out
		printf("\nTEST FILE: %s -> %d ERRORS\n", directory_entry->d_name, num_errors);

		//Bookkeeping for final printing
		if(num_errors > 0){
			//Get this into the list
			strncpy(files_in_error[number_of_error_files], directory_entry->d_name, 300);
			number_of_error_files++;
		}


		//Increment the overall number
		total_errors += num_errors;
	}

	//Close the directory
	closedir(directory);

	printf("================================ Ollie Memory Check Summary =================================== \n");
	printf("TOTAL ERRORS: %d\n", total_errors);

	//Only print out if we need to
	if(total_errors > 0){
		printf("FILES IN ERROR:\n");

		//Print out all of them
		for(int32_t i = 0; i < number_of_error_files; i++){
			printf("%d) %s\n", i, files_in_error[i]);
		}

		//One final error for emphasis
		printf("\n\nMEMORY CHECK FAILURE: DEVELOPER ATTENTION IS REQUIRED\n\n");
	}

	printf("================================ Ollie Memory Check Summary =================================== \n");
	
	//All went well
	return total_errors;
}
