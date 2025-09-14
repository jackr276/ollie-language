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
#include <stdint.h>
#include <sys/types.h>

/**
 * All nodes here are N-ary trees. This means that, in addition
 * to all of the data that each unique one holds, they all also 
 * hold references to their first child and next sibling, with
 * some exceptions
 */

//A generic AST node can be any AST node
typedef struct generic_ast_node_t generic_ast_node_t;

/**
 * Is this an assignable variable?
 */
typedef enum{
	NOT_ASSIGNABLE,
	ASSIGNABLE
} variable_assignability_t;

//What type is in the AST node?
typedef enum ast_node_type_t{
	AST_NODE_TYPE_PROG,
	AST_NODE_TYPE_ALIAS_STMT,
	AST_NODE_TYPE_FOR_LOOP_CONDITION,
	AST_NODE_TYPE_TERNARY_EXPRESSION,
	AST_NODE_TYPE_DECL_STMT,
	AST_NODE_TYPE_LET_STMT,
	AST_NODE_TYPE_IDLE_STMT,
	AST_NODE_TYPE_FUNC_DEF,
	AST_NODE_TYPE_CONSTANT,
	AST_NODE_TYPE_IDENTIFIER,
	AST_NODE_TYPE_ASNMNT_EXPR,
	AST_NODE_TYPE_BINARY_EXPR,
	AST_NODE_TYPE_POSTFIX_EXPR,
	AST_NODE_TYPE_UNARY_EXPR,
	AST_NODE_TYPE_UNARY_OPERATOR,
	AST_NODE_TYPE_STRUCT_ACCESSOR,
	AST_NODE_TYPE_STRUCT_POINTER_ACCESSOR,
	AST_NODE_TYPE_UNION_ACCESSOR, //For accessing union types
	AST_NODE_TYPE_ARRAY_ACCESSOR,
	AST_NODE_TYPE_FUNCTION_CALL,
	AST_NODE_TYPE_INDIRECT_FUNCTION_CALL, //An indirect call, for function pointers
	AST_NODE_TYPE_CASE_STMT,
	AST_NODE_TYPE_C_STYLE_CASE_STMT, //With fallthrough
	AST_NODE_TYPE_DEFAULT_STMT,
	AST_NODE_TYPE_C_STYLE_DEFAULT_STMT, //With fallthrough
	AST_NODE_TYPE_LABEL_STMT,
	AST_NODE_TYPE_IF_STMT,
	AST_NODE_TYPE_ELSE_IF_STMT,
	AST_NODE_TYPE_JUMP_STMT,
	AST_NODE_TYPE_CONDITIONAL_JUMP_STMT,
	AST_NODE_TYPE_BREAK_STMT,
	AST_NODE_TYPE_CONTINUE_STMT,
	AST_NODE_TYPE_RET_STMT,
	AST_NODE_TYPE_SWITCH_STMT,
	AST_NODE_TYPE_C_STYLE_SWITCH_STMT, //Special kind of switch that's C-style
	AST_NODE_TYPE_WHILE_STMT,
	AST_NODE_TYPE_DO_WHILE_STMT,
	AST_NODE_TYPE_FOR_STMT,
	AST_NODE_TYPE_COMPOUND_STMT,
	//Has no body
	AST_NODE_TYPE_DEFER_STMT,
	//For assembly inline statements
	AST_NODE_TYPE_ASM_INLINE_STMT,
	//An array initializer node
	AST_NODE_TYPE_ARRAY_INITIALIZER_LIST,
	//A struct initializer
	AST_NODE_TYPE_STRUCT_INITIALIZER_LIST,
	//A string initializer node
	AST_NODE_TYPE_STRING_INITIALIZER,
	AST_NODE_TYPE_ERR_NODE, /* errors as values approach going forward */
} ast_node_type_t;


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
	//The string value could hold an identifier, string constant, or it could hold
	//an assembly inline statement. It all depends based on context
	dynamic_string_t string_value;
	//What is the next created AST NODE? Used for memory deallocation
	generic_ast_node_t* next_created_ast_node;
	//What is the inferred type of the node
	generic_type_t* inferred_type;
	//These are the two pointers that make up the whole of the tree
	generic_ast_node_t* first_child;
	generic_ast_node_t* next_sibling;
	//What variable do we have?
	symtab_variable_record_t* variable;
	//The symtab function record
	symtab_function_record_t* func_record;
	//The type record that we have
	symtab_type_record_t* type_record;
	//Storing the constant values
	union {
		int64_t signed_long_value;
		u_int64_t unsigned_long_value;
		int32_t signed_int_value;
		u_int32_t unsigned_int_value;
		double double_value;
		float float_value;
		char char_value;
	} constant_value;
	//Holds the token for what kind of constant it is
	Token constant_type;
	//The upper and lower bound for switch statements
	int32_t lower_bound;
	int32_t upper_bound;
	//What line number is this from
	u_int16_t line_number;
	//Store a binary operator(if one exists)
	Token binary_operator;
	//Store a unary operator(if one exists)
	Token unary_operator;
	//Is this assignable?
	variable_assignability_t is_assignable;
	//What side is this node on
	side_type_t side;
	//What kind of node is it?
	ast_node_type_t ast_node_type;
	//The type address specifier - for types
	address_specifier_type_t address_type;
};

/**
 * This helper function negates a constant node's value
 */
void negate_constant_value(generic_ast_node_t* constant_node);

/**
 * This helper function decrements a constant node's value
 */
void decrement_constant_value(generic_ast_node_t* constant_node);

/**
 * This helper function increments a constant node's value
 */
void increment_constant_value(generic_ast_node_t* constant_node);

/**
 * This helper function will logically not a consant node's value
 */
void bitwise_not_constant_value(generic_ast_node_t* constant_node);

/**
 * This helper function will logically not a consant node's value
 */
void logical_not_constant_value(generic_ast_node_t* constant_node);

/**
 * Global node allocation function
 */
generic_ast_node_t* ast_node_alloc(ast_node_type_t ast_node_type, side_type_t side);

/**
 * A utility function for node duplication
 */
generic_ast_node_t* duplicate_node(generic_ast_node_t* node);

/**
 * A helper function that will appropriately add a child node into the parent
 */
void add_child_node(generic_ast_node_t* parent, generic_ast_node_t* child);


/**
 * Global tree deallocation function
 */
void ast_dealloc();

#endif /* AST_T */
