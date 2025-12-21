/**
* Author: Jack Robbins
* This specialized test suite will use the "valgrind" command
* to run through every single test suite and check to see what kind of
* memory errors that we have, if any at all
*/

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>


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

	//Here is the executable(should be something like ./oc/out/ocd)
	char* executable_path = argv[1];

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
	//The number of files in error
	char** files_in_error = calloc(1, sizeof(char[300]));

	//So long as we can keep reading from the directory
	while((directory_entry = readdir(directory)) != NULL){
		printf("=========== Checking %s =================\n", directory_entry->d_name);

		//Our command. We use 2>&1 to write all errors to stdout so that we can grep it
		sprintf(command, "valgrind ./oc/out/ocd -ditsa@ -f ./oc/test_files/%s 2>&1 | grep \"SUMMARY\" | sed 's/.*ERROR SUMMARY: \\([0-9]\\+\\).*/\\1/'", directory_entry->d_name);
		printf("Running test command: %s\n", command);

		//Run the command and get the error count back
		int error_count = system(command);

		//Get the error count out
		printf("TEST FILE: %s -> %d ERRORS\n", directory_entry->d_name, error_count);

		//Increment the overall number
		total_errors += error_count;
	}

	//Close the directory
	closedir(directory);

	printf("================================ Ollie Memory Check Summary =================================== \n");
	printf("TOTAL ERRORS: %d\n", total_errors);
	printf("================================ Ollie Memory Check Summary =================================== \n");
	
	//All went well
	return total_errors;
}
