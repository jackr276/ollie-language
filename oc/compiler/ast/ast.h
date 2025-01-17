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
#include <sys/types.h>

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
//A parameter list node
typedef struct param_list_ast_node_t param_list_ast_node_t;
//A parameter node
typedef struct param_decl_ast_node_t param_decl_ast_node_t;
//An identifier node
typedef struct identifier_ast_node_t identifier_ast_node_t;
//A label identifier node. Label identifiers always begin with dollar signs
typedef struct label_identifier_ast_node_t label_identifier_ast_node_t;
//A constant node. Can represent any of the four kinds of constant
typedef struct constant_ast_node_t constant_ast_node_t;
//A type specifier node
typedef struct type_spec_ast_node_t type_spec_ast_node_t;
//A type name node
typedef struct type_name_ast_node_t type_name_ast_node_t;
//Type address specifier node
typedef struct type_address_specifier_ast_node_t type_address_specifier_ast_node_t;
//The top level node for expressions
typedef struct top_level_expr_ast_node_t top_level_expr_ast_node_t;
//The top level node that holds together an assignment expression
typedef struct asnmnt_expr_ast_node_t asnmnt_expr_ast_node_t;
//A declaration AST node
typedef struct decl_ast_node_t decl_ast_node_t;


//What type is in the AST node?
typedef enum ast_node_class_t{
	AST_NODE_CLASS_PROG,
	AST_NODE_CLASS_FUNC_DEF,
	AST_NODE_CLASS_FUNC_SPECIFIER,
	AST_NODE_CLASS_PARAM_LIST,
	AST_NODE_CLASS_CONSTANT,
	AST_NODE_CLASS_PARAM_DECL,
	AST_NODE_CLASS_IDENTIFER,
	AST_NODE_CLASS_LABEL_IDENTIFIER,
	AST_NODE_CLASS_TYPE_SPECIFIER,
	AST_NODE_CLASS_TYPE_ADDRESS_SPECIFIER,
	AST_NODE_CLASS_TYPE_NAME,
	AST_NODE_CLASS_TOP_LEVEL_EXPR,
	AST_NODE_CLASS_ASNMNT_EXPR,
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


//Represents a top level function definition
struct func_def_ast_node_t{
	//The symtable function record that is created in parallel
	symtab_function_record_t* func_record;
};


//Holds the static or external keywords for a function
struct func_specifier_ast_node_t{
	//Just holds a token for us
	Token funcion_storage_class_tok;
	STORAGE_CLASS_T function_storage_class;
};


//Holds references to our parameter list
struct param_list_ast_node_t{
	//Hold how many params that we actually have
	//This is all we really care about here
	u_int8_t num_params;
};


//Holds information about an identifier that's been seen
struct identifier_ast_node_t{
	//Holds the lexeme of the identifer: max size 1000 bytes(may change)
	char identifier[1000];
};

//Holds information about an identifier that's been seen
struct label_identifier_ast_node_t{
	//Holds the lexeme of the label identifier
	char label_identifier[1000];
};

//Holds information about a constant
struct constant_ast_node_t{
	//Holds the token for what kind of constant it is
	Token constant_type;
	//Holds the intialized value of the constant
	char constant[1000];
};

//Holds information about a parameter declaration. This will also hold 
//a reference to the associated record in the symtab
struct param_decl_ast_node_t{
	//Holds a reference to the symtab record
	symtab_variable_record_t* param_record;
};


//Holds information about a type
struct type_spec_ast_node_t{
	//Hold the type record
	symtab_type_record_t* type_record;
};


//Simply holds a name that we get for a type
struct type_name_ast_node_t{
	char type_name[MAX_TYPE_NAME_LENGTH];
};


//Hold the address specifier
struct type_address_specifier_ast_node_t{
	//Either see an & or array brackets
	char address_specifer[10];
};

//Hold information about the top level expression.
//This is where we will try to infer the type of the 
//expression
struct top_level_expr_ast_node_t{
	generic_type_t* inferred_type;
};


//This node will hold data about an assignment expression
struct asnmnt_expr_ast_node_t{
	//Nothing here currently, except for
	//maybe type matching
	u_int8_t types_matched; //Will probably delete this or not use
};

/**
 * Global node allocation function
 */
generic_ast_node_t* ast_node_alloc(ast_note_class_t CLASS);


/**
 * A helper function that will appropriately add a child node into the parent
 */
void add_child_node(generic_ast_node_t* parent, generic_ast_node_t* child);


/**
 * Global tree deallocation function
 */
void deallocate_ast(generic_ast_node_t* root);


#endif /* AST_T */
