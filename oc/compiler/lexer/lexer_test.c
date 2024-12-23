/**
 * Generic testing suite for the lexer exclusively
*/

#include <stdio.h>
#include <sys/types.h>
#include "lexer.h"

int main(int argc, char** argv){
	FILE* fl;

	fl = fopen(argv[1], "r");

	//Very rudimentary here
	get_next_token(fl);

	printf("I do not yet do anything.\n");
	return 0;
}
