/**
 * Author: Jack Robbins
 * This file contains the implementations for APIs defined in the header file simulator.h
 */

#include "simulator.h"
#include <stdint.h>
#include <math.h>
#include <sys/types.h>

//Special search result type for block searching
typedef struct search_result_t search_result_t;

/**
 * When we do a search in a block we can return nothing, a variable
 * or a constant in the end
 */
typedef enum {
	SEARCH_RESULT_TYPE_NONE,
	SEARCH_RESULT_TYPE_VAR,
	SEARCH_RESULT_TYPE_CONST,
} search_result_type_t;


/**
 * The search result is a tagged union that may contain
 * nothing(default), a variable or a constant. It is up
 * to the user to unpack accordingly
 */
struct search_result_t {
	union {
		three_addr_var_t* variable;
		three_addr_const_t* constant;
	} result;
	search_result_type_t result_type;
};


/**
 * Utility for making a blank search result
 */
#define BLANK_SEARCH_RESULT (search_result_t){{NULL}, SEARCH_RESULT_TYPE_NONE}


/**
 * Trigger a fatal internal compiler error panic with the given message
 * 
 * NOTE: THIS WILL CRASH THE PROGRAM DELIBERATELY
 */
static inline void trigger_ice_panic(char* message){
	fprintf(stderr, "Fatal Internal Compiler Error: %s\n", message);
	exit(1);
}


/**
 * Search this entire block only, starting at the start instruction and working our way *BACKWARDS*,
 * until we find what was assigned to equal "variable" or until we hit the start of the block. Based
 * on what the variable was assigned to, we'll return a search result that contains nothing, a constant or
 * a variable. This will be used by the caller in their simulation efforts
 *
 * Example:
 * 	.L5:
 * 		x_1 <- 5 	  <--- we'll eventually find this
 * 		....
 * 		....
 * 		....
 * 		t3 <- x_1 > 3  <--- start searching here
 * 		branch_le .L6 else .L4
 *
 * An important note: we are not seeking to simplify or remove instructions in any way. We just wish to look
 * for where this variable was assigned to see if there's a simple value that we can report back
 */
static search_result_t search_for_variable_assignment_in_block(basic_block_t* block, instruction_t* start_instruction, three_addr_var_t* variable){
	search_result_t search_result = BLANK_SEARCH_RESULT;

	/**
	 * Crawl up the block starting at the provided start instruction
	 */
	instruction_t* cursor = start_instruction;
	while(cursor != NULL){
		/**
		 * If the given variable and the assignee are equal, we *may* be able to
		 * report something if the given instruction is an assignment instruction. If
		 * it's a more complex instruction then we're out of luck here
		 */
		if(variables_equal(variable, cursor->operands.oir.assignee) == TRUE){
			/**
			 * If we have an ASSN statement then we've got some other variable. We
			 * won't try to keep chaining with assignments as that is unlikely to 
			 * work
			 */
			if(cursor->statement_type == THREE_ADDR_CODE_ASSN_STMT){
				search_result.result_type = SEARCH_RESULT_TYPE_VAR;
				search_result.result.variable = cursor->operands.oir.operand1;

			/**
			 * This is what we're really after. An ASSN_CONST statement allows
			 * us to simulate a lot
			 */
			} else if(cursor->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT){
				search_result.result_type = SEARCH_RESULT_TYPE_CONST;
				search_result.result.constant = cursor->operands.oir.constant_operand;

			/**
			 * This is not an ASSN or an ASSN_CONST statement, so we can't do anything
			 * with it. We are not trying to simulate super complex things like loads,
			 * binary operations, etc.. Since we've found it, we can break out the loop
			 * here
			 */
			} else {
				break;
			}
		}

		/**
		 * We go up the block *BACKWARDS*
		 */
		cursor = cursor->previous_statement;
	}

	return search_result;
}


/**
 * Simulate a bitwise AND statement given 2 constants and their type. Bitwise AND 
 * can only ever set the sign flag, zero flag and parity flags
 *
 * NOTE: bitwise AND can also be used to represent the TEST command in this context
 */
static simulated_execution_result_t simulate_bitwise_and_with_constants(u_int64_t raw_constant1, u_int64_t raw_constant2, generic_type_t* operand_type){
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
	 * Perform the bitwise AND and then use the mask to retain only the
	 * bits that we're after
	 */
	u_int64_t result = (raw_constant1 & raw_constant2) & mask;

	/**
	 * Zero flag is set if we're 0
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
	 * GCC __builtin_parity returns 0 if we contain an even number of set
	 * bits, so for our parity flag we'll set it when __builtin_parity returns
	 * 0 on the lowest 8 bits
	 */
	if(__builtin_parity(result & 0xFF) == 0){
		set_parity_flag(&execution_result.flag_register);
	}

	return execution_result;
}


/**
 * F32 comparison via UCOMISS will set the zero flag, parity flag and carry flag as needed. The
 * logic is as follows:
 *
 * 	If constant1 or constant2 is not a number:
 * 		zf = 1
 * 		pf = 1
 * 		cf = 1
 * 	else if constant1 > constant2:
 * 		zf = 0
 * 		pf = 0
 * 		cf = 0
 * 	else if constant1 < constant2:
 * 		zf = 0
 * 		pf = 0
 * 		cf = 1
 * 	else(constant1 == constant2):
 * 		zf = 1
 * 		pf = 0
 * 		cf = 0
 */
static simulated_execution_result_t simulate_f32_constant_comparison(float constant1, float constant2){
	simulated_execution_result_t result = {{0}, TRUE};

	/**
	 * Case 1: if either values is not a number, we set all 3 flags
	 */
	if(isnan(constant1) || isnan(constant2)){
		set_zero_flag(&result.flag_register);
		set_parity_flag(&result.flag_register);
		set_carry_flag(&result.flag_register);

	/**
	 * Case 2: constant1 if more than constant2. In this case no zero flag
	 * and no carry flag will be set. This is because the underlying logic
	 * is subtraction
	 */
	} else if(constant1 > constant2){

	/**
	 * Case 3: constant1 is less than constant2. The subtraction in this
	 * case would have caused the carry flag to be set
	 */
	} else if(constant1 < constant2){
		set_carry_flag(&result.flag_register);
		
	/**
	 * Case 4: constant1 is equal to constant2. The underlying subtraction in this case would
	 * cause the zero flag to be set
	 */
	} else {
		set_zero_flag(&result.flag_register);
	}

	return result;
}


/**
 * F64 comparison via UCOMISD will set the zero flag, parity flag and carry flag as needed. The
 * logic is as follows:
 *
 * 	If constant1 or constant2 is not a number:
 * 		zf = 1
 * 		pf = 1
 * 		cf = 1
 * 	else if constant1 > constant2:
 * 		zf = 0
 * 		pf = 0
 * 		cf = 0
 * 	else if constant1 < constant2:
 * 		zf = 0
 * 		pf = 0
 * 		cf = 1
 * 	else(constant1 == constant2):
 * 		zf = 1
 * 		pf = 0
 * 		cf = 0
 */
static simulated_execution_result_t simulate_f64_constant_comparison(double constant1, double constant2){
	simulated_execution_result_t result = {{0}, TRUE};

	/**
	 * Case 1: if either values is not a number, we set all 3 flags
	 */
	if(isnan(constant1) || isnan(constant2)){
		set_zero_flag(&result.flag_register);
		set_parity_flag(&result.flag_register);
		set_carry_flag(&result.flag_register);

	/**
	 * Case 2: constant1 if more than constant2. In this case no zero flag
	 * and no carry flag will be set. This is because the underlying logic
	 * is subtraction
	 */
	} else if(constant1 > constant2){

	/**
	 * Case 3: constant1 is less than constant2. The subtraction in this
	 * case would have caused the carry flag to be set
	 */
	} else if(constant1 < constant2){
		set_carry_flag(&result.flag_register);
		
	/**
	 * Case 4: constant1 is equal to constant2. The underlying subtraction in this case would
	 * cause the zero flag to be set
	 */
	} else {
		set_zero_flag(&result.flag_register);
	}

	return result;
}


/**
 * Simulate a test if not zero statement. This simulator will decide first if it's even possible to simulate
 * the statement, and then will call out to the appropriate simulation method based on whether or not we're
 * dealing with a floating point or general purpose value
 */
static simulated_execution_result_t simulate_test_if_not_zero_statement(basic_block_t* block, instruction_t* instruction){
	/**
	 * We need to have a constant operand in order for any simulation to work. If we, as
	 * of right now, have a non-constant operand, we'll need to first attempt to find
	 * a constant assignment for the non-constant operand. If we do not then this simulation
	 * is considered impossible
	 */
	three_addr_const_t* constant_operand = NULL;
	generic_type_t* operand_type = NULL;
	if(instruction->operands.oir.operand1 != NULL){
		three_addr_var_t* operand = instruction->operands.oir.operand1;
		operand_type = operand->type;

		/**
		 * Let's see if we can find out what this variable was assigned to. There is a chance that
		 * this was assigned to be a constant. If that's the case, we will be able to simulate this
		 * further
		 */
		search_result_t search_result = search_for_variable_assignment_in_block(block, instruction->previous_statement, operand);

		/**
		 * This is usually what happens - we found nothing or we found a variable assignment. In this case it's not possible
		 * to simulate any further, so we'll return the blank simulation result which tells the caller that we could do
		 * nothing with this
		 */
		if(search_result.result_type == SEARCH_RESULT_TYPE_NONE || search_result.result_type == SEARCH_RESULT_TYPE_VAR){
			return BLANK_SIMULATED_EXECUTION_RESULT;
		}

		/**
		 * If we get here then we know that we've got a constant operand, so we'll assign
		 * this over to the holder for the next step
		 */
		constant_operand = search_result.result.constant;

	/**
	 * We have a constant baked into the given statement - no searching needed
	 */
	} else {
		constant_operand = instruction->operands.oir.constant_operand;
		operand_type = constant_operand->type;
	}

	/**
	 * Case 1: for non-floating point constants, the test command is simply the
	 * "andX" instruction without setting results. This is easy enough to simulate
	 * for us
	 */
	if(IS_FLOATING_POINT(operand_type) == FALSE){
		u_int64_t raw_constant_value = constant_operand->constant_value.unsigned_long_constant;
		return simulate_bitwise_and_with_constants(raw_constant_value, raw_constant_value, operand_type);

	/**
	 * Case 2: for floating point constants, the test if not 0 instruction will be turned into
	 * a UCOMISS/UCOMISSD with 0 as the other operand. We will simulate this accordingly
	 */
	} else {
		/**
		 * This is either an F32 or F64, we will call out to the appropriate handlers
		 * based on the type
		 */
		if(operand_type->basic_type_token == F32){
			float float_value = constant_operand->constant_value.float_constant;
			return simulate_f32_constant_comparison(float_value, 0.0f);
		} else {
			double double_value= constant_operand->constant_value.double_constant;
			return simulate_f32_constant_comparison(double_value, 0.0);
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
			//printf("TODO NOT IMPLEMENTED\n");
			return BLANK_SIMULATED_EXECUTION_RESULT;
		}

		case THREE_ADDR_CODE_LOGICAL_NOT_STMT: {
			//printf("TODO NOT IMPLEMENTED\n");
			return BLANK_SIMULATED_EXECUTION_RESULT;

		}

		case THREE_ADDR_CODE_INC_STMT: {
			//printf("TODO NOT IMPLEMENTED\n");
			return BLANK_SIMULATED_EXECUTION_RESULT;

		}

		case THREE_ADDR_CODE_DEC_STMT: {
			//printf("TODO NOT IMPLEMENTED\n");
			return BLANK_SIMULATED_EXECUTION_RESULT;

		}

		case THREE_ADDR_CODE_BIN_OP_STMT: {
			//printf("TODO NOT IMPLEMENTED\n");
			return BLANK_SIMULATED_EXECUTION_RESULT;

		}

		case THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT: {
			//printf("TODO NOT IMPLEMENTED\n");
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


/**
 * Given the flags at the time of branch execution, determine which branch in the given branch
 * statement we'd jump to. This will always return a valid block, because a branch must always execute
 * in a way that gives one block back
 */
basic_block_t* simulate_branch_execution(instruction_t* branch_statement, RFLAGS_t flags){
	/**
	 * Extract the branch type and create a placeholder for our result
	 */
	branch_type_t branch_type = branch_statement->branch_type;
	basic_block_t* branch_execution_result = NULL;

	switch(branch_type){
		case BRANCH_NE: {
			//TODO

			break;
		}

		case BRANCH_E: {
			//TODO

			break;
		}

		case BRANCH_NZ: {
			//TODO

			break;
		}

		case BRANCH_Z: {
			//TODO

			break;
		}

		case BRANCH_L: {
			//TODO

			break;
		}

		case BRANCH_LE: {
			//TODO

			break;
		}

		case BRANCH_G: {
			//TODO

			break;
		}

		case BRANCH_GE: {
			//TODO

			break;
		}

		case BRANCH_A: {
			//TODO

			break;
		}

		case BRANCH_AE: {
			//TODO

			break;
		}

		case BRANCH_B: {
			//TODO

			break;
		}

		case BRANCH_BE: {
			//TODO

			break;
		}


		case NO_BRANCH: {
			trigger_ice_panic("Invalid branch type of NO_BRANCH encountered in simulator");
			break;
		}
	}

	return branch_execution_result;
}
