/**
 * Author: Jack Robbins
 * This file defines the Ollie IR's instruction struct. Ollie instructions start their life as "three address code statements"
 * and will eventually be selected into x86 assembly instructions. The Ollie instruction struct carries the information
 * to support both of these variations. Instructions are packaged into blocks, which are packaged into functions, which
 * are the building blocks of the Ollie language
 */

#ifndef OLLIE_INSTRUCTION_H
#define OLLIE_INSTRUCTION_H

#include <sys/types.h>
#include "parameter_result_array/parameter_result_array.h"
#include "stack_management_structs.h"
#include "token.h"
#include "dynamic_array/dynamic_array.h"
#include "dynamic_string/dynamic_string.h"
#include "ollie_intermediary_representation.h"
#include "x86_assembly_instruction.h"
#include "x86_genpurpose_registers.h"
#include "x86_sse_registers.h"
#include "three_address_variable.h"
#include "three_address_constant.h"
#include "three_address_initializer.h"

/**
 * An overall structure for an instruction. Instructions start their life
 * as three address code statements, and eventually become assembly instructions
 */
typedef struct instruction_t instruction_t;
typedef struct addressing_operands_t addressing_operands_t;

/**
 * This utility struct is used for passing around addressing
 * operands when we don't want to pass everything at once
 */
struct addressing_operands_t {
	three_addr_var_t* address_operand1;
	three_addr_var_t* address_operand2;
	three_addr_const_t* address_offset;
	u_int64_t address_multiplier;
	three_addr_var_t* rip_offset_var;
	memory_addressing_mode_t addressing_mode;
};


/**
 * Utility to initialize NULL addressing operands
 */
#define INITIALIZE_NULL_ADDRESS_OPERANDS (addressing_operands_t){NULL, NULL, NULL, 0, NULL, ADDRESSING_MODE_NONE}

/**
 * Are we forcing something to be signed or unsigned
 * regardless of the assignee type? This is mainly used
 * for shift operations but I can see it potentially
 * being useful elsewhere so we'll build this out
 */
typedef enum {
	FORCED_SIGNEDNESS_DONT_CARE=0, //Default case
	FORCED_UNSIGNED,
	FORCED_SIGNED
} forced_signedness_type_t;

/**
 * Will we be printing these out as instructions or as three address code
 * statements?
 */
typedef enum {
	PRINT_THREE_ADDRESS_CODE,
	PRINT_INSTRUCTION
} instruction_printing_mode_t;

/**
 * A generic struct that encapsulates most of our instructions
 */
struct instruction_t{
	//For linked list properties -- the next statement
	instruction_t* next_statement;
	//For doubly linked list properties -- the previous statement
	instruction_t* previous_statement;
	//What is the three address code type
	instruction_stmt_type_t statement_type;
	//What is the x86-64 instruction
	instruction_type_t instruction_type;
	//What is the operator for the instruction?
	ollie_token_t op;
	/**
	 * For the sake of efficiency - we only ever need to use
	 * one of these structs at at time. It's for this reason
	 * that we have both structs wrapped inside of a union
	 * to save on space
	 */
	struct {
		/**
		 * Storage for the variables that are used inside of OIR. 
		 */
		struct {
			//First operand inside of an expresion
			three_addr_var_t* operand1;
			//Second operand inside of an expression
			three_addr_var_t* operand2;
			//Constant operand if need be
			three_addr_const_t* constant_operand;
			//Assignee for the expression
			three_addr_var_t* assignee;

			/**
			 * Since we can have OIR expressions with a memory
			 * source(lea(kindof), and load), and we can have expressions
			 * with a memory destination(store), and we can have expressions
			 * with a memory source and destination(memory copy), we have 
			 * room for both in here
			 */
			addressing_operands_t source_address_expression;
			addressing_operands_t destination_address_expression;

			/**
			 * We are able to initialize from an initializer into a destination memory 
			 * area. This initializer operand supports that
			 */
			three_addr_initializer_t* initializer_operand;
		} oir; 

		/**
		 * Storage for the variables that represent the registers inside of
		 * our representation of x86 assembly
		 */
		struct {
			//First source register for x86 assembly
			three_addr_var_t* source_register1;
			//Second source register for x86 assembly
			three_addr_var_t* source_register2;
			/**
			 * Division instructions have higher order bits
			 * on their dividend that are populated after an
			 * extension. This will always be precolored to %rdx
			 */
			three_addr_var_t* higher_order_dividend_bits;
			//Immediate value for x86 assembly
			three_addr_const_t* source_immediate;
			//First destination register
			three_addr_var_t* destination_register;
			//Second destination register - some instructions have these but it is rare
			three_addr_var_t* destination_register2;
			/**
			 * We maintain separate variable storage for lea/address
			 * calculation statements. Reminder that these statements
			 * go like(at their maximum): offset(register1, register2, multiplier)
			 */
			three_addr_const_t* address_offset;
			three_addr_var_t* address_register1;
			three_addr_var_t* address_register2;
			//This can never be anything besides a 64 bit integer
			u_int64_t address_multiplier;
			/**
			 * Some variables are represented as RIP offsets. We will use a special
			 * space here so that they are excluded from the register allocator's
			 * processing
			 */
			three_addr_var_t* rip_offset_var;
			//Separate memory addressing mode for the x86 values
			memory_addressing_mode_t addressing_mode;
		} x86;
	} operands;

	/**
	 * Parameters exist for phi functions and function calls. We'll just have one
	 * area to store them for both kinds
	 */
	dynamic_array_t parameters;

	//Optional variable storage to determine what a value relies on
	three_addr_var_t* relies_on;

	/**
	 * Function calls are a special kind of instruction that will have
	 * a lot of stuff hanging off of them. As such, we'll bundle it all
	 * into one big struct for ease of use
	 */
	struct {
		//The function that was called(this is for direct calls)
		symtab_function_record_t* called_function;

		//Some function calls require their own stack setup
		stack_data_area_t stack_parameter_area;

		//Store the parameter results for a function call
		parameter_results_array_t parameter_results;

		/**
		 * Function calls that call return-by-copy functions must have
		 * a stack region created for them to copy into inside of the
		 * callee's stack. This region is also what these functions will
		 * return in the end(%rax is always the value from %rdi, which is
		 * the return by copy region). We will create these return by
		 * copy regions the instant that we see a function call that
		 * has them, and we'll store a reference to it here
		 *
		 * We will maintain a symtab variable that represents a return
		 * by copy variable. This allows us to have the same variable name
		 * for every associated memory address region, which makes tracking much
		 * easier for us. If we ever wish to get the actual return by copy region,
		 * we need only get the stack region off of this variable
		 */
		symtab_variable_record_t* return_by_copy_variable;

		//The second error assignee for an errorable function
		three_addr_var_t* error_assignee;

		/**
		 * Function calls exist in a weird hybrid state where they're
		 * not lowered into their lowest OIR until the instruction selector's
		 * simply stage. This flag will tell us what state they're in
		 */
		u_int8_t has_been_lowered;

		//Is this an inlined function call or not?
		u_int8_t is_inlined_call;

		//Is this function call eligible for return region forwarding?
		u_int8_t is_rrf_eligible;
	} call_storage;

	/**
	 * Optional storage values that are not used enough to justify
	 * their own dedicated field
	 */
	union {
		//Store inlined assembly in a string
		dynamic_string_t inlined_assembly;
		//Store the byte amount that we want to copy by
		u_int64_t byte_amount_to_copy;
		//Signedness forcing - used specifically for shifting
		forced_signedness_type_t forced_signedness;
		//The label that we are jumping to
		symtab_label_record_t* jumping_to_label;
	} optional_storage;

	/**
	 * Optional storage for a type. The union is for readability and intentionality, 
	 * we know that it's not really needed
	 */
	union {
		/**
		 * What is the type of the memory that we are trying to access? This is done
		 * to maintain separation from the base addresses and the memory that we're using
		 */
		generic_type_t* memory_read_write_type;
		/**
		 * What is the result type of a computation? For various reasons the result type
		 * of a computation may actually be different as compared to a final type
		 */
		generic_type_t* result_type;
	} type_storage;

	/**
	 * Store all branch info inside of one unified area
	 */
	struct {
		/**
		 * Store the branch type
		 */
		branch_type_t branch_type;
		/**
		 * Store the if and else blocks. These are *raw* if and else blocks, they do not 
		 * represent the actual result in the end
		 */
		void* if_block;
		void* else_block;
		/**
		 * BRANCH_CATEGORY_NORMAL - If block is the affirmative condition
		 * BRANCH_CATEGORY_INVERSE - Else block is the affirmative condition
		 */
		branch_category_t branch_category;
		//
		/**
		 * Does this branch derive from floating point comparison. This
		 * is only ever set by the instruction selector
		 */
		u_int8_t branch_derives_from_float_comparison;
	} branch_info;

	//What block holds this?
	void* block_contained_in;
	//Instruction's line number
	int32_t line_number;
	/**
	 * Is this an instruction that we want to specifically exclude when we are
	 * inlining a function call?
	 */
	u_int8_t exclude_when_inlining;
	//Is this operation critical?
	u_int8_t mark;
	//Do we need to stop this value from being coalesced. This is only used
	//very specifically in the logical not handler currently
	u_int8_t cannot_be_combined;
	//Does this instruction handle callee saving?
	u_int8_t is_callee_saving_instruction;
	//The conditional movement type that we have
	conditional_movement_type_t movement_type;
	//Do we have a read, write, or no attempt to access memory(default)
	memory_access_type_t memory_access_type;
	//The register that we're popping or pushing
	union{
		general_purpose_register_t gen_purpose;
		sse_register_t sse_register;
	} push_or_pop_reg;
};


/**
 * Is an operator short circuit eligible? Return TRUE if yes, FALSE
 * if no
 */
static inline u_int8_t is_op_short_circuit_eligible(ollie_token_t op){
	switch(op){
		case DOUBLE_AND:
		case DOUBLE_OR:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Will a given operator and operand type, when lowered to x86 assembly, set flags that
 * are actionable for a branch? This is very specific: we aren't just asking if it sets flags
 * or not, we explicitly require that this sets things like the zero flag that a branch can
 * act on
 */
static inline u_int8_t does_operator_set_branch_actionable_RFLAGS(ollie_token_t op, generic_type_t* operand_type){
	switch(op){
		/**
		 * Comparison operators(<, <=, >, >=, !=, ==), logical not(!), 
		 * right/left shift(<<, >>), bitwise operators(^, |, &), and 
		 * logical and/or(&&, ||) will all set condition codes regardless
		 * of the operand type once lowered to x86 assembly
		 */
		case L_THAN:
		case L_THAN_OR_EQ:
		case G_THAN:
		case G_THAN_OR_EQ:
		case DOUBLE_EQUALS:
		case NOT_EQUALS:
		case EXCLAMATION:
		case CARROT:
		case SINGLE_AND:
		case SINGLE_OR:
		case DOUBLE_AND:
		case DOUBLE_OR: {
			return TRUE;
		}

		/**
		 * For addition(addX, addss, addsd) and subtraction(subX, subss, subsd), the
		 * integer versions of the operation *DO* set flags as needed. However, the floating
		 * point versions do not. As such, if there are integer operands then this does
		 * work for branching, and if they are floating point it does not
		 */
		case PLUS:
		case MINUS: {
			return !(IS_FLOATING_POINT(operand_type));
		}

		/**
		 * For ++/--, the INC and DEC commands do set flags but they are weird
		 * in that they don't ever set the CF. The determination has been made that,
		 * since they do not set the carry flag, we will not count them as setting
		 * flags at all. Instead we'll just insert a TEST_IF_NOT_ZERO command after
		 * any INC/DEC if needed
		 */
		case PLUSPLUS:
		case MINUSMINUS: {
			return FALSE;
		}

		/**
		 * Modulo and division(integer or floating point) do not provide meaningful
		 * flag information. Specifically the divX/idivX/divss/divsd operations do
		 * not ever set the zero flag. As such they are useless for branching and
		 * we will mark them FALSE here
		 */
		case F_SLASH:
		case MOD: {
			return FALSE;
		}

		/**
		 * Multiplication(integer and floating point) does not provide meaningful flag
		 * information on x86. Specifcally, we will never see the zero flag set by a mulitplication
		 * instruction. As such they alone are useless for branching and we will mark
		 * them FALSE here
		 */
		case STAR: {
			return FALSE;
		}

		/**
		 * Shifting(salX, shlX, sarX, shrX) is unique in that it does set the zero flag
		 * in all but *ONE* scenario. That scenario being that, if the count is 0, we do
		 * not set the 0 flag even if the result is 0. Since we have no way of divining this
		 * in most areas of our compiler pipeline, we will need to consider all left/right
		 * shift operations as not eligible to be branched off of
		 */
		case L_SHIFT:
		case R_SHIFT: {
			return FALSE;
		}

		/**
		 * Operators like bitwise not(~) do not set flags
		 * at all so we exclude them
		 */
		default: {
			return FALSE;
		}
	}
}


/**
 * Does the given OIR statement alter flags in any way? This drops the strict
 * requirement of the flags being branch eligible
 */
static inline u_int8_t does_OIR_statement_alter_flags(instruction_t* statement){
	switch(statement->statement_type){
		/**
		 * For binary operations and negation, whether or not we alter flags depends on what
		 * we're operating over and the operand itself
		 */
		case THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT:
		case THREE_ADDR_CODE_NEG_STATEMENT: 
		case THREE_ADDR_CODE_BIN_OP_STMT: {
			/**
			 * Extract the type that we're operating on to determine
			 * if we set flags or not
			 */
			generic_type_t* type_operating_on = NULL;
			if(statement->type_storage.result_type != NULL){
				type_operating_on = statement->type_storage.result_type;
			} else {
				type_operating_on = statement->operands.oir.operand1->type;
			}

			/**
			 * Based on the operator we will determine if flags are set or
			 * unchanged by this operation
			 */
			switch(statement->op){
				/**
				 * All of the below will always set flags no matter what
				 */
				case L_THAN:
				case L_THAN_OR_EQ:
				case G_THAN:
				case G_THAN_OR_EQ:
				case DOUBLE_EQUALS:
				case NOT_EQUALS:
				case EXCLAMATION:
				case CARROT:
				case SINGLE_AND:
				case SINGLE_OR:
				case DOUBLE_AND:
				case DOUBLE_OR:
				case PLUSPLUS:
				case MINUSMINUS:
				case L_SHIFT:
				case R_SHIFT: {
					return TRUE;
				}

				/**
				 * +, -, *, / and % all mangle the flags when they are done over integers. If
				 * done over floating point operands, then no flags are touched. To be specific,
				 * the x86 standard lists these as "undefined" for integer operands, meaning that
				 * no guarnatees are made at an architecture level as to what the values are
				 */
				case PLUS:
				case MINUS:
				case STAR:
				case F_SLASH:
				case MOD: {
					return !(IS_FLOATING_POINT(type_operating_on));
				}

				/**
				 * Anything else is undefined and false
				 */
				default: {
					return FALSE;
				}
			}
		}

		/**
		 * All of the following are going to set flags no matter what, so we have to
		 * include them below
		 */
		case THREE_ADDR_CODE_INC_STMT:
		case THREE_ADDR_CODE_DEC_STMT:
		case THREE_ADDR_CODE_LOGICAL_NOT_STMT:
		case THREE_ADDR_CODE_TEST_IF_NOT_ZERO_STMT:
		case THREE_ADDR_CODE_CLEAR_STMT: {
			return TRUE;
		}

		/**
		 * By default assume no condition codes are set
		 */
		default: {
			return FALSE;
		}
	}
}


/**
 * Does the given OIR statement set the RFLAGS register in a way that is sufficient
 * to be branched on? This means that incomplete or undefined flag setting will
 * be ignored(example: division, shifting, inc/dec, etc)
 */
static inline u_int8_t does_OIR_statement_set_branch_eligible_flags(instruction_t* statement){
	switch(statement->statement_type){
		/**
		 * For the following statement types, whether or not we set condition
		 * codes depends entirely on the type that we're operating over.
		 */
		case THREE_ADDR_CODE_NEG_STATEMENT: 
		case THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT:
		case THREE_ADDR_CODE_LOGICAL_NOT_STMT:
		case THREE_ADDR_CODE_BIN_OP_STMT: {
			/**
			 * Extract the type that we're operating on to determine
			 * if we set flags or not
			 */
			generic_type_t* type_operating_on = NULL;
			if(statement->type_storage.result_type != NULL){
				type_operating_on = statement->type_storage.result_type;
			} else {
				type_operating_on = statement->operands.oir.operand1->type;
			}

			return does_operator_set_branch_actionable_RFLAGS(statement->op, type_operating_on);
		}

		/**
		 * Test if not zero is purpose built to always set condition codes, so we will
		 * always return TRUE in that case
		 */
		case THREE_ADDR_CODE_TEST_IF_NOT_ZERO_STMT: {
			return TRUE;
		}

		/**
		 * By default assume no condition codes are set
		 */
		default: {
			return FALSE;
		}
	}
}


/**
 * Is the given binary operator commutative? This relies on the
 * operator and the type. Some operations(like multiplication) are
 * commutative for integers but noncommutative for floats
 */
static inline u_int8_t is_binary_operator_commutative(ollie_token_t op, generic_type_t* operand_type){
	switch(op){
		/**
		 * Bitwse operations(&, |, ^) are always commutative
		 */
		case SINGLE_AND:
		case SINGLE_OR:
		case CARROT: {
			return TRUE;
		}

		/**
		 * Logical operations are also always commutative
		 */
		case DOUBLE_AND:
		case DOUBLE_OR: {
			return TRUE;
		}

		/**
		 * ==/!= are reversible and as such commutative
		 */
		case DOUBLE_EQUALS:
		case NOT_EQUALS: {
			return TRUE;
		}

		/**
		 * Multiplcation and addition are commutative for non-floats only
		 */
		case STAR:
		case PLUS: {
			return !(IS_FLOATING_POINT(operand_type));
		}

		/**
		 * Everything else is by default non-commutative
		 */
		default: {
			return FALSE;
		}
	}
}

#endif /* OLLIE_INSTRUCTION_H */
