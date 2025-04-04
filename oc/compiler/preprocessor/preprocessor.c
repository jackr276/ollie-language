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
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

//Default error size
#define DEFAULT_ERROR_SIZE 1000

/**
 * This dictates any errors that we print out
 */
typedef enum{
	PREPROC_ERR = 0,
	PREPROC_WARN,
	PREPROC_INFO,
} preproc_msg_type_t;

//The token of the current file that we are in
static char* current_file_token = NULL;


/**
 * Print out a custom, stylized preprocessor error for the user
 */
static void print_preproc_error(preproc_msg_type_t type, char* error_message){
	//For ease of printing
	char* message_types[3] = {"ERROR", "WARNING", "INFO"};

	//Print out the error in a stylized manner
	if(current_file_token == NULL){
		printf("[PREPROCESSOR %s]: %s\n", message_types[type], error_message);
	//We can add the name in here
	} else {
		printf("[FILE: %s] --> [PREPROCESSOR %s]: %s\n", current_file_token, message_types[type], error_message);
	}
}


/**
 * Print out a custom, stylized preprocessor error for the user with number line
 */
static void print_preproc_error_linenum(preproc_msg_type_t type, char* error_message, u_int16_t line_num){
	//For ease of printing
	char* message_types[] = {"ERROR", "WARNING", "INFO"};

	//Print out the error in a stylized manner
	if(current_file_token == NULL){
		printf("[LINE %d | PREPROCESSOR %s]: %s\n", line_num, message_types[type], error_message);
	} else {
		printf("[FILE: %s] --> [LINE %d | PREPROCESSOR %s]: %s\n", current_file_token, line_num, message_types[type], error_message);
	}
}


/**
 * Parse the beginning parts of a file and determine any/all dependencies.
 * The dependencies that we have here will be used to build the overall dependency
 * tree, which will determine the entire order of compilation
*/
static dependency_package_t build_dependency_tree(FILE* fl){
	//For any/all error printing
	char info[DEFAULT_ERROR_SIZE];
	//We will be returning a copy here, no need for dynamic allocation
	dependency_package_t return_package;
	//The lookahead token
	Lexer_item lookahead;

	//The parser line number -- largely unused in this module
	u_int16_t parser_line_num = 0;

	//We will run through the opening part of the file. If we do not
	//see the comptime guards, we will back right out
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

	//If we see a DEPENDENCIES token, we need to keep going. However if we don't see this, we're
	//completely done here
	if(lookahead.tok != DEPENDENCIES){
		//This is totally fine, we just move right along
		return_package.return_token = PREPROC_SUCCESS;
		//0 dependencies here
		return_package.root = NULL;
		//Give it back
		return return_package;
	}

	//Otherwise it did have a comptime guard. As such, we'll need to parse through
	//require statements one by one here, seeing which files are requested
	
	//We will go until we either a) see something that isn't "require"(an error)
	//						  or b) see the ending #comptime guard

	//Get our token here to seed the search
	lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

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
				print_preproc_error_linenum(PREPROC_ERR, "Filename required after \"lib\" keyword", parser_line_num);
				//Package up and return an error here
				return_package.return_token = PREPROC_ERROR;
				return return_package;
			}

			//This is a linux-specific thing: max filename sizes are 255 bytes. If
			//the length of the string is more than 255 bytes, it can't possibly
			//be a valid linux file
			if(strlen(lookahead.lexeme) >= 255){
				//Throw the error
				sprintf(info, "\"%s\" is not a valid filename", lookahead.lexeme); 
				print_preproc_error_linenum(PREPROC_ERR, info, parser_line_num);
				//Package up and return an error here
				return_package.return_token = PREPROC_ERROR;
				return return_package;
			}

			//Allocate it 
			char* added_filename = calloc(FILE_NAME_LENGTH + 1, sizeof(char));

			//Copy the lexeme over
			strncpy(added_filename, lookahead.lexeme, lookahead.char_count + 1);

			//One last thing that we need to see -- closing semicolon
			lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

			//If it's not a semicolon, we fail
			if(lookahead.tok != SEMICOLON){
				print_preproc_error_linenum(PREPROC_ERR, "Semicolon required after require statement", parser_line_num);
				//Package up and return an error here
				return_package.return_token = PREPROC_ERROR;
				return return_package;
			}


			//Otherwise it worked, so we can add this into the list

		} else if(lookahead.tok == STR_CONST){
			//This is a linux-specific thing: max filename sizes are 255 bytes. If
			//the length of the string is more than 255 bytes, it can't possibly
			//be a valid linux file
			if(strlen(lookahead.lexeme) >= 255){
				//Throw the error
				sprintf(info, "\"%s\" is not a valid filename", lookahead.lexeme); 
				print_preproc_error_linenum(PREPROC_ERR, info, parser_line_num);
				//Package up and return an error here
				return_package.return_token = PREPROC_ERROR;
				return return_package;
			}
			
			//Allocate it 
			char* added_filename = calloc(FILE_NAME_LENGTH + 1, sizeof(char));

			//Copy the lexeme over
			strncpy(added_filename, lookahead.lexeme, lookahead.char_count + 1);

			//One last thing that we need to see -- closing semicolon
			lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);

			//If it's not a semicolon, we fail
			if(lookahead.tok != SEMICOLON){
				print_preproc_error_linenum(PREPROC_ERR, "Semicolon required after require statement", parser_line_num);
				//Package up and return an error here
				return_package.return_token = PREPROC_ERROR;
				return return_package;
			}

		} else {
			//This is an error here
			print_preproc_error(PREPROC_ERR, "\"lib\" keyword or filename required after \"require\" keyword");
			//Package up and return an error here
			return_package.return_token = PREPROC_ERROR;
			return return_package;
		}

		//Refresh the token
		lookahead = get_next_token(fl, &parser_line_num, NOT_SEARCHING_FOR_CONSTANT);
	}

	//At the very end, if what we saw here causing us to exit was not a COMPTIME token, we
	//have some kind of issue
	if(lookahead.tok != DEPENDENCIES){
		print_preproc_error(PREPROC_ERR, "#dependencies end guard expected after preprocessor region");
		//Package up an error and send it out
		return_package.return_token = PREPROC_ERROR;
		return return_package;
	}

	//If we have a completely empty package, we should throw a warning
	if(return_package.root == NULL){
		print_preproc_error(PREPROC_WARN, "Empty #dependencies region given. Consider removing this region entirely if not in use.");
	}

	//Give it back
	return return_package;
}


/**
 * Our entry point method to the ollie preprocessor. This also serves
 * as a useful first check to see if any files do not exist(fail to open).
 * As a reminder, Ollie lang does not do incremental builds. It does a full build,
 * from scratch, every time. This ensures that there are no incremental build errors, but
 * it also means that if one file can't be compiled, the whole thing will fail
 */
dependency_package_t preprocess(char* fname){
	//For any/all error printing
	char info[DEFAULT_ERROR_SIZE];

	//The return token. Remember that OC uses an "errors-as-values" approach, 
	//so this return token will be what we use to communicate errors as well
	dependency_package_t dep_package;

	//Attempt to open the file
	FILE* fl = fopen(fname, "r");

	//Error out completely here
	if(fl == NULL){
		sprintf(info, "File \"%s\" could not be opened", fname);
		print_preproc_error(PREPROC_ERR, "info");
		dep_package.return_token = PREPROC_ERROR;
		return dep_package;
	}

	//Otherwise it did work. In this instance, we will return the dependency package result in here. The actual 
	//orienting of compiler direction is done by a different submodule
	dep_package = build_dependency_tree(fl);

	//Now we close the file
	fclose(fl);

	//Give this one back
	return dep_package;
}
