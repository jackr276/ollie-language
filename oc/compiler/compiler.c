/**
 * Author: Jack Robbins
 *
 * The compiler for Ollie-Lang. Depends on the lexer and the parser. See documentation for
 * full option details
*/
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include "ast/ast.h"
#include "parser/parser.h"
#include "code_generator/code_generator.h"
#include "symtab/symtab.h"
#include "cfg/cfg.h"
#include "assembler/assembler.h"
#include "optimizer/optimizer.h"

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
static void compile(char* fname, front_end_results_package_t* results){
	//For errors
	char info[2000];
	//These are all NULL initially
	results->constant_symtab = NULL;
	results->function_symtab = NULL;
	results->type_symtab = NULL;
	results->variable_symtab = NULL;
	results->os = NULL;
	results->root = NULL;

	//First we try to open the file
	FILE* fl = fopen(fname, "r");

	//If this is the case, we fail immediately
	if(fl == NULL){
		sprintf(info, "The file %s either could not be found or could not be opened", fname);
		//Error out
		print_parse_message(PARSE_ERROR, info, 0);
		num_errors++;
		return;
	}

	//Now we'll parse the whole thing
	//results = parse(fl, dependencies.file_name);
	*results = parse(fl, fname);

	//Increment these while we're here
	num_errors += results->num_errors;
	num_warnings += results->num_warnings;

	//Now that we're done, we can close
	fclose(fl);

	//Give back the results
	return;
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

	//Display the filename for now
	fprintf(stdout, "INPUT FILE: %s\n\n", argv[1]);

	//Just hop out here
	if(argc < 2){
		fprintf(stderr, "Ollie compiler requires a filename to be passed in\n");
		goto final_printout; 
	}

	//Start the timer
	clock_t begin = clock();

	//Grab whatever this file is
	char* fname = argv[1];

	//Call the compiler, let this handle it
	front_end_results_package_t results;
	compile(fname, &results);

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

	//FOR NOW to simplify debugging
	printf("============================================= BEFORE OPTIMIZATION =======================================\n");
	print_all_cfg_blocks(cfg);
	printf("============================================= BEFORE OPTIMIZATION =======================================\n");

	//Now we will run the optimizer
	cfg = optimize(cfg, results.os, 5);

	printf("============================================= AFTER OPTIMIZATION =======================================\n");
	print_all_cfg_blocks(cfg);
	printf("============================================= AFTER OPTIMIZATION =======================================\n");

	//Invoke the back end
	generate_assembly_code(cfg);


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
