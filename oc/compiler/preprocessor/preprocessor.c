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
 *
 *
 *
 *
 * This entire file is deprecated. We need to reconsider completely how this all should work
*/

#include "preprocessor.h"
//For tokenizing/lexical analyzing purposes
#include "../lexer/lexer.h"
//We'll make use of this too
#include "../utils/dynamic_array/dynamic_array.h"
#include "../utils/constants.h"
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

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
//static dependency_tree_node_t* build_dependency_tree_rec(char* fname){
//}


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
	dependency_package_t dep_package = {NULL, PREPROC_SUCCESS};

	//Both 0 to start with
	num_errors = 0;
	num_warnings = 0;	

	//Otherwise it did work. In this instance, we will return the dependency package result in here. The actual 
	//orienting of compiler direction is done by a different submodule
	//build_dependency_tree_rec(fname);

	//Give this one back
	return dep_package;
}
