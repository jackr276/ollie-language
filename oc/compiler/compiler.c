/**
 * The compiler for Ollie-Lang. Depends on the lexer and the parser. See documentation for
 * full option details
*/

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include "ast/ast.h"
#include "parser/parser.h"
#include "symtab/symtab.h"
#include "cfg/cfg.h"

/**
 * The main entry point for the compiler. This will be expanded as time goes on
 *
 * COMPILER OPTIONS:
 * 	1.) -f passing a file in
*/
int main(int argc, char** argv){
	//How much time we've spent
	double time_spent;

	printf("==================================== Ollie Compiler ======================================\n");
	//Just hop out here
	if(argc < 2){
		fprintf(stderr, "Ollie compiler requires a filename to be passed in\n");
		exit(1);
	}

	front_end_results_package_t results;

	//Start the timer
	clock_t begin = clock();

	// ================== Front End ===========================
		/**
		 * The front end consists of the lexer and parser. When the front end
		 * completes, we are guaranteed the following:
		 * 	1.) A syntactically correct program
		 * 	2.) A fully complete symbol table for each area of the program
		 * 	3.) The elaboration and elimination of all preprocessor/compiler-only operations
		 *  4.) A fully fleshed out Abstract-Syntax-Tree(AST) that can be used by the middle end
		*/

	//We compile in one giant chain
	for(u_int16_t i = 1; i < argc; i++){
		//Grab whatever this file is
		char* fname = argv[i];

		printf("Attempting to compile: %s\n", fname);

		//Once we get down here we should have the file available for parsing
		FILE* fl = fopen(fname, "r");
	
		//Fail out if bad
		if(fl == NULL){
			fprintf(stderr, "File %s could not be found or opened", fname);
			exit(1);
		}

		//Otherwise we are all good to pass to the parser
		
		//Parse the file
		results = parse(fl);
		//Timer end
		clock_t end = clock();
		//Crude time calculation
		time_spent = (double)(end - begin) / CLOCKS_PER_SEC;


		//Close the file
		fclose(fl);
	}

	//If the AST root is bad, there's no use in going on here
	if(results.root->CLASS == AST_NODE_CLASS_ERR_NODE){
		goto final_printout;
	}

	//Run through and check for any unused functions. This generates warnings for the user,
	//and can be done before any construction of a CFG. As such, we do this here
	check_for_unused_functions(results.function_symtab, &results.num_warnings);

	//We'll store the number of warnings and such here locally
	u_int32_t num_warnings = results.num_warnings;
	u_int32_t num_errors = results.num_errors;

	//============================= Middle End =======================================
		/**
	 	 * The middle end is responsible for control-flow checks and optimization for the parser. The first 
		 * part of this is the construction of the control-flow-graph
		*/
	cfg_t* cfg = build_cfg(results, &num_warnings, &num_errors);




	final_printout:
	//If we failed
	if(results.root->CLASS == AST_NODE_CLASS_ERR_NODE || num_errors > 0){
		char info[500];
		sprintf(info, "Parsing failed with %d errors and %d warnings in %.8f seconds", num_errors, num_warnings, time_spent);
		printf("\n===================== Ollie Compiler Summary ==========================\n");
		printf("Lexer processed %d lines\n", results.lines_processed);
		printf("%s\n", info);
		printf("=======================================================================\n\n");
		//Failure here
		results.success = 0;
	} else {
		printf("\n===================== Ollie Compiler Summary ==========================\n");
		printf("Lexer processed %d lines\n", results.lines_processed);
		printf("Parsing succeeded in %.8f seconds with %d warnings\n", time_spent, num_warnings);
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
