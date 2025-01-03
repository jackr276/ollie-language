/**
 * A simple tester program that tests our parsing ability
*/

#include "parser.h"
#include <stdio.h>
#include <sys/types.h>


int main(int argc, char** argv){
	if(argc < 2){
		fprintf(stderr, "ERROR: Please provide a filename\n");
	}
	
	FILE* fl;

	for(int32_t i = 1; i < argc; i++){
		fl = fopen(argv[i], "r");
		parse(fl);
		fclose(fl);
	}
	
	return 0;
}
