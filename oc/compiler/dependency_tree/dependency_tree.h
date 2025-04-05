/**
 * Author: Jack Robbins
 * This header file defines the dependency analyzer for Ollie. This analyzer deals with the file
 * depenedency, and will determine compile-time order of compilation for the language. It also
 * checks for disallowed circular dependencies and will provide errors where appropriate
*/

//Include guards
#ifndef DEPENDENCY_TREE_H 
#define DEPENDENCY_TREE_H 
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


/**
 * The dependency tree is a way to organize our order of compilation. In the ideal scenario,
 * the user will be able to have a main file that references any other file they need. If they
 * compile the main file, the ollie compiler will automatically pull in any other file that they need
 *
 * We call it a tree here, but really it's a directed acyclic graph. It's acyclic because, if we have
 * a circular dependency, we end up in a "chicken or the egg" problem, where we don't know what
 * to compile first. 
 *
 * The root of the tree is ALWAYS the file that was passed into the ollie compiler. Every child of said root
 * is a dependency that will need to be compiled first
 */
struct dependency_tree_node_t{
	//What is the next-created node
	dependency_tree_node_t* next_created;
	//N-ary tree structure, first child and next sibling
	dependency_tree_node_t* first_child;
	dependency_tree_node_t* next_sibling;
	//Has it been visited?
	u_int8_t visited;
	//The file that we'll need to compile
	char filename[FILENAME_LENGTH];
};

/**
 * Allocate a dependency node
 */
dependency_tree_node_t* dependency_tree_node_alloc(char* filename);

/**
 * Initialize a dependency tree
 */
dependency_tree_node_t* initialize_dependency_tree();

/**
 * Add a directed connection between two nodes. This kind of relationship
 * represents that the "parent" DEPENDS ON the "child"
 */
void add_dependency_node(dependency_tree_node_t* parent, dependency_tree_node_t* child);

/**
 * Destructor - used for memory freeing
*/
void dependency_tree_dealloc();

#endif /* DEPENDENCY_TREE_H */
