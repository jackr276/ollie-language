/**
 * Author: Jack Robbins
 * This file contains the implementations for APIs defined in the header file simulator.h
 */

#include "simulator.h"

/**
 * Simulate the execution of the given instruction. The simulation will not return the actual value result, instead
 * we will return a struct that contains a boolean noting whether we even could simulate this, and a flag register
 * that represents the state of RFLAGS after this instruction executes
 *
 * There is no guarantee that an instruction passed into this simulator can be simulated
 */
simulated_execution_result_t simulate_instruction_execution(basic_block_t* block, instruction_t* instruction){
	switch(instruction->statement_type){
		case THREE_ADDR_CODE_TEST_IF_NOT_ZERO_STMT:

		case THREE_ADDR_CODE_BITWISE_NOT_STMT:

		case THREE_ADDR_CODE_LOGICAL_NOT_STMT:

		case THREE_ADDR_CODE_INC_STMT:

		case THREE_ADDR_CODE_DEC_STMT:

		case THREE_ADDR_CODE_BIN_OP_STMT:

		case THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT:

		/**
		 * By default we weren't able to simulate anything, so kickback
		 * a 0'd out flags register with a FALSE flag
		 */
		default: {
			return (simulated_execution_result_t){0, FALSE};
		}
	}
}



