/**
 * Author: Jack Robbins
 * This C file implements the APIs defined in the header
 * file of the same name
 */

#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/types.h>
#include "option_parser.h"
#include "../constants.h"

//Objectfile opt for getopt_long
#define objectfile_opt 5

/**
 * Simply prints a parse message in a nice formatted way
 */
static void print_compiler_message(error_message_type_t message_type, char* info){
	//Now print it
	const char* type[] = {"WARNING", "ERROR", "INFO", "DEBUG"};

	//Print this out on a single line
	fprintf(stdout, "\n[COMPILER %s]: %s\n", type[message_type], info);
}


/**
 * A help printer function for users of the compiler
 */
static void print_help(){
	printf("\n===================================== Ollie Compiler Options =====================================\n");
	printf("\n######################################## Required Fields #########################################\n");
	printf("-f <filename>: Required field. Specifies the .ol source file to be compiled\n");
	printf("\n######################################## Optional Fields #########################################\n");
	printf("-o <filename>: Specificy the output file. If none is given, a.out will be used\n");
	printf("--to-object-file: Compile the entire thing to an object(.o) file. If you do not know what this is then you shoud not be using it\n");
	printf("-s: Show a summary at the end of compilation\n");
	printf("-a: Generate an assembly code file with a .s extension. Note that this will stop the actual assembler from running\n");
	printf("-d: Show all debug information printed. This includes compiler warnings, info statements\n");
	printf("-r: Print the result of the register allocation. This is done by default in -i\n");
	printf("-t: Time execution of compiler. Can be used for performance testing\n");
	printf("-m: Time each module of the compiler. This is used for even more granular performance testing\n");
	printf("-@: Should only be used for CI runs. Avoids generating any assembly/object files\n");
	printf("-i: Print intermediate representations. This will generate *a lot* of text, so be careful\n");
	printf("-h: Show help\n");
	printf("\n==================================================================================================\n");
}


/**
 * We'll use this helper function to process the compiler flags and return a structure that
 * tells us what we need to do throughout the compiler
 */
compiler_options_t* parse_and_store_options(int argc, char** argv, u_int32_t* num_warnings, u_int32_t* num_errors){
	//Allocate it
	compiler_options_t* options = calloc(1, sizeof(compiler_options_t));

	//By default, assume we are requesting a full compilation
	options->output_type = OUTPUT_TYPE_FULL_COMPILATION;

	/**
	 * Longopts for us to use. Currently we only have the objectfile
	 * longopt here
	 */
	const struct option long_opts[] = {
		{"to-object-file", no_argument, NULL, objectfile_opt},
		//Null terminator
		{0,0,0,0}
	};
	
	//Run through all of our options
	int32_t opt;
	while((opt = getopt_long(argc, argv, "rima@tdhsf:o:?", long_opts, NULL)) != -1){
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

			//Test run flag means that we have no output
			case '@':
				options->output_type = OUTPUT_TYPE_NO_OUTPUT;
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
				//Input validation based on past state
				switch(options->output_type){
					case OUTPUT_TYPE_OBJECT_FILE:
						print_compiler_message(MESSAGE_TYPE_ERROR, "The --to-object-file flag was already provided. These two flags are mutually exclusive");
						exit(1);
					case OUTPUT_TYPE_NO_OUTPUT:
						print_compiler_message(MESSAGE_TYPE_ERROR, "The @ flag already marked this as a test run. -a request is ignored");
						break;
					default:
						options->output_type = OUTPUT_TYPE_ASSEMBLY_ONLY;
						break;
				}

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

			//Specify that we want to specifically output to an objectfile *only*
			case objectfile_opt:
				//Input validation based on past state
				switch(options->output_type){
					case OUTPUT_TYPE_ASSEMBLY_ONLY:
						print_compiler_message(MESSAGE_TYPE_ERROR, "The -a \"to assembly\" flag was already provided. These two flags are mutually exclusive");
						exit(1);
					case OUTPUT_TYPE_NO_OUTPUT:
						print_compiler_message(MESSAGE_TYPE_ERROR, "The @ flag already marked this as a test run. --to-object-file request is ignored");
						break;
					default:
						options->output_type = OUTPUT_TYPE_OBJECT_FILE;
						break;
				}

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

	/**
	 * If we don't get an input file it's either going to be a.s or a.out. We will warn about this
	 */
	if(options->output_file == NULL){
		switch(options->output_type){
			case OUTPUT_TYPE_OBJECT_FILE:
				options->output_file = "a.o";

				//Warn the user
				print_compiler_message(MESSAGE_TYPE_WARNING, "No ouput file was given, \"a.o\" will be used");
				num_warnings++;

				break;

			case OUTPUT_TYPE_ASSEMBLY_ONLY:
				options->output_file = "a.s";

				//Warn the user
				print_compiler_message(MESSAGE_TYPE_WARNING, "No ouput file was given, \"a.s\" will be used");
				num_warnings++;

				break;

			case OUTPUT_TYPE_FULL_COMPILATION:
				options->output_file = "a.out";

				//Warn the user
				print_compiler_message(MESSAGE_TYPE_WARNING, "No ouput file was given, \"a.out\" will be used");
				num_warnings++;

				break;

			default:	
				break;
		}
	}

	//Give back the options we got in the structure
	return options;
}
