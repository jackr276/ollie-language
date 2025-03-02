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
#include <sys/types.h>

#define MAX_DEPENDENCIES 100;

//A struct that will be used for the compiler order of compilation
typedef struct compiler_order_graph_t compiler_order_graph_t;
//A node item intended as a linked list
typedef struct compiler_order_node_t compiler_order_node_t;

//Determine the health of the compiler order
typedef enum{
	COMPILER_ORDER_ERR,
	COMPILER_ORDER_GOOD,
	COMPILER_ORDER_CIRC_DEP
} compiler_order_status_t;

struct compiler_order_graph_t{
	//We simply maintain a first node as a reference to the graph in
	//memory
	compiler_order_node_t* first_node;
	//And we'll also keep track of the compiler order's status
	compiler_order_status_t status;
};

struct compiler_order_node_t{
	//The next node - linked list functionality
	compiler_order_node_t* connections[100];
	//Keep track of how many that we have
	u_int16_t num_connections;
	//Has it been visited?
	u_int8_t visited;
	//The file that we'll need to compile
	char filename[256];
};

/**
 * Determine the overall dependency chain
 */
//compiler_order_t determine_compiler_order(dependency_package_t* dependencies);

/**
 * Initialize the dependency graph
 */
compiler_order_graph_t* initialize_dependency_graph();

/**
 * Create and add a node to the graph
 */
compiler_order_node_t* create_and_add_node(compiler_order_graph_t* graph, char* filename);

/**
 * Add a directed connection between two nodes
 */
void add_connections(compiler_order_node_t* from, compiler_order_graph_t* to);

/**
 * Destructor - used for memory freeing
*/
void destroy_dependency_graph(compiler_order_graph_t* compiler_order);

#endif
