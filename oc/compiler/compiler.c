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
#include "file_builder/file_builder.h"
#include "optimizer/optimizer.h"
#include "type_system/type_system.h"

//For standardization across all modules
#define TRUE 1
#define FALSE 0

//The number of errors and warnings
u_int32_t num_errors;
u_int32_t num_warnings;


/**
 * A help printer function for users of the compiler
 */
static void print_help(){
	printf("===================================== Ollie Compiler Options =====================================\n");
	printf("-f <filename>: Required field. Specifies the .ol source file to be compiled\n");
	printf("-o <filename>: Optional field. Specificy the output location. If none is given, out.ol will be used\n");
	printf("-d: Optional field. Show all debug information printed to stdout\n");
	printf("-h: Show help\n");
	printf("==================================================================================================\n");
}


/**
 * We'll use this helper function to process the compiler flags and return a structure that
 * tells us what we need to do throughout the compiler
 */
static compiler_options_t* parse_and_store_options(int argc, char** argv){
	//Allocate it
	compiler_options_t* options = calloc(1, sizeof(compiler_options_t));
	
	//For storing our opt
	int opt;

	//Run through all of our options
	while((opt = getopt(argc, argv, "atdhfs:o:?")) != -1){
		//Switch based on opt
		switch(opt){
			//Invalid option
			case '?':
				printf("Invalid option: %c\n", optopt);
				print_help();
				exit(0);
			//After we print help we exit
			case 'h':
				print_help();
				exit(0);
			//Time execution for performance test
			case 't':
				options->time_execution = TRUE;
				break;
			//Store the input file name
			case 'f':
				options->file_name = optarg;
				break;
			//Turn on debug printing
			case 'd':
				options->enable_debug_printing = TRUE;
				break;
			//Output to assembly only
			case 'a':
				options->go_to_assembly = TRUE;
				break;
			//Specify that we want a summary to be shown
			case 's':
				options->show_summary = TRUE;
				break;
			//Specific output file
			case 'o':
				options->output_file = optarg;
				break;
		}
	}

	//This is an error, so we'll fail out here
	if(options->file_name == NULL){
		printf("[COMPILER ERROR]: No input file name provided. Use -f <filename> to specify a .ol source file\n");
		exit(1);
	}

	//Give back the options we got in the structure
	return options;
}


/**
 * Print a final summary for the ollie compiler. This could show success or
 * failure, based on what the caller wants
 */
static void print_summary(){
	printf("============================================= SUMMARY =======================================\n");

	printf("=============================================================================================\n");
}


/**
 * The compile function handles all of the compilation logic for us. Compilation
 * in oc requires the passing of data between one module and another. This function
 * manages that for us
 */
static void compile(compiler_options_t* options){
	//For any/all error printing
	char info[2000];

	//Now we'll parse the whole thing
	//results = parse(fl, dependencies.file_name);
	front_end_results_package_t* results = parse(options);

	//Increment these while we're here
	num_errors += results->num_errors;
	num_warnings += results->num_warnings;

	//This is our fail case
	if(results->root->CLASS == AST_NODE_CLASS_ERR_NODE){

	}

	//Now we'll build the cfg using our results
	cfg_t* cfg = build_cfg(results, &num_errors, &num_warnings);

	//If we're doing debug printing, then we'll print this
	if(options->enable_debug_printing == TRUE){
		printf("============================================= BEFORE OPTIMIZATION =======================================\n");
		print_all_cfg_blocks(cfg);
		printf("============================================= BEFORE OPTIMIZATION =======================================\n");
	}

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
	//Let the helper run through and store all of our options. This will
	//also error out if any options are bad
	compiler_options_t* options = parse_and_store_options(argc, argv);

	//Call the compiler, let this handle it
	front_end_results_package_t results;
	compile(options);


	//FOR NOW to simplify debugging

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
	} else {
		printf("\n===================== Ollie Compiler Summary ==========================\n");
		printf("Lexer processed %d lines\n", results.lines_processed);
		printf("Parsing succeeded in %.8f seconds with %d warnings\n", time_spent, num_warnings);
		printf("=======================================================================\n\n");
	}

	printf("==========================================================================================\n\n");
}
