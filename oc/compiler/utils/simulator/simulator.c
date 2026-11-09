/**
 * Author: Jack Robbins
 * This file contains the implementations for APIs defined in the header file simulator.h
 */

#include "simulator.h"
#include <stdint.h>
#include <sys/types.h>


/**
 * Simulate a bitwise AND statement given 2 constants and their type. Bitwise AND 
 * can only ever set the sign flag, zero flag and parity flags
 *
 * NOTE: bitwise AND can also be used to represent the TEST command in this context
 */
static inline simulated_execution_result_t simulate_bitwise_and_with_constants(u_int64_t raw_constant1, u_int64_t raw_constant2, generic_type_t* operand_type){
	/**
	 * Initialize our results. If we get here then we know that this is going to work
	 */
	simulated_execution_result_t execution_result = {{0}, TRUE};

	/**
	 * Get the actual operand bit width and then create an appropriate mask
	 */
	u_int64_t operand_bit_width = operand_type->type_size * 8;
	u_int64_t mask = UINT64_MAX >> (64 - operand_bit_width);

	/**
	 * Extract the 64 bit version of the constant value just so we have anything. The
	 * size does not matter here for this. To get the result, we'll populate the constant
	 * value with itself
	 */
	u_int64_t result = (raw_constant1 & raw_constant2) & mask;

	/**
	 * The carry flag, overflow flag and auxiliary carry flag remain complete unset
	 * here. We need to worry about setting the sign flag, overflow flag and parity flag
	 */
	if(result == 0){
		set_zero_flag(&execution_result.flag_register);
	}

	/**
	 * Set sign flag if the sign bit is 1
	 */
	if(((result >> (operand_bit_width - 1)) & 0x01) == 1){
		set_sign_flag(&execution_result.flag_register);
	}

	/**
	 * Set the parity flag when the lowest 8 bits of the result contain an
	 * even number of set bits. This is how the x86 parity flag is set. The
	 * GCC __builtin_parity returns 1 if we contain an even number of set
	 * bits, so we'll need to invert it
	 */
	if(__builtin_parity(result & 0xFF) == 0){
		set_parity_flag(&execution_result.flag_register);
	}

	return execution_result;
}


/**
 * Simulate a test if not zero statement
 */
static simulated_execution_result_t simulate_test_if_not_zero_statement(basic_block_t* block, instruction_t* instruction){
	/**
	 * Initialize a blank result pack to start
	 */
	simulated_execution_result_t results = BLANK_SIMULATED_EXECUTION_RESULT;

	/**
	 * Case 1: we have a variable operand1. This means that we do not have a constant
	 * operand as what we're testing so we'll need to do some block traversal to even
	 * potentially determine what this is
	 */
	if(instruction->operands.oir.operand1 != NULL){
		printf("TODO NOT IMPLEMENTED\n");
		exit(1);

	/**
	 * Case 2: we have a constant operand in this case. This means that we can for sure simulate
	 * the execution result without any searching at all
	 */
	} else {
		/**
		 * We know for a fact that this will work
		 */
		results.simulation_possible = TRUE;
		generic_type_t* operand_type = instruction->operands.oir.constant_operand->type;

		/**
		 * Case 2.1: for non-floating point constants, the test command is simply the
		 * "andX" instruction without setting results. This is easy enough to simulate
		 * for us
		 */
		if(IS_FLOATING_POINT(operand_type) == FALSE){
			u_int64_t raw_constant_value = instruction->operands.oir.constant_operand->constant_value.unsigned_long_constant;
			return simulate_bitwise_and_with_constants(raw_constant_value, raw_constant_value, operand_type);

		} else {
			printf("TODO NOT IMPLEMENTED\n");
			exit(1);

			return results;
		}
	}
}


/**
 * Simulate the execution of the given instruction. The simulation will not return the actual value result, instead
 * we will return a struct that contains a boolean noting whether we even could simulate this, and a flag register
 * that represents the state of RFLAGS after this instruction executes
 *
 * There is no guarantee that an instruction passed into this simulator can be simulated
 */
simulated_execution_result_t simulate_instruction_execution(basic_block_t* block, instruction_t* instruction){
	/**
	 * We will go based on the statement type and make call outs as appropriate. Note
	 * that not all statements can be simulated. If it's not in our predetermined list
	 * we will return a simulation failure
	 */
	switch(instruction->statement_type){
		case THREE_ADDR_CODE_TEST_IF_NOT_ZERO_STMT: {
			return simulate_test_if_not_zero_statement(block, instruction);
		}

		case THREE_ADDR_CODE_BITWISE_NOT_STMT: {
			printf("TODO NOT IMPLEMENTED\n");
			return BLANK_SIMULATED_EXECUTION_RESULT;
		}

		case THREE_ADDR_CODE_LOGICAL_NOT_STMT: {
			printf("TODO NOT IMPLEMENTED\n");
			return BLANK_SIMULATED_EXECUTION_RESULT;

		}

		case THREE_ADDR_CODE_INC_STMT: {
			printf("TODO NOT IMPLEMENTED\n");
			return BLANK_SIMULATED_EXECUTION_RESULT;

		}

		case THREE_ADDR_CODE_DEC_STMT: {
			printf("TODO NOT IMPLEMENTED\n");
			return BLANK_SIMULATED_EXECUTION_RESULT;

		}

		case THREE_ADDR_CODE_BIN_OP_STMT: {
			printf("TODO NOT IMPLEMENTED\n");
			return BLANK_SIMULATED_EXECUTION_RESULT;

		}

		case THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT: {
			printf("TODO NOT IMPLEMENTED\n");
			return BLANK_SIMULATED_EXECUTION_RESULT;

		}

		/**
		 * By default we aren't able to simulate anything, so kickback a blank
		 * simulated execution result
		 */
		default: {
			return BLANK_SIMULATED_EXECUTION_RESULT;
		}
	}
}



