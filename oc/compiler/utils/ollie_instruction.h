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
#include "token.h"
#include "dynamic_array/dynamic_array.h"
#include "dynamic_string/dynamic_string.h"
#include "ollie_intermediary_representation.h"
#include "x86_assembly_instruction.h"
#include "x86_genpurpose_registers.h"
#include "x86_sse_registers.h"
#include "three_address_variable.h"
#include "three_address_constant.h"

/**
 * An overall structure for an instruction. Instructions start their life
 * as three address code statements, and eventually become assembly instructions
 */
typedef struct instruction_t instruction_t;

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
	//What kind of memory addressing mode do we have?
	memory_addressing_mode_t addressing_mode;
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
			//Assignee
			three_addr_var_t* assignee;
			/**
			 * We maintain separate variable storage for lea/address
			 * calculation statements. Reminder that these statements
			 * go like(at their maximum): offset(operand1, operand2, multiplier)
			 */
			three_addr_const_t* address_offset;
			three_addr_var_t* address_operand1;
			three_addr_var_t* address_operand2;
			//This can never be anything besides a 64 bit integer
			u_int64_t address_multiplier;
			/**
			 * Some variables are represented as RIP offsets. We will use a special
			 * space here so that they are excluded from the register allocator's
			 * processing
			 */
			three_addr_var_t* rip_offset_var;
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

		} x86;
	} operands;

	//Generic parameter list - could be used for phi functions or function calls
	dynamic_array_t parameters;
	//We have 2 ways to jump. The if jump is our affirmative jump,
	//else is our alternative
	void* if_block;
	void* else_block;
	//Optional variable storage to determine what a value relies on
	three_addr_var_t* relies_on;

	/**
	 * Optional storage values that are not used enough to justify
	 * their own dedicated field
	 */
	union {
		//Store inlined assembly in a string
		dynamic_string_t inlined_assembly;
		//The second error assignee for an errorable function
		three_addr_var_t* error_assignee;
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

	//The function called
	symtab_function_record_t* called_function;
	//What block holds this?
	void* block_contained_in;
	//Instruction's line number
	int32_t line_number;
	//Is this operation critical?
	u_int8_t mark;
	//Is this a regular or inverse branch
	u_int8_t inverse_branch;
	//Do we need to stop this value from being coalesced. This is only used
	//very specifically in the logical not handler currently
	u_int8_t cannot_be_combined;
	//Does this instruction handle callee saving?
	u_int8_t is_callee_saving_instruction;
	//Is this an inlined function call or not?
	u_int8_t is_inlined_call;
	//If it's a branch statment, then we'll use this
	branch_type_t branch_type;
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


#endif /* OLLIE_INSTRUCTION_H */
