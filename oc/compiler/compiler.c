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
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "symtab/symtab.h"
#include "cfg/cfg.h"
#include "register_allocator/register_allocator.h"
#include "instruction_selector/instruction_selector.h"
#include "instruction_scheduler/instruction_scheduler.h"
#include "file_builder/file_builder.h"
#include "optimizer/optimizer.h"
#include "utils/constants.h"

//The number of errors and warnings
u_int32_t num_errors;
u_int32_t num_warnings;


/**
 * A help printer function for users of the compiler
 */
static void print_help(){
	printf("\n===================================== Ollie Compiler Options =====================================\n");
	printf("\n######################################## Required Fields #########################################\n");
	printf("-f <filename>: Required field. Specifies the .ol source file to be compiled\n");
	printf("\n######################################## Optional Fields #########################################\n");
	printf("-o <filename>: Specificy the output location. If none is given, out.s will be used\n");
	printf("-s: Show a summary at the end of compilation\n");
	printf("-a: Generate an assembly code file with a .s extension\n");
	printf("-d: Show all debug information printed. This includes compiler warnings, info statements\n");
	printf("-r: Print the result of the register allocation. This is done by default in -i\n");
	printf("-t: Time execution of compiler. Can be used for performance testing\n");
	printf("-m: Time each module of the compiler. This is used for even more granular performance testing\n");
	printf("-@: Should only be used for CI runs. Avoids generating any assembly files\n");
	printf("-i: Print intermediate representations. This will generate *a lot* of text, so be careful\n");
	printf("-h: Show help\n");
	printf("\n==================================================================================================\n");
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
	while((opt = getopt(argc, argv, "rima@tdhsf:o:?")) != -1){
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
			//Flag that this is a test run
			case '@':
				options->is_test_run = TRUE;
				break;
			//Flag that this is a test run
			case 'r':
				options->print_post_allocation = TRUE;
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
			//Specify that we want to print intermediate representations
			case 'i':
				options->print_irs = TRUE;
				break;
			//Specify that we want to have timing that is specific by module
			case 'm':
				options->module_specific_timing = TRUE;
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
static void print_summary(compiler_options_t* options, module_times_t* times, u_int32_t lines_processed, u_int32_t num_errors, u_int32_t num_warnings, u_int8_t success){
	//For holding our message
	char info[500];

	//Show a success
	if(success == TRUE){
		sprintf(info, "Ollie compiler successfully compiled %s with %d warnings", options->file_name, num_warnings);
	} else {
		sprintf(info, "Parsing failed with %d errors and %d warnings", num_errors, num_warnings);
	}

	printf("============================================= SUMMARY =======================================\n");
	printf("Lexer processed %d lines\n", lines_processed);

	//If we want module specific timing, we'll print out here
	if(options->module_specific_timing == TRUE){
		printf("Lexer took: %.8f seconds\n", times->lexer_time);
		printf("Parser took: %.8f seconds\n", times->parser_time);
		printf("CFG constuctor took: %.8f seconds\n", times->cfg_time);
		printf("Optimizer took: %.8f seconds\n", times->optimizer_time);
		printf("Instruction Selector took: %.8f seconds\n", times->selector_time);
		printf("Instruction Scheduler took: %.8f seconds\n", times->scheduler_time);
		printf("Register Allocator took: %.8f seconds\n", times->allocator_time);
	}

	//Print out the total time
	if(options->time_execution == TRUE || options->module_specific_timing == TRUE){
		printf("Compilation took %.8f seconds\n", times->total_time);
	}

	printf("%s\n", info);
	printf("=============================================================================================\n");
}


/**
 * The compile function handles all of the compilation logic for us. Compilation
 * in oc requires the passing of data between one module and another. This function
 * manages that for us
 */
static u_int8_t compile(compiler_options_t* options){
	//Declare our times and set all to 0
	module_times_t times = {0, 0, 0, 0, 0, 0, 0, 0};

	//Print out the file name if we're debug printing
	printf("Compiling source file: %s\n\n\n", options->file_name);

	//Warn the user if no file name is given
	if(options->output_file == NULL){
		printf("[WARNING]: No output file name given. The name \"out.s\" will be used\n\n");
	}

	//And we'll keep track of everything we have here
	clock_t begin = 0;
	clock_t lexer_end = 0;
	clock_t parser_end = 0;
	clock_t cfg_end = 0;
	clock_t optimizer_end = 0;
	clock_t selector_end = 0;
	clock_t scheduler_end = 0;
	clock_t allocator_end = 0;

	//This is the true "end" when all has finished
	clock_t end = 0;

	//If we want to time the execution, we'll start the clock
	if(options->time_execution == TRUE || options->module_specific_timing == TRUE){
		begin = clock();
	}

	//Invoke the lexer. This handles all file IO
	ollie_token_stream_t token_stream = tokenize(options->file_name);

	//If it failed, we need to leave immediately
	if(token_stream.status == STREAM_STATUS_FAILURE){
		print_parse_message(PARSE_ERROR, "Tokenizing failed. Please remedy the tokenizer errors and recompile", 0);
		//1 - it failed
		return 1;
	}

	//If we are doing module specific timing, store the lexer time
	if(options->module_specific_timing == TRUE){
		//End the parser timer
		lexer_end = clock();

		//Crude time calculation
		times.lexer_time = (double)(lexer_end - begin) / CLOCKS_PER_SEC;
	}

	//Now we cache the token stream reference inside of the options. The parser will reference this for
	//all of its operations
	options->token_stream = &token_stream;

	//Now we'll parse the whole thing
	//results = parse(fl, dependencies.file_name);
	front_end_results_package_t* results = parse(options);

	//Increment these while we're here
	num_errors += results->num_errors;
	num_warnings += results->num_warnings;

	//This is our fail case
	if(results->root->ast_node_type == AST_NODE_TYPE_ERR_NODE){
		//Timer end
		end = clock();

		//Crude time calculation
		times.total_time = (double)(end - begin) / CLOCKS_PER_SEC;

		//Print summary with a failure here
		if(options->show_summary == TRUE){
			print_summary(options, &times, results->lines_processed, num_errors, num_warnings, FALSE);
		}

		//If this is a test run, we will return 0 because we don't want to show a makefile error. If it 
		//is not, we'll return 1 to show the error
		if(options->is_test_run == TRUE){
			return 0;
		} else {
			return 1;
		}
	}

	//If we are doing module specific timing, store the parser time
	if(options->module_specific_timing == TRUE){
		//End the parser timer
		parser_end = clock();

		//Crude time calculation
		times.parser_time = (double)(parser_end - lexer_end) / CLOCKS_PER_SEC;
	}

	//Now we'll build the cfg using our results
	cfg_t* cfg = build_cfg(results, &num_errors, &num_warnings);

	//If we're doing debug printing, then we'll print this
	if(options->print_irs == TRUE){
		printf("============================================= BEFORE OPTIMIZATION =======================================\n");
		print_all_cfg_blocks(cfg);
		printf("============================================= BEFORE OPTIMIZATION =======================================\n");
	}

	//If we are doing module specific timing, store the cfg time
	if(options->module_specific_timing == TRUE){
		//End the parser timer
		cfg_end = clock();

		//Crude time calculation. The CFG starts when the parser ends
		times.cfg_time = (double)(cfg_end - parser_end) / CLOCKS_PER_SEC;
	}

	//Now we will run the optimizer
	cfg = optimize(cfg);

	//Again if we're doing debug printing, this is coming out
	if(options->print_irs == TRUE){
		printf("============================================= AFTER OPTIMIZATION =======================================\n");
		print_all_cfg_blocks(cfg);
		printf("============================================= AFTER OPTIMIZATION =======================================\n");
	}

	//If we are doing module specific timing, store the optimizer time
	if(options->module_specific_timing == TRUE){
		//End the optimizer timer
		optimizer_end = clock();

		//Crude time calculation. The optimizer starts when the cfg ends
		times.optimizer_time = (double)(optimizer_end - cfg_end) / CLOCKS_PER_SEC;
	}

	//First we'll go through instruction selection
	if(options->print_irs == TRUE){
		printf("=============================== Instruction Selection ==================================\n");
	}
	
	//Run the instruction selector. This simplifies and selects instructions
	select_all_instructions(options, cfg);

	//If we are doing module specific timing, store the selector time
	if(options->module_specific_timing == TRUE){
		//End the selector timer
		selector_end = clock();

		//Crude time calculation. The selector starts when the optimizer ends
		times.selector_time = (double)(selector_end - optimizer_end) / CLOCKS_PER_SEC;
	}

	if(options->print_irs == TRUE){
		printf("=============================== Instruction Selection ==================================\n");
		printf("=============================== Instruction Scheduling =================================\n");
	}

	//Now we need to schedule all of the instructions
	cfg = schedule_all_instructions(cfg, options);
	
	//If we are doing module specific timing, store the selector time
	if(options->module_specific_timing == TRUE){
		//End the selector timer
		scheduler_end = clock();

		//Crude time calculation. The scheduler starts when the selector ends
		times.scheduler_time = (double)(scheduler_end - selector_end) / CLOCKS_PER_SEC;
	}

	if(options->print_irs == TRUE){
		printf("=============================== Instruction Scheduling =================================\n");
		printf("=============================== Register Allocation ====================================\n");
	}
	//Run the register allocator. This will take the OIR version and truly put it into assembler-ready code
	allocate_all_registers(options, cfg);

	//If we are doing module specific timing, store the selector time
	if(options->module_specific_timing == TRUE){
		//End the selector timer
		allocator_end = clock();

		//Crude time calculation. The allocator starts when the selector ends
		times.allocator_time = (double)(allocator_end - scheduler_end) / CLOCKS_PER_SEC;
	}

	if(options->print_irs == TRUE){
		printf("=============================== Register Allocation  ===================================\n");
	}

	//Now we'll assemble the file *if* we are not doing a CI run
	if(options->is_test_run == FALSE){
		output_generated_code(options, cfg);
	}

	//Finish the timer here if we need to
	if(options->time_execution == TRUE || options->module_specific_timing == TRUE){
		//Timer end
		clock_t end = clock();

		//Crude time calculation
		times.total_time = (double)(end - begin) / CLOCKS_PER_SEC;
	}

	//Show the summary if we need to
	if(options->show_summary == TRUE){
		print_summary(options, &times, results->lines_processed, num_errors, num_warnings, TRUE);
	}

	/**
	 * We can deallocate memory as we go along here
	 *
	 * It's not entirely necessary to deallocate all of this memory, recall that
	 * the operating system reclaims all of it once the program runs. Since a compiler is
	 * not something that runs perpetually, we really don't need to worry about freeing the memory
	 */
	//Deallocate the ast
	ast_dealloc();
	free(results->os);
	function_symtab_dealloc(results->function_symtab);
	type_symtab_dealloc(results->type_symtab);
	variable_symtab_dealloc(results->variable_symtab);
	constants_symtab_dealloc(results->constant_symtab);
	dealloc_cfg(cfg);

	//Destroy the options array
	free(options);
	free(results);

	//Return 0 for success
	return 0;
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

	//Invoke the compiler
	return compile(options);
}
