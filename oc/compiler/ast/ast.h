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
#include <stddef.h>
#include <sys/types.h>

/**
 * Most inline assembly statements are small, so the default statement 
 * size is only 1000 bytes. This can be realloc'd as needed automatically
 */
#define DEFAULT_ASM_INLINE_SIZE 1000

/**
 * All nodes here are N-ary trees. This means that, in addition
 * to all of the data that each unique one holds, they all also 
 * hold references to their first child and next sibling, with
 * some exceptions
 */

//A generic AST node can be any AST node
typedef struct generic_ast_node_t generic_ast_node_t;
//A constant node. Can represent any of the four kinds of constant
typedef struct constant_ast_node_t constant_ast_node_t;
//A type name node
typedef struct type_name_ast_node_t type_name_ast_node_t;
//A struct accessor node
typedef struct construct_accessor_ast_node_t construct_accessor_ast_node_t;
//A construct member list node
typedef struct construct_member_list_ast_node_t construct_member_list_ast_node_t;
//A construct member node
typedef struct construct_member_ast_node_t construct_member_ast_node_t;
//An enumarated list node
typedef struct enum_member_list_ast_node_t enum_member_list_ast_node_t;
//An enumerated member node
typedef struct enum_member_ast_node_t enum_member_ast_node_t;
//An AST node for if statements
typedef struct if_stmt_ast_node_t if_stmt_ast_node_t;
//An AST node for alias statements
typedef struct alias_stmt_ast_node_t alias_stmt_ast_node_t;
//An AST node for assembly inline statements
typedef struct asm_inline_stmt_ast_node_t asm_inline_stmt_ast_node_t;

/**
 * Is this an assignable variable?
 */
typedef enum{
	NOT_ASSIGNABLE,
	ASSIGNABLE
} variable_assignability_t;

//What type is in the AST node?
typedef enum ast_node_class_t{
	AST_NODE_CLASS_PROG,
	AST_NODE_CLASS_ALIAS_STMT,
	AST_NODE_CLASS_FOR_LOOP_CONDITION,
	AST_NODE_CLASS_DECL_STMT,
	AST_NODE_CLASS_LET_STMT,
	AST_NODE_CLASS_IDLE_STMT,
	AST_NODE_CLASS_FUNC_DEF,
	AST_NODE_CLASS_PARAM_LIST,
	AST_NODE_CLASS_CONSTANT,
	AST_NODE_CLASS_PARAM_DECL,
	AST_NODE_CLASS_IDENTIFIER,
	AST_NODE_CLASS_TYPE_SPECIFIER,
	AST_NODE_CLASS_TYPE_ADDRESS_SPECIFIER,
	AST_NODE_CLASS_TYPE_NAME,
	AST_NODE_CLASS_ASNMNT_EXPR,
	AST_NODE_CLASS_BINARY_EXPR,
	AST_NODE_CLASS_POSTFIX_EXPR,
	AST_NODE_CLASS_UNARY_EXPR,
	AST_NODE_CLASS_UNARY_OPERATOR,
	AST_NODE_CLASS_CONSTRUCT_ACCESSOR,
	AST_NODE_CLASS_ARRAY_ACCESSOR,
	AST_NODE_CLASS_FUNCTION_CALL,
	AST_NODE_CLASS_CONSTRUCT_MEMBER_LIST,
	AST_NODE_CLASS_CONSTRUCT_MEMBER,
	AST_NODE_CLASS_ENUM_MEMBER_LIST,
	AST_NODE_CLASS_ENUM_MEMBER,
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
	//Has no body
	AST_NODE_CLASS_DEFER_STMT,
	//For special elaborative parameters
	AST_NODE_CLASS_ELABORATIVE_PARAM,
	//For assembly inline statements
	AST_NODE_CLASS_ASM_INLINE_STMT,
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
	//What is the next created AST NODE? Used for memory deallocation
	generic_ast_node_t* next_created_ast_node;
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
	//Store a binary operator(if one exists)
	Token binary_operator;
	//Store a unary operator(if one exists)
	Token unary_operator;
	//Store an ident if we have one
	char* identifier;
	//Is this assignable?
	variable_assignability_t is_assignable;
	//Is this a deferred node?
	u_int8_t is_deferred;
	//The number of parameters
	u_int8_t num_params;
	//The type address specifier - for types
	address_specifier_type_t address_type;
	//What is the value of this case statement
	int64_t case_statement_value;
	//What is the size of it's inner node
	size_t inner_node_size;
	//This is where we hold the actual node
	void* node;
	//What variable do we have?
	symtab_variable_record_t* variable;
	//The symtab function record
	symtab_function_record_t* func_record;

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

//Simply holds a name that we get for a type
struct type_name_ast_node_t{
	//Holds the name of the type as a string
	char type_name[MAX_TYPE_NAME_LENGTH];
	//The type record that we have
	symtab_type_record_t* type_record;
};


//The construct accessor node
struct construct_accessor_ast_node_t{
	//The token that we saw(either : or =>)
	Token tok;
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

//An alias stmt
struct alias_stmt_ast_node_t{
	//Hold the alias that we made
	symtab_type_record_t* alias;
};

//An assembly inline statement
struct asm_inline_stmt_ast_node_t{
	//We just need to hold all of the statements in a big chunk
	char* asm_line_statements;
	//The currently string length
	u_int16_t length;
	//The current max length(will be realloc'd if needed)
	u_int16_t max_length;
};

/**
 * Global node allocation function
 */
generic_ast_node_t* ast_node_alloc(ast_node_class_t CLASS);

/**
 * A utility function for node duplication
 */
generic_ast_node_t* duplicate_node(const generic_ast_node_t* node);

/**
 * A helper function that will appropriately add a child node into the parent
 */
void add_child_node(generic_ast_node_t* parent, generic_ast_node_t* child);


/**
 * Global tree deallocation function
 */
void ast_dealloc();

#endif /* AST_T */
