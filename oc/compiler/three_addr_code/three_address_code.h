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
 * What kind of word length do we have -- used for instructions
 */
typedef enum{
	WORD,
	DOUBLE_WORD,
	LONG_WORD,
	QUAD_WORD,
} variable_size_t;


/**
 * For variable printing, where we're printing
 * matters. The user must specify if it's
 * block or inline mode
 */
typedef enum{
	PRINTING_VAR_INLINE,
	PRINTING_VAR_BLOCK_HEADER,
} variable_printing_mode_t;


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
	//A bitwise not statement
	THREE_ADDR_CODE_BITWISE_NOT_STMT,
	//A logical not statement
	THREE_ADDR_CODE_LOGICAL_NOT_STMT,
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
	//A direct to label jump statement
	THREE_ADDR_CODE_DIR_JUMP_STMT,
	//A label statement
	THREE_ADDR_CODE_LABEL_STMT,
	//A function call node
	THREE_ADDR_CODE_FUNC_CALL,
	//An idle statement(nop)
	THREE_ADDR_CODE_IDLE_STMT,
	//A negation statement
	THREE_ADDR_CODE_NEG_STATEMENT,
	//SPECIAL CASE - assembly inline statement
	THREE_ADDR_CODE_ASM_INLINE_STMT,
	//Another special case - a switch statement
	THREE_ADDR_CODE_SWITCH_STMT,
	//A "Load effective address(lea)" instruction
	THREE_ADDR_CODE_LEA_STMT,
	//A phi function - for SSA analysis only
	THREE_ADDR_CODE_PHI_FUNC
} three_addr_code_stmt_class_t;

/**
 * A three address var may be a temp variable or it may be
 * linked to a non-temp variable. It keeps a generation counter
 * for eventual SSA and type information
*/
struct three_addr_var_t{
	//For memory management. An extra 10 is given for SSA
	char var_name[MAX_IDENT_LENGTH + 10];
	//Link to symtab(NULL if not there)
	symtab_variable_record_t* linked_var;
	//Is this a temp variable?
	u_int8_t is_temporary;
	//Is this a constant?
	u_int8_t is_constant;
	//What is the indirection level
	u_int16_t indirection_level;
	//What is the size of this variable
	variable_size_t variable_size;
	//Store the type info for faster access
	//Types will be used for eventual register assignment
	generic_type_t* type;
	//For memory management
	three_addr_var_t* next_created;
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
	char str_const[MAX_TOKEN_LENGTH];
	char char_const;
	float float_const;
	int int_const;
	//For memory management
	three_addr_const_t* next_created;
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
	//The LEA addition
	u_int64_t lea_multiplicator;
	//The actual operator, stored as a token for size requirements
	Token op;
	//Store a reference to the block that we're jumping to
	void* jumping_to_block;
	//Is this a jump table? -- for use in switch statements
	u_int8_t is_jump_table;
	//If it's a jump statement, what's the type?
	jump_type_t jump_type;
	//The function called
	symtab_function_record_t* func_record;
	//The variable record
	symtab_variable_record_t* var_record;
	//The list of temp variable parameters at most 6
	three_addr_var_t* params[MAX_FUNCTION_PARAMS];
	//Very special case, only for inlined assembly
	char* inlined_assembly;
	//The phi function parameters - stored in a dynamic array
	void* phi_function_parameters;
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
three_addr_var_t* emit_var(symtab_variable_record_t* var, u_int8_t assignment, u_int8_t is_label);

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
 * Emit a statement that is in LEA form
 */
three_addr_code_stmt_t* emit_lea_stmt_three_addr_code(three_addr_var_t* assignee, three_addr_var_t* op1, three_addr_var_t* op2, u_int64_t type_size);

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
 * Emit a negation(negX) statement
 */
three_addr_code_stmt_t* emit_neg_stmt_three_addr_code(three_addr_var_t* assignee, three_addr_var_t* negatee);

/**
 * Emit a bitwise not instruction
 */
three_addr_code_stmt_t* emit_not_stmt_three_addr_code(three_addr_var_t* var);

/**
 * Emit a label statement here
 */
three_addr_code_stmt_t* emit_label_stmt_three_addr_code(three_addr_var_t* var);

/**
 * Emit a logical not instruction
 */
three_addr_code_stmt_t* emit_logical_not_stmt_three_addr_code(three_addr_var_t* assignee, three_addr_var_t* var);

/**
 * Emit a jump statement. The jump statement can take on several different types of jump
 */
three_addr_code_stmt_t* emit_jmp_stmt_three_addr_code(void* jumping_to_block, jump_type_t jump_type);

/**
 * Emit a direct jump statement. This is used only with jump statements the user has made
 */
three_addr_code_stmt_t* emit_dir_jmp_stmt_three_addr_code(three_addr_var_t* jumping_to);

/**
 * Emit a function call statement. Once emitted, no paramters will have been added in
 */
three_addr_code_stmt_t* emit_func_call_three_addr_code(symtab_function_record_t* func_record, three_addr_var_t* assigned_to);

/**
 * Emit an assembly inline statement. Once emitted, these statements are final and are ignored
 * by any future optimizations
 */
three_addr_code_stmt_t* emit_asm_statement_three_addr_code(asm_inline_stmt_ast_node_t* asm_inline_node);

/**
 * Emit a phi function statement. Once emitted, these statements are for the exclusive use of the compiler
 */
three_addr_code_stmt_t* emit_phi_function(symtab_variable_record_t* variable);

/**
 * Emit an idle statement
 */
three_addr_code_stmt_t* emit_idle_statement_three_addr_code();

/**
 * Pretty print a three address code statement
*/
void print_three_addr_code_stmt(three_addr_code_stmt_t* stmt);

/**
 * Print a variable and everything about it. If the variable is in
 * "Block header" mode, we won't print out any dereferencing info
 */
void print_variable(three_addr_var_t* variable, variable_printing_mode_t mode);

/**
 * Destroy a three address variable
*/
void three_addr_var_dealloc(three_addr_var_t* var);

/**
 * Destroy an entire three address code statement
*/
void three_addr_stmt_dealloc(three_addr_code_stmt_t* stmt);

/**
 * Destroy all variables
*/
void deallocate_all_vars();

/**
 * Destroy all constants
*/
void deallocate_all_consts();

#endif
