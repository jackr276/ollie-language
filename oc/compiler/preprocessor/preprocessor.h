/**
 * Author: Jack Robbins
 *
 * The ollie language preprocessor. Handles anything with imports through "Link" statements
*/

//Include guards
#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include <stdio.h>
#include <sys/types.h>

//Max length for most linux-based file systems
#define FILE_NAME_LENGTH 255
//The default number of dependencies
#define DEFAULT_DEPENDENCIES 10

//The dependency package
typedef struct dependency_package_t dependency_package_t;

/**
 * The type of error that we give back. Ollie compiler uses an
 * errors-as-values approach, so this will dictate how the regular 
 * compiler responds
 */
typedef enum {
	PREPROC_SUCCESS,
	PREPROC_ERROR,
} preproc_return_token_t;


/**
 * When we're done preprocessing, we'll be handing this back
 * to the compiler
 */
struct dependency_package_t{
	//What do we depend on? We don't want to be limited here
	char** dependencies;
	//The file name, we'll hang onto it for printing
	char file_name[FILE_NAME_LENGTH];
	//Count how many we have
	u_int16_t num_dependencies;
	//Max dependencies. This can be realloc'd if certain files
	//demand it
	u_int16_t max_dependencies;
	//What kind of token do we have(errors as values approach)
	preproc_return_token_t return_token;
};

/**
 * The Ollie preprocessor's current sole job is to determine
 * if the current file has any dependencies. These dependencies 
 * are required to be listed within the given #comptime dividing
 * bars. Not all files have these, and it is not required that they do.
 * 
 * However, if a file does have external dependencies, they will need to be
 * declared at the absolute top of the file
*/
dependency_package_t preprocess(char* fname);

/**
 * For convenience, destroy the dependency package in its entirety
*/
void destroy_dependency_package(dependency_package_t* package);

#endif
