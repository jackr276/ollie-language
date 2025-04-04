/**
 * The compiler for Ollie-Lang. Depends on the lexer and the parser. See documentation for
 * full option details
*/

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include "ast/ast.h"
#include "parser/parser.h"
#include "preprocessor/preprocessor.h"
#include "symtab/symtab.h"
#include "cfg/cfg.h"
#include "optimizer/optimizer.h"

// A list of currently compiled files
typedef struct compiled_file_token_t compiled_file_token_t;

//The number of errors and warnings
u_int32_t num_errors;
u_int32_t num_warnings;


/**
 *	Compile an individual file. This function can be recursively called to deal 
 *	with dependencies
 *
 *  STRATEGY: This function will first invoke the preprocessor. The preprocessor
 *  will perform dependency analysis and return a tree, rooted at the current node, 
 *  that gives us the order needed for compilation. We will then perform a reverse
 *  level-order traversal and compile in that order
 */
static front_end_results_package_t compile(char* fname){
	//Declare our return package
	front_end_results_package_t results;
	//These are all NULL initially
	results.constant_symtab = NULL;
	results.function_symtab = NULL;
	results.type_symtab = NULL;
	results.variable_symtab = NULL;
	results.os = NULL;
	results.root = NULL;

	//First we try to open the file
	FILE* fl = fopen(fname, "r");

	//If this fails, the whole thing is done
	if(fl == NULL){
		fprintf(stderr, "[FATAL COMPILER ERROR]: Failed to open file \"%s\"", fname);
		results.num_errors = 1;
		results.lines_processed = 0;
		//Failed here
		results.result_type = PARSER_RESULT_FAILURE;
		//Give it back
		return results;
	}

	//Otherwise it opened, so we now need to process it and compile dependencies
	dependency_package_t dependencies = preprocess(fname);

	//If this fails, we error out
	if(dependencies.return_token == PREPROC_ERROR){
		results.num_errors = 1;
		results.lines_processed = 0;
		//Failed here
		results.result_type = PARSER_RESULT_FAILURE;
		//Give it back
		return results;
	}

	/**
	 * We now need to analyze the dependencies of this file. If there are dependencies,
	 * the parser requires that those be loaded into memory first. Failure to do this will
	 * result in parser errors about files not existing
	 */
	//We will go through dependency by dependency
	/*
	for(u_int16_t i = 0; i < dependencies.num_dependencies; i++){
		//Grab the current one out of here
		char* current_dependency = dependencies.dependencies[i];

		//There is a (strong) chance that whatever we've grabbed here has already been 
		//compiled. If so, we'll just skip it. We don't want to continuously recompile commonly
		//used files. To achieve this, we maintain a list of everything that has been compiled(specifically)
		//their FILE_TOKENS, and we will cross these off as we compile them 

		//If this has not been compiled already
		if(has_file_been_compiled(current_dependency) == 0){
			//Then we'll compile it
			results = compile(current_dependency);

			//If results is bad, we fail here
			if(results.root == NULL || results.root->CLASS == AST_NODE_CLASS_ERR_NODE){
				return results;
			}
		}
	}
	*/

	//Now we'll parse the whole thing
	//results = parse(fl, dependencies.file_name);
	results = parse(fl, "FIX ME");

	//Increment these while we're here
	num_errors += results.num_errors;
	num_warnings += results.num_warnings;

	//Now that we're done, we can close
	fclose(fl);

	//Give back the results
	return results;
}


/**
 * The main entry point for the compiler. This will be expanded as time goes on
 *
 * COMPILER OPTIONS:
 *  NOTE: The compiler only accepts one file at a time. This is because Ollie handles 
 *  building all dependencies automatically, so there is no need to pass in more than
 *  one file at a time. The file that you pass in should have dependencies declared
 *  in the #dependencies block
*/
int main(int argc, char** argv){
	//How much time we've spent
	double time_spent;
	//By default, we assume that we've errored
	ast_node_class_t CLASS = AST_NODE_CLASS_ERR_NODE;

	//We'll be giving summaries for the user as we go
	fprintf(stderr, "==================================== Ollie Compiler ======================================\n");

	//Just hop out here
	if(argc < 2){
		fprintf(stderr, "Ollie compiler requires a filename to be passed in\n");
		goto final_printout; 
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

	//Grab whatever this file is
	char* fname = argv[1];

	//Call the compiler, let this handle it
	results = compile(fname);

	//If the AST root is bad, there's no use in going on here
	if(results.root == NULL || results.root->CLASS == AST_NODE_CLASS_ERR_NODE){
		goto final_printout;
	}

	//============================= Middle End =======================================
		/**
	 	 * The middle end is responsible for control-flow checks and optimization for the parser. The first 
		 * part of this is the construction of the control-flow-graph
		*/
	cfg_t* cfg = build_cfg(results, &num_errors, &num_warnings);

	//Grab bfore freeing
	CLASS = results.root->CLASS;

	//FOR NOW -- deallocate this stuff
	ast_dealloc();
	//Free the call graph holder
	free(results.os);
	function_symtab_dealloc(results.function_symtab);
	type_symtab_dealloc(results.type_symtab);
	variable_symtab_dealloc(results.variable_symtab);
	constants_symtab_dealloc(results.constant_symtab);
	dealloc_cfg(cfg);
	
	//Timer end
	clock_t end = clock();
	//Crude time calculation
	time_spent = (double)(end - begin) / CLOCKS_PER_SEC;

	final_printout:
	//If we failed
	if(CLASS == AST_NODE_CLASS_ERR_NODE || num_errors > 0){
		char info[500];
		sprintf(info, "Parsing failed with %d errors and %d warnings in %.8f seconds", num_errors, num_warnings, time_spent);
		printf("\n===================== Ollie Compiler Summary ==========================\n");
		printf("Lexer processed %d lines\n", results.lines_processed);
		printf("%s\n", info);
		printf("=======================================================================\n\n");
		//Failure here
		results.result_type = PARSER_RESULT_FAILURE;
	} else {
		printf("\n===================== Ollie Compiler Summary ==========================\n");
		printf("Lexer processed %d lines\n", results.lines_processed);
		printf("Parsing succeeded in %.8f seconds with %d warnings\n", time_spent, num_warnings);
		printf("=======================================================================\n\n");
		//If we get here we know that we succeeded
		results.result_type = PARSER_RESULT_SUCCESS;
	}

	printf("==========================================================================================\n\n");
	// ========================================================
}
