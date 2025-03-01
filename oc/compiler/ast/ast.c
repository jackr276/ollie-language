/**
 * The abstract syntax tree is what will be generated through the very first run of the compiler.
 * It is currently close-to-source, but will eventually expand to be more low-level
*/

//Link to AST
#include "ast.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
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

	//Now we must also copy the entire node
	if(node->inner_node_size != 0){
		duplicated->node = calloc(1, node->inner_node_size);
		memcpy(duplicated->node, node->node, node->inner_node_size);

		//Double special case for assembly nodes
		if(node->CLASS == AST_NODE_CLASS_ASM_INLINE_STMT){
			//Grab a reference for convenience
			asm_inline_stmt_ast_node_t* duplicated_asm = (asm_inline_stmt_ast_node_t*)(duplicated->node);
			asm_inline_stmt_ast_node_t* old_asm = (asm_inline_stmt_ast_node_t*)(node->node);
			duplicated_asm->asm_line_statements = calloc(sizeof(char), old_asm->max_length);
			duplicated_asm->max_length = old_asm->max_length;
			duplicated_asm->length = old_asm->length;

			//Copy over the entirety of the inlined assembly
			strcpy(duplicated_asm->asm_line_statements, old_asm->asm_line_statements);
		}
	}

	//We don't want to hold onto any of these old references here
	duplicated->first_child = NULL;
	duplicated->next_sibling = NULL;
	duplicated->next_statement = NULL;
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

	switch (CLASS) {
		//The starting node of the entire AST
		case AST_NODE_CLASS_PROG:
			//Has no inner node size
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_PROG;
			break;

		//The for-loop condition AST node
		case AST_NODE_CLASS_FOR_LOOP_CONDITION:
			//Has no inner node size
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_FOR_LOOP_CONDITION;
			break;

		case AST_NODE_CLASS_DEFER_STMT:
			//Has no inner size
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_DEFER_STMT;
			break;

		//The parameter elaboration node, only for type system
		case AST_NODE_CLASS_ELABORATIVE_PARAM:
			//Just stuff the class in here
			node->CLASS = AST_NODE_CLASS_ELABORATIVE_PARAM;
			node->inner_node_size = 0;
			break;

		//The function specifier AST node
		case AST_NODE_CLASS_FUNC_DEF:
			//Just allocate the proper size and set the class
			node->node = calloc(1, sizeof(func_def_ast_node_t));
			node->inner_node_size = sizeof(func_def_ast_node_t);
			node->CLASS = AST_NODE_CLASS_FUNC_DEF;
			break;

		//The function specifier AST node
		case AST_NODE_CLASS_PARAM_LIST:
			//Just allocate the proper size and set the class
			node->node = calloc(1, sizeof(param_list_ast_node_t));
			node->inner_node_size = sizeof(param_list_ast_node_t);
			node->CLASS = AST_NODE_CLASS_PARAM_LIST;

			//Initialize this here, although in theory calloc should've
			((param_list_ast_node_t*)(node->node))->num_params = 0;

			break;

		//The parameter declaration node
		case AST_NODE_CLASS_PARAM_DECL:
			//No inner node size
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_PARAM_DECL;
			break;

		//The type specifier node
		case AST_NODE_CLASS_TYPE_SPECIFIER:
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_TYPE_SPECIFIER;
			break;

		//The type name node
		case AST_NODE_CLASS_TYPE_NAME:
			//Just allocate the proper size and set the class
			node->node = calloc(1, sizeof(type_name_ast_node_t));
			node->inner_node_size = sizeof(type_name_ast_node_t);
			node->CLASS = AST_NODE_CLASS_TYPE_NAME;
			break;

		//The type address specifier node
		case AST_NODE_CLASS_TYPE_ADDRESS_SPECIFIER:
			//Just allocate the proper size and set the class
			node->node = calloc(1, sizeof(type_address_specifier_ast_node_t));
			node->inner_node_size = sizeof(type_address_specifier_ast_node_t);
			node->CLASS = AST_NODE_CLASS_TYPE_ADDRESS_SPECIFIER;
			break;
		
		//An idle statement
		case AST_NODE_CLASS_IDLE_STMT:
			//No inner node size
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_IDLE_STMT;
			break;

		//An identifier of any kind
		case AST_NODE_CLASS_IDENTIFIER:
			//Just allocate the proper size and set the class
			node->node = calloc(1, sizeof(identifier_ast_node_t));
			node->inner_node_size = sizeof(identifier_ast_node_t);
			node->CLASS = AST_NODE_CLASS_IDENTIFIER;
			break;

		//Constant case
		case AST_NODE_CLASS_CONSTANT:
			//Just allocate the proper size and set the class
			node->node = calloc(1, sizeof(constant_ast_node_t));
			node->inner_node_size = sizeof(constant_ast_node_t);
			node->CLASS = AST_NODE_CLASS_CONSTANT;
			break;

		//Assignment expression node
		case AST_NODE_CLASS_ASNMNT_EXPR:
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_ASNMNT_EXPR;
			break;

		//Binary expression node
		case AST_NODE_CLASS_BINARY_EXPR:
			//Just allocate the proper size and set the class
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_BINARY_EXPR;
			break;

		//Function call node
		case AST_NODE_CLASS_FUNCTION_CALL:
			//Just allocate the proper size and set the class
			node->node = calloc(1, sizeof(function_call_ast_node_t));
			node->inner_node_size = sizeof(function_call_ast_node_t);
			node->CLASS = AST_NODE_CLASS_FUNCTION_CALL;
			break;

		//Unary expression node
		case AST_NODE_CLASS_UNARY_EXPR:
			//Just allocate the proper size and set the class
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_UNARY_EXPR;
			break;

		//Unary operator node
		case AST_NODE_CLASS_UNARY_OPERATOR:
			//No inner node 
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_UNARY_OPERATOR;
			break;

		//Construct accessor node
		case AST_NODE_CLASS_CONSTRUCT_ACCESSOR:
			//Just allocate the proper size and set the class
			node->node = calloc(1, sizeof(construct_accessor_ast_node_t));
			node->inner_node_size = sizeof(construct_accessor_ast_node_t);
			node->CLASS = AST_NODE_CLASS_CONSTRUCT_ACCESSOR;
			break;

		//Array accessor AST node
		case AST_NODE_CLASS_ARRAY_ACCESSOR:
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_ARRAY_ACCESSOR;
			break;

		//Postfix expression AST node
		case AST_NODE_CLASS_POSTFIX_EXPR:
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_POSTFIX_EXPR;
			break;

		//Construct member list ast node
		case AST_NODE_CLASS_CONSTRUCT_MEMBER_LIST:
			//Just allocate the proper size and set the class
			node->node = calloc(1, sizeof(construct_member_list_ast_node_t));
			node->inner_node_size = sizeof(construct_member_list_ast_node_t);
			node->CLASS = AST_NODE_CLASS_CONSTRUCT_MEMBER_LIST;
			break;

		//Construct member ast node
		case AST_NODE_CLASS_CONSTRUCT_MEMBER:
			//Just allocate the proper size and set the class
			node->node = calloc(1, sizeof(construct_member_ast_node_t));
			node->inner_node_size = sizeof(construct_member_ast_node_t);
			node->CLASS = AST_NODE_CLASS_CONSTRUCT_MEMBER;
			break;

		//Enum list ast node
		case AST_NODE_CLASS_ENUM_MEMBER_LIST:
			//Just allocate the proper size and set the class
			node->node = calloc(1, sizeof(enum_member_list_ast_node_t));
			node->inner_node_size = sizeof(enum_member_list_ast_node_t);
			node->CLASS = AST_NODE_CLASS_ENUM_MEMBER_LIST;
			break;

		//Enum list member node
		case AST_NODE_CLASS_ENUM_MEMBER:
			//Just allocate the proper size and set the class
			node->node = calloc(1, sizeof(enum_member_ast_node_t));
			node->inner_node_size = sizeof(enum_member_ast_node_t);
			node->CLASS = AST_NODE_CLASS_ENUM_MEMBER;
			break;

		//Case stmt node
		case AST_NODE_CLASS_CASE_STMT:
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_CASE_STMT;
			break;

		//Default statement node
		case AST_NODE_CLASS_DEFAULT_STMT:
			//No inner node size
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_DEFAULT_STMT;
			break;
			
		//Label statement node
		case AST_NODE_CLASS_LABEL_STMT:
			//No inner node size
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_LABEL_STMT;
			break;

		//If statement node
		case AST_NODE_CLASS_IF_STMT:
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_IF_STMT;
			break;

		//Jump statement node
		case AST_NODE_CLASS_JUMP_STMT:
			//No inner node size
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_JUMP_STMT;
			break;

		//Break statement node
		case AST_NODE_CLASS_BREAK_STMT:
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_BREAK_STMT;
			break;

		//Continue statement node
		case AST_NODE_CLASS_CONTINUE_STMT:
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_CONTINUE_STMT;
			break;

		//Ret statement node
		case AST_NODE_CLASS_RET_STMT:
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_RET_STMT;
			break;

		//Switch statement node
		case AST_NODE_CLASS_SWITCH_STMT:
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_SWITCH_STMT;
			break;

		//While statement node
		case AST_NODE_CLASS_WHILE_STMT:
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_WHILE_STMT;
			break;

		//Do-while statement node
		case AST_NODE_CLASS_DO_WHILE_STMT:
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_DO_WHILE_STMT;
			break;

		//For statement node
		case AST_NODE_CLASS_FOR_STMT:
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_FOR_STMT;
			break;

		//A compound statement node
		case AST_NODE_CLASS_COMPOUND_STMT:
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_COMPOUND_STMT;
			break;

		//A declare statement node
		case AST_NODE_CLASS_DECL_STMT:
			//Set the size to be 0
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_DECL_STMT;
			break;

		//A let statement node
		case AST_NODE_CLASS_LET_STMT:
			//Set the size to be 0
			node->inner_node_size = 0;
			node->CLASS = AST_NODE_CLASS_LET_STMT;
			break;
		
		//An assembly inline statement
		case AST_NODE_CLASS_ASM_INLINE_STMT:
			//Allocate the inner node with the proper size
			node->node = calloc(1, sizeof(asm_inline_stmt_ast_node_t));
			node->inner_node_size = sizeof(asm_inline_stmt_ast_node_t);
			//We need to allocate the inside string as well
			((asm_inline_stmt_ast_node_t*)(node->node))->asm_line_statements = calloc(sizeof(char), DEFAULT_ASM_INLINE_SIZE);
			((asm_inline_stmt_ast_node_t*)(node->node))->length = 0;
			((asm_inline_stmt_ast_node_t*)(node->node))->max_length = DEFAULT_ASM_INLINE_SIZE;
			node->CLASS = AST_NODE_CLASS_ASM_INLINE_STMT;
			break;

		//An alias statement node
		case AST_NODE_CLASS_ALIAS_STMT:
			//Just allocate the proper size and set the class
			node->node = calloc(1, sizeof(alias_stmt_ast_node_t));
			node->inner_node_size = sizeof(alias_stmt_ast_node_t);
			node->CLASS = AST_NODE_CLASS_ALIAS_STMT;
			break;

		//Generic error node
		case AST_NODE_CLASS_ERR_NODE:
			//Just assign that it is an error and get out
			node->CLASS = AST_NODE_CLASS_ERR_NODE;
			node->inner_node_size = 0;
			break;

		default:
			printf("YOU DID NOT IMPLEMENT THIS ONE\n");
			return NULL;
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
void deallocate_ast(){
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

		//Destroy temp here
		free(temp);
	}
}
