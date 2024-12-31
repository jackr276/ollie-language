/**
 * Generic testing suite for the lexer exclusively
*/

#include <stdio.h>
#include <sys/types.h>
#include "lexer.h"

int main(int argc, char** argv){
	if(argc < 2){
		fprintf(stderr, "A filename must be provided\n");
	}

	FILE* fl;
	Lexer_item l;

	for(int32_t i = 1; i < argc; i++){
		//Open the file for reading only
		fl = fopen(argv[i], "r");
	
		if(fl == NULL){
			fprintf(stderr, "FILE could not be opened\n");
			return 1;
		}

		//Very rudimentary here
		while((l = get_next_token(fl)).tok != DONE){
			print_token(&l);
		}
		//Print the last one
		print_token(&l);
		//Close the file when done
		fclose(fl);
	}

	return 0;
}
