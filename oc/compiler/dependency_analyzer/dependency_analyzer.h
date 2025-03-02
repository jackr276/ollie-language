/**
 * Author: Jack Robbins
 * This header file defines the dependency analyzer for Ollie. This analyzer deals with the file
 * depenedency, and will determine compile-time order of compilation for the language. It also
 * checks for disallowed circular dependencies and will provide errors where appropriate
*/

//Include guards
#ifndef DEPENDENCY_ANALYZER_H
#define DEPENDENCY_ANALYZER_H
#include <sys/types.h>

//The length of filenames with some extra space for good measure
#define FILENAME_LENGTH 260 

//A node for tree
typedef struct dependency_tree_node_t dependency_tree_node_t;

//Determine the health of the compiler order
typedef enum{
	COMPILER_ORDER_ERR,
	COMPILER_ORDER_GOOD,
	COMPILER_ORDER_CIRC_DEP
} compiler_order_status_t;

struct dependency_tree_node_t{
	//What is the next-created node
	dependency_tree_node_t* next_created;
	//N-ary tree structure, first child and next sibling
	dependency_tree_node_t* first_child;
	dependency_tree_node_t* next_sibling;
	//Keep track of how many that we have
	u_int16_t num_connections;
	//Has it been visited?
	u_int8_t visited;
	//The file that we'll need to compile
	char filename[FILENAME_LENGTH];
};

/**
 * Create and add a node to the graph
 */
dependency_tree_node_t* dependency_tree_node_alloc(char* filename);

/**
 * Add a directed connection between two nodes. This kind of relationship
 * represents that the "parent" DEPENDS ON the "child"
 */
void add_dependency_node(dependency_tree_node_t* parent, dependency_tree_node_t* child);

/**
 * Destructor - used for memory freeing
*/
void destroy_dependency_tree();

#endif
