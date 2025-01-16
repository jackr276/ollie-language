/**
 * The abstract syntax tree is what will be generated through the very first run of the compiler.
 * It is currently close-to-source, but will eventually expand to be more low-level
*/

#ifndef AST_H
#define AST_H

//Need the lexer and the types here
#include "../lexer/lexer.h"
#include "../type_system/type_system.h"

/**
 * All nodes here are N-ary trees. This means that, in addition
 * to all of the data that each unique one holds, they all also 
 * hold references to their first child and next sibling, with
 * some exceptions
 */

//A generic AST node can be any AST node
typedef struct generic_ast_node_t generic_ast_node_t;
//The starting AST node
typedef struct prog_ast_node_t prog_ast_node_t;
//A function definition AST node
typedef struct func_def_ast_node_t func_def_ast_node_t;
//A declaration AST node
typedef struct decl_ast_node_t decl_ast_node_t;


//What type is in the AST node?
typedef enum ast_node_class_t{
	AST_NODE_CLASS_PROG,
	AST_NODE_CLASS_INT_CONST,
	AST_NODE_CLASS_FLOAT_CONST,
	AST_NODE_CLASS_CHAR_CONST,
	AST_NODE_CLASS_STRING_CONST,
	AST_NODE_CLASS_IDENTIFER
} ast_note_class_t;


//The starting AST node really only exists to hold the tree root
struct prog_ast_node_t{
	//This node has no "next sibling", it only has a "first child"
	func_def_ast_node_t* first_child_func; //If the first child is a function definition
	decl_ast_node_t* first_child_decl; //If the first child is a declaration
	//What class of node is it(only two options here)
	ast_note_class_t CLASS;
	//The lexer item, really a formality
	Lexer_item lex;
};


/**
 * Global node allocation function
 */
void* ast_node_alloc(ast_note_class_t CLASS);

/**
 * Current implementation is an N-ary tree. Each node holds pointers to its
 * first child and next sibling.
 *
 * THIS IS SUBJECT TO CHANGE
*/
struct generic_ast_node_t{
};

#endif /* AST_T */
