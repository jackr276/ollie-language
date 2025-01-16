/**
 * The abstract syntax tree is what will be generated through the very first run of the compiler.
 * It is currently close-to-source, but will eventually expand to be more low-level
*/

#ifndef AST_H
#define AST_H

//Need the lexer and the types here
#include "../lexer/lexer.h"
#include "../type_system/type_system.h"
#include "../symtab/symtab.h"

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
//A function specifier node
typedef struct func_specifier_ast_node_t func_specifier_ast_node_t;
//A declaration AST node
typedef struct decl_ast_node_t decl_ast_node_t;


//What type is in the AST node?
typedef enum ast_node_class_t{
	AST_NODE_CLASS_PROG,
	AST_NODE_CLASS_FUNC_DEF,
	AST_NODE_CLASS_FUNC_SPECIFIER,
	AST_NODE_CLASS_INT_CONST,
	AST_NODE_CLASS_FLOAT_CONST,
	AST_NODE_CLASS_CHAR_CONST,
	AST_NODE_CLASS_STRING_CONST,
	AST_NODE_CLASS_IDENTIFER
} ast_note_class_t;



/**
 * Current implementation is an N-ary tree. Each node holds pointers to its
 * first child and next sibling. The generic node also holds a pointer 
 * to what the actual node is
*/
struct generic_ast_node_t{
	//These are the two pointers that make up the whole of the tree
	generic_ast_node_t* first_child;
	generic_ast_node_t* next_sibling;
	//What kind of node is it?
	ast_note_class_t CLASS;
	//This is where we hold the actual node
	void* node;
};


//The starting AST node really only exists to hold the tree root
struct prog_ast_node_t{
	//The lexer item, really a formality
	Lexer_item lex;
};

//Holds the static or external keywords for a function
struct func_specifier_ast_node_t{
	//Just holds a token for us
	Token funcion_storage_class;
};


/**
 * Global node allocation function
 */
generic_ast_node_t* ast_node_alloc(ast_note_class_t CLASS);

/**
 * Global tree deallocation function
 */
void deallocate_ast(generic_ast_node_t* root);


#endif /* AST_T */
