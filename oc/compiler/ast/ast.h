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
//The primary expression node
typedef struct primary_expr_ast_node_t primary_expr_ast_node_t;
//The unary operator node
typedef struct unary_operator_ast_node_t unary_operator_ast_node_t;
//The function call node
typedef struct function_call_ast_node_t function_call_ast_node_t;
//A struct accessor node
typedef struct construct_accessor_ast_node_t construct_accessor_ast_node_t;
//An array accessor node
typedef struct array_accessor_ast_node_t array_accessor_ast_node_t;
//A construct member list node
typedef struct construct_member_list_ast_node_t construct_member_list_ast_node_t;
//A construct member node
typedef struct construct_member_ast_node_t construct_member_ast_node_t;
//An enumarated list node
typedef struct enum_member_list_ast_node_t enum_member_list_ast_node_t;
//An enumerated member node
typedef struct enum_member_ast_node_t enum_member_ast_node_t;
//A declaration AST node
typedef struct decl_ast_node_t decl_ast_node_t;
//An AST node for expression statements
typedef struct expression_stmt_ast_node_t expression_stmt_ast_node_t;
//An AST node for the case blocks in switches
typedef struct case_stmt_ast_node_t case_stmt_ast_node_t;
//An AST node for the default blocks in switches
typedef struct default_stmt_ast_node_t default_stmt_ast_node_t;
//An AST node for label statements
typedef struct label_stmt_ast_node_t label_stmt_ast_node_t;
//An AST node for if statements
typedef struct if_stmt_ast_node_t if_stmt_ast_node_t;
//An AST node for jump statements
typedef struct jump_stmt_ast_node_t jump_stmt_ast_node_t;
//An AST node for break statements
typedef struct break_stmt_ast_node_t break_stmt_ast_node_t;
//An AST node for continue statements
typedef struct continue_stmt_ast_node_t continue_stmt_ast_node_t;
//An AST node for ret statements
typedef struct ret_stmt_ast_node_t ret_stmt_ast_node_t;
//An AST node for switch statements
typedef struct switch_stmt_ast_node_t switch_stmt_ast_node_t;
//An AST node for while statements
typedef struct while_stmt_ast_node_t while_stmt_ast_node_t;
//An AST node for do-while statements
typedef struct do_while_stmt_ast_node_t do_while_stmt_ast_node_t;
//An AST node for compound statements
typedef struct compound_stmt_ast_node_t compound_stmt_ast_node_t;
//An AST node for for statements
typedef struct for_stmt_ast_node_t for_stmt_ast_node_t;
//An AST node for alias statements
typedef struct alias_stmt_ast_node_t alias_stmt_ast_node_t;
//An AST node for declaration statements
typedef struct decl_stmt_ast_node_t decl_stmt_ast_node_t;
//An AST node for let statements
typedef struct let_stmt_ast_node_t let_stmt_ast_node_t;
//An AST node for defer statements
typedef struct defer_stmt_ast_node_t defer_stmt_ast_node_t;
//An AST node for for-loop conditions
typedef struct for_loop_condition_ast_node_t for_loop_condition_ast_node_t;

//What type is in the AST node?
typedef enum ast_node_class_t{
	AST_NODE_CLASS_PROG,
	AST_NODE_CLASS_ALIAS_STMT,
	AST_NODE_CLASS_DEFER_STMT,
	AST_NODE_CLASS_FOR_LOOP_CONDITION,
	AST_NODE_CLASS_DECL_STMT,
	AST_NODE_CLASS_LET_STMT,
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
	AST_NODE_CLASS_PRIMARY_EXPR,
	AST_NODE_CLASS_UNARY_EXPR,
	AST_NODE_CLASS_UNARY_OPERATOR,
	AST_NODE_CLASS_CONSTRUCT_ACCESSOR,
	AST_NODE_CLASS_ARRAY_ACCESSOR,
	AST_NODE_CLASS_FUNCTION_CALL,
	AST_NODE_CLASS_CONSTRUCT_MEMBER_LIST,
	AST_NODE_CLASS_CONSTRUCT_MEMBER,
	AST_NODE_CLASS_ENUM_MEMBER_LIST,
	AST_NODE_CLASS_ENUM_MEMBER,
	AST_NODE_CLASS_EXPR_STMT,
	AST_NODE_CLASS_CASE_STMT,
	AST_NODE_CLASS_DEFAULT_STMT,
	AST_NODE_CLASS_LABEL_STMT,
	AST_NODE_CLASS_IF_STMT,
	AST_NODE_CLASS_JUMP_STMT,
	AST_NODE_CLASS_BREAK_STMT,
	AST_NODE_CLASS_CONTINUE_STMT,
	AST_NODE_CLASS_RET_STMT,
	AST_NODE_CLASS_SWITCH_STMT,
	AST_NODE_CLASS_WHILE_STMT,
	AST_NODE_CLASS_DO_WHILE_STMT,
	AST_NODE_CLASS_FOR_STMT,
	AST_NODE_CLASS_COMPOUND_STMT,
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
	//What is the next statement? This is used in our CFG
	generic_ast_node_t* next_statement;
	//What is the inferred type of the node
	generic_type_t* inferred_type;
	//These are the two pointers that make up the whole of the tree
	generic_ast_node_t* first_child;
	generic_ast_node_t* next_sibling;
	//What kind of node is it?
	ast_node_class_t CLASS;
	//What line number is this from
	u_int16_t line_number;
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

//Holds a defer statement for us
struct defer_stmt_ast_node_t{
};

//Holds references to our parameter list
struct param_list_ast_node_t{
	//Hold how many params that we actually have
	//This is all we really care about here
	u_int8_t num_params;
};


//Holds information about a variable identifier that's been seen
struct identifier_ast_node_t{
	//Holds the lexeme of the identifer: max size 500 bytes(may change)
	char identifier[MAX_TOKEN_LENGTH];
};

//Holds information about a constant
struct constant_ast_node_t{
	//Holds the token for what kind of constant it is
	Token constant_type;
	//It's cheap enough for us to just hold all of these here
	int32_t int_val;
	int64_t long_val;
	float float_val;
	char char_val;
	char string_val[MAX_TOKEN_LENGTH];
};

//Holds information about a parameter declaration. This will also hold 
//a reference to the associated record in the symtab
struct param_decl_ast_node_t{
	//Holds a reference to the symtab record
	symtab_variable_record_t* param_record;
};


//Holds information about a type
struct type_spec_ast_node_t{
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
	//What operator is it
	Token binary_operator;
};

//This node will hold data about any primary expression
struct primary_expr_ast_node_t{
};

//The cast expression node is reached if we actually make a cast
struct cast_expr_ast_node_t{
	//What type does it have?
	generic_type_t* casted_type;
};

//The function node call
struct function_call_ast_node_t{
};

//The unary expression node
struct unary_expr_ast_node_t{
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
};

//The postfix expression ast node. Does not do all that much currently
struct postfix_expr_ast_node_t{
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
};

//A case statement node
struct case_stmt_ast_node_t{
	//Just hold the memory offset for now
	u_int64_t mem_location;
};

//A default statement node
struct default_stmt_ast_node_t{
	//Just hold the memory location for now
	u_int64_t mem_location;
};

//A label statement node
struct label_stmt_ast_node_t{
	//Just hold the memory location for now
	u_int64_t mem_location;
	//The variable that is associated with it
	symtab_variable_record_t* associate_var;
};

//An if statement node
struct if_stmt_ast_node_t{
	//Just hold status for now
	u_int8_t status; //todo may get rid of
};

//A jump statement node
struct jump_stmt_ast_node_t{
	//Just hold status for now
	u_int8_t status; //todo may get rid of
	//Contain where we are jumping to
	symtab_variable_record_t* label_record;
};

//A break statement node
struct break_stmt_ast_node_t{
	//Just hold status for now
	u_int8_t status; //todo may get rid of
};

//A continue statement node
struct continue_stmt_ast_node_t{
	//Just hold status for now
	u_int8_t status; //todo may get rid of
};

//A ret statement node
struct ret_stmt_ast_node_t{
	//Just hold status for now
	u_int8_t status; //todo may get rid of
};

//A switch statement
struct switch_stmt_ast_node_t{
	//Just hold status for now
	u_int8_t status;
};

//A while statement
struct while_stmt_ast_node_t{
	//Just hold status for now
	u_int8_t status;
};

//A do-while statement
struct do_while_stmt_ast_node_t{
	//Just hold status for now
	u_int8_t status;
};

//A for statement
struct for_stmt_ast_node_t{
	//Just hold status for now
	u_int8_t status;
};

//A compound statement
struct compound_stmt_ast_node_t{
	//Just hold status for now
	u_int8_t status;
};

//An alias stmt
struct alias_stmt_ast_node_t{
	//Hold the alias that we made
	symtab_type_record_t* alias;
};

//A declaration stmt
struct decl_stmt_ast_node_t{
	//Hold the variable that we declaraed
	symtab_variable_record_t* declared_var;
};

//A let statement
struct let_stmt_ast_node_t{
	//Hold the variable that we declared
	symtab_variable_record_t* declared_var;
};

//An AST for loop condition
struct for_loop_condition_ast_node_t{
	//Is blank is true if there is no actual expression
	u_int8_t is_blank;
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
