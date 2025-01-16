/**
 * The abstract syntax tree is what will be generated through the very first run of the compiler.
 * It is currently close-to-source, but will eventually expand to be more low-level
*/

//Link to AST
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>


/**
 * Simple function that handles all of the hard work for node allocation for us. The user gives us the pointer
 * that they want to use. It is assumed that the user already knows the proper type and takes appropriate action based
 * on that
*/
generic_ast_node_t* ast_node_alloc(ast_note_class_t CLASS){
	//We always have a generic AST node
	generic_ast_node_t* node = calloc(1, sizeof(generic_ast_node_t));

	switch (CLASS) {
		//The starting node of the entire AST
		case AST_NODE_CLASS_PROG:
			//If we have this kind of node, we'll allocate and set
			//the void ptr to be what we allocate
			node->node = calloc(1, sizeof(prog_ast_node_t));
			node->CLASS = AST_NODE_CLASS_PROG;
			
			break;

		//The function specifier AST node
		case AST_NODE_CLASS_FUNC_SPECIFIER:
			//Just allocate the proper size and set the class
			node->node = calloc(1, sizeof(func_specifier_ast_node_t));
			node->CLASS = AST_NODE_CLASS_FUNC_SPECIFIER;
			break;
				
		
		default:
			return NULL;
	}


	return node;
}

/**
 * Global tree deallocation function
 */
void deallocate_ast(generic_ast_node_t* root){
	//Base case
	if(root == NULL){
		return;
	}

	//We can off the bat free it's data
	free(root->node);

	//Recursively free it's subtree first
	generic_ast_node_t* sub_tree = root->first_child;
	generic_ast_node_t* sibling = root->next_sibling;

	//Once we have the pointers we actually no longer need root
	free(root);
	
	//Recursively call deallocate on both of these references
	deallocate_ast(sub_tree);
	deallocate_ast(sibling);
}


