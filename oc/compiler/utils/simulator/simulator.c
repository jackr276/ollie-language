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
			 * TODO IF WE HAVE A LOCAL CONSTANT LOAD FOR A FLOAT THAT SHOULD WORK
			 */

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
 * Simulate the subtraction of 2 constants. We will always do raw_constant1 - raw_constant2
 *
 * Integer subtraction(subX) can set: CF, OF, SF, ZF, AF, PF
 */
static simulated_execution_result_t simulate_subtraction_with_constants(u_int64_t raw_constant1, u_int64_t raw_constant2, generic_type_t* operand_type){
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
	 * Get a sign bit mask that has 1 where the sign bit would be
	 */
	u_int64_t sign_bit_mask = (u_int64_t)(1) << (operand_bit_width - 1);

	/**
	 * Cut down both operands to be mask sized
	 */
	raw_constant1 &= mask;
	raw_constant2 &= mask;

	/**
	 * Perform the subtraction and then use the mask to retain only
	 * what we're after
	 */
	u_int64_t result = (raw_constant1 - raw_constant2) & mask;

	//TODO BREAK OUT WHAT WE CAN INTO DIFFERENT FUNCTIONS AS HELPERS


	/**
	 * Unsigned overflow occurs when the first operand is less than the second
	 */
	if(raw_constant1 < raw_constant2){
		set_carry_flag(&execution_result.flag_register);
	}

	/**
	 * Signed overflow occurs when the operands have different signs and the
	 * result's sign differs from A
	 *
	 * raw_constant1 ^ raw_constant2 = sign bit is 1 if they have different signs
	 * raw_constant1 ^ result = sign bit is 1 if they have different signs
	 *
	 * (raw_constant1 ^ raw_constant2) & (raw_constant1 ^ result) & sign_bit_mask 
	 * This is nonzero when the two operands have different signs, and the result's sign
	 * differs from the first constant, meaning that a signed overflow has occurred
	 */
	if(((raw_constant1 ^ raw_constant2) & (raw_constant1 ^ result) & sign_bit_mask) != 0){
		set_overflow_flag(&execution_result.flag_register);
	}

	/**
	 * Zero flag is set if we're 0
	 */
	if(result == 0){
		set_zero_flag(&execution_result.flag_register);
	}

	/**
	 * Set sign flag if the sign bit is 1. We can figure this
	 * out be bitwise ANDing with the sign bit
	 */
	if((result & sign_bit_mask) != 0){
		set_sign_flag(&execution_result.flag_register);
	}

	/**
	 * Set auxiliary carry when we had to borrow from bit 4. This will only
	 * happen when raw_constant1 ^ raw_constant2 ^ result is nonzero TODO
	 * DOC WHY WITH EXAMPLE
	 */
	if(((raw_constant1 ^ raw_constant2 ^ result) & 0x10) != 0){
		set_auxiliary_carry_flag(&execution_result.flag_register);
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
 * Simulate a bitwise AND statement given 2 constants and their type
 *
 * Bitwise AND can set: SF, ZF, PF
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
	 * TODO REDO SIGN BIT
	 *
	 *
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
 * Simulate a bitwise OR statement given 2 constants and their type
 *
 * Bitwise OR can set: SF, ZF, PF
 */
static simulated_execution_result_t simulate_bitwise_or_with_constants(u_int64_t raw_constant1, u_int64_t raw_constant2, generic_type_t* operand_type){
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
	 * Perform the bitwise OR and then use the mask to retain only the
	 * bits that we're after
	 */
	u_int64_t result = (raw_constant1 | raw_constant2) & mask;

	/**
	 * Zero flag is set if we're 0
	 */
	if(result == 0){
		set_zero_flag(&execution_result.flag_register);
	}

	/**
	 * TODO REDO SIGN BIT
	 *
	 *
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
 * Simulate a bitwise XOR statement given 2 constants and their type
 *
 * Bitwise XOR can set: SF, ZF, PF
 */
static simulated_execution_result_t simulate_bitwise_xor_with_constants(u_int64_t raw_constant1, u_int64_t raw_constant2, generic_type_t* operand_type){
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
	 * Perform the bitwise XOR and then use the mask to retain only the
	 * bits that we're after
	 */
	u_int64_t result = (raw_constant1 ^ raw_constant2) & mask;

	/**
	 * Zero flag is set if we're 0
	 */
	if(result == 0){
		set_zero_flag(&execution_result.flag_register);
	}

	/**
	 * TODO REDO SIGN BIT
	 *
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
		if(search_result.result_type != SEARCH_RESULT_TYPE_CONST){
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
 * Simulate a logical not statement if at all possible. This simulation is going
 * to be different for floats/non float operations. In the event that no simulation
 * is possible, this statement will return a blank simulated execution result that 
 * tells the caller to move along without simulating
 *
 * NOTE: even though we call this logical not, in reality all branches with logical
 * not are just checking to see if the value is zero or not. As such, we really
 * only need to check if we have a zero here
 */
simulated_execution_result_t simulate_logical_not_statement(basic_block_t* block, instruction_t* instruction){
	/**
	 * First extract the logical not operand and search for it's assignment
	 * within the block
	 */
	three_addr_var_t* logical_not_operand = instruction->operands.oir.operand1;
	generic_type_t* operand_type = logical_not_operand->type;
	search_result_t search_result = search_for_variable_assignment_in_block(block, instruction, logical_not_operand);

	/**
	 * This is usually what happens - we found nothing or we found a variable assignment. In this case it's not possible
	 * to simulate any further, so we'll return the blank simulation result which tells the caller that we could do
	 * nothing with this
	 */
	if(search_result.result_type != SEARCH_RESULT_TYPE_CONST){
		return BLANK_SIMULATED_EXECUTION_RESULT;
	}

	/**
	 * Extract the constant operand and go from here
	 */
	three_addr_const_t* constant_operand = search_result.result.constant;

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
 * Simulate a negation statement in OIR. Negation statements operate differently if they're 
 * operating over floats or integers. Only negation statements over integers will be considered 
 * for this simulation
 *
 * NOTE: floating point negation should never actually be used to set flags. In the event that 
 * we encounter a floating point negation that is supposedly setting flags here, we will panic
 */
static simulated_execution_result_t simulate_negation_statement(basic_block_t* block, instruction_t* instruction){
	/**
	 * First extract the negation operand and its type. In the event that we have
	 * a floating point operand we will panic
	 */
	three_addr_var_t* logical_not_operand = instruction->operands.oir.operand1;
	generic_type_t* operand_type = logical_not_operand->type;
	if(IS_FLOATING_POINT(operand_type) == TRUE){
		trigger_ice_panic("Floating point negation has been marked for use as a flag setting statement");
	}

	/**
	 * Attempt to find the variable's assignment inside of the block
	 */
	search_result_t search_result = search_for_variable_assignment_in_block(block, instruction, logical_not_operand);
	if(search_result.result_type != SEARCH_RESULT_TYPE_CONST){
		return BLANK_SIMULATED_EXECUTION_RESULT;
	}

	/**
	 * If we get here we know that the execution result is a pure constant and it's not a floating point
	 * value. In this case we are able to simulate the execution of the negation. Integer negation
	 * is in reality equivalent to doing "0 - x", so that is what we will simulate for our flags
	 */
	//TODO
	return BLANK_SIMULATED_EXECUTION_RESULT;
}


static simulated_execution_result_t simulate_binary_operation_statement(basic_block_t* block, instruction_t* instruction){
	//TODO
	return BLANK_SIMULATED_EXECUTION_RESULT;
}


static simulated_execution_result_t simulate_binary_operation_with_constant_statement(basic_block_t* block, instruction_t* instruction){
	//TODO
	return BLANK_SIMULATED_EXECUTION_RESULT;
}


/**
 * Simulate the flags of the given instruction after execution. For this, we really don't care about the actual
 * value of the result variable. Rather, since these instructions are being used for conditional execution, what
 * we really care about is the state of the flags register after they run
 *
 * There is no guarantee that an instruction passed into this simulator can be simulated
 */
simulated_execution_result_t simulate_instruction_flags_after_execution(basic_block_t* block, instruction_t* instruction){
	/**
	 * We will go based on the statement type and make call outs as appropriate. Note
	 * that not all statements can be simulated. If it's not in our predetermined list
	 * we will return a simulation failure
	 */
	switch(instruction->statement_type){
		case THREE_ADDR_CODE_TEST_IF_NOT_ZERO_STMT: {
			return simulate_test_if_not_zero_statement(block, instruction);
		}

		case THREE_ADDR_CODE_LOGICAL_NOT_STMT: {
			return simulate_logical_not_statement(block, instruction);
		}

		case THREE_ADDR_CODE_NEG_STATEMENT: {
			return simulate_negation_statement(block, instruction);
		}

		case THREE_ADDR_CODE_BIN_OP_STMT: {
			return simulate_binary_operation_statement(block, instruction);
		}

		case THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT: {
			return simulate_binary_operation_with_constant_statement(block, instruction);
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
 * For most branches, we will not need to consider the parity flag during our execution of it. This 
 * helper function does simulation for branches of that kind. Since there is no parity flag consideration,
 * the branch will execute the same way regardless of if its an inverse or regular branch. This function
 * always returns the block that the branch would take given the flags
 */
static inline basic_block_t* simulate_branch_without_parity_consideration(instruction_t* branch_statement, RFLAGS_t flags){
	/**
	 * Get these two blocks out for easy use
	 */
	basic_block_t* if_block = branch_statement->branch_info.if_block;
	basic_block_t* else_block = branch_statement->branch_info.else_block;

	/**
	 * Based on the branch type and flags exclusively we are able to
	 * execute this branch
	 */
	branch_type_t branch_type = branch_statement->branch_info.branch_type;
	switch(branch_type){
		/**
		 * Branch if not equal looks at the zero flag exclusively
		 *
		 * Condition to jump to physical if block: ZF == 0
		 */
		case BRANCH_NE: {
			if(get_zero_flag(&flags) == 0){
				return if_block;
			} else {
				return else_block;
			}
		}

		/**
		 * Branch if equal looks at the zero flag exclusively
		 *
		 * Condition to jump to physical if block: ZF == 1
		 */
		case BRANCH_E: {
			if(get_zero_flag(&flags) == 1){
				return if_block;
			} else {
				return else_block;
			}
		}

		/**
		 * Branch if not zero looks at the zero flag exclusively
		 *
		 * Condition to jump to physical if block: ZF == 0
		 */
		case BRANCH_NZ: {
			if(get_zero_flag(&flags) == 0){
				return if_block;
			} else {
				return else_block;
			}
		}

		/**
		 * Branch if zero looks at the zero flag exclusively
		 *
		 * Condition to jump to physical if block: ZF == 1
		 */
		case BRANCH_Z: {
			if(get_zero_flag(&flags) == 1){
				return if_block;
			} else {
				return else_block;
			}
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


	return NULL;
}


/**
 * TODO DOC
 */
static inline basic_block_t* simulate_branch_with_parity_consideration(instruction_t* branch_statement, RFLAGS_t flags){
	printf("TODO NOT DONE YET\n");
	exit(1);

	return NULL;
}


/**
 * Given the flags at the time of branch execution, determine which branch in the given branch
 * statement we'd jump to. This will always return a valid block, because a branch must always execute
 * in a way that gives one block back
 */
basic_block_t* simulate_branch_execution_given_flags(instruction_t* branch_statement, u_int8_t comes_from_fp_comparison, RFLAGS_t flags){
	/**
	 * We will execute the branch differently based on whether or not we're
	 * branching based on a fp comparison
	 */
	if(comes_from_fp_comparison == FALSE){
		return simulate_branch_without_parity_consideration(branch_statement, flags);
	} else {
		return simulate_branch_with_parity_consideration(branch_statement, flags);
	}
}
