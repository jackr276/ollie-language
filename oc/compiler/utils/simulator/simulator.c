/**
 * Author: Jack Robbins
 * This file contains the implementations for APIs defined in the header file simulator.h
 */

#include "simulator.h"

/**
 * Simulate a test if not zero statement
 */
static simulated_execution_result_t simulate_test_if_not_zero_statement(basic_block_t* block, instruction_t* instruction){
	/**
	 * Case 1: we have a variable operand1. This means that we do not have a constant
	 * operand as what we're testing so we'll need to do some block traversal to even
	 * potentially determine what this is
	 */
	if(instruction->operands.oir.operand1 != NULL){

	/**
	 * Case 2: we have a constant operand in this case. This means that we can for sure simulate
	 * the execution result without any searching at all
	 */
	} else {

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



