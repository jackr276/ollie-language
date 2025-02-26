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
#include <sys/types.h>
#include <stdlib.h>


/**
 * Parse the beginning parts of a file and determine any/all dependencies.
 * The dependencies that we have here will be used to build the overall dependency
 * tree, which will determine the entire order of compilation
*/
dependency_package_t determine_linkage_and_dependencies(){
	//We will be returning a copy here, no need for dynamic allocation
	dependency_package_t return_package;


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





