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
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <libgen.h>

#include <stdio.h>
#include <sys/types.h>
#include "assembler.h"
#include "../utils/constants.h"
#include "../utils/error_management.h"
#include "../utils/dynamic_string/dynamic_string.h"

#define TEMP_FILE_NAME_MAX_LENGTH 1000
#define MAX_COMMAND_LENGTH 2000

//The current tmp file id
static u_int32_t current_tmp_file_id = 0;

//For any/all error printing
static char info[ERROR_SIZE * 3];

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
	current_tmp_file_id++;

	return current_tmp_file_id;
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

	//We'll first print the text segment of the program. Make sure that we use the basename
	//for this
	print_start_section(basename(options->output_file), output, cfg);

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
 * Construct the assembly for the program and output it to a temp file. This temp file will 
 * be placed inside of the /tmp/oc/ directory while it is waiting for final assembly and linkage
 * into the file that we're after
 */
static u_int8_t output_generated_assembly_to_temp_file(compiler_options_t* options, cfg_t* cfg, dynamic_array_t* outputted_files){
	//The output file(Null initally)
	FILE* output = NULL;

	//Heap allocate this so we can keep it in the dynamic array
	dynamic_string_t* temporary_file_name = dynamic_string_heap_alloc();

	//Just add it in now
	dynamic_array_add(outputted_files, temporary_file_name);

	//We need to create the name ourselves
	char outputted_assembly_file[1000];
	
	//Output it like so, we need to set the dynamic string to be this
	sprintf(outputted_assembly_file, "/tmp/oc/ollie_asm_tmp%d.s", increment_and_get_tmp_file_id());

	//Set the dynamic string to be this
	dynamic_string_set(temporary_file_name, outputted_assembly_file);

	//Open the file for the purpose of writing
	output = fopen(outputted_assembly_file, "w");

	//If the file is null, we fail out here
	if(output == NULL){
		sprintf(info, "Failed to create the output file: %s\n", outputted_assembly_file);
		print_assembler_message(MESSAGE_TYPE_ERROR, info);
		error_count++;
		return FAILURE;
	}

	//We'll first print the text segment of the program
	print_start_section(basename(outputted_assembly_file), output, cfg);

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
 * Recursively delete all files and directories inside of the "path"
 * directory
 */
static u_int8_t clear_directory_recursive(char* path){
	//We need this for printing
	char full_path[1000];

	//Open up the temp directory
	DIR* tmp_directory = opendir(path);
	//Entry pointer
	struct dirent* entry;
	//Status struct
	struct stat st;

	/**
	 * This should absolutely never happen. If it does it is a critcal error
	 */
	if(tmp_directory == NULL){
		fprintf(stderr, "Fatal internal compiler error: Attempt to open /tmp/oc/ failed\n");
		return FAILURE;
	}

	//Infinite loop until we break out
	while(TRUE){
		//Seed the entry
		entry = readdir(tmp_directory);

		//We're done
		if(entry == NULL){
			break;
		}

		//Don't want to delete . or .. here, skip them
		if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
			continue;
		}

		//Here's our full path
		snprintf(full_path, 1000, "%s/%s", path, entry->d_name);

		//If we can read it
		if(stat(full_path, &st) == 0){
			if(S_ISDIR(st.st_mode)){
				clear_directory_recursive(full_path);
			} else {
				if(unlink(full_path) != 0){
					return FAILURE;
				}
			}
		}
	}

	//Close it down before we leave
	closedir(tmp_directory);

	return SUCCESS;
}


/**
 * Perform the directory cleanup needed which includes wiping out all of the
 * object(.o) and assembly(.s) files that were placed in here. Ollie compilations 
 * are meant to never be incremental, and we enforce this by wiping out all of our
 * old compiled files at the end of every build
 */
static inline u_int8_t perform_tmp_directory_cleanup(){
	//Seed the helper and let it do the rest
	return clear_directory_recursive("/tmp/oc");
}


/**
 * Convert an assembly file name into a .o file name. This is really
 * as simple as replacing the ".s" with a ".o". We return a new string for
 * this
 *
 * This assumes we have a pre-allocated result buffer that we will be storing
 * to. We return void because there's no case where this fails
 */
static inline void convert_assembly_file_name_to_object_file_name(char* result, char* full_file_path){
	//Get the base name
	char* base_name = basename((char*)full_file_path);

	//Copy the base name into the result
	strncpy(result, base_name, TEMP_FILE_NAME_MAX_LENGTH);

	//Convert the .s into a .o
	char* pointer = result;

	//Run through everything here - the null terminator is more of a precaution
	while(*pointer != '\0'){
		//If we find the .s just make the s into an o
		if(*pointer == '.' && *(pointer + 1) == 's'){
			*(pointer + 1) = 'o';
			return;
		}

		pointer++;
	}

	//If we somehow got here then we actually didn't find a .s at all
	fprintf(stderr, "Fatal internal compiler error: string %s does not end in the standard assembly .s extension\n", full_file_path);
}


/**
 * Handle all of the process management using fork() to call as on a given
 * full file path. These full files will always output to /oc/tmp/ as
 * .o files
 */
static inline u_int8_t run_file_through_assembler(char* full_file_path){
	//We will need this for the eventual output
	char object_file_name[TEMP_FILE_NAME_MAX_LENGTH];
	char command[MAX_COMMAND_LENGTH];

	//Let the converter get us the .o file name
	convert_assembly_file_name_to_object_file_name(object_file_name, full_file_path);

	//Now create the command
	snprintf(command, MAX_COMMAND_LENGTH, "as %s -o /tmp/oc/%s", full_file_path, object_file_name);

	//Run the command in the shell
	int result = system(command);

	if(result == 0) { 
		return SUCCESS;
	} else {
		sprintf(info, "The command %s failed with the error code %d", command, result);
		print_assembler_message(MESSAGE_TYPE_ERROR, info);
		(*error_count)++;
		return FAILURE;
	}
}


/**
 * Take all of the generated assembly that we've produced and convert
 * it into object files using AS. These object files will also reside in
 * /tmp/oc/ and are only alive for the duration of the program
 *
 * NOTE: This will spawn child processes so that we can run the GNU assembler
 */
static u_int8_t assemble_code(dynamic_array_t* outputted_files){
	u_int8_t result;

	/**
	 * Step 1: assemble the builtin _start.s file into /tmp/oc/_start.o
	 */
	result = run_file_through_assembler("./oc/builtins/precompiled_builtins/_start.s");

	if(result == FAILURE){
		return FAILURE;
	}

	/**
	 * Step 2: assemble the builtin __ostl_start_main.s file into /tmp/oc/__ostl_start_main.o
	 */
	result = run_file_through_assembler("./oc/builtins/precompiled_builtins/__ostl_start_main.s");

	if(result == FAILURE){
		return FAILURE;
	}

	/**
	 * Step 3: For everything in our list of temporary assembly files, assembly
	 * them into their own .o files respectively
	 */
	for(u_int32_t i = 0; i < outputted_files->current_index; i++){
		//Get the full path name to the file
		dynamic_string_t* file_to_compile = dynamic_array_get_at(outputted_files, i);

		//Run it through the assembler
		result = run_file_through_assembler(file_to_compile->string);

		//The error already got printed out by the callee so just fail out
		if(result == FAILURE){
			return FAILURE;
		}
	}

	//If we have made it to here then we have success
	return SUCCESS;
}


/**
 * Link the code using ld and produce the final executable. Our strategy
 * for linking is simple - every single file inside of /tmp/oc/ that ends in 
 * a ".o" extension is being linked together.
 */
static u_int8_t link_and_produce_final_executable(compiler_options_t* options){
	//We'll need a buffer for file printing
	char buffer[TEMP_FILE_NAME_MAX_LENGTH];

	//No real choice but to use dynamic allocation here
	dynamic_string_t command_string = dynamic_string_alloc();

	//Print out the ld -o <output_name> part of our string
	snprintf(buffer, TEMP_FILE_NAME_MAX_LENGTH, "ld -o %s ", options->output_file);
	dynamic_string_set(&command_string, buffer);

	//Open up the tmp directory
	DIR* tmp_directory = opendir("/tmp/oc");

	//Entry pointer
	struct dirent* entry;
	//Status struct
	struct stat st;

	/**
	 * This should absolutely never happen. If it does it is a critcal error
	 */
	if(tmp_directory == NULL){
		fprintf(stderr, "Fatal internal compiler error: Attempt to open /tmp/oc failed\n");
		return FAILURE;
	}

	//Infinite loop until we break out
	while(TRUE){
		//Seed the entry
		entry = readdir(tmp_directory);

		//We're done
		if(entry == NULL){
			break;
		}

		//Don't want to delete . or .. here, skip them
		if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
			continue;
		}

		/**
		 * We now need to determine if this file ends in a ".o". If it does it's
		 * going onto the list
		 */
		int32_t file_length = strlen(entry->d_name);

		/**
		 * If we end in ".o", it's coming along for the ride in the linker. We will
		 * dynamically construct what we need and go from there
		 */
		if(entry->d_name[file_length - 2] == '.' && entry->d_name[file_length - 1] == 'o'){
			snprintf(buffer, TEMP_FILE_NAME_MAX_LENGTH, "/tmp/oc/%s", entry->d_name);
			dynamic_string_concatenate(&command_string, buffer);
		}
	}

	//Close it down before we leave
	closedir(tmp_directory);

	fprintf(stderr, "Fatal internal compiler error: Linking failed. Linker command attempted was: %s\n", command_string.string);
	//We should now have a fully formed command string to run
	//int result = system(command_string.string);


	//Fatal error if this fails
	/*
	if(result != 0){
		fprintf(stderr, "Fatal internal compiler error: Linking failed. Linker command attempted was: %s\n", command_string.string);
		return FAILURE;
	}
	*/

	//Destroy it
	dynamic_string_dealloc(&command_string);

	return SUCCESS;
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
	 * Step 2: We will now clean everything else inside of our directory out. There will
	 * likely be stuff in here from old runs
	 */
	u_int32_t directory_clean_result = perform_tmp_directory_cleanup();

	//Fatal error here so we get out if it fails
	if(directory_clean_result == FAILURE){
		return;
	}

	/**
	 * Step 2: we need to now output the cfg into a temporary .s assembly
	 * file. We will place this temporary .s assembly file inside of the
	 * oc/tmp directory waiting to be assembled
	 */
	dynamic_array_t outputted_files = dynamic_array_alloc();

	//Let the helper produce this
	u_int8_t assembly_outputter_result = output_generated_assembly_to_temp_file(options, cfg, &outputted_files);

	//It didn't work so don't bother going on
	if(assembly_outputter_result == FAILURE){
		return;
	}

	/**
	 * Step 3: we now need to take that assembly *and* the compiler builtins that we have and
	 * assemble them into .o files. These .o files will also all reside inside of the /oc/tmp/
	 * directory. If any of these files fail to assemble then we fail out
	 */
	u_int8_t assembler_result = assemble_code(&outputted_files);

	//This means we generated incorrect assembly which would be bad
	if(assembler_result == FAILURE){
		return;
	}

	u_int8_t linker_result = link_and_produce_final_executable(options);

	//Destroy all of the outputted files
	dynamic_array_dealloc(&outputted_files);
	

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

	//We assume that most of the time we actually want to compile
	if(options->go_to_assembly == FALSE){
		//Let the helper assemble and link with our temporary files
		assemble_and_link_with_temp_files(options, cfg);

	//Otherwise we likely have a test run - we need to just ouput the assembly *ONLY*
	} else {
		output_generated_assembly_only(options, cfg);
	}
}
