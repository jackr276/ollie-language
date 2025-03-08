/**
 * A simple tester program that tests our parsing ability
*/

#include "parser.h"
#include <stdio.h>
#include <sys/types.h>
#include "../ast/ast.h"


/**
 * FOR TESTING PURPOSES ONLY: NOT MEMORY SAFE
*/
int main(int argc, char** argv){
	if(argc < 2){
		fprintf(stderr, "ERROR: Please provide a filename\n");
	}
	
	FILE* fl;

	for(int32_t i = 1; i < argc; i++){
		printf("Testing file %s\n", argv[i]);
		fl = fopen(argv[i], "r");

		//If it failed
		if(fl == NULL){
			printf("COULD NOT OPEN FILE %s\n", argv[i]);
		}

		front_end_results_package_t results = parse(fl, "Sample");
		ast_dealloc();

		fclose(fl);
	}
	
	return 0;
}
