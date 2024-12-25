/**
 * Generic testing suite for the lexer exclusively
*/

#include <stdio.h>
#include <sys/types.h>
#include "lexer.h"

int main(int argc, char** argv){
	FILE* fl;
	Lexer_item l;

	//Open the file for reading only
	fl = fopen(argv[1], "r");

	if(fl == NULL){
		fprintf(stderr, "FILE could not be opened\n");
		return 1;
	}

	//Very rudimentary here
	while((l = get_next_token(fl)).tok != DONE){
		print_token(&l);
	}

	//Close the file when done
	fclose(fl);

	return 0;
}
