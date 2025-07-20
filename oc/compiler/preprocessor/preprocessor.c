/**
 * Author: Jack Robbins
 *
 * The implementation file for the ollie preprocessor. The ollie preprocessor will tell 
 * the compiler what it needs to compile to make something run. It will also check for 
 * cyclical dependencies, etc
 *
 * If a link file is included in "<>", this instructs the compiler to look for it in the 
 * location of /usr/lib. If it is enclosed in double quotes, the compiler will only use the
 * absolute path of the file
*/

#include "preprocessor.h"
//For tokenizing/lexical analyzing purposes
#include "../lexer/lexer.h"
//We'll make use of this too
#include "../dynamic_array/dynamic_array.h"
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

//Default error size
#define DEFAULT_ERROR_SIZE 1000

//For standardization across all modules
#define TRUE 1
#define FALSE 0

/**
 * This dictates any errors that we print out
 */
typedef enum{
	PREPROC_ERR = 0,
	PREPROC_WARN,
	PREPROC_INFO,
} preproc_msg_type_t;

//These come from the parser
extern u_int32_t num_errors;
extern u_int32_t num_warnings;

/**
 * Print out a custom, stylized preprocessor error for the user
 */
static void print_preproc_error(preproc_msg_type_t type, char* error_message, char* filename){
	//For ease of printing
	char* message_types[3] = {"ERROR", "WARNING", "INFO"};

	//Print out the error in a stylized manner
	if(filename == NULL){
		printf("[PREPROCESSOR %s]: %s\n", message_types[type], error_message);
	//We can add the name in here
	} else {
		printf("[FILE: %s] --> [PREPROCESSOR %s]: %s\n", filename, message_types[type], error_message);
	}
}


/**
 * Print out a custom, stylized preprocessor error for the user with number line
 */
static void print_preproc_error_linenum(preproc_msg_type_t type, char* error_message, u_int16_t line_num, char* filename){
	//For ease of printing
	char* message_types[] = {"ERROR", "WARNING", "INFO"};

	//Print out the error in a stylized manner
	if(filename == NULL){
		printf("[LINE %d | PREPROCESSOR %s]: %s\n", line_num, message_types[type], error_message);
	} else {
		printf("[FILE: %s] --> [LINE %d | PREPROCESSOR %s]: %s\n", filename, line_num, message_types[type], error_message);
	}
}

/**
 * Create and return a dependency tree node that is in error. This is to support
 * our errors-as-values approach
 */
static dependency_tree_node_t* create_and_return_error_node(){
		//Create and return an error node
		dependency_tree_node_t* err = dependency_tree_node_alloc(NULL);
		//Set it to be in error
		err->is_in_error = TRUE;
		//Give it back
		return err;
}


/**
 * Recursively build a dependency tree. We'll take in the parent node for the tree,
 * and process all of it's dependencies by opening the file and parsing. In theory,
 * by the time that we're done, we'll have a fully built tree with the root of the tree
 * being the file that was origianlly passed in
 */
static dependency_tree_node_t* build_dependency_tree_rec(char* fname){
	//For any/all error printing
	char info[DEFAULT_ERROR_SIZE];
	//The parser line number -- largely unused in this module, but there is a chance
	//that we'll need it for errors if for some reason we run into an error preprocessing
	u_int16_t parser_line_num = 0;
	//The lookahead token
	lexitem_t lookahead;
	//Keep a running list of what we need to compile
	dynamic_array_t* dependency_list;

	//Open the file first off
	FILE* fl = fopen(fname, "r");

	//If it fails we're done
	if(fl == NULL){
		sprintf(info, "File %s could not be opened", fname);
		print_preproc_error(PREPROC_ERR, info, NULL);
		//Give back an error
		return create_and_return_error_node();
	}

	//Now that we know this file has actually openened, we're safe to create a dependency tree node
	//for it
	dependency_tree_node_t* root_node = dependency_tree_node_alloc(fname);

	//We will run through the opening part of the file. If we do not
	//see the comptime guards, we will back right out
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we see a DEPENDENCIES token, we need to keep going. However if we don't see this, we're
	//completely done here
	if(lookahead.tok != DEPENDENCIES){
		//Close the file
		fclose(fl);
		//Just give back the root node here -- there's no issue
		return root_node;
	}

	//If we make it here then we did see the dependencies directive. We can do a very
	//simple check for a common error by seeing if the next token is the dependencies
	//end guard. In doing this we'll save processing time and ensure that any allocations
	//going forward are actually needed
	lookahead = get_next_token(fl,  &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	if(lookahead.tok == DEPENDENCIES){
		//Throw a warning for the user
		print_preproc_error_linenum(PREPROC_WARN, "Empty \"dependencies\" region detected, consider removing it.", parser_line_num, fname);
		//And we'll close the file, and give it back
		fclose(fl);
		return root_node;
	}

	//Now that we know we did see this, we should actually create our dynamic array
	dependency_list = dynamic_array_alloc(); 
	
	//Otherwise it did have a comptime guard. As such, we'll need to parse through
	//require statements one by one here, seeing which files are requested
	//So long as we keep seeing require -- there is no limit here
	while(lookahead.tok == REQUIRE){
		//After the require keyword, we can either see the "lib" keyword or a string constant
		lookahead = get_next_token(fl, &parser_line_num, SEARCHING_FOR_CONSTANT);

		//We have a library file here -- special location
		//TODO LIKELY NOT DONE
		if(lookahead.tok == LIB){
			//We still need to see a string constant
			lookahead = get_next_token(fl, &parser_line_num, SEARCHING_FOR_CONSTANT);

			//If we don't see one here, then it's immediately a failure
			if(lookahead.tok != STR_CONST){
				print_preproc_error_linenum(PREPROC_ERR, "Filename required after \"lib\" keyword", parser_line_num, fname);
			}

			//Allocate it 
			char* added_filename = calloc(FILE_NAME_LENGTH + 1, sizeof(char));

			//Copy the lexeme over
			strncpy(added_filename, lookahead.lexeme.string, strlen(lookahead.lexeme.string) + 1);

			//One last thing that we need to see -- closing semicolon
			lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

			//If it's not a semicolon, we fail
			if(lookahead.tok != SEMICOLON){
				print_preproc_error_linenum(PREPROC_ERR, "Semicolon required after require statement", parser_line_num, fname);
				fclose(fl);
				//Package up and return an error here
				return create_and_return_error_node();
			}

		} else if(lookahead.tok == STR_CONST){
			
			//Allocate it 
			char* added_filename = calloc(FILE_NAME_LENGTH + 1, sizeof(char));

			//Copy the lexeme over
			strncpy(added_filename, lookahead.lexeme.string, strlen(lookahead.lexeme.string) + 1);

			//One last thing that we need to see -- closing semicolon
			lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

			//If it's not a semicolon, we fail
			if(lookahead.tok != SEMICOLON){
				print_preproc_error_linenum(PREPROC_ERR, "Semicolon required after require statement", parser_line_num, fname);
				fclose(fl);
				//Package up and return an error here
				return create_and_return_error_node();
			}

		} else {
			//This is an error here
			print_preproc_error(PREPROC_ERR, "\"lib\" keyword or filename required after \"require\" keyword", fname);
			fclose(fl);
			//Package up and return an error here
			return create_and_return_error_node();
		}

		//Refresh the token
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	}

	//At the very end, if what we saw here causing us to exit was not a COMPTIME token, we
	//have some kind of issue
	if(lookahead.tok != DEPENDENCIES){
		print_preproc_error(PREPROC_ERR, "#dependencies end guard expected after preprocessor region", fname);
		fclose(fl);
		//Package up an error and send it out
		return create_and_return_error_node();
	}

	//Finally close the file
	fclose(fl);

	//We're done with our dependency list, so deallocate it
	dynamic_array_dealloc(dependency_list);
	
	//Finally give back the root node that we made here
	return root_node;
}


/**
 * Our entry point method to the ollie preprocessor. This also serves
 * as a useful first check to see if any files do not exist(fail to open).
 * As a reminder, Ollie lang does not do incremental builds. It does a full build,
 * from scratch, every time. This ensures that there are no incremental build errors, but
 * it also means that if one file can't be compiled, the whole thing will fail
 */
dependency_package_t preprocess(char* fname){
	//The return token. Remember that OC uses an "errors-as-values" approach, 
	//so this return token will be what we use to communicate errors as well
	dependency_package_t dep_package;

	//Both 0 to start with
	num_errors = 0;
	num_warnings = 0;	

	//Otherwise it did work. In this instance, we will return the dependency package result in here. The actual 
	//orienting of compiler direction is done by a different submodule
	build_dependency_tree_rec(fname);

	//Give this one back
	return dep_package;
}
