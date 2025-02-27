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

/**
 * This dictates any errors that we print out
 */
typedef enum{
	PREPROC_ERR = 0,
	PREPROC_WARN,
	PREPROC_INFO,
} preproc_msg_type_t;


/**
 * Print out a custom, stylized preprocessor error for the user
 */
static void print_preproc_error(preproc_msg_type_t type, char* error_message){
	//For ease of printing
	char* message_types[3] = {"ERROR", "WARNING", "INFO"};

	//Print out the error in a stylized manner
	printf("[PREPROCESSOR %s]: %s\n", message_types[type], error_message);
}


/**
 * Parse the beginning parts of a file and determine any/all dependencies.
 * The dependencies that we have here will be used to build the overall dependency
 * tree, which will determine the entire order of compilation
*/
static dependency_package_t determine_linkage_and_dependencies(){
	//We will be returning a copy here, no need for dynamic allocation
	dependency_package_t return_package;

	//We will run through the opening part of the file. If we do not
	//see the comptime guards, we will back right out
	Lexer_item lookahead;



	return return_package;
}


/**
 * A convenient freer method that we have for destroying
 * dependencies
*/
void destroy_dependency_package(dependency_package_t* package){
	//If it's null we're done here
	if(package->dependencies == NULL){
		return;
	}
	
	//Run through all of the records, deallocating them one by one
	for(u_int16_t i = 0; i < package->num_dependencies; i++){
		free(*(package->dependencies + i));
	}

	//At the very end, free the overall pointer
	free(package->dependencies);

	//Set this as a warning
	package->dependencies = NULL;
}



/**
 * Our entry point method to the ollie preprocessor. This also serves
 * as a useful first check to see if any files do not exist(fail to open).
 * As a reminder, Ollie lang does not do incremental builds. It does a full build,
 * from scratch, every time. This ensures that there are no incremental build errors, but
 * it also means that if one file can't be compiled, the whole thing will fail
 */
dependency_package_t preprocess(const char* filename){
	//For any/all error printing
	char info[500];
	//The return token. Remember that OC uses an "errors-as-values" approach, 
	//so this return token will be what we use to communicate errors as well
	dependency_package_t ret_package;

	//First and most obvious check
	if(filename == NULL){
		print_preproc_error(PREPROC_ERR, "Empty string given as filename");
	}

	//Let's open the file up. This open operation will eventually result in a close, we just need
	//to read the preprocessor information
	FILE* fl = fopen(filename, "r");

	//If this didn't work, we're in bad territory
	if(fl == NULL){
		sprintf(info, "The file \"%s\" could not be opened.", filename);
		print_preproc_error(PREPROC_ERR, info);

		//We will package and return an error node here
		ret_package.return_token = PREPROC_ERROR;
		//Give it back -- we're done here
		return ret_package;
	}

	//And at the end we'll close it
	fclose(fl);

	return ret_package;

}





