/**
 * Author: Jack Robbins
 *
 * This header file defines methods that are used in the production and interpretation of
 * three address code. Three address code is what the middle-level IR of the compiler
 * is, and occupies the basic blocks of the CFG.
*/

#ifndef THREE_ADDRESS_CODE_H
#define THREE_ADDRESS_CODE_H

//For symtab linking
#include "../symtab/symtab.h"
#include "../lexer/lexer.h"
#include "../ast/ast.h"
#include <sys/types.h>

//A struct that holds all knowledge of three address codes
typedef struct three_addr_code_stmt_t three_addr_code_stmt_t;
//A struct that holds our three address variables
typedef struct three_addr_var_t three_addr_var_t;
//A struct that holds our three address constants
typedef struct three_addr_const_t three_addr_const_t;

/**
 * What kind of jump statement do we have?
 */
typedef enum{
	JUMP_TYPE_JNE,
	JUMP_TYPE_JE,
	JUMP_TYPE_JNZ,
	JUMP_TYPE_JZ,
	JUMP_TYPE_JL,
	JUMP_TYPE_JG,
	//TODO may add more
} jump_type_t;



/**
 * What kind of three address code statement do we have?
 */
typedef enum{
	THREE_ADDR_CODE_BIN_OP_STMT,
	//Regular two address assignment
	THREE_ADDR_CODE_ASSN_STMT,
	//Assigning a constant to a variable
	THREE_ADDR_CODE_ASSN_CONST_STMT,
	//A return statement
	THREE_ADDR_CODE_RET_STMT,
	//A jump statement -- used for control flow
	THREE_ADDR_CODE_JUMP_STMT
} three_addr_code_stmt_class_t;

/**
 * A three address var may be a temp variable or it may be
 * linked to a non-temp variable. It keeps a generation counter
 * for eventual SSA and type information
*/
struct three_addr_var_t{
	//For convenience
	char var_name[115];
	//Link to symtab(NULL if not there)
	symtab_variable_record_t* linked_var;
	//Is this a temp variable?
	u_int8_t is_temporary;
	//Is this a constant?
	u_int8_t is_constant;
	//Store the type info for faster access
	//Types will be used for eventual register assignment
	generic_type_t* type;
};


/**
 * A three address constant always holds the value of the constant
 */
struct three_addr_const_t{
	//We hold the type info
	generic_type_t* type;
	//What kind of constant is it
	Token const_type;
	//And we hold everything relevant about the constant
	long long_const;
	char str_const[500];
	char char_const;
	float float_const;
	int int_const;
};


struct three_addr_code_stmt_t{
	//For linked list properties -- the next statement
	three_addr_code_stmt_t* next_statement;
	//A three address code always has 2 operands and an assignee
	three_addr_var_t* op1;
	//For convenience: op1 can also be a const sometimes
	three_addr_const_t* op1_const;
	three_addr_var_t* op2;
	three_addr_var_t* assignee;
	three_addr_code_stmt_class_t CLASS;
	//The actual operator, stored as a token for size requirements
	Token op;
	//If we have a jump statement, where we're jumping to
	int32_t jumping_to_id;
	//If it's a jump statement, what's the type?
	jump_type_t jump_type;
	//TODO may add more
};


/**
 * Create and return a temporary variable
*/
three_addr_var_t* emit_temp_var(generic_type_t* type);

/**
 * Create and return a three address var from an existing variable
*/
three_addr_var_t* emit_var(symtab_variable_record_t* var);

/**
 * Create and return a constant three address var
 */
three_addr_const_t* emit_constant(generic_ast_node_t* const_node);

/**
 * Emit a statement using three vars and a binary operator
 * ALL statements are of the form: assignee <- op1 operator op2
*/
three_addr_code_stmt_t* emit_bin_op_three_addr_code(three_addr_var_t* assignee, three_addr_var_t* op1, Token op, three_addr_var_t* op2); 

/**
 * Emit a statement that only uses two vars of the form var1 <- var2
 */
three_addr_code_stmt_t* emit_assn_stmt_three_addr_code(three_addr_var_t* assignee, three_addr_var_t* op1);

/**
 * Emit a statement that is assigning a const to a var i.e. var1 <- const
 */
three_addr_code_stmt_t* emit_assn_const_stmt_three_addr_code(three_addr_var_t* assignee, three_addr_const_t* constant);

/**
 * Emit a return statement. The return statement can optionally have a node that we're returning.
 * Returnee may or may not be null
 */
three_addr_code_stmt_t* emit_ret_stmt_three_addr_code(three_addr_var_t* returnee);

/**
 * Emit a jump statement. The jump statement can take on several different types of jump
 */
three_addr_code_stmt_t* emit_jmp_stmt_three_addr_code(int32_t jumping_to_id, jump_type_t jump_type);

/**
 * Pretty print a three address code statement
*/
void print_three_addr_code_stmt(three_addr_code_stmt_t* stmt);

/**
 * Destroy a three address variable
*/
void deallocate_three_addr_var(three_addr_var_t* var);

/**
 * Destroy an entire three address code statement
*/
void deallocate_three_addr_stmt(three_addr_code_stmt_t* stmt);

#endif
