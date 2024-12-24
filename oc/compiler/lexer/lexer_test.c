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
		printf("FILE could not be opened");
		return 1;
	}

	//Very rudimentary here
	while((l = get_next_token(fl)).tok != DONE){
		if(l.tok == DIV_EQUALS){
			printf("FOUND DIV_EQUALS\n");
		}
		
		if(l.tok == PLUS_EQUALS){
			printf("FOUND PLUS EQUALS\n");
		}

		if(l.tok == PLUS){
			printf("FOUND PLUS\n");
		}

		if(l.tok == EQUALS){
			printf("FOUND EQUALS\n");
		}
	}
	get_next_token(fl);

	//Close the file when done
	fclose(fl);

	return 0;
}
