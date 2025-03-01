/**
 * Author: Jack Robbins
 * This header file defines the dependency analyzer for Ollie. This analyzer deals with the file
 * depenedency, and will determine compile-time order of compilation for the language. It also
 * checks for disallowed circular dependencies and will provide errors where appropriate
*/

//Include guards
#ifndef DEPENDENCY_ANALYZER_H
#define DEPENDENCY_ANALYZER_H
#include "../preprocessor/preprocessor.h"

//A struct that will be used for the compiler order of compilation
typedef struct compiler_order_t compiler_order_t;
//A node item intended as a linked list
typedef struct compiler_order_node_t compiler_order_node_t;

//Determine the health of the compiler order
typedef enum{
	COMPILER_ORDER_ERR,
	COMPILER_ORDER_GOOD,
	COMPILER_ORDER_CIRC_DEP
	//TODO may add more
} compiler_order_status_t;

struct compiler_order_t{
	//We have a head node here. This represents the first
	//in line for compilation
	compiler_order_node_t* head;
	//We'll also store the number of things in the order
	u_int16_t number_of_deps;
	//And we'll also keep track of the compiler order's status
	compiler_order_status_t status;
};

struct compiler_order_node_t{
	//The next node - linked list functionality
	compiler_order_node_t* next;
	//The file that we'll need to compile
	char filename[256];
};

/**
 * Determine the overall dependency chain
 */
compiler_order_t determine_compiler_order(dependency_package_t* dependencies);

/**
 * Destructor - used for memory freeing
*/
void destroy_compiler_order(compiler_order_t* compiler_order);

#endif

