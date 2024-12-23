/**
 * Generic testing suite for the lexer exclusively
*/

#include <stdio.h>
#include <sys/types.h>
#include "lexer.h"

int main(int argc, char** argv){
	FILE* fl;

	//Open the file for reading only
	fl = fopen(argv[1], "r");

	//Very rudimentary here
	get_next_token(fl);

	//Close the file when done
	fclose(fl);

	return 0;
}
