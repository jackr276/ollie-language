/**
 * Author: Jack Robbins
 *
 * This file contains the implementations for the assembler.h file
 *
 * GENERAL COMPILATION FLOW
 *
 * User enters a file to be compiled(one .ol file)
 * The compiler uses the #link statements to determine what dependencies there are - NOT IMPLEMENTED/SPEC NOT FINALIZED
 * The compiler will orient the linked statements into one giant token array and compile as a monolith - NOT IMPLEMENTED/SPEC NOT FINALIZED
 * Following all of that, we end up in the file builder(here)
 * We will spit out generated assembly:
 * 	 Option 1: the user is compiling with "-a". -a means only output assembly. We will simply write out a .s file with the requested name(a.s if not)
 * 	 Option 2(most common): the user is just compiling:
 * 	 	We generate the assembly into a TEMPORARY .s file inside of /tmp/oc/
 * 	 	We invoke the GNU assembler(as) to assemble the file
 * 	 	We have the object(.o) file inside of the temp directory
 * 	 	We then compile __ostl_start_main.ol and put that in the temp directory
 * 	 	We then assemble _start.s and put that in the temp directory
 * 	 	We call the linked(ld) to link everything and put it into a final executable
  */
//For directory management
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

#include <stdio.h>
#include <sys/types.h>
#include "file_builder.h"
#include "../utils/constants.h"
#include "../utils/error_management.h"
#include "../utils/dynamic_string/dynamic_string.h"

//The current tmp file id
static u_int32_t current_tmp_file_id = 0;

//For any/all error printing
static char info[ERROR_SIZE];

//Hold all of our error counts
static u_int32_t* error_count;
static u_int32_t* warning_count;

/**
 * Simply prints a parse message in a nice formatted way
 */
static void print_assembler_message(error_message_type_t message_type, char* info){
	//Now print it
	const char* type[] = {"WARNING", "ERROR", "INFO", "DEBUG"};

	//Print this out on a single line
	fprintf(stdout, "\n[COMPILER %s]: %s\n", type[message_type], info);
}


/**
 * Helper that grabs the file id for us
 */
static inline u_int32_t increment_and_get_tmp_file_id(){
	return current_tmp_file_id++;
}


/**
 * Print an assembly block out
*/
static inline void print_assembly_block(FILE* fl, basic_block_t* block){
	//If this is some kind of switch block, we first print the jump table
	if(block->jump_table != NULL){
		print_jump_table(fl, block->jump_table);
	}

	//If it's a function entry block, we need to print this out
	if(block->block_type == BLOCK_TYPE_FUNC_ENTRY){
		//Now print the .text signifer so that GAS knows that this goes into .text
		fprintf(fl, "\t.text\n");

		//If this is a public function, we'll print out the ".globl" so
		//that it can be exposed to ld
		if(block->function_defined_in->signature->internal_types.function_type->is_public == TRUE){
			fprintf(fl, "\t.globl %s\n", block->function_defined_in->func_name.string);
		}

		//Now regardless of what kind of function it is, we'll use the @function tag
		//to tell AS that this is a function
		fprintf(fl, "\t.type %s, @function\n", block->function_defined_in->func_name.string);

		//Then the function name
		fprintf(fl, "%s:\n", block->function_defined_in->func_name.string);
	} else {
		fprintf(fl, ".L%d:\n", block->block_id);
	}

	//Now grab a cursor and print out every statement that we 
	//have
	instruction_t* cursor = block->leader_statement;

	//So long as it isn't null
	while(cursor != NULL){
		//We actually no longer need these
		if(cursor->instruction_type != PHI_FUNCTION){
			//Print a tab in the beginning for spacing
			fprintf(fl, "\t");
			print_instruction(fl, cursor, PRINTING_REGISTERS);
		}

		//Move along to the next one
		cursor = cursor->next_statement;
	}
}


/**
 * Print all assembly blocks in a CFG in order. Remember, by the time that we reach
 * here, these blocks will all already be in order from the block ordering procedure
 */
static void print_all_basic_blocks(FILE* fl, cfg_t* cfg){
	//Run through all blocks here
	for(u_int16_t i = 0; i < cfg->function_entry_blocks.current_index; i++){
		//Grab the head block out
		basic_block_t* current = dynamic_array_get_at(&(cfg->function_entry_blocks), i);

		//We can use the direct successor strategy here
		while(current != NULL){
			//Print it out
			print_assembly_block(fl, current);

			//Advance the pointer
			current = current->direct_successor;
		}
	}
}


/**
 * Print the .text section by running through and printing all of our basic blocks in assembly
 */
static inline void print_start_section(char* file_name, FILE* fl, cfg_t* cfg){
	//Declare the start of the new file to gas
	fprintf(fl, "\t.file\t\"%s\"\n", file_name);

	//Now that we've printed the text section, we need to print all basic blocks
	print_all_basic_blocks(fl, cfg);
}


/**
 * Assemble the program by writing it to a .s file. This is specifically intended for when the user
 * does *not* want to actually compile the program and requests that we output an assembly file only.
 * This case really only happens when we are doing test runs
*/
static u_int8_t output_generated_assembly_only(compiler_options_t* options, cfg_t* cfg){
	//The output file(Null initally)
	FILE* output = NULL;

	//Open the file for the purpose of writing
	output = fopen(options->output_file, "w");

	//If the file is null, we fail out here
	if(output == NULL){
		sprintf(info, "[ASSEMBLER ERROR]: Failed to create the output file: %s\n", options->output_file);
		print_assembler_message(MESSAGE_TYPE_ERROR, info);
		error_count++;
		return FAILURE;
	}

	//We'll first print the text segment of the program
	print_start_section(options->output_file, output, cfg);

	//Handle all of the global vars first
	print_all_global_variables(output, &(cfg->global_variables));

	//Print all of the local constants as well
	print_local_constants(output, &(cfg->local_string_constants), &(cfg->local_f32_constants), &(cfg->local_f64_constants), &(cfg->local_xmm128_constants));

	//Once we're done, close the file
	fclose(output);

	//Tell the caller that all went well
	return SUCCESS;
}


/**
 * We need to verify if the /tmp/oc/ directory exists on the machine. If it does not, we
 * will need to create it. We will not empty the directory in this step because that will
 * be handled by the cleanup function at the end of all compilation
 */
static u_int8_t perform_tmp_directory_management(){
	//Can we open /tmp?
	DIR* root_temp_dir = opendir("/tmp");

	/**
	 * If we can't even open the /tmp directory then the user
	 * is going to need to make it - we aren't going to mess
	 * around with that
	 */
	if(root_temp_dir == NULL){
		//Most likely thing - it just isn't there
		if(errno == ENOENT){
			print_assembler_message(MESSAGE_TYPE_ERROR, "The \"/tmp/\" directory does not exist on your system. Please create it and retry.");
		} else {
			print_assembler_message(MESSAGE_TYPE_ERROR, "Unable to open the \"/tmp/\" directory. If you are running linux, please create this directory and retry.");
		}

		(*error_count)++;
		return FAILURE;
	}

	//Remove the resource when done
	closedir(root_temp_dir);

	//For holding our directory
	struct stat stats = {0};

	/**
	 * Now that we know that the /tmp directory exists, we will
	 * now need to create the /tmp/oc/ directory to store all
	 * of our compiled files
	 */
	int32_t stat_result = stat("/tmp/oc/", &stats);

	/**
	 * If it's not here, then we need to make it
	 */
	if(stat_result != 0 || S_ISDIR(stats.st_mode) == FALSE){
		//Invoke the system call to make it
		int32_t mkdir_result = mkdir("/tmp/oc", 0777);

		//Not being 0 is unsuccessful, not much we can do here but leave if this happens
		if(mkdir_result != 0){
			print_assembler_message(MESSAGE_TYPE_ERROR, "OC was unable to create the needed \"/tmp/oc/\" directory. Please create it manually.");
			(*error_count)++;
			return FAILURE;
		}
	}


	//If we made it to here then this worked
	return SUCCESS;
}


/**
 * Take the generated assembly and convert it to an object file using GAS
 */
static void assemble_code(compiler_options_t* options){

}



/**
 * Perform the directory cleanup needed which includes wiping out all of the
 * object(.o) and assembly(.s) files that were placed in here. Ollie compilations 
 * are meant to never be incremental, and we enforce this by wiping out all of our
 * old compiled files at the end of every build
 */
static void perform_tmp_directory_cleanup(){
	//Open up the temp directory
	DIR* tmp_directory = opendir("/tmp/oc/");

	/**
	 * This should absolutely never happen. If it does it is a critcal error
	 */
	if(tmp_directory == NULL){
		fprintf(stderr, "Fatal internal compiler error: Attempt to open /tmp/oc/ failed\n");
		exit(1);
	}

	//Close it down before we leave
	closedir(tmp_directory);
}


/**
 * This inlined helper will perform all of the work, including management of the /tmp/oc/ directory
 * in order for us to compiler and link into a final executable
 */
static inline void assemble_and_link_with_temp_files(compiler_options_t* options, cfg_t* cfg){
	/**
	 * Step 1: OC requires that we have a /tmp/oc/ directory to hold all of our temporary
	 * compiled files. This is the first step that we need to take to ensure we're good
	 * to even go forward
	 */
	u_int32_t directory_management_result = perform_tmp_directory_management();

	//If this fails we're done
	if(directory_management_result == FAILURE){
		return;
	}

	/**
	 * Step TODO FILL ME OUT: OC will always clean up after itself. Ollie does not 
	 * do incremental builds, and the .o object files that it produces are single use. Once
	 * we've linked them into an executable, they're going to be scrapped.
	 *
	 * NOTE: if this fails it's a critical error and will hard crash the program, no need
	 * for error codes here at all
	 */
	perform_tmp_directory_cleanup();


	printf("TODO NOT DONE\n\n\n\n");
	exit(1);
}



/**
 * Perform all of the assembly and linkage that we need to do here. This is the only
 * API that is accessible for the final builder
 */
void assemble_and_link(compiler_options_t* options, cfg_t* cfg, u_int32_t* num_errors, u_int32_t* num_warnings){
	//Save these so we can update easily
	error_count = num_errors;
	warning_count = num_warnings;

	//Instruct the compiler to only output assembly
	u_int8_t output_assembly_only = options->go_to_assembly;

	//We assume that most of the time we actually want to compile
	if(options->go_to_assembly == FALSE){
		//Let the helper assemble and link with our temporary files
		assemble_and_link_with_temp_files(options, cfg);

	//Otherwise we likely have a test run - we need to just ouput the assembly *ONLY*
	} else {
		output_generated_assembly_only(options, cfg);
	}
}
