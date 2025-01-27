/**
 * The compiler for Ollie-Lang. Depends on the lexer and the parser. See documentation for
 * full option details
*/

#include <unistd.h>
#include <stdio.h>
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
	//Just hop out here
	if(argc < 2){
		fprintf(stderr, "Ollie compiler requires a filename to be passed in\n");
		exit(1);
	}
	
	//Let's now grab what we need using getopt
	int32_t opt;
	//The filename
	char* fname;

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
		
	//Parse the file
	front_end_results_package_t results = parse(fl);
	
	//Close the file
	fclose(fl);
	
	if(results.success == 0){
		fprintf(stderr, "Parsing failed\n");
	}

	//FOR NOW -- deallocate this stuff
	deallocate_ast(results.root);
	destroy_function_symtab(results.function_symtab);
	destroy_type_symtab(results.type_symtab);
	destroy_variable_symtab(results.variable_symtab);
	
	// ========================================================
}
