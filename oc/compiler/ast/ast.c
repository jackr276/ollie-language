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
 * This helper function negates a constant node's value
 */
void negate_constant_value(generic_ast_node_t* constant_node){
	//Switch based on the value here
	switch(constant_node->constant_type){
		//Negate these accordingly
		case INT_CONST_FORCE_U:
			constant_node->constant_value.unsigned_int_value *= -1;
			break;
		case INT_CONST:
			constant_node->constant_value.signed_int_value *= -1;
			break;
		case LONG_CONST_FORCE_U:
			constant_node->constant_value.unsigned_long_value *= -1;
			break;
		case LONG_CONST:
			constant_node->constant_value.signed_long_value *= -1;
			break;
		case FLOAT_CONST:
			constant_node->constant_value.float_value *= -1;
			break;
		case CHAR_CONST:
			constant_node->constant_value.char_value *= -1;
			break;
		//This should never happen
		default:
			return;
	}
}


/**
 * This helper function decrements a constant node's value
 */
void decrement_constant_value(generic_ast_node_t* constant_node){
	//Switch based on the value here
	switch(constant_node->constant_type){
		//Negate these accordingly
		case INT_CONST_FORCE_U:
			constant_node->constant_value.unsigned_int_value--;
			break;
		case INT_CONST:
			constant_node->constant_value.signed_int_value--;
			break;
		case LONG_CONST_FORCE_U:
			constant_node->constant_value.unsigned_long_value--;
			break;
		case LONG_CONST:
			constant_node->constant_value.signed_long_value--;
			break;
		case FLOAT_CONST:
			constant_node->constant_value.float_value--;
			break;
		case CHAR_CONST:
			constant_node->constant_value.char_value--;
			break;
		//This should never happen
		default:
			return;
	}
}


/**
 * This helper function increments a constant node's value
 */
void increment_constant_value(generic_ast_node_t* constant_node){
	//Switch based on the value here
	switch(constant_node->constant_type){
		//Negate these accordingly
		case INT_CONST_FORCE_U:
			constant_node->constant_value.unsigned_int_value++;
			break;
		case INT_CONST:
			constant_node->constant_value.signed_int_value++;
			break;
		case LONG_CONST_FORCE_U:
			constant_node->constant_value.unsigned_long_value++;
			break;
		case LONG_CONST:
			constant_node->constant_value.signed_long_value++;
			break;
		case FLOAT_CONST:
			constant_node->constant_value.float_value++;
			break;
		case CHAR_CONST:
			constant_node->constant_value.char_value++;
			break;
		//This should never happen
		default:
			return;
	}
}


/**
 * This helper function will logically not a consant node's value
 */
void logical_not_constant_value(generic_ast_node_t* constant_node){
	//Switch based on the value here
	switch(constant_node->constant_type){
		//Negate these accordingly
		case INT_CONST_FORCE_U:
			constant_node->constant_value.unsigned_int_value = !(constant_node->constant_value.unsigned_int_value);
			break;
		case INT_CONST:
			constant_node->constant_value.signed_int_value = !(constant_node->constant_value.signed_int_value);
			break;
		case LONG_CONST_FORCE_U:
			constant_node->constant_value.unsigned_long_value = !(constant_node->constant_value.unsigned_long_value);
			break;
		case LONG_CONST:
			constant_node->constant_value.signed_long_value = !(constant_node->constant_value.signed_long_value);
			break;
		case CHAR_CONST:
			constant_node->constant_value.char_value = !(constant_node->constant_value.char_value);
			break;
		//This should never happen
		default:
			return;
	}
}


/**
 * This helper function will logically not a consant node's value
 */
void bitwise_not_constant_value(generic_ast_node_t* constant_node){
	//Switch based on the value here
	switch(constant_node->constant_type){
		//Negate these accordingly
		case INT_CONST_FORCE_U:
			constant_node->constant_value.unsigned_int_value = ~(constant_node->constant_value.unsigned_int_value);
			break;
		case INT_CONST:
			constant_node->constant_value.signed_int_value = ~(constant_node->constant_value.signed_int_value);
			break;
		case LONG_CONST_FORCE_U:
			constant_node->constant_value.unsigned_long_value = ~(constant_node->constant_value.unsigned_long_value);
			break;
		case LONG_CONST:
			constant_node->constant_value.signed_long_value = ~(constant_node->constant_value.signed_long_value);
			break;
		case CHAR_CONST:
			constant_node->constant_value.char_value = ~(constant_node->constant_value.char_value);
			break;
		//This should never happen
		default:
			return;
	}
}


/**
 * A utility function for duplicating nodes
 */
generic_ast_node_t* duplicate_node(generic_ast_node_t* node){
	//First allocate the overall node here
	generic_ast_node_t* duplicated = calloc(1, sizeof(generic_ast_node_t));

	//We will perform a deep copy here
	memcpy(duplicated, node, sizeof(generic_ast_node_t));

	//Let's see if we have any special cases here that require extra attention
	switch(node->ast_node_type){
		//Asm inline is a special case because we'll need to copy the assembly over
		case AST_NODE_TYPE_ASM_INLINE_STMT:
		case AST_NODE_TYPE_IDENTIFIER:
			duplicated->string_value = clone_dynamic_string(&(node->string_value));
			break;

		//Constants are another special case, because they contain a special inner node
		case AST_NODE_TYPE_CONSTANT:
			//If we have a string constant, we'll duplicate the dynamic string
			if(node->constant_type == STR_CONST){
				duplicated->string_value = clone_dynamic_string(&(node->string_value));
			}

			break;

		//By default we do nothing, this is just there for the compiler to not complain
		default:
			break;

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
generic_ast_node_t* ast_node_alloc(ast_node_type_t ast_node_type, side_type_t side){
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
	node->ast_node_type = ast_node_type;

	//Assign the side of the node
	node->side = side;

	//And give it back
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
			switch(temp->ast_node_type){
				//Don't free these here, they'd be freed elsewhere
				case AST_NODE_TYPE_ARRAY_INITIALIZER_LIST:
				case AST_NODE_TYPE_STRING_INITIALIZER:
				case AST_NODE_TYPE_STRUCT_INITIALIZER_LIST:
					break;
				default:
					//Otherwise we can
					free(temp->node);
			}
		}

		//Some additional freeing may be needed
		switch(temp->ast_node_type){
			case AST_NODE_TYPE_IDENTIFIER:
			case AST_NODE_TYPE_ASM_INLINE_STMT:
				dynamic_string_dealloc(&(temp->string_value));
				break;

			//We could see a case where this is a string const
			case AST_NODE_TYPE_CONSTANT:
				if(temp->constant_type == STR_CONST){
					dynamic_string_dealloc(&(temp->string_value));
				}
				break;

			//By default we don't need to worry about this
			default:
				break;
		}

		//Destroy temp here
		free(temp);
	}
}
