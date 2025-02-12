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
	JUMP_TYPE_JMP,
	JUMP_TYPE_JGE,
	JUMP_TYPE_JLE,
	//TODO may add more
} jump_type_t;


/**
 * What kind of three address code statement do we have?
 */
typedef enum{
	//Binary op with all vars
	THREE_ADDR_CODE_BIN_OP_STMT,
	//An increment statement
	THREE_ADDR_CODE_INC_STMT,
	//A decrement statement
	THREE_ADDR_CODE_DEC_STMT,
	//An indirection statement
	THREE_ADDR_CODE_DEREF_STMT,
	//Binary op with const
	THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT,
	//Regular two address assignment
	THREE_ADDR_CODE_ASSN_STMT,
	//Assigning a constant to a variable
	THREE_ADDR_CODE_ASSN_CONST_STMT,
	//A return statement
	THREE_ADDR_CODE_RET_STMT,
	//A jump statement -- used for control flow
	THREE_ADDR_CODE_JUMP_STMT,
	//A function call node
	THREE_ADDR_CODE_FUNC_CALL,
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
	//What is the indirection level
	u_int8_t indirection_level;
	//Store the type info for faster access
	//Types will be used for eventual register assignment
	generic_type_t* type;
	//Linked list functionality
	three_addr_var_t* next;
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


/**
 * A generic struct that encapsulates most of our three address code
 * statements
 */
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
	//The function called
	symtab_function_record_t* func_record;
	//The list of temp variable parameters at most 6
	three_addr_var_t* params[6];
	//TODO may add more
};


/**
 * Create and return a temporary variable
*/
three_addr_var_t* emit_temp_var(generic_type_t* type);

/**
 * Create and return a three address var from an existing variable. If 
 * we are assigning to a variable, that will create a new generation of variable.
 * As such, we will pass 1 in as a flag here
*/
three_addr_var_t* emit_var(symtab_variable_record_t* var, u_int8_t assignment);

/**
 * Emit a variable copied from another variable
 */
three_addr_var_t* emit_var_copy(three_addr_var_t* var);

/**
 * Create and return a constant three address var
 */
three_addr_const_t* emit_constant(generic_ast_node_t* const_node);

/**
 * Emit an int constant in a very direct way
 */
three_addr_const_t* emit_int_constant_direct(int int_const);

/**
 * Emit a statement using three vars and a binary operator
 * ALL statements are of the form: assignee <- op1 operator op2
*/
three_addr_code_stmt_t* emit_bin_op_three_addr_code(three_addr_var_t* assignee, three_addr_var_t* op1, Token op, three_addr_var_t* op2); 

/**
 * Emit a statement using two vars and a constant
 */
three_addr_code_stmt_t* emit_bin_op_with_const_three_addr_code(three_addr_var_t* assignee, three_addr_var_t* op1, Token op, three_addr_const_t* op2); 

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
 * Emit an increment instruction
 */
three_addr_code_stmt_t* emit_inc_stmt_three_addr_code(three_addr_var_t* incrementee);

/**
 * Emit a decrement instruction
 */
three_addr_code_stmt_t* emit_dec_stmt_three_addr_code(three_addr_var_t* decrementee);

/**
 * Emit a jump statement. The jump statement can take on several different types of jump
 */
three_addr_code_stmt_t* emit_jmp_stmt_three_addr_code(int32_t jumping_to_id, jump_type_t jump_type);

/**
 * Emit a function call statement. Once emitted, no paramters will have been added in
 */
three_addr_code_stmt_t* emit_func_call_three_addr_code(symtab_function_record_t* func_record, three_addr_var_t* assigned_to);

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
