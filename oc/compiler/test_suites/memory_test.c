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

	//Close the directory
	closedir(directory);

	//All went well
	return 0;
}
