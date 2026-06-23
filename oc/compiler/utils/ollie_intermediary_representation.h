/**
 * Author: Jack Robbins
 * This header defines the types an OIR statement
 * can take
*/

//Include guards
#ifndef OLLIE_INTERMEDIARY_REPRESENTATION_H
#define OLLIE_INTERMEDIARY_REPRESENTATION_H

/**
 * Do we want to have a regular branch or an inverse
 * branch? An inverse branch will down the road set the conditional
 * to be the opposite of what is put in.
 *
 * So for example
 *
 * if(x == 3) then A else B
 *
 * Regular mode
 * cmp 3, x
 * je A <-----  if
 * jmp B <----- else
 *
 * Inverse mode
 * cmp 3, x
 * jne A <-----  if(inverted)
 * jmp B <----- else
 */
typedef enum {
	BRANCH_CATEGORY_NORMAL,
	BRANCH_CATEGORY_INVERSE
} branch_category_t;


/**
 * Define the kind of branch that we have in an ollie branch
 * command
 */
typedef enum {
	NO_BRANCH, //This is the default, and what we get when we have 0
	BRANCH_NE,
	BRANCH_E,
	BRANCH_NZ,
	BRANCH_Z,
	BRANCH_L, //Branch LT(SIGNED)
	BRANCH_G, //Branch GT(SIGNED)
	BRANCH_GE, //Branch GE(SIGNED)
	BRANCH_LE, //Branch LE(SIGNED)
	BRANCH_A, //Branch GT(UNSIGNED)
	BRANCH_AE, //Branch GE(UNSIGNED)
	BRANCH_B, //Branch LT(UNSIGNED)
	BRANCH_BE, //Branch LE(UNSIGNED)
} branch_type_t;

/**
 * The kind of conditional movement that we are doing
 */
typedef enum {
	NO_CONDITIONAL_MOVEMENT, //This is the default, and what we get when we have 0
	MOVE_NE,
	MOVE_E,
	MOVE_NZ,
	MOVE_Z,
	MOVE_L, //Move LT(SIGNED)
	MOVE_G, //Move GT(SIGNED)
	MOVE_GE, //Move GE(SIGNED)
	MOVE_LE, //Move LE(SIGNED)
	MOVE_A, //Move GT(UNSIGNED)
	MOVE_AE, //Move GE(UNSIGNED)
	MOVE_B, //Move LT(UNSIGNED)
	MOVE_BE, //Move LE(UNSIGNED)
} conditional_movement_type_t;


/**
 * All OIR statement types
 */
typedef enum {
	//Binary op with all vars
	THREE_ADDR_CODE_BIN_OP_STMT,
	//A setne statement
	THREE_ADDR_CODE_SETNE_STMT,
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
	//A raise statement(basically a RET)
	THREE_ADDR_CODE_RAISE_STMT,
	//A jump statement -- used for control flow
	THREE_ADDR_CODE_JUMP_STMT,
	//A branch statement, also used for control flow
	THREE_ADDR_CODE_BRANCH_STMT,
	//A three address code conditional movement statement
	THREE_ADDR_CODE_CONDITIONAL_MOVEMENT_STMT,
	//An indirect jump statement -- used for switch statement jump tables
	THREE_ADDR_CODE_INDIRECT_JUMP_STMT,
	//A function call statement 
	THREE_ADDR_CODE_FUNC_CALL,
	//And indirect function call statement
	THREE_ADDR_CODE_INDIRECT_FUNC_CALL,
	//An idle statement(nop)
	THREE_ADDR_CODE_IDLE_STMT,
	//A negation statement
	THREE_ADDR_CODE_NEG_STATEMENT,
	//Generic store to memory statement(uses addressing mode)
	THREE_ADDR_CODE_STORE_STATEMENT,
	//Geneirc load from memory statement(uses addressing mode)
	THREE_ADDR_CODE_LOAD_STATEMENT,
	//SPECIAL CASE - assembly inline statement
	THREE_ADDR_CODE_ASM_INLINE_STMT,
	//Test if not 0 statement
	THREE_ADDR_CODE_TEST_IF_NOT_ZERO_STMT,
	//A "Load effective address(lea)" instruction
	THREE_ADDR_CODE_LEA_STMT,
	//A phi function - for SSA analysis only
	THREE_ADDR_CODE_PHI_FUNC,
	//A specialized CLEAR instruction
	THREE_ADDR_CODE_CLEAR_STMT,
	//A specialized instruction used when we need temporary stack allocations
	THREE_ADDR_CODE_STACK_ALLOCATION_STMT,
	//A specialized instruction used when we need temporary stack allocations
	THREE_ADDR_CODE_STACK_DEALLOCATION_STMT,
	//Specialized instruction for when we need to copy from one memory region to another
	THREE_ADDR_CODE_MEMORY_COPY_STATEMENT,
	//Specialized offset statement for elaborative params
	THREE_ADDR_CODE_ELABORATIVE_PARAM_OFFSET,
} instruction_stmt_type_t;

#endif /* OLLIE_INTERMEDIARY_REPRESENTATION_H */
