/**
 * Author: Jack Robbins
 *
 * This file is the entry point for the entire OC compiler
*/
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <time.h>
#include "ast/ast.h"
#include "build_system/build_system.h"
#include "parser/parser.h"
#include "preprocessor/preprocessor.h"
#include "symtab/symtab.h"
#include "cfg/cfg.h"
#include "register_allocator/register_allocator.h"
#include "instruction_selector/instruction_selector.h"
#include "instruction_scheduler/instruction_scheduler.h"
#include "assembler/assembler.h"
#include "optimizer/optimizer.h"
#include "utils/compiler_output_type.h"
#include "utils/constants.h"
#include "utils/error_management.h"

//The number of errors and warnings
u_int32_t num_errors = 0;
u_int32_t num_warnings = 0;

//Objectfile opt for getopt_long
#define objectfile_opt 5


/**
 * Print a final summary for the ollie compiler. This could show success or
 * failure, based on what the caller wants
 */
static void print_summary(compiler_options_t* options, module_times_t* times, u_int32_t lines_processed, u_int8_t success){
	//For holding our message
	char info[2000];

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
		printf("Lexer & Build System took: %.8f seconds\n", times->lexer_time);
		printf("Preprocessor took: %.8f seconds\n", times->preprocessor_time);
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
	module_times_t times = {0, 0, 0, 0, 0, 0, 0, 0, 0};

	//Print out the file name if we're debug printing
	printf("Compiling source file: %s\n\n\n", options->file_name);

	//And we'll keep track of everything we have here
	clock_t begin = 0;
	clock_t lexer_end = 0;
	clock_t preprocessor_end = 0;
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

	/**
	 * Step 1: run the build system first. The build system will give back a dynamic array
	 * of build system nodes that each have their own independent token streams which we will
	 * need to string together in the later nodes
	 */
	build_system_results_t build_system_results = construct_build_order(options, FALSE);

	//Save the compilation order here
	options->build_order = build_system_results.compilation_order;

	//If it failed, we need to leave immediately
	if(build_system_results.status == BUILD_SYSTEM_STATUS_FAILURE){
		fprintf(stdout, "\n[FILE: %s]: Tokenizing/build system failed. Please remedy the errors and recompile\n\n", options->file_name);
		num_errors++;

		//Timer end
		end = clock();

		//Crude time calculation
		times.total_time = (double)(end - begin) / CLOCKS_PER_SEC;

		//Print summary with a failure here
		if(options->show_summary == TRUE){
			print_summary(options, &times, 0, FALSE);
		}

		/**
		 * If this is a test run, we will return 0 because we don't want to show a makefile error. If it 
		 * is not, we'll return 1 to show the error
		 */
		if(options->output_type != OUTPUT_TYPE_NO_OUTPUT){
			return 1;
		} else {
			return 0;
		}
	}

	//If we are doing module specific timing, store the lexer time
	if(options->module_specific_timing == TRUE){
		//End the parser timer
		lexer_end = clock();

		//Crude time calculation
		times.lexer_time = (double)(lexer_end - begin) / CLOCKS_PER_SEC;
	}

	/**
	 * Let the preprocessor handle everything to do with macros. Note that this does have the potential
	 * to fail
	 */
	preprocessor_results_t preprocessor_results = preprocess(options);

	//Update the warnings/errors if there are any
	num_errors += preprocessor_results.error_count;
	num_warnings += preprocessor_results.warning_count;

	//If we failed here then we are done 
	if(preprocessor_results.status == PREPROCESSOR_FAILURE){
		//Timer end
		end = clock();

		//Crude time calculation
		times.total_time = (double)(end - begin) / CLOCKS_PER_SEC;

		//Print summary with a failure here
		if(options->show_summary == TRUE){
			print_summary(options, &times, 0, FALSE);
		}

		/**
		 * If this is a test run, we will return 0 because we don't want to show a makefile error. If it 
		 * is not, we'll return 1 to show the error
		 */
		if(options->output_type != OUTPUT_TYPE_NO_OUTPUT){
			return 1;
		} else {
			return 0;
		}
	}

	//If we are doing module specific timing, store the preprocessor time
	if(options->module_specific_timing == TRUE){
		//End the parser timer
		preprocessor_end = clock();

		//Crude time calculation
		times.preprocessor_time = (double)(preprocessor_end - lexer_end) / CLOCKS_PER_SEC;
	}

	//Now we'll parse the whole thing
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
			print_summary(options, &times, results->lines_processed, FALSE);
		}

		/**
		 * If this is a test run, we will return 0 because we don't want to show a makefile error. If it 
		 * is not, we'll return 1 to show the error
		 */
		if(options->output_type != OUTPUT_TYPE_NO_OUTPUT){
			return 1;
		} else {
			return 0;
		}
	}

	//If we are doing module specific timing, store the parser time
	if(options->module_specific_timing == TRUE){
		//End the parser timer
		parser_end = clock();

		//Crude time calculation
		times.parser_time = (double)(parser_end - preprocessor_end) / CLOCKS_PER_SEC;
	}

	//Now we'll build the cfg using our results
	cfg_t* cfg = build_cfg(results, &num_errors, &num_warnings);

	/**
	 * If we have a CFG failure, we need to fail out here and not process
	 * any further
	 */
	if(cfg->result == CFG_RESULT_FAILURE){
		//Timer end
		end = clock();

		//Crude time calculation
		times.total_time = (double)(end - begin) / CLOCKS_PER_SEC;

		//Print summary with a failure here
		if(options->show_summary == TRUE){
			print_summary(options, &times, results->lines_processed, FALSE);
		}

		/**
		 * If this is a test run, we will return 0 because we don't want to show a makefile error. If it 
		 * is not, we'll return 1 to show the error
		 */
		if(options->output_type != OUTPUT_TYPE_NO_OUTPUT){
			return 1;
		} else {
			return 0;
		}
	}

	//If we're doing debug printing, then we'll print this
	if(options->print_irs == TRUE){
		printf("============================================= BEFORE OPTIMIZATION =======================================\n");
		//Print the adjacency matrix out
		print_call_graph_adjacency_matrix(stdout, results->function_symtab);
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

	/**
	 * Note that if we are doing a test run, we will not do any file outputting at all. Our
	 * guard against that is here
	 */
	if(options->output_type != OUTPUT_TYPE_NO_OUTPUT){
		//Run the assembler/linker. This will update errors if we have them
		assemble_and_link(options, cfg, &num_errors, &num_warnings);
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
		print_summary(options, &times, results->lines_processed, TRUE);
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
	function_symtab_dealloc(results->function_symtab);
	type_symtab_dealloc(results->type_symtab);
	variable_symtab_dealloc(results->variable_symtab);
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
