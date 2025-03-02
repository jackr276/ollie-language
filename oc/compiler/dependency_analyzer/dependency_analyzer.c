/**
 * Author: Jack Robbins
 * This is the implementation file for the dependency analyzer, linked
 * to the header file of the same name
*/

#include "dependency_analyzer.h"
#include <stdlib.h>

/**
 * Initialize the dependency graph
 */
compiler_order_graph_t* initialize_dependency_graph(){
	//Allocate it
	compiler_order_graph_t* graph = calloc(1, sizeof(compiler_order_graph_t));

	//And we return it
	return graph;
}





