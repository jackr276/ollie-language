/**
 * The abstract syntax tree is what will be generated through the very first run of the compiler.
 * It is currently close-to-source, but will eventually expand to be more low-level
*/

//Link to AST
#include "ast.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

//The very first created node
static generic_ast_node_t* head_node = NULL;
//The current node, this is used for our memory creation scheme
static generic_ast_node_t* current_ast_node = NULL;


/**
 * A utility function for duplicating nodes
 */
generic_ast_node_t* duplicate_node(const generic_ast_node_t* node){
	//First allocate the overall node here
	generic_ast_node_t* duplicated = calloc(1, sizeof(generic_ast_node_t));

	//We will perform a deep copy here
	memcpy(duplicated, node, sizeof(generic_ast_node_t));

	//Special case for assembly nodes
	if(node->CLASS == AST_NODE_CLASS_ASM_INLINE_STMT){
		//Allocate the inner node
		duplicated->node = calloc(1, sizeof(AST_NODE_CLASS_ASM_INLINE_STMT));
		//Grab a reference for convenience
		asm_inline_stmt_ast_node_t* duplicated_asm = (asm_inline_stmt_ast_node_t*)(duplicated->node);
		asm_inline_stmt_ast_node_t* old_asm = (asm_inline_stmt_ast_node_t*)(node->node);
		duplicated_asm->asm_line_statements = calloc(sizeof(char), old_asm->max_length);
		duplicated_asm->max_length = old_asm->max_length;
		duplicated_asm->length = old_asm->length;

		//Copy over the entirety of the inlined assembly
		strcpy(duplicated_asm->asm_line_statements, old_asm->asm_line_statements);
	}

	//Special case as well for constant nodes
	if(node->CLASS == AST_NODE_CLASS_CONSTANT){
		//Allocate the constant node
		duplicated->node = calloc(1, sizeof(constant_ast_node_t));

		//And then we'll copy the two onto eachother
		memcpy(duplicated->node, node->node, sizeof(constant_ast_node_t));
	}

	//If it's an ident, we'll need to duplicate that
	if(node->CLASS == AST_NODE_CLASS_IDENTIFIER){
		//Allocate this
		duplicated->identifier = calloc(MAX_IDENT_LENGTH, sizeof(char));
		//Copy the string over
		memcpy(duplicated->identifier, node->identifier, MAX_IDENT_LENGTH);
	}
	
	//We don't want to hold onto any of these old references here
	duplicated->first_child = NULL;
	duplicated->next_sibling = NULL;
	duplicated->next_created_ast_node = NULL;

	//And now we'll link this in to our linked list here
	//If we have the very first node
	if(head_node == NULL){
		head_node = duplicated;
		current_ast_node = duplicated;
	} else {
		//Add to the back of the list
		current_ast_node->next_created_ast_node = duplicated;
		current_ast_node = duplicated;
	}

	//Give back the duplicated node
	return duplicated;
}


/**
 * Simple function that handles all of the hard work for node allocation for us. The user gives us the pointer
 * that they want to use. It is assumed that the user already knows the proper type and takes appropriate action based
 * on that
*/
generic_ast_node_t* ast_node_alloc(ast_node_class_t CLASS){
	//We always have a generic AST node
	generic_ast_node_t* node = calloc(1, sizeof(generic_ast_node_t));

	//If we have the very first node
	if(head_node == NULL){
		head_node = node;
		current_ast_node = node;
	} else {
		//Add to the back of the list
		current_ast_node->next_created_ast_node = node;
		current_ast_node = node;
	}
	//Assign the class
	node->CLASS = CLASS;
	
	/**
	 * Handle the various special cases that we have here
	 */
	//Assembly nodes make use of the external pointer
	if(CLASS == AST_NODE_CLASS_ASM_INLINE_STMT){
		//Allocate the inner node with the proper size
		node->node = calloc(1, sizeof(asm_inline_stmt_ast_node_t));
		//We need to allocate the inside string as well
		((asm_inline_stmt_ast_node_t*)(node->node))->asm_line_statements = calloc(sizeof(char), DEFAULT_ASM_INLINE_SIZE);
		((asm_inline_stmt_ast_node_t*)(node->node))->length = 0;
		((asm_inline_stmt_ast_node_t*)(node->node))->max_length = DEFAULT_ASM_INLINE_SIZE;
	
	//Constant nodes also make use of the external pointer
	} else if(CLASS == AST_NODE_CLASS_CONSTANT){
		node->node = calloc(1, sizeof(constant_ast_node_t));
	//Type and ident nodes make use of the internal string pointers
	} else if(CLASS == AST_NODE_CLASS_IDENTIFIER){
		node->identifier = calloc(MAX_IDENT_LENGTH, sizeof(char));
	}

	return node;
}


/**
 * A helper function that will appropriately add a child node into the parent
 */
void add_child_node(generic_ast_node_t* parent, generic_ast_node_t* child){
	//We first deal with a special case -> if this is the first child
	//If so, we just add it in and leave
	if(parent->first_child == NULL){
		parent->first_child = child;
		return;
	}

	/**
	 * But if we make it here, we now know that there are other children. As such,
	 * we need to move to the end of the child linked list and append it there
	 */
	generic_ast_node_t* cursor = parent->first_child;

	//As long as there are more siblings
	while(cursor->next_sibling != NULL){
		cursor = cursor->next_sibling;
	}

	//When we get here, we know that we're at the very last child, so
	//we'll add it in and be finished
	cursor->next_sibling = child;
}


/**
 * Global tree deallocation function
 */
void ast_dealloc(){
	//For our own safety here
	if(head_node == NULL){
		return;
	}

	//Store our temp var here
	generic_ast_node_t* temp;

	while(head_node != NULL){
		//Grab a reference to it
		temp = head_node;

		//Advance root along
		head_node = head_node->next_created_ast_node;

		//We can off the bat free it's data
		if(temp->node != NULL){
			//Special case here, we need to free the interior string
			if(temp->CLASS == AST_NODE_CLASS_ASM_INLINE_STMT){
				//Deallocate this string in here
				free(((asm_inline_stmt_ast_node_t*)(temp->node))->asm_line_statements);
			}
			//No matter what, we always free this
			free(temp->node);
		}

		//Free this if needed
		if(temp->CLASS == AST_NODE_CLASS_IDENTIFIER){
			free(temp->identifier);
		}

		//Destroy temp here
		free(temp);
	}
}
