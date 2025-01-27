/**
 * The compiler for Ollie-Lang. Depends on the lexer and the parser. See documentation for
 * full option details
*/

#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include "ast/ast.h"
#include "parser/parser.h"
#include "symtab/symtab.h"

/**
 * The main entry point for the compiler. This will be expanded as time goes on
 *
 * COMPILER OPTIONS:
 * 	1.) -f passing a file in
*/
int main(int argc, char** argv){
	printf("==================================== Ollie Compiler ======================================\n");
	//Just hop out here
	if(argc < 2){
		fprintf(stderr, "Ollie compiler requires a filename to be passed in\n");
		exit(1);
	}
	
	//FOR NOW ONLY - only 1 file at a time
	char* fname = argv[1];
	printf("Attempting to compile: %s\n", fname);

	/*
	//Let's now grab what we need using getopt
	int32_t opt;
	//The filename

	opt = getopt(argc, argv, "f:?");

	while(opt != -1){
		switch(opt){
			case 'f':
				//We have our filname here
				fname = optarg;
				break;
			case '?':
				if(optopt == 'f'){
					fprintf(stderr, "-f option requires a filename\n");
				} else {
					fprintf(stderr, "Unrecognized argument -%c", opt);
				}
				break;
		}

		//Refresh opt
		opt = getopt(argc, argv, "f:?");
	}
	*/

	//Once we get down here we should have the file available for parsing
	FILE* fl = fopen(fname, "r");
	
	//Fail out if bad
	if(fl == NULL){
		fprintf(stderr, "File %s could not be found or opened", fname);
		exit(1);
	}
	//Otherwise we are all good to pass to the parser

	// ================== Front End ===========================
	/**
	 * The front end consists of the lexer and parser. When the front end
	 * completes, we are guaranteed the following:
	 * 	1.) A syntactically correct program
	 * 	2.) A fully complete symbol table for each area of the program
	 * 	3.) The elaboration and elimination of all preprocessor/compiler-only operations
	 *  4.) A fully fleshed out Abstract-Syntax-Tree(AST) that can be used by the middle end
	*/
	double time_spent;

	//Start the timer
	clock_t begin = clock();
	
	//Parse the file
	front_end_results_package_t results = parse(fl);

	//Timer end
	clock_t end = clock();
	//Crude time calculation
	time_spent = (double)(end - begin) / CLOCKS_PER_SEC;


	//Close the file
	fclose(fl);
	
	check_for_unused_functions(results.function_symtab, &results.num_warnings);

	//If we didn't find a main function, we're done here
	if(results.os->num_callees == 0){
		print_parse_message(PARSE_ERROR, "No main function found", 0);
		results.num_errors++;
	}

	//If we failed
	if(results.root->CLASS == AST_NODE_CLASS_ERR_NODE){
		char info[500];
		sprintf(info, "Parsing failed with %d errors and %d warnings in %.8f seconds", results.num_errors, results.num_warnings, time_spent);
		printf("\n===================== Ollie Compiler Summary ==========================\n");
		printf("Lexer processed %d lines\n", results.lines_processed);
		printf("%s\n", info);
		printf("=======================================================================\n\n");
		//Failure here
		results.success = 0;
	} else {
		printf("\n===================== Ollie Compiler Summary ==========================\n");
		printf("Lexer processed %d lines\n", results.lines_processed);
		printf("Parsing succeeded in %.8f seconds with %d warnings\n", time_spent, results.num_warnings);
		printf("=======================================================================\n\n");
		//If we get here we know that we succeeded
		results.success = 1;
	}

	//FOR NOW -- deallocate this stuff
	deallocate_ast(results.root);
	destroy_function_symtab(results.function_symtab);
	destroy_type_symtab(results.type_symtab);
	destroy_variable_symtab(results.variable_symtab);
	
	printf("==========================================================================================\n\n");
	// ========================================================
}
