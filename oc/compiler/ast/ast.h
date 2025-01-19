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
//A constant node. Can represent any of the four kinds of constant
typedef struct constant_ast_node_t constant_ast_node_t;
//A type specifier node
typedef struct type_spec_ast_node_t type_spec_ast_node_t;
//A type name node
typedef struct type_name_ast_node_t type_name_ast_node_t;
//Type address specifier node
typedef struct type_address_specifier_ast_node_t type_address_specifier_ast_node_t;
//The top level node that holds together an assignment expression
typedef struct asnmnt_expr_ast_node_t asnmnt_expr_ast_node_t;
//The binary expression node
typedef struct binary_expr_ast_node_t binary_expr_ast_node_t;
//The cast expression node
typedef struct cast_expr_ast_node_t cast_expr_ast_node_t;
//The postfix expression node
typedef struct postfix_expr_ast_node_t postfix_expr_ast_node_t;
//The unary expression node
typedef struct unary_expr_ast_node_t unary_expr_ast_node_t;
//The unary operator node
typedef struct unary_operator_ast_node_t unary_operator_ast_node_t;
//The function call node
typedef struct function_call_ast_node_t function_call_ast_node_t;
//A struct accessor node
typedef struct construct_accessor_ast_node_t construct_accessor_ast_node_t;
//An array accessor node
typedef struct array_accessor_ast_node_t array_accessor_ast_node_t;
//A construct definer node
typedef struct construct_definer_ast_node_t construct_definer_ast_node_t;
//A construct member list node
typedef struct construct_member_list_ast_node_t construct_member_list_ast_node_t;
//A construct member node
typedef struct construct_member_ast_node_t construct_member_ast_node_t;
//An enumerated definer node
typedef struct enum_definer_ast_node_t enum_definer_ast_node_t;
//An enumarated list node
typedef struct enum_member_list_ast_node_t enum_member_list_ast_node_t;
//An enumerated member node
typedef struct enum_member_ast_node_t enum_member_ast_node_t;
//A declaration AST node
typedef struct decl_ast_node_t decl_ast_node_t;
//An AST node for expression statements
typedef struct expression_stmt_ast_node_t expression_stmt_ast_node_t;


//What type is in the AST node?
typedef enum ast_node_class_t{
	AST_NODE_CLASS_PROG,
	AST_NODE_CLASS_FUNC_DEF,
	AST_NODE_CLASS_FUNC_SPECIFIER,
	AST_NODE_CLASS_PARAM_LIST,
	AST_NODE_CLASS_CONSTANT,
	AST_NODE_CLASS_PARAM_DECL,
	AST_NODE_CLASS_IDENTIFIER,
	AST_NODE_CLASS_TYPE_SPECIFIER,
	AST_NODE_CLASS_TYPE_ADDRESS_SPECIFIER,
	AST_NODE_CLASS_TYPE_NAME,
	AST_NODE_CLASS_ASNMNT_EXPR,
	AST_NODE_CLASS_BINARY_EXPR,
	AST_NODE_CLASS_CAST_EXPR,
	AST_NODE_CLASS_POSTFIX_EXPR,
	AST_NODE_CLASS_UNARY_EXPR,
	AST_NODE_CLASS_UNARY_OPERATOR,
	AST_NODE_CLASS_CONSTRUCT_ACCESSOR,
	AST_NODE_CLASS_ARRAY_ACCESSOR,
	AST_NODE_CLASS_FUNCTION_CALL,
	AST_NODE_CLASS_CONSTRUCT_DEFINER,
	AST_NODE_CLASS_CONSTRUCT_MEMBER_LIST,
	AST_NODE_CLASS_CONSTRUCT_MEMBER,
	AST_NODE_CLASS_ENUM_DEFINER,
	AST_NODE_CLASS_ENUM_MEMBER_LIST,
	AST_NODE_CLASS_ENUM_MEMBER,
	AST_NODE_CLASS_EXPR_STMT,
	AST_NODE_CLASS_ERR_NODE, /* errors as values approach going forward */
} ast_node_class_t;


//What kind of address type specifier is it
typedef enum address_specifier_type_t{
	ADDRESS_SPECIFIER_ARRAY,
	ADDRESS_SPECIFIER_ADDRESS,
} address_specifier_type_t;

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
	ast_node_class_t CLASS;
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


//Holds information about a variable identifier that's been seen
struct identifier_ast_node_t{
	//Holds the lexeme of the identifer: max size 1000 bytes(may change)
	char identifier[1000];
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
	//Holds the name of the type as a string
	char type_name[MAX_TYPE_NAME_LENGTH];
	//The type record that we have
	symtab_type_record_t* type_record;
};


//Hold the address specifier
struct type_address_specifier_ast_node_t{
	//Is it an address or array?
	address_specifier_type_t address_type;
};

//This node will hold data about an assignment expression
struct asnmnt_expr_ast_node_t{
	//Nothing here currently, except for
	//maybe type matching
	u_int8_t types_matched; //Will probably delete this or not use
};

//This node will hold data about any binary expression
struct binary_expr_ast_node_t{
	//What type does it produce?
	generic_type_t* inferred_type;
	//What operator is it
	Token binary_operator;
};

//The cast expression node is reached if we actually make a cast
struct cast_expr_ast_node_t{
	//What type does it have?
	generic_type_t* casted_type;
};

//The function node call
struct function_call_ast_node_t{
	//What is the inferred type of the called function // may be removed
	generic_type_t* inferred_type;
};

//The unary expression node
struct unary_expr_ast_node_t{
	//We will keep the inferred type here for convenience
	generic_type_t* inferred_type;
};

//The unary operator node
struct unary_operator_ast_node_t{
	//We will keep the token of the unary operator
	Token unary_operator;
};

//The construct accessor node
struct construct_accessor_ast_node_t{
	//The token that we saw(either : or =>)
	Token tok;
};

//The array accessor AST Node. Doesn't really do much, just there
//to represent what we're doing
struct array_accessor_ast_node_t{
	//What is the inferred type -- not yet implemented
	generic_type_t* inferred_type;
};

//The postfix expression ast node. Does not do all that much currently
struct postfix_expr_ast_node_t{
	//What type do we think it is--not yet implemented
	generic_type_t* inferred_type;
};

//The construct definer node
struct construct_definer_ast_node_t{
	//Keep a reference to the type that was made for the construct
	generic_type_t* created_construct;
};

//The construct member list node
struct construct_member_list_ast_node_t{
	//We'll just keep a count of how many members
	u_int8_t num_members;
};

//The construct member node itself
struct construct_member_ast_node_t{
	//Keep a reference to the variable record
	symtab_variable_record_t* member_var;
};

//The enum definer node
struct enum_definer_ast_node_t{
	//Holds a reference to the type
	generic_type_t* created_enum;
};

//The enum list node for the definition
struct enum_member_list_ast_node_t{
	//Holds the number of members
	u_int8_t num_members;
};

//The enum member node
struct enum_member_ast_node_t{
	//Hold the associate symtable record
	symtab_variable_record_t* member_var;
};

//An expression statement node
struct expression_stmt_ast_node_t{
	//The inferred type
	generic_type_t* inferred_type;
};

/**
 * Global node allocation function
 */
generic_ast_node_t* ast_node_alloc(ast_node_class_t CLASS);


/**
 * A helper function that will appropriately add a child node into the parent
 */
void add_child_node(generic_ast_node_t* parent, generic_ast_node_t* child);


/**
 * Global tree deallocation function
 */
void deallocate_ast(generic_ast_node_t* root);


#endif /* AST_T */
