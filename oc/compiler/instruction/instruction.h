/**
 * Author: Jack Robbins
 *
 * This header file defines methods that are used in the production and interpretation of
 * three address code. Three address code is what the middle-level IR of the compiler
 * is, and occupies the basic blocks of the CFG. The end IR of Ollie will be an instruction. 
 * Everything begins its life as a three address code statement, and ends its life as an instruction
*/

#ifndef INSTRUCTION_H 
#define INSTRUCTION_H 

//For symtab linking
#include "../symtab/symtab.h"
#include "../lexer/lexer.h"
#include "../ast/ast.h"
#include <sys/types.h>

//An overall structure for an instruction. Instructions start their life
//as three address code statements, and eventually become assembly instructions
typedef struct instruction_t instruction_t;
//A struct that holds our three address variables
typedef struct three_addr_var_t three_addr_var_t;
//A struct that holds our three address constants
typedef struct three_addr_const_t three_addr_const_t;


/**
 * What type of instruction do we have? This saves us a lot of space
 * as opposed to storing strings. These are x86-64 assembly instructions
 */
typedef enum{
	NONE = 0, //The NONE instruction, this is our default and we'll get this when we calloc
	PHI_FUNCTION, //Not really an instruction, but we still need to account for these
	RET,
	CALL,
	MOVW, //Regular register-to-register or immediate to register
	MOVL,
	MOVQ,
	REG_TO_MEM_MOVW,
	REG_TO_MEM_MOVL,
	REG_TO_MEM_MOVQ,
	MEM_TO_REG_MOVW,
	MEM_TO_REG_MOVL,
	MEM_TO_REG_MOVQ,
	LEAL,
	LEAQ,
	INDIRECT_JMP, //For our switch statements
	NOP,
	JMP,
	JNE,
	JE,
	JNZ,
	JZ,
	JGE,
	JG,
	JLE,
	JL,
	ADDW,
	ADDL,
	ADDQ,
	MULL,
	MULQ,
	IMULL,
	IMULQ,
	DIVL,
	DIVQ,
	IDIVL,
	IDIVQ,
	SUBW,
	SUBL,
	SUBQ,
	ASM_INLINE, //ASM inline statements aren't really instructions
	SHRL,
	SHRQ, 
	SARQ, //Signed shift
	SARL, //Signed shift
	SALL, //Signed shift 
	SALQ, //Signed shift
	SHLL,
	SHLQ,
	INCL,
	INCQ,
	DECL,
	DECQ,
	NEGL,
	NEGQ,
	NOTL,
	NOTQ,
	XORL,
	XORQ,
	ORL,
	ORQ,
	ANDL,
	ANDQ,
	CMPL,
	CMPQ,
	TEST,
	TESTQ,
	SETE, //Set if equal
	SETNE,
	MOVZBL, //move if zero or below long word
} instruction_type_t;


/**
 * Define the standard x86-64 register table
 */
typedef enum{
	NO_REG = 0, //Default is that there's no register used
	AL, //%al register
} register_64_t;


/**
 * What kind of jump statement do we have?
 */
typedef enum{
	NO_JUMP, //This is the default, and what we get when we have 0
	JUMP_TYPE_JNE,
	JUMP_TYPE_JE,
	JUMP_TYPE_JNZ,
	JUMP_TYPE_JZ,
	JUMP_TYPE_JL,
	JUMP_TYPE_JG,
	JUMP_TYPE_JMP,
	JUMP_TYPE_JGE,
	JUMP_TYPE_JLE,
} jump_type_t;


/**
 * What kind of word length do we have -- used for instructions
 */
typedef enum{
	WORD,
	DOUBLE_WORD,
	QUAD_WORD,
	SINGLE_PRECISION,
	DOUBLE_PRECISION //For floats
} variable_size_t;


/**
 * What kind of memory addressing mode do we have?
 */
typedef enum{
	ADDRESS_CALCULATION_MODE_NONE = 0, //default is always none
	ADDRESS_CALCULATION_MODE_OFFSET_ONLY, // 4(%rax)
	ADDRESS_CALCULATION_MODE_REGISTERS_ONLY, // (%rax, %rcx)
	ADDRESS_CALCULATION_MODE_REGISTERS_AND_OFFSET, // 4(%rax, %rcx)
	ADDRESS_CALCULATION_MODE_REGISTERS_AND_SCALE, // (%rax, rcx, 8)
} address_calculation_mode_t;

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
 * For a given statement, are we writing to or reading from memory?
 */
typedef enum {
	MEMORY_ACCESS_NONE = 0,
	MEMORY_ACCESS_WRITE,
	MEMORY_ACCESS_READ,
} memory_access_type_t;

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
	//An indirect jump statement -- used for switch statement jump tables
	THREE_ADDR_CODE_INDIRECT_JUMP_STMT,
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
	//A "Load effective address(lea)" instruction
	THREE_ADDR_CODE_LEA_STMT,
	//An indirect jump address calculation instruction, very similar to lea
	THREE_ADDR_CODE_INDIR_JUMP_ADDR_CALC_STMT,
	//A phi function - for SSA analysis only
	THREE_ADDR_CODE_PHI_FUNC,
	//A memory access statement
	THREE_ADDR_CODE_MEM_ACCESS_STMT
} instruction_stmt_class_t;

/**
 * A three address var may be a temp variable or it may be
 * linked to a non-temp variable. It keeps a generation counter
 * for eventual SSA and type information
*/
struct three_addr_var_t{
	//Link to symtab(NULL if not there)
	symtab_variable_record_t* linked_var;
	//Types will be used for eventual register assignment
	generic_type_t* type;
	//What is this related to the writing of?
	symtab_variable_record_t* related_write_var;
	//For memory management
	three_addr_var_t* next_created;
	//What is the ssa generation level?
	u_int32_t ssa_generation;
	//What's the temp var number
	u_int32_t temp_var_number;
	//What is the indirection level
	u_int16_t indirection_level;
	//Is this a temp variable?
	u_int8_t is_temporary;
	//What is the size of this variable
	variable_size_t variable_size;
	//Store the type info for faster access
	//Memory access type, if one exists
	memory_access_type_t access_type;
};


/**
 * A three address constant always holds the value of the constant
 */
struct three_addr_const_t{
	char str_const[MAX_TOKEN_LENGTH];
	//For memory management
	three_addr_const_t* next_created;
	//We hold the type info
	generic_type_t* type;
	//And we hold everything relevant about the constant
	long long_const;
	float float_const;
	int int_const;
	//What kind of constant is it
	Token const_type;
	char char_const;
};


/**
 * A generic struct that encapsulates most of our instructions
 */
struct instruction_t{
	//What block holds this?
	void* block_contained_in;
	//For linked list properties -- the next statement
	instruction_t* next_statement;
	//For doubly linked list properties -- the previous statement
	instruction_t* previous_statement;
	//A three address code always has 2 operands and an assignee
	three_addr_var_t* op1;
	three_addr_var_t* op2;
	//For convenience: op1 can also be a const sometimes
	three_addr_const_t* op1_const;
	three_addr_var_t* assignee;
	//Now for the assembly operations, we have a source and destination
	three_addr_var_t* source_register;
	//We can have more than one source, usually for CMP instructions
	three_addr_var_t* source_register2;
	//If we're trying to move a constant in
	three_addr_const_t* source_immediate;
	//Our destination register/variable
	three_addr_var_t* destination_register;
	/**
	 * ADDRESS CALCULATIONS
	 *
	 * ADDRESS_CALCULATION_MODE_CONST_ONLY
	 * <constant_additive>(<source/dest>) = <constant_additive> + <source/dest>
	 * Constant additive is stored in variable constant_additive
	 * 
	 * ADDRESS_CALCULATION_MODE_REGISTER_ONLY
	 * (<source>/<dest>, <register_additive>) = <source>/<dest> + <register_additive>
	 * Register additive stored in varibale register_additive
	 */
	three_addr_const_t* offset;
	//The address calculation registers
	three_addr_var_t* address_calc_reg1;
	three_addr_var_t* address_calc_reg2;
	//The offset
	//Store a reference to the block that we're jumping to
	void* jumping_to_block;
	//The LEA addition
	u_int64_t lea_multiplicator;
	//The function called
	symtab_function_record_t* func_record;
	//The variable record
	symtab_variable_record_t* var_record;
	//What function are we currently in?
	symtab_function_record_t* function;
	//Very special case, only for inlined assembly
	char* inlined_assembly;
	//The phi function parameters - stored in a dynamic array
	void* phi_function_parameters;
	//The list of temp variable parameters at most 6
	void* function_parameters;
	//What is the three address code class
	instruction_stmt_class_t CLASS;
	//What is the x86-64 instruction
	instruction_type_t instruction_type;
	//The actual operator, stored as a token for size requirements
	Token op;
	//Is this a jump table? -- for use in switch statements
	u_int8_t is_jump_table;
	//Is this operation critical?
	u_int8_t mark;
	//Is this operation eligible for logical short-circuiting optimizations
	u_int8_t is_short_circuit_eligible;
	//Is this operation a "branch-ending" operation. This would encompass
	//things like if statement decisions and loop conditions
	u_int8_t is_branch_ending;
	//Are we jumping to if?(Affirmative jump) Our if statements and do while blocks
	//use this in the conditional. Otherwise, we're jumping to else, which is an inverse jump
	u_int8_t inverse_jump;
	//If it's a jump statement, what's the type?
	jump_type_t jump_type;
	//Memory access type
	TYPE_CLASS access_class;
	//What kind of address calculation mode do we have?
	address_calculation_mode_t calculation_mode;
};


/**
 * Declare that we are in a new function
 */
void set_new_function(symtab_function_record_t* func);

/**
 * Create and return a temporary variable
*/
three_addr_var_t* emit_temp_var(generic_type_t* type);

/**
 * Create and return a three address var from an existing variable. If 
 * we are assigning to a variable, that will create a new generation of variable.
 * As such, we will pass 1 in as a flag here
*/
three_addr_var_t* emit_var(symtab_variable_record_t* var, u_int8_t is_label);

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
three_addr_const_t* emit_int_constant_direct(int int_const, type_symtab_t* symtab);

/**
 * Emit an unsigned int constant directly
 */
three_addr_const_t* emit_unsigned_int_constant_direct(int int_const, type_symtab_t* symtab);

/**
 * Emit a long constant direct from value
 */
three_addr_const_t* emit_long_constant_direct(long long_const, type_symtab_t* symtab);

/**
 * Emit a statement that is in LEA form
 */
instruction_t* emit_lea_instruction(three_addr_var_t* assignee, three_addr_var_t* op1, three_addr_var_t* op2, u_int64_t type_size);

/**
 * Emit an indirect jump calculation that includes a block label in three address code form
 */
instruction_t* emit_indir_jump_address_calc_instruction(three_addr_var_t* assignee, void* jump_table, three_addr_var_t* op2, u_int64_t type_size);

/**
 * Emit a statement using three vars and a binary operator
 * ALL statements are of the form: assignee <- op1 operator op2
*/
instruction_t* emit_binary_operation_instruction(three_addr_var_t* assignee, three_addr_var_t* op1, Token op, three_addr_var_t* op2); 

/**
 * Emit a statement using two vars and a constant
 */
instruction_t* emit_binary_operation_with_const_instruction(three_addr_var_t* assignee, three_addr_var_t* op1, Token op, three_addr_const_t* op2); 

/**
 * Emit a statement that only uses two vars of the form var1 <- var2
 */
instruction_t* emit_assignment_instruction(three_addr_var_t* assignee, three_addr_var_t* op1);

/**
 * Emit a statement that is assigning a const to a var i.e. var1 <- const
 */
instruction_t* emit_assignment_with_const_instruction(three_addr_var_t* assignee, three_addr_const_t* constant);

/**
 * Emit a memory access statement
 */
instruction_t* emit_memory_access_instruction(three_addr_var_t* assignee, three_addr_var_t* op1, memory_access_type_t access_type);

/**
 * Emit a return statement. The return statement can optionally have a node that we're returning.
 * Returnee may or may not be null
 */
instruction_t* emit_ret_instruction(three_addr_var_t* returnee);

/**
 * Emit an increment instruction
 */
instruction_t* emit_inc_instruction(three_addr_var_t* incrementee);

/**
 * Emit a decrement instruction
 */
instruction_t* emit_dec_instruction(three_addr_var_t* decrementee);

/**
 * Emit a negation(negX) statement
 */
instruction_t* emit_neg_instruction(three_addr_var_t* assignee, three_addr_var_t* negatee);

/**
 * Emit a bitwise not instruction
 */
instruction_t* emit_not_instruction(three_addr_var_t* var);

/**
 * Emit a label statement here
 */
instruction_t* emit_label_instruction(three_addr_var_t* var);

/**
 * Emit a left shift statement
 */
instruction_t* emit_left_shift_stmt_instruction(three_addr_var_t* assignee, three_addr_var_t* var, three_addr_var_t* shift_amount_var, three_addr_const_t* shift_amount_const);

/**
 * Emit a right shift statement
 */
instruction_t* emit_right_shift_instruction(three_addr_var_t* assignee, three_addr_var_t* var, three_addr_var_t* shift_amount, three_addr_const_t* shift_amount_const);

/**
 * Emit a logical not instruction
 */
instruction_t* emit_logical_not_instruction(three_addr_var_t* assignee, three_addr_var_t* op1);

/**
 * Emit a jump statement. The jump statement can take on several different types of jump
 */
instruction_t* emit_jmp_instruction(void* jumping_to_block, jump_type_t jump_type);

/**
 * Emit an indirect jump statement. The jump statement can take on several different types of jump
 */
instruction_t* emit_indirect_jmp_instruction(three_addr_var_t* address, jump_type_t jump_type);

/**
 * Emit a direct jump statement. This is used only with jump statements the user has made
 */
instruction_t* emit_direct_jmp_instruction(three_addr_var_t* jumping_to);

/**
 * Emit a function call statement. Once emitted, no paramters will have been added in
 */
instruction_t* emit_function_call_instruction(symtab_function_record_t* func_record, three_addr_var_t* assigned_to);

/**
 * Emit an assembly inline statement. Once emitted, these statements are final and are ignored
 * by any future optimizations
 */
instruction_t* emit_asm_inline_instruction(asm_inline_stmt_ast_node_t* asm_inline_node);

/**
 * Emit a phi function statement. Once emitted, these statements are for the exclusive use of the compiler
 */
instruction_t* emit_phi_function(symtab_variable_record_t* variable);

/**
 * Emit an idle statement
 */
instruction_t* emit_idle_instruction();

/**
 * Are two variables equal? A helper method for searching
 */
u_int8_t variables_equal(three_addr_var_t* a, three_addr_var_t* b, u_int8_t ignore_indirection_level);

/**
 * Are two variables equal regardless of their SSA status? This function should only ever be used
 * by the instruction selector, under very careful circumstances
 */
u_int8_t variables_equal_no_ssa(three_addr_var_t* a, three_addr_var_t* b, u_int8_t ignore_indirect_level);

/**
 * Emit a complete, one-for-one copy of an instruction
 */
instruction_t* copy_instruction(instruction_t* copied);

/**
 * Emit the sum of two given constants. The result will overwrite the second constant given
 *
 * The result will be: constant2 = constant1 + constant2
 */
three_addr_const_t* add_constants(three_addr_const_t* constant1, three_addr_const_t* constant2);

/**
 * Pretty print a three address code statement
*/
void print_three_addr_code_stmt(instruction_t* stmt);

/**
 * Print an instruction that has not yet been given registers
 */
void print_instruction(instruction_t* instruction);

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
void instruction_dealloc(instruction_t* stmt);

/**
 * Destroy all variables
*/
void deallocate_all_vars();

/**
 * Destroy all constants
*/
void deallocate_all_consts();

#endif /* INSTRUCTION_H */
