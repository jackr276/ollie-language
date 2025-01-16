/**
 * The abstract syntax tree is what will be generated through the very first run of the compiler.
 * It is currently close-to-source, but will eventually expand to be more low-level
*/

//Link to AST
#include "ast.h"
#include <stdlib.h>
#include <sys/types.h>


/**
 * Simple function that handles all of the hard work for node allocation for us. The user gives us the pointer
 * that they want to use. It is assumed that the user already knows the proper type and takes appropriate action based
 * on that
*/
void* ast_node_alloc(ast_note_class_t CLASS){
	void* node;

	switch (CLASS) {
		//The starting node of the entire AST
		case AST_NODE_CLASS_PROG:
			//Allocate appropriate size
			node = calloc(1, sizeof(prog_ast_node_t));
			((prog_ast_node_t*)node)->CLASS = AST_NODE_CLASS_PROG;

			//Give the node back
			return node;
		
		default:
			return NULL;
	}


	return NULL;
}
