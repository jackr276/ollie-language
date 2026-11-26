/**
 * Author: Jack Robbins
 *
 * This file contains the implementations for the assembler.h file
 *
 * GENERAL COMPILATION FLOW
 * User enters a file to be compiled(one .ol file)
 * The compiler uses the #link statements to determine what dependencies there are
 * The compiler will orient the linked statements into one giant token array and compile as a monolith
 * Following all of that, we end up in the file builder(here)
 * We will spit out generated assembly:
 * 	 Option 1: the user is compiling with "-a". -a means only output assembly. We will simply write out a .s file with the requested name(a.s if not)
 * 	 Option 2(most common): the user is just compiling:
 * 	 	We generate a unique temporary directory underneat /tmp/oc/ that will store all of our compilation artifacts
 * 	 	We generate the assembly into a TEMPORARY .s file inside of /tmp/oc/<temp directory>
 * 	 	We invoke the GNU assembler(as) to assemble the file
 * 	 	We have the object(.o) file inside of the temp directory
 * 	 	We then compile __ostl_start_main.ol and put that in the temp directory
 * 	 	We then assemble _start.s and put that in the temp directory
 * 	 	We call the linked(ld) to link everything and put it into a final executable in the specified output file
 *
 * All temporary files should not survive past compilation. When done, the /tmp/oc/ directory should be entirely
 * empty if everything is working properly
 */

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
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

#define MAX_COMMAND_LENGTH 2000

//For any/all error printing
static char info[ERROR_SIZE * 3];

//Hold all of our error counts
static u_int32_t* error_count;
static u_int32_t* warning_count;

/**
 * Trigger a compiler panic with the given message
 * 
 * NOTE: this will intentionally crash the program
 */
static inline void trigger_ice_panic(char* message){
	fprintf(stderr, "Fatal internal compiler error: %s\n", message);
	exit(1);
}


/**
 * Simply prints a parse message in a nice formatted way
 */
static void print_assembler_message(error_message_type_t message_type, char* info){
	//Now print it
	const char* type[] = {"WARNING", "ERROR", "INFO", "DEBUG"};

	//Print this out on a single line
	fprintf(stdout, "\n[COMPILER %s]: %s\n", type[message_type], info);
}

//====================================================== PRINTING UTILITIES =============================================================
/**
 * Print an assembly block out
*/
static inline void print_assembly_block(FILE* fl, basic_block_t* block){
	/**
	 * If we have a jump table, we will output the jump table using the specific
	 * assembly generator helper
	 */
	if(block->jump_table != NULL){
		generate_jump_table_assembly(fl, block->jump_table);
	}

	//If it's a function entry block, we need to print this out
	if(block->block_type == BLOCK_TYPE_FUNC_ENTRY){
		//Now print the .text signifer so that GAS knows that this goes into .text
		fprintf(fl, "\t.text\n");

		//If this is a public function, we'll print out the ".globl" so
		//that it can be exposed to ld
		if(block->function_defined_in->signature->internal_types.function_type->visibility == VISIBILITY_TYPE_PUBLIC){
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

	//Now grab a cursor and print out every statement that we have
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
	/**
	 * Get the basename of our stripped file inside of a local copy
	 */
	char stripped_file_name[FILENAME_MAX];
	strncpy(stripped_file_name, file_name, FILENAME_MAX);

	//Declare the start of the new file to gas
	fprintf(fl, "\t.file\t\"%s\"\n", basename(stripped_file_name));

	//Now that we've printed the text section, we need to print all basic blocks
	print_all_basic_blocks(fl, cfg);
}
//====================================================== PRINTING UTILITIES =============================================================

//===================================================== DIRECTORY MANAGEMENT UTILTIES ==================================================
/**
 * We need to verify if the /tmp/oc/ directory exists on the machine. If it does not, we
 * will need to create it. This step is purely to guarantee that this folder is in existence
 * on the host machine. All other steps rely on this existing
 */
static u_int8_t validate_or_create_oc_tmp_folder(){
	/**
	 * If we can't even open the /tmp directory then the user
	 * is going to need to make it - we aren't going to mess
	 * around with that
	 */
	DIR* root_temp_dir = opendir("/tmp");
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
	 * Now that we know the /tmp directory exists, we will need
	 * to verify that /tmp/oc/ exists. This is the parent directory
	 * where all of our program subdirectories will live
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

	/**
	 * If we made it to here then this worked and we are all ready to proceed
	 */
	return SUCCESS;
}


/**
 * Create a temporary working directory where all of the outputted .s/.o files will be stored
 * during compilation. This working directory will be generated using mkdtemp() to guarantee complete
 * uniqueness. We will print the entire directory name into the preallocated buffer provided to us
 */
static inline void create_temp_working_directory(char* directory_name){
	sprintf(directory_name, "/tmp/oc/ollie_tmp_dir_XXXXXX");

	/**
	 * If we can't create it then fail out
	 */
	if(mkdtemp(directory_name) == NULL){
		trigger_ice_panic("Failed to create temporary directory for compilation");
	}
}


/**
 * Delete an entire directory by first deleting all files and subdirectories and then
 * deleting the directory itself
 */
static u_int8_t delete_directory(char* directory_path){
	/**
	 * Full path for our filename
	 */
	char full_path[FILENAME_MAX];

	/**
	 * This should absolutely never happen. If it does it is a critcal error
	 */
	DIR* tmp_directory = opendir(directory_path);
	if(tmp_directory == NULL){
		sprintf(info, "Unable to open directory %s", directory_path);
		trigger_ice_panic(info);
	}

	/**
	 * Loop infinitely until we have nothing left
	 */
	while(TRUE){
		/**
		 * Try to read the directory entry. If we can't read it
		 * then we're done
		 */
		struct dirent* entry = readdir(tmp_directory);
		if(entry == NULL){
			break;
		}

		/**
		 * Don't want to delete . or .. here, skip them
		 */
		if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
			continue;
		}

		/**
		 * Generate the fully qualified name for this entry
		 */
		snprintf(full_path, FILENAME_MAX, "%s/%s", directory_path, entry->d_name);

		/**
		 * Attempt to read the entry's full path to take action on deletion
		 */
		struct stat st;
		if(stat(full_path, &st) == 0){
			/**
			 * If this is a directory then recursively call this function
			 */
			if(S_ISDIR(st.st_mode)){
				delete_directory(full_path);

			/**
			 * Otherwise attempt to delete the entry
			 */
			} else {
				if(unlink(full_path) != 0){
					return FAILURE;
				}
			}
		}
	}

	/**
	 * Close the directory before leaving
	 */
	closedir(tmp_directory);

	/**
	 * This should now be empty, so we can use rmdir() to delete it
	 */
	if(rmdir(directory_path) == -1){
		fprintf(stderr, "Failed to delete directory %s", directory_path);
	}

	/**
	 * Otherwise this worked so tell the caller that all went well
	 */
	return SUCCESS;
}
//===================================================== DIRECTORY MANAGEMENT UTILTIES ==================================================

/**
 * Create a new temporary file underneath the given directory and populate it with the generated
 * assembly. The newly made temporary file will be populated into the "file_name" buffer
 */
static u_int8_t output_generated_assembly_to_temp_file(cfg_t* cfg, char* temporary_directory_path, char* file_name){
	/**
	 * Print out the template using the given temp
	 */
	sprintf(file_name, "%s/ollie_asm_tmp_XXXXXX.s", temporary_directory_path);
	int32_t file_descriptor = mkstemps(file_name, 2);

	/**
	 * Trigger a panic if this failed
	 */
	if(file_descriptor == -1){
		trigger_ice_panic("OC was unable to generate a unique temporary assembly file");
	}

	/**
	 * Close it when we're done. We will set the assembly file dynamic string to be equal
	 * to this given file
	 */
	close(file_descriptor);

	/**
	 * Open the new temp file for the purpose of writing
	 */
	FILE* output = fopen(file_name, "w");
	if(output == NULL){
		sprintf(info, "Failed to create the output file: %s\n", file_name);
		print_assembler_message(MESSAGE_TYPE_ERROR, info);
		error_count++;
		return FAILURE;
	}

	/**
	 * First print out the start section, then the global variables and finally all
	 * local constants
	 */
	print_start_section(file_name, output, cfg);
	print_all_global_variables(output, &(cfg->global_variables));
	print_local_constants(output, &(cfg->local_string_constants), &(cfg->local_f32_constants), &(cfg->local_f64_constants), &(cfg->local_xmm128_constants));

	/**
	 * Close the file and tell the caller that all went well
	 */
	fclose(output);
	return SUCCESS;
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
	/**
	 * Copy the entire thing into the result first
	 */
	strncpy(result, full_file_path, FILENAME_MAX);

	/**
	 * Scan through the string until we're able to find the ".s" pattern. We simply
	 * replace the "s" with an "o" and get out
	 */
	char* pointer = result;
	while(*pointer != '\0'){
		if(*pointer == '.' && *(pointer + 1) == 's'){
			*(pointer + 1) = 'o';
			return;
		}

		pointer++;
	}

	/**
	 * If we somehow got here then we actually didn't find a .s at all. This should never happen, but
	 * if it does wwe will panic
	 */
	sprintf(info, "String %s does not end in the standard assembly .s extension", full_file_path);
	trigger_ice_panic(info);
}


/**
 * Use the system() command to invoke the GNU assembly on the source file, with the output
 * directed towards the destination file
 */
static inline u_int8_t run_file_through_assembler(char* source_file_path, char* dest_file_path, u_int8_t debug_printing){
	/**
	 * Generate the command to use AS
	 */
	char command[MAX_COMMAND_LENGTH];
	snprintf(command, MAX_COMMAND_LENGTH, "as %s -o %s", source_file_path, dest_file_path);

	/**
	 * Show the assembler command if we're debugging
	 */
	if(debug_printing == TRUE){
		printf("ASSEMBLER COMMAND: %s\n", command);
	}

	/**
	 * Run the command in a subshell. Display any errors if we fail
	 */
	int result = system(command);
	if(result == 0) { 
		return SUCCESS;
	} else {
		sprintf(info, "The command %s failed with the error code %d", command, WEXITSTATUS(result));
		print_assembler_message(MESSAGE_TYPE_ERROR, info);
		(*error_count)++;
		return FAILURE;
	}
}


/**
 * Take all of the generated assembly that we've produced and convert
 * it into object files using AS. These object files will also reside in
 * the dedicated temporary directory and are only alive for the duration of the program
 */
static u_int8_t assemble_builtins_and_code(compiler_options_t* options, char* working_directory, char* temp_asm_file){
	char output_file[FILENAME_MAX];
	u_int8_t result;

	/**
	 * Step 1: assemble the builtin _start.s file into <working_directory>/_start.o
	 */
	sprintf(output_file, "%s/_start.o", working_directory);
	result = run_file_through_assembler("./oc/builtins/precompiled_builtins/_start.s", output_file, options->enable_debug_printing);
	if(result == FAILURE){
		return FAILURE;
	}

	/**
	 * Step 2: assemble the builtin __ostl_start_main.s file into <working_directory>/__ostl_start_main.o
	 */
	sprintf(output_file, "%s/__ostl_start_main.o", working_directory);
	result = run_file_through_assembler("./oc/builtins/precompiled_builtins/__ostl_start_main.s", output_file, options->enable_debug_printing);
	if(result == FAILURE){
		return FAILURE;
	}

	/**
	 * Step 3: assemble the temporary asm file into an object file underneath the working directory
	 */
	convert_assembly_file_name_to_object_file_name(output_file, temp_asm_file);
	result = run_file_through_assembler(temp_asm_file, output_file, options->enable_debug_printing);
	if(result == FAILURE){
		return FAILURE;
	}

	/**
	 * If we have made it to here then we have success
	 */
	return SUCCESS;
}


/**
 * Link the code using ld and produce the final executable. Our strategy
 * for linking is simple - every single file inside of the given working
 * directory that ends in a .o file will be linked
 */
static u_int8_t link_and_produce_final_executable(compiler_options_t* options, char* working_directory){
	/**
	 * We'll need a buffer for string printing. We'll use a dynamic string
	 * for our command since this may get long
	 */
	char buffer[FILENAME_MAX];
	dynamic_string_t command_string = dynamic_string_alloc();

	/**
	 * Print out the ld -o <output_name> part of our string
	 */
	snprintf(buffer, FILENAME_MAX, "ld -o %s ", options->output_file);
	dynamic_string_set(&command_string, buffer);

	/**
	 * Validate that we are able to access the working directory. Fail out
	 * if we cannot
	 */
	DIR* tmp_directory = opendir(working_directory);
	if(tmp_directory == NULL){
		sprintf(info, "Attempt to open %s failed", working_directory);
		trigger_ice_panic(info);
	}

	/**
	 * Run through every entry in the directory looking for object
	 * files inside of it
	 */
	while(TRUE){
		/**
		 * Read the directory entry. If it's NULL then we've got
		 * nothing left to read
		 */
		struct dirent* entry = readdir(tmp_directory);
		if(entry == NULL){
			break;
		}

		/**
		 * We never want to access these files
		 */
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
			snprintf(buffer, FILENAME_MAX, "%s/%s ", working_directory, entry->d_name);
			dynamic_string_concatenate(&command_string, buffer);
		}
	}

	/**
	 * Close the directory and show the linker command if we're after that
	 */
	closedir(tmp_directory);
	if(options->enable_debug_printing == TRUE){
		printf("LINKER COMMAND: %s\n", command_string.string);
	}

	/**
	 * We should now have a fully formed command string to run. Execute the linker and
	 * report back if it fails
	 */
	int32_t result = system(command_string.string);
	if(result != 0){
		sprintf(info, "Linking failed. Linker command attempted was: %s", command_string.string);
	}

	/**
	 * Destroy the command string and tell the caller that all went well
	 */
	dynamic_string_dealloc(&command_string);
	return SUCCESS;
}


//================================================================== COMPILATION RUNNERS ==============================================================
/**
 * Assemble the program by writing it to a .s file. This is specifically intended for when the user
 * does *not* want to actually compile the program and requests that we output an assembly file only.
 * This case really only happens when we are doing test runs
 */
static u_int8_t compile_to_assembly(compiler_options_t* options, cfg_t* cfg){
	/**
	 * Create the given file for the purpose of writing
	 */
	FILE* output = fopen(options->output_file, "w");
	if(output == NULL){
		sprintf(info, "[ASSEMBLER ERROR]: Failed to create the output file: %s\n", options->output_file);
		print_assembler_message(MESSAGE_TYPE_ERROR, info);
		error_count++;
		return FAILURE;
	}

	/**
	 * Generate the start section, then global variables, then local constants
	 */
	print_start_section(options->output_file, output, cfg);
	print_all_global_variables(output, &(cfg->global_variables));
	print_local_constants(output, &(cfg->local_string_constants), &(cfg->local_f32_constants), &(cfg->local_f64_constants), &(cfg->local_xmm128_constants));

	/**
	 * Close the file and tell the caller that all went well
	 */
	fclose(output);
	return SUCCESS;
}


/**
 * Compile and assemble the given CFG into a .o file inside of the user provided
 * output file
 */
static inline void compile_to_object_file(compiler_options_t* options, cfg_t* cfg){
	/**
	 * Step 0: before we do anything else, we need to validate that the /tmp/oc/ directory
	 * exists for us to use
	 */
	int32_t result = validate_or_create_oc_tmp_folder();
	if(result == FAILURE){
		trigger_ice_panic("Unable to validate or create the /tmp/oc/ folder");
	}

	/**
	 * Step 1: once we know that /tmp/oc exists, we will create a temporary output
	 * folder to hold all of our working files for this compilation run. This will
	 * be done using mkdtemp
	 */
	char working_directory[FILENAME_MAX];
	create_temp_working_directory(working_directory);

	/**
	 * Step 2: now that we have a temporary working directory to use, we can
	 * output the assembly to a temporary file that exists exclusively in that
	 * directory. This helper function will populate the temporary file string
	 * buffer with the file that it made
	 */
	char temporary_file[FILENAME_MAX];
	u_int8_t temp_file_result = output_generated_assembly_to_temp_file(cfg, working_directory, temporary_file);
	if(temp_file_result == FAILURE){
		return;
	}

	/**
	 * Step 3: Invoke the helper to run this temporary file through the assembler(GAS) and output
	 * it to the proveded output file
	 */
	run_file_through_assembler(temporary_file, options->output_file, options->enable_debug_printing);

	/**
	 * Step 4: the temporary directory that we've created is now useless. We will use the delete directoyr
	 * helper function to delete all temporary files and delete the temp directory
	 */
	delete_directory(working_directory);
}


/**
 * Compile, assemble and link the given CFG into a complete executable inside of the user provided
 * output file
 */
static inline u_int8_t compile_to_executable(compiler_options_t* options, cfg_t* cfg){
	/**
	 * Step 0: before we do anything else, we need to validate that the /tmp/oc/ directory
	 * exists for us to use
	 */
	int32_t result = validate_or_create_oc_tmp_folder();
	if(result == FAILURE){
		trigger_ice_panic("Unable to validate or create the /tmp/oc/ folder");
	}

	/**
	 * Step 1: once we know that /tmp/oc exists, we will create a temporary output
	 * folder to hold all of our working files for this compilation run. This will
	 * be done using mkdtemp
	 */
	char working_directory[FILENAME_MAX];
	create_temp_working_directory(working_directory);

	/**
	 * Step 2: now that we have a temporary working directory to use, we can
	 * output the assembly to a temporary file that exists exclusively in that
	 * directory. This helper function will populate the temporary file string
	 * buffer with the file that it made
	 */
	char temporary_file[FILENAME_MAX];
	u_int8_t temp_file_result = output_generated_assembly_to_temp_file(cfg, working_directory, temporary_file);
	if(temp_file_result == FAILURE){
		return FAILURE;
	}

	/**
	 * Step 3: we now need to take that assembly *and* the compiler builtins that we have and
	 * assemble them into .o files. These .o files will all reside in the dedicated temporary directory
	 * that we have
	 */
	u_int8_t assembler_result = assemble_builtins_and_code(options, working_directory, temporary_file);
	if(assembler_result == FAILURE){
		return FAILURE;
	}

	/**
	 * Step 4: take all of the .o files in the working directory and link them together into our final
	 * executable
	 */
	u_int8_t linker_result = link_and_produce_final_executable(options, working_directory);
	if(linker_result == FAILURE){
		return FAILURE;
	}

	/**
	 * Step 5: the temporary directory that we've created is now useless. We will use the delete directoyr
	 * helper function to delete all temporary files and delete the temp directory
	 */
	delete_directory(working_directory);
	
	/**
	 * If we got to here it worked
	 */
	return SUCCESS;
}

//================================================================== COMPILATION RUNNERS ==============================================================
/**
 * Perform all of the assembly and linkage that we need to do here. This is the only
 * API that is accessible for the final builder
 */
void assemble_and_link(compiler_options_t* options, cfg_t* cfg, u_int32_t* num_errors, u_int32_t* num_warnings){
	//Save these so we can update easily
	error_count = num_errors;
	warning_count = num_warnings;

	/**
	 * Based on what we were given, should be able to output things appropriately here
	 */
	switch(options->output_type){
		/**
		 * Just leave - nothing to do
		 */
		case OUTPUT_TYPE_NO_OUTPUT: {
			return;
		}

		/**
		 * Exclusively output assembly(.s)
		 *
		 * NOTE: this does *NOT* require temporary directory management
		 */
		case OUTPUT_TYPE_ASSEMBLY_ONLY: {
			compile_to_assembly(options, cfg);
			break;
		}
		
		/**
		 * Exclusively output the object(.o) file
		 *
		 * NOTE: this *DOES* require temporary directory management
		 */
		case OUTPUT_TYPE_OBJECT_FILE: {
			compile_to_object_file(options, cfg);
			break;
		}

		/**
		 * Full compilation into an executable
		 *
		 * NOTE: this *DOES* require temporary directory management
		 */
		case OUTPUT_TYPE_FULL_COMPILATION: {
			u_int8_t result = compile_to_executable(options, cfg);

			//Give a good exit code if we failed
			if(result == FAILURE){
				trigger_ice_panic("Program Compilation Failed");
			}

			break;
		}
	}
}
