/**
 * Author: Jack Robbins
 *
 * This program tests the front end of the compiler only
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>

//Link to the parser
#include "../parser/parser.h"
//Link to cfg
#include "../cfg/cfg.h"

/**
 * Our main and only function
*/
int main(int argc, char** argv){
	//How much time we've spent
	double time_spent;

	fprintf(stderr, "==================================== FRONT END TEST ======================================\n");

	//Just hop out here
	if(argc < 2){
		fprintf(stderr, "Ollie compiler requires a filename to be passed in\n");
		//Jump to the very end
		goto final_printout;
	}

	//Just for user convenience
	fprintf(stderr, "INPUT FILE: %s\n\n", argv[1]);

	//First we try to open the file
	FILE* fl = fopen(argv[1], "r");

	//If this fails, the whole thing is done
	if(fl == NULL){
		fprintf(stderr, "[FATAL COMPILER ERROR]: Failed to open file \"%s\"", argv[1]);
		goto final_printout;
	}

	//Start the timer
	clock_t begin = clock();

	//Now that we can actually open the file, we'll parse
	front_end_results_package_t parse_results = parse(fl, "PARSER TEST INPUT");

	//Let's see what kind of results we got
	if(parse_results.root->CLASS == AST_NODE_CLASS_ERR_NODE){
		//Timer end
		clock_t end = clock();

		//Calculate the final time
		time_spent = (double)(end - begin)/CLOCKS_PER_SEC;

		char info[500];
		sprintf(info, "Parsing failed with %d errors and %d warnings in %.8f seconds", parse_results.num_errors, parse_results.num_warnings, time_spent);
		printf("\n===================== Ollie Compiler Summary ==========================\n");
		printf("Lexer processed %d lines\n", parse_results.lines_processed);
		printf("%s\n", info);
		printf("=======================================================================\n\n");
		//Jump to the end, we're done here
		goto final_printout;
	}

	//The number of warnings and errors
	u_int32_t num_warnings = parse_results.num_warnings;
	u_int32_t num_errors = parse_results.num_errors;

	//Now we'll invoke the cfg builder
	cfg_t* cfg = build_cfg(parse_results, &num_errors, &num_warnings);

	//And once we're done - for the front end test, we'll want all of this printed
	print_all_cfg_blocks(cfg);

	//Deallocate everything at the end
	ast_dealloc();
	//Free the call graph holder
	free(parse_results.os);
	function_symtab_dealloc(parse_results.function_symtab);
	type_symtab_dealloc(parse_results.type_symtab);
	variable_symtab_dealloc(parse_results.variable_symtab);
	constants_symtab_dealloc(parse_results.constant_symtab);
	dealloc_cfg(cfg);

	//Now stop the clock - we want to test the deallocation overhead too
	//Timer end
	clock_t end = clock();

	//Calculate the final time
	time_spent = (double)(end - begin)/CLOCKS_PER_SEC;

	//Print out the summary now that we're done
	printf("\n===================== FRONT END TEST SUMMARY ==========================\n");
	printf("Lexer processed %d lines\n", parse_results.lines_processed);
	printf("Parsing succeeded in %.8f seconds with %d warnings\n", time_spent, num_warnings);
	printf("=======================================================================\n\n");

final_printout:
	fprintf(stderr, "==================================== END  ================================================\n");
}
