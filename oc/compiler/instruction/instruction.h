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
#include "../dynamic_array/dynamic_array.h"
#include <stdint.h>
#include <sys/types.h>

//An overall structure for an instruction. Instructions start their life
//as three address code statements, and eventually become assembly instructions
typedef struct instruction_t instruction_t;
//A struct that holds our three address variables
typedef struct three_addr_var_t three_addr_var_t;
//A struct that holds our three address constants
typedef struct three_addr_const_t three_addr_const_t;
//A struct that stores all of our live ranges
typedef struct live_range_t live_range_t;

/**
 * What kind of jump do we want to select
 */
typedef enum{
	JUMP_CATEGORY_INVERSE,
	JUMP_CATEGORY_NORMAL,
} jump_category_t;



/**
 * What type of instruction do we have? This saves us a lot of space
 * as opposed to storing strings. These are x86-64 assembly instructions
 */
typedef enum{
	NO_INSTRUCTION_SELECTED = 0, //The NONE instruction, this is our default and we'll get this when we calloc
	PHI_FUNCTION, //Not really an instruction, but we still need to account for these
	RET,
	CALL,
	MOVB,
	MOVW, //Regular register-to-register or immediate to register
	MOVL,
	MOVQ,
	MOVSX, //Move with sign extension from small to large register
	MOVZX, //Move with zero extension from small to large register
	REG_TO_MEM_MOVB,
	REG_TO_MEM_MOVW,
	REG_TO_MEM_MOVL,
	REG_TO_MEM_MOVQ,
	MEM_TO_REG_MOVB,
	MEM_TO_REG_MOVW,
	MEM_TO_REG_MOVL,
	MEM_TO_REG_MOVQ,
	LEAW,
	LEAL,
	LEAQ,
	INDIRECT_JMP, //For our switch statements
	CQTO, //convert quad-to-octa word
	CLTD, //convert long-to-double-long(quad)
	CWTL, //Convert word to long word
	CBTW, //Convert byte to word
	NOP,
	JMP, //Unconditional jump
	JNE, //Jump not equal
	JE, //Jump if equal
	JNZ, //Jump if not zero
	JZ, //Jump if zero
	JGE, //Jump GE(SIGNED)
	JG, //Jump GT(SIGNED)
	JLE, //Jump LE(SIGNED)
	JL, //JUMP LT(SIGNED)
	JA, //JUMP GT(UNSIGNED)
	JAE, //JUMP GE(UNSIGNED)
	JB, //JUMP LT(UNSIGNED)
	JBE, //JUMP LE(UNSIGNED)
	ADDB,
	ADDW,
	ADDL,
	ADDQ,
	MULB,
	MULW,
	MULL,
	MULQ,
	IMULB,
	IMULW,
	IMULL,
	IMULQ,
	DIVB,
	DIVW,
	DIVL,
	DIVQ,
	IDIVB,
	IDIVW,
	IDIVL,
	IDIVQ,
	IDIVB_FOR_MOD,
	IDIVW_FOR_MOD,
	IDIVL_FOR_MOD,
	IDIVQ_FOR_MOD,
	DIVB_FOR_MOD,
	DIVW_FOR_MOD,
	DIVL_FOR_MOD,
	DIVQ_FOR_MOD,
	SUBB,
	SUBW,
	SUBL,
	SUBQ,
	ASM_INLINE, //ASM inline statements aren't really instructions
	SHRB,
	SHRW,
	SHRL,
	SHRQ, 
	SARB,
	SARW,
	SARL, //Signed shift
	SARQ, //Signed shift
	SALW,
	SALB,
	SALL, //Signed shift 
	SALQ, //Signed shift
	SHLB,
	SHLW,
	SHLL,
	SHLQ,
	INCB,
	INCW,
	INCL,
	INCQ,
	DECB,
	DECW,
	DECL,
	DECQ,
	NEGB,
	NEGW,
	NEGL,
	NEGQ,
	NOTB,
	NOTW,
	NOTL,
	NOTQ,
	XORB,
	XORW,
	XORL,
	XORQ,
	ORB,
	ORW,
	ORL,
	ORQ,
	ANDB,
	ANDW,
	ANDL,
	ANDQ,
	CMPB,
	CMPW,
	CMPL,
	CMPQ,
	TESTB,
	TESTW,
	TESTL,
	TESTQ,
	PUSH,
	POP,
	SETE, //Set if equal
	SETNE, //Set if not equal
	SETGE, //Set >= signed
	SETLE, //Set <= signed
	SETL, //Set < signed
	SETG, //Set > signed
	SETAE, //Set >= unsigned
	SETA, //Set > unsigned
	SETBE, //Set <= unsigned
	SETB, //Set < unsigned
} instruction_type_t;


/**
 * Define the standard x86-64 register table
 */
typedef enum{
	NO_REG = 0, //Default is that there's no register used
	RAX,
	RCX,
	RDX,
	RSI,
	RDI,
	R8,
	R9,
	R10,
	R11,
	R12,
	R13,
	R14,
	R15, 
	RBX,
	RBP, //base pointer
	//ALL general purpose registers come first(items 1-15)
	RSP, //Stack pointer
	RIP, //Instruction pointer
} register_holder_t;


/**
 * What kind of jump statement do we have?
 */
typedef enum{
	NO_JUMP, //This is the default, and what we get when we have 0
	JUMP_TYPE_JNE,
	JUMP_TYPE_JE,
	JUMP_TYPE_JNZ,
	JUMP_TYPE_JZ,
	JUMP_TYPE_JL, //Jump LT(SIGNED)
	JUMP_TYPE_JG, //Jump GT(SIGNED)
	JUMP_TYPE_JGE, //Jump GE(SIGNED)
	JUMP_TYPE_JLE, //Jump LE(SIGNED)
	JUMP_TYPE_JA, //Jump GT(UNSIGNED)
	JUMP_TYPE_JAE, //Jump GE(UNSIGNED)
	JUMP_TYPE_JB, //Jump LT(UNSIGNED)
	JUMP_TYPE_JBE, //Jump LE(UNSIGNED)
	JUMP_TYPE_JMP,
} jump_type_t;


/**
 * What kind of jump statement do we have?
 */
typedef enum{
	NO_CONDITIONAL_MOVE = 0, //This is the default, and what we get when we have 0
	CONDITIONAL_MOVE_NE,
	CONDITIONAL_MOVE_E,
	CONDITIONAL_MOVE_NZ,
	CONDITIONAL_MOVE_Z,
	CONDITIONAL_MOVE_L, // LT(SIGNED)
	CONDITIONAL_MOVE_G, //GT(SIGNED)
	CONDITIONAL_MOVE_GE, //GE(SIGNED)
	CONDITIONAL_MOVE_LE, //LE(SIGNED)
	CONDITIONAL_MOVE_A, //GT(UNSIGNED)
	CONDITIONAL_MOVE_AE, //GE(UNSIGNED)
	CONDITIONAL_MOVE_B, // LT(UNSIGNED)
	CONDITIONAL_MOVE_BE, //LE(UNSIGNED)
} conditional_move_type_t;


/**
 * What kind of word length do we have -- used for instructions
 */
typedef enum{
	BYTE,
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
	ADDRESS_CALCULATION_MODE_DEREF_ONLY_SOURCE, //(%rax) - only the deref depending on how much indirection
	ADDRESS_CALCULATION_MODE_DEREF_ONLY_DEST, //(%rax) - only the deref depending on how much indirection
	ADDRESS_CALCULATION_MODE_OFFSET_ONLY, // 4(%rax)
	ADDRESS_CALCULATION_MODE_REGISTERS_ONLY, // (%rax, %rcx)
	ADDRESS_CALCULATION_MODE_REGISTERS_AND_OFFSET, // 4(%rax, %rcx)
	ADDRESS_CALCULATION_MODE_REGISTERS_AND_SCALE, // (%rax, %rcx, 8)
	ADDRESS_CALCULATION_MODE_REGISTERS_OFFSET_AND_SCALE // 4(%rax, %rcx, 8)
} address_calculation_mode_t;

/**
 * For variable printing, where we're printing
 * matters. The user must specify if it's
 * block or inline mode
 */
typedef enum{
	PRINTING_VAR_INLINE,
	PRINTING_VAR_BLOCK_HEADER,
	PRINTING_VAR_IN_INSTRUCTION,
	PRINTING_LIVE_RANGES,
	PRINTING_REGISTERS, //Use the allocate registers for this
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
	//A three address code conditional movement statement
	THREE_ADDR_CODE_CONDITIONAL_MOVEMENT_STMT,
	//An indirect jump statement -- used for switch statement jump tables
	THREE_ADDR_CODE_INDIRECT_JUMP_STMT,
	//A direct to label jump statement
	THREE_ADDR_CODE_DIR_JUMP_STMT,
	//A label statement
	THREE_ADDR_CODE_LABEL_STMT,
	//A function call statement 
	THREE_ADDR_CODE_FUNC_CALL,
	//And indirect function call statement
	THREE_ADDR_CODE_INDIRECT_FUNC_CALL,
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
	THREE_ADDR_CODE_MEM_ACCESS_STMT,
	//An address assignment instruction for memory address
	THREE_ADDR_CODE_MEM_ADDR_ASSIGNMENT
} instruction_stmt_class_t;


/**
 * For our live ranges, we'll really only need the name and
 * the variables
 */
struct live_range_t{
	//Hold all the variables that it has
	dynamic_array_t* variables;
	//And we'll hold an adjacency list for interference
	dynamic_array_t* neighbors;
	//What function does this come from?
	symtab_function_record_t* function_defined_in;
	//The degree of this live range
	u_int16_t degree;
	//The interference graph index of it
	u_int16_t interference_graph_index;
	//Store the heuristic spill cost
	int16_t spill_cost;
	//Store the id of the live range
	u_int16_t live_range_id;
	//Does this carry a function parameter?
	u_int8_t carries_function_param;
	//Does this carry a pre-colored value
	u_int8_t is_precolored;
	//Does this live range need to be spilled?
	u_int8_t must_be_spilled;
	//What register is this live range in?
	register_holder_t reg; 
	//The size of the variable in the live range
	variable_size_t size;
};


/**
 * A three address var may be a temp variable or it may be
 * linked to a non-temp variable. It keeps a generation counter
 * for eventual SSA and type information
*/
struct three_addr_var_t{
	//Link to symtab(NULL if not there)
	symtab_variable_record_t* linked_var;
	//Link to the function record(NULL if not there)
	symtab_function_record_t* linked_function;
	//Types will be used for eventual register assignment
	generic_type_t* type;
	//What is this related to the writing of?
	symtab_variable_record_t* related_write_var;
	//For memory management
	three_addr_var_t* next_created;
	//The related stack data area node. Not all variables have these,
	//but "spilled" variables and address variables do
	stack_data_area_node_t* related_node;
	//What live range is this variable associate with
	live_range_t* associated_live_range;
	//What is the stack offset(i.e. %rsp + __) of this variable?
	u_int32_t stack_offset;
	//What is the ssa generation level?
	u_int32_t ssa_generation;
	//What's the temp var number
	u_int32_t temp_var_number;
	//What's the reference count of this variable.
	//This will be needed later on down the line in 
	//the instruction selector
	u_int32_t use_count;
	//What is the indirection level
	u_int16_t indirection_level;
	//Is this a temp variable?
	u_int8_t is_temporary;
	//Is this a stack pointer?
	u_int8_t is_stack_pointer;
	//Is this a function variable
	u_int8_t is_function_variable;
	//What is the parameter number of this var? Used for parameter passing. If
	//it is 0, it's ignored
	u_int8_t parameter_number;
	//What is the size of this variable
	variable_size_t variable_size;
	//Store the type info for faster access
	//Memory access type, if one exists
	memory_access_type_t access_type;
	//What register is this in?
	register_holder_t variable_register;
};


/**
 * A three address constant always holds the value of the constant
 */
struct three_addr_const_t{
	//The string constant
	dynamic_string_t string_constant;
	//For memory management
	three_addr_const_t* next_created;
	//The constant's function record
	symtab_function_record_t* function_name;
	//We hold the type info
	generic_type_t* type;
	//And we hold everything relevant about the constant
	long long_const;
	float float_const;
	int int_const;
	//What kind of constant is it
	Token const_type;
	char char_const;
	//Is the value of this constant 0?
	u_int8_t is_value_0;
};


/**
 * A generic struct that encapsulates most of our instructions
 */
struct instruction_t{
	//Store inlined assembly in a string
	dynamic_string_t inlined_assembly;
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
	three_addr_const_t* offset;
	//The address calculation registers
	three_addr_var_t* address_calc_reg1;
	three_addr_var_t* address_calc_reg2;
	//The remainder register(used for div & mod)
	three_addr_var_t* remainder_register;
	//The offset
	//Store a reference to the block that we're jumping to
	void* jumping_to_block;
	//The LEA addition
	u_int64_t lea_multiplicator;
	//The function called
	symtab_function_record_t* called_function;
	//The variable record
	symtab_variable_record_t* var_record;
	//What function are we currently in?
	symtab_function_record_t* function;
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
	//What is the indirection level?
	u_int8_t indirection_level;
	//Cannot be coalesced
	u_int8_t cannot_be_combined;
	//Is this a converting move of some kind?
	u_int8_t is_converting_move;
	//Does this have a multiplicator
	u_int8_t has_multiplicator;
	//If it's a jump statement, what's the type?
	jump_type_t jump_type;
	//If this is a conditional move statement, what's the class?
	conditional_move_type_t move_type;
	//Memory access type
	TYPE_CLASS access_class;
	//What kind of address calculation mode do we have?
	address_calculation_mode_t calculation_mode;
};

/**
 * A helper function for our atomically increasing temp id
 */
int32_t increment_and_get_temp_id();

/**
 * Insert an instruction in a block before the given instruction
 */
void insert_instruction_before_given(instruction_t* insertee, instruction_t* given);

/**
 * Insert an instruction in a block after the given instruction
 */
void insert_instruction_after_given(instruction_t* insertee, instruction_t* given);

/**
 * Declare that we are in a new function
 */
void set_new_function(symtab_function_record_t* func);

/**
 * Select the size based only on a type
 */
variable_size_t select_type_size(generic_type_t* type);

/**
 * Select the size of a constant based on its type
 */
variable_size_t select_constant_size(three_addr_const_t* constant);

/**
 * Select the size of a given variable based on its type
 */
variable_size_t select_variable_size(three_addr_var_t* variable);

/**
 * Determine the signedness of a jump type
 */
u_int8_t is_jump_type_signed(jump_type_t type);

/**
 * Helper function to determine if an operator is a relational operator
 */
u_int8_t is_operator_relational_operator(Token op);

/**
 * Helper function to determine if an instruction is a binary operation
 */
u_int8_t is_instruction_binary_operation(instruction_t* instruction);

/**
 * Helper function to determine if an instruction is an assignment operation
 */
u_int8_t is_instruction_assignment_operation(instruction_t* instruction);

/**
 * Does a given operation overwrite it's source? Think add, subtract, etc
 */
u_int8_t is_destination_also_operand(instruction_t* instruction);

/**
 * Is this operation a pure copy? In other words, is it a move instruction
 * that moves one register to another?
 */
u_int8_t is_instruction_pure_copy(instruction_t* instruction);

/**
 * Is this an unsigned multiplication instruction?
 */
u_int8_t is_unsigned_multplication_instruction(instruction_t* instruction);

/**
 * Is this a division instruction?
 */
u_int8_t is_division_instruction(instruction_t* instruction);

/**
 * Is this a division instruction that is intended for modulus?
 */
u_int8_t is_modulus_instruction(instruction_t* instruction);

/**
 * Create and return a temporary variable
*/
three_addr_var_t* emit_temp_var(generic_type_t* type);

/**
 * Create and return a temporary variable from a live range
*/
three_addr_var_t* emit_temp_var_from_live_range(live_range_t* range);

/**
 * Create and return a three address var from an existing variable. If 
 * we are assigning to a variable, that will create a new generation of variable.
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
 * Emit a push instruction. We only have one kind of pushing - quadwords - we don't
 * deal with getting granular when pushing
 */
instruction_t* emit_push_instruction(three_addr_var_t* pushee);

/**
 * Emit a movzx(zero extend) instruction
 */
instruction_t* emit_movzx_instruction(three_addr_var_t* source, three_addr_var_t* destination);

/**
 * Emit a movsx(sign extend) instruction
 */
instruction_t* emit_movsx_instruction(three_addr_var_t* source, three_addr_var_t* destination);

/**
 * Emit a pop instruction. We only have one kind of popping - quadwords - we don't
 * deal with getting granular when popping 
 */
instruction_t* emit_pop_instruction(three_addr_var_t* popee);

/**
 * Emit a movX instruction
 *
 * This is used for when we need extra moves(after a division/modulus)
 */
instruction_t* emit_movX_instruction(three_addr_var_t* destination, three_addr_var_t* source);

/**
 * Emit a lea statement with no type size multiplier on it
 */
instruction_t* emit_lea_instruction_no_mulitplier(three_addr_var_t* assignee, three_addr_var_t* op1, three_addr_var_t* op2);

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
 * Emit a conditional assignment statement
 */
instruction_t* emit_conditional_assignment_instruction(three_addr_var_t* assignee, three_addr_var_t* op1, Token prior_operator, u_int8_t is_signed, u_int8_t inverse_assignment);

/**
 * Emit a statement that only uses two vars of the form var1 <- var2
 */
instruction_t* emit_assignment_instruction(three_addr_var_t* assignee, three_addr_var_t* op1);

/**
 * Emit a memory address assignment statement
 */
instruction_t* emit_memory_address_assignment(three_addr_var_t* assignee, three_addr_var_t* op1);

/**
 * Emit a statement that is assigning a const to a var i.e. var1 <- const
 */
instruction_t* emit_assignment_with_const_instruction(three_addr_var_t* assignee, three_addr_const_t* constant);

/**
 * Emit a memory access statement
 */
instruction_t* emit_memory_access_instruction(three_addr_var_t* assignee, three_addr_var_t* op1, memory_access_type_t access_type);

/**
 * Emit a load statement directly. This should only be used during spilling in the register allocator
 */
instruction_t* emit_load_instruction(three_addr_var_t* assignee, three_addr_var_t* stack_pointer, type_symtab_t* symtab, u_int64_t offset);

/**
 * Emit a store statement directly. This should only be used during spilling in the register allocator
 */
instruction_t* emit_store_instruction(three_addr_var_t* source, three_addr_var_t* stack_pointer, type_symtab_t* symtab, u_int64_t offset);

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
 * Emit an indirect function call statement. Once emitted, no paramters will have been added in
 */
instruction_t* emit_indirect_function_call_instruction(three_addr_var_t* function_pointer, three_addr_var_t* assigned_to);

/**
 * Emit an assembly inline statement. Once emitted, these statements are final and are ignored
 * by any future optimizations
 */
instruction_t* emit_asm_inline_instruction(generic_ast_node_t* asm_inline_node);

/**
 * Emit a phi function statement. Once emitted, these statements are for the exclusive use of the compiler
 */
instruction_t* emit_phi_function(symtab_variable_record_t* variable, generic_type_t* type);

/**
 * Emit an idle statement
 */
instruction_t* emit_idle_instruction();

/**
 * Emit a setX instruction
 */
instruction_t* emit_setX_instruction(Token op, three_addr_var_t* destination_register, u_int8_t is_signed);

/**
 * Emit a stack allocation statement
 */
instruction_t* emit_stack_allocation_statement(three_addr_var_t* stack_pointer, type_symtab_t* type_symtab, u_int64_t offset);

/**
 * Emit a stack deallocation statement
 */
instruction_t* emit_stack_deallocation_statement(three_addr_var_t* stack_pointer, type_symtab_t* type_symtab, u_int64_t offset);

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
 * Select the appropriate jump type given the circumstances, including the operand and the signedness
 */
jump_type_t select_appropriate_jump_stmt(Token op, jump_category_t jump_type, u_int8_t is_signed);

/**
 * Select the appropriate set type given the circumstances, including the operand and the signedness
 */
instruction_type_t select_appropriate_set_stmt(Token op, u_int8_t is_signed);

/**
 * Is the given register caller saved?
 */
u_int8_t is_register_caller_saved(register_holder_t reg);

/**
 * Is the given register callee saved?
 */
u_int8_t is_register_callee_saved(register_holder_t reg);

/**
 * Pretty print a three address code statement
*/
void print_three_addr_code_stmt(FILE* fl, instruction_t* stmt);

/**
 * Print an instruction that has not yet been given registers
 */
void print_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode);

/**
 * Print a variable and everything about it. If the variable is in
 * "Block header" mode, we won't print out any dereferencing info
 */
void print_variable(FILE* fl, three_addr_var_t* variable, variable_printing_mode_t mode);

/**
 * Print a live range out
 */
void print_live_range(FILE* fl, live_range_t* live_range);

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
