/**
 * Author: Jack Robbins
 * This file contains the implementations for APIs defined in the header file simulator.h
 */

#include "simulator.h"
#include "../../instruction/instruction.h"
#include <math.h>
#include <sys/types.h>

/**
 * For storing any compiler errors that we print out
 */
static char compiler_error_info[ERROR_SIZE * 3];


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



static inline search_result_t search_for_constant_in_addressing_expression(addressing_operands_t* operands){

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
static search_result_t search_for_variable_assignment_in_block(instruction_t* start_instruction, three_addr_var_t* variable){
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
			switch(cursor->statement_type){
				/**
				 * This is what we're really after. An ASSN_CONST statement allows
				 * us to simulate a lot
				 */
				case THREE_ADDR_CODE_ASSN_CONST_STMT: {
					search_result.result_type = SEARCH_RESULT_TYPE_CONST;
					search_result.result.constant = cursor->operands.oir.constant_operand;
					return search_result;
				}

				/**
				 * If we have an ASSN statement then we've got some other variable. We
				 * won't try to keep chaining with assignments as that is unlikely to 
				 * work
				 */
				case THREE_ADDR_CODE_ASSN_STMT: {
					search_result.result_type = SEARCH_RESULT_TYPE_VAR;
					search_result.result.variable = cursor->operands.oir.operand1;
					return search_result;
				}

				/**
				 * By the time we try to simulate instructions, all floating point constants
				 * will have been removed from raw OIR and replaced with local constant load
				 * statements. In the event that we encounter one of these, it's easy for
				 * us to claw that value back out of the local constant load
				 */
				case THREE_ADDR_CODE_LOAD_STATEMENT: {
					/**
					 * If it's not rip-relative then this is something else and we can't use it at all
					 */
					if(cursor->operands.oir.source_address_expression.addressing_mode != ADDRESSING_MODE_RIP_RELATIVE){
						return search_result;
					}

					/**
					 * Extract the rip offset variable. If it's not stamped explicitly as a local constant then
					 * we can't use this and we'll move along
					 */
					three_addr_var_t* local_constant_var = cursor->operands.oir.source_address_expression.rip_offset_var;
					if(local_constant_var->variable_type != VARIABLE_TYPE_LOCAL_CONSTANT){
						return search_result;
					}

					/**
					 * Extract the local constant. If it's not a floating point type then this is useless
					 * and we fail out
					 */
					local_constant_t* local_constant = local_constant_var->associated_memory_region.local_constant;
					if(IS_FLOATING_POINT(local_constant->type) == FALSE){
						return search_result;
					}

					/**
					 * Get the value out of here. The float values have had their bits stored inside of u_int32_t/u_int64_t
					 * bitmaps, but it's easy enough to get them out with some address manipulation
					 */
					three_addr_const_t* final_constant = NULL;
					if(local_constant->type->basic_type_token == F32){
						float float_value = *((float*)(&(local_constant->local_constant_value.f32_bit_equivalent)));
						final_constant = emit_f32_constant(float_value, local_constant->type);
					} else {
						double double_value = *((double*)(&(local_constant->local_constant_value.f64_bit_equivalent)));
						final_constant = emit_f64_constant(double_value, local_constant->type);
					}

					/**
					 * Package up and return the final result
					 */
					search_result.result_type = SEARCH_RESULT_TYPE_CONST;
					search_result.result.constant = final_constant;
					return search_result;
				}

				/**
				 * This is not an ASSN or an ASSN_CONST statement, so we can't do anything
				 * with it. We are not trying to simulate super complex things like loads,
				 * binary operations, etc.. Since we've found it, we can break out the loop
				 * here
				 */
				default: {
					return search_result;
				}
			}
		}

		/**
		 * We go up the block *BACKWARDS*
		 */
		cursor = cursor->previous_statement;
	}

	return search_result;
}

//======================================= Constant Operation Helpers ========================================
/**
 * Simulate the subtraction of 2 *INTEGER* constants. We will always do raw_constant1 - raw_constant2
 *
 * Integer subtraction(subX) will set: CF, OF, SF, ZF, AF, PF
 */
static simulated_execution_result_t simulate_flags_from_subtraction_with_constants(u_int64_t raw_constant1, u_int64_t raw_constant2, generic_type_t* operand_type){
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
	u_int64_t sign_bit_mask = (u_int64_t)(1UL) << (operand_bit_width - 1);

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

	/**
	 * CARRY FLAG
	 *
	 * Unsigned overflow occurs when the first operand is less than the second
	 */
	if(raw_constant1 < raw_constant2){
		set_carry_flag(&execution_result.flag_register);
	}

	/**
	 * OVERFLOW FLAG
	 *
	 * Signed overflow occurs when the operands have different signs and the
	 * result's sign differs from A, signifying that we have gone from negative
	 * to positive or positive to negative
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
	 * SIGN FLAG
	 *
	 * Set sign flag if the sign bit is 1. We can figure this
	 * out be bitwise ANDing with the sign bit
	 */
	if((result & sign_bit_mask) != 0){
		set_sign_flag(&execution_result.flag_register);
	}

	/**
	 * ZERO FLAG
	 *
	 * Zero flag is set if we're 0
	 */
	if(result == 0){
		set_zero_flag(&execution_result.flag_register);
	}

	/**
	 * AUXILIARY CARRY FLAG
	 *
	 * The auxiliary carry flag is set when we had to perform a borrow
	 * from bit 4 into bit 3. If we did this, we'll see a a 1 inside of 
	 * the 4th bit
	 *
	 * Example: 16 - 5 = 11
	 * 		   0001 0000
	 * 		 - 0000 0101
	 * 		 = 0000 1111
	 *
	 * In this case we had to borrow from bit 4. To identify this in a convenient
	 * bitwise expression, we can do the following.
	 * 		1.) The value in bit 4 for the result(R4) is A4 ^ B4 for subtraction with *NO* carry
	 * 		2.) If there is a carry into bit 4 then R4 = A4 ^ B4 ^ 1. Therefore
	 * 			due to the properties of XOR, we can rearrange to get 1 = R4 ^ A4 ^ B4
	 */
	if(((raw_constant1 ^ raw_constant2 ^ result) & 0x10) != 0){
		set_auxiliary_carry_flag(&execution_result.flag_register);
	}

	/**
	 * PARITY FLAG
	 *
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
 * Simulate the addition of 2 *INTEGER* constants. We will always do raw_constant1 + raw_constant2
 *
 * Integer addition(addX) will set: CF, OF, SF, ZF, AF, PF
 */
static simulated_execution_result_t simulate_flags_from_addition_with_constants(u_int64_t raw_constant1, u_int64_t raw_constant2, generic_type_t* operand_type){
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
	u_int64_t sign_bit_mask = (u_int64_t)(1UL) << (operand_bit_width - 1);

	/**
	 * Cut down both operands to be mask sized
	 */
	raw_constant1 &= mask;
	raw_constant2 &= mask;

	/**
	 * Perform the addition and then use the mask to retain only
	 * what we're after
	 */
	u_int64_t result = (raw_constant1 + raw_constant2) & mask;

	/**
	 * CARRY FLAG
	 *
	 * Unsigned overflow occurs when the result is smaller than the first operand,
	 * signifying that an overflow occurred
	 */
	if(result < raw_constant1){
		set_carry_flag(&execution_result.flag_register);
	}

	/**
	 * OVERFLOW FLAG
	 *
	 * Signed overflow occurs when the operators have the same sign originally but
	 * the result has a different sign
	 *
	 * ~(raw_constant1 ^ raw_constant2) -> MSB is 1 if both operands have the same sign
	 * (raw_constant1 ^ result) -> MSB is 1 if the result and first constant have different signs
	 *
	 * If we AND these together, the MSB will be 1 only if the two original operands had the same
	 * sign, but the result had a different sign, signifying a signed overflow
	 */
	if(((~(raw_constant1 ^ raw_constant2)) & (raw_constant1 ^ result) & sign_bit_mask) != 0){
		set_overflow_flag(&execution_result.flag_register);
	}

	/**
	 * SIGN FLAG
	 *
	 * Set sign flag if the sign bit is 1. We can figure this
	 * out be bitwise ANDing with the sign bit
	 */
	if((result & sign_bit_mask) != 0){
		set_sign_flag(&execution_result.flag_register);
	}

	/**
	 * ZERO FLAG
	 *
	 * Zero flag is set if we're 0
	 */
	if(result == 0){
		set_zero_flag(&execution_result.flag_register);
	}

	/**
	 * AUXILIARY CARRY FLAG
	 *
	 * The auxiliary carry flag is set when we had to perform a borrow
	 * from bit 4 into bit 3. If we did this, we'll see a a 1 inside of 
	 * the 4th bit
	 *
	 * Example: 16 - 5 = 11
	 * 		   0001 0000
	 * 		 - 0000 0101
	 * 		 = 0000 1111
	 *
	 * In this case we had to borrow from bit 4. To identify this in a convenient
	 * bitwise expression, we can do the following.
	 * 		1.) The value in bit 4 for the result(R4) is A4 ^ B4 for subtraction with *NO* carry
	 * 		2.) If there is a carry into bit 4 then R4 = A4 ^ B4 ^ 1. Therefore
	 * 			due to the properties of XOR, we can rearrange to get 1 = R4 ^ A4 ^ B4
	 */
	if(((raw_constant1 ^ raw_constant2 ^ result) & 0x10) != 0){
		set_auxiliary_carry_flag(&execution_result.flag_register);
	}

	/**
	 * PARITY FLAG
	 *
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
 * Bitwise AND will set: SF, ZF, PF
 *
 * NOTE: bitwise AND can also be used to represent the TEST command in this context
 */
static simulated_execution_result_t simulate_flags_from_bitwise_and_with_constants(u_int64_t raw_constant1, u_int64_t raw_constant2, generic_type_t* operand_type){
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
	u_int64_t sign_bit_mask = (u_int64_t)(1UL) << (operand_bit_width - 1);

	/**
	 * Cut down both operands to be mask sized
	 */
	raw_constant1 &= mask;
	raw_constant2 &= mask;

	/**
	 * Perform the bitwise AND and then use the mask to retain only
	 * what we're after
	 */
	u_int64_t result = (raw_constant1 & raw_constant2) & mask;

	/**
	 * SIGN FLAG
	 *
	 *
	 * Sign flag is set if the sign bit is set. We'll use the
	 * mask to get this
	 */
	if((result & sign_bit_mask) != 0){
		set_sign_flag(&execution_result.flag_register);
	}

	/**
	 * ZERO FLAG
	 *
	 * Zero flag is set if we're 0
	 */
	if(result == 0){
		set_zero_flag(&execution_result.flag_register);
	}

	/**
	 * PARITY FLAG
	 *
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
 * Bitwise OR will set: SF, ZF, PF
 */
static simulated_execution_result_t simulate_flags_from_bitwise_or_with_constants(u_int64_t raw_constant1, u_int64_t raw_constant2, generic_type_t* operand_type){
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
	u_int64_t sign_bit_mask = (u_int64_t)(1UL) << (operand_bit_width - 1);

	/**
	 * Cut down both operands to be mask sized
	 */
	raw_constant1 &= mask;
	raw_constant2 &= mask;

	/**
	 * Perform the bitwise OR and then use the mask to retain only
	 * what we're after
	 */
	u_int64_t result = (raw_constant1 | raw_constant2) & mask;

	/**
	 * SIGN FLAG
	 *
	 * Sign flag is set if the sign bit is set. We'll use the
	 * mask to get this
	 */
	if((result & sign_bit_mask) != 0){
		set_sign_flag(&execution_result.flag_register);
	}

	/**
	 * ZERO FLAG
	 *
	 * Zero flag is set if we're 0
	 */
	if(result == 0){
		set_zero_flag(&execution_result.flag_register);
	}

	/**
	 * PARITY FLAG
	 *
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
 * Bitwise XOR will set: SF, ZF, PF
 */
static simulated_execution_result_t simulate_flags_from_bitwise_xor_with_constants(u_int64_t raw_constant1, u_int64_t raw_constant2, generic_type_t* operand_type){
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
	u_int64_t sign_bit_mask = (u_int64_t)(1UL) << (operand_bit_width - 1);

	/**
	 * Cut down both operands to be mask sized
	 */
	raw_constant1 &= mask;
	raw_constant2 &= mask;

	/**
	 * Perform the bitwise XOR and then use the mask to retain only
	 * what we're after
	 */
	u_int64_t result = (raw_constant1 ^ raw_constant2) & mask;

	/**
	 * SIGN FLAG
	 *
	 * Sign flag is set if the sign bit is set. We'll use the
	 * mask to get this
	 */
	if((result & sign_bit_mask) != 0){
		set_sign_flag(&execution_result.flag_register);
	}

	/**
	 * ZERO FLAG
	 *
	 * Zero flag is set if we're 0
	 */
	if(result == 0){
		set_zero_flag(&execution_result.flag_register);
	}

	/**
	 * PARITY FLAG
	 *
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
static simulated_execution_result_t simulate_flags_from_f32_constant_comparison(float constant1, float constant2){
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
static simulated_execution_result_t simulate_flags_from_f64_constant_comparison(double constant1, double constant2){
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

//======================================= Constant Operation Helpers ========================================

/**
 * Simulate a test if not zero statement. This simulator will decide first if it's even possible to simulate
 * the statement, and then will call out to the appropriate simulation method based on whether or not we're
 * dealing with a floating point or general purpose value
 */
static simulated_execution_result_t simulate_test_if_not_zero_flags_after_execution(instruction_t* instruction){
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
		search_result_t search_result = search_for_variable_assignment_in_block(instruction->previous_statement, operand);

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
		return simulate_flags_from_bitwise_and_with_constants(raw_constant_value, raw_constant_value, operand_type);

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
			return simulate_flags_from_f32_constant_comparison(float_value, 0.0f);
		} else {
			double double_value= constant_operand->constant_value.double_constant;
			return simulate_flags_from_f64_constant_comparison(double_value, 0.0);
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
simulated_execution_result_t simulate_logical_not_flags_after_execution(instruction_t* instruction){
	/**
	 * First extract the logical not operand and search for it's assignment
	 * within the block
	 */
	three_addr_var_t* logical_not_operand = instruction->operands.oir.operand1;
	generic_type_t* operand_type = logical_not_operand->type;
	search_result_t search_result = search_for_variable_assignment_in_block(instruction, logical_not_operand);

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
		return simulate_flags_from_bitwise_and_with_constants(raw_constant_value, raw_constant_value, operand_type);

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
			return simulate_flags_from_f32_constant_comparison(float_value, 0.0f);
		} else {
			double double_value= constant_operand->constant_value.double_constant;
			return simulate_flags_from_f64_constant_comparison(double_value, 0.0);
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
static simulated_execution_result_t simulate_negation_flags_after_execution(instruction_t* instruction){
	/**
	 * First extract the negation operand and its type. In the event that we have
	 * a floating point operand we will panic
	 */
	three_addr_var_t* logical_not_operand = instruction->operands.oir.operand1;
	generic_type_t* operand_type = logical_not_operand->type;
	if(IS_FLOATING_POINT(operand_type) == TRUE){
		trigger_ice_panic("Floating point negation has been invalidly marked for use as a flag setting statement");
	}

	/**
	 * Attempt to find the variable's assignment inside of the block
	 */
	search_result_t search_result = search_for_variable_assignment_in_block(instruction, logical_not_operand);
	if(search_result.result_type != SEARCH_RESULT_TYPE_CONST){
		return BLANK_SIMULATED_EXECUTION_RESULT;
	}

	/**
	 * If we get here we know that the execution result is a pure constant and it's not a floating point
	 * value. In this case we are able to simulate the execution of the negation. Integer negation
	 * is in reality equivalent to doing "0 - x", so that is what we will simulate for our flags
	 */
	return simulate_flags_from_subtraction_with_constants(0UL, search_result.result.constant->constant_value.unsigned_long_constant, operand_type);
}


/**
 * For binary operation instructions, we store their overall "destination type" inside of the "result_type" field.
 * However, due to legacy implementations, this field is not always going to be populated. This special unpacker
 * function here will contain all the logic for every kind of binary operation to unpack and return that field
 */
static inline generic_type_t* get_destination_type_for_binary_operation_instruction(instruction_t* instruction){
	//First thing we do is default to using the type storage field
	generic_type_t* destination_type = instruction->type_storage.result_type;

	/**
	 * If that ended up being NULL, now we'll have to go into our specific
	 * decision logic based on what kind of binary operation it is
	 */
	if(destination_type == NULL){
		switch(instruction->op){
			case PLUS:
			case MINUS:
			case L_SHIFT:
			case R_SHIFT:
			case SINGLE_OR:
			case SINGLE_AND:
			case CARROT:
			case MOD:
			case STAR:
			case F_SLASH: {
				destination_type = instruction->operands.oir.assignee->type;
				break;
			}

			/**
			 * Relational operators follow a different pattern than above. We still
			 * attempt to get the result type from the type storage, but if we cannot
			 * find it, then we have to rely on a special helper to get the operand
			 * type for a logical operation. Still this relies on acutal three address
			 * variables, and if we don't have those, then we have to resort to exclusively
			 * using the op1 type
			 */
			case L_THAN:
			case G_THAN:
			case L_THAN_OR_EQ:
			case G_THAN_OR_EQ:
			case DOUBLE_EQUALS:
			case NOT_EQUALS: {
				destination_type = instruction->operands.oir.operand1->type;
				break;
			}

			/**
			 * For logical and/or, we will be grabbing the *assignee* type if we are unable to determine the
			 * destination type from the type storage field on the instruction
			 */
			case DOUBLE_AND:
			case DOUBLE_OR: {
				destination_type = instruction->operands.oir.assignee->type;
				break;
			}

			default:
				fprintf(stderr, "Fatal interal compiler error: unrecognzied binary operatore in destination type determinator\n");
				exit(1);
		}
	}

	return destination_type;
}


/**
 * Simulate the state of the flags register after a given binary operation has executed if possible. This is
 * of course only possible if we are able to trace back both of the variable operands to constant assignments
 * up earlier in the block
 *
 * NOTE: we will only do this for binary operations that do set flags. If we encounter a binary operation
 * that does not set flags we will panic
 */
static simulated_execution_result_t simulate_binary_operation_flags_after_execution(instruction_t* instruction){
	/**
	 * Extract the first operand and get the type that we're after
	 */
	three_addr_var_t* operand1 = instruction->operands.oir.operand1;
	generic_type_t* operand_type = get_destination_type_for_binary_operation_instruction(instruction);

	/**
	 * First attempt to find the first operand's result inside of the block. If it's not a constant
	 * then there's nothing more we can do
	 */
	search_result_t op1_result = search_for_variable_assignment_in_block(instruction, operand1);
	if(op1_result.result_type != SEARCH_RESULT_TYPE_CONST){
		return BLANK_SIMULATED_EXECUTION_RESULT;
	}

	/**
	 * Remember that binary operations may or may not have a LOAD attached as
	 * their second operand. In the event that they do, we'll still want to 
	 * search that LOAD to see if it is a constant float load. Otherwise, we'll
	 * use the standard procedure to get what we're after
	 *
	 *
	 * Case 1: there is no memory access so we can search for operand2 and go from there
	 */
	search_result_t op2_result = BLANK_SEARCH_RESULT;
	if(instruction->memory_access_type == NO_MEMORY_ACCESS){
		search_result_t op2_result = search_for_variable_assignment_in_block(instruction, instruction->operands.oir.operand2);

	/**
	 * Case 2: we do have memory access. In this case we'll have a LOAD with the addressing information
	 * stored in the source_address_expression. We will scan that to determine if we have a float
	 * load or if we have nothing useful
	 */
	} else {
		//TODO

	}


	if(op2_result.result_type != SEARCH_RESULT_TYPE_CONST){
		return BLANK_SIMULATED_EXECUTION_RESULT;
	}


	/**
	 * Then attempt to find the second operand's result inside of the block. Again if it's not a constant
	 * then there's nothing more that we can do
	 */



	switch(instruction->op){
		case CARROT: {
			/**
			 * Sanity check at this point - no floats allowed
			 */
			if(IS_FLOATING_POINT(operand_type) == TRUE){
				trigger_ice_panic("Invalid floating point operand for ^ operator");
			}

			u_int64_t op1_raw_const = op1_result.result.constant->constant_value.unsigned_long_constant;
			u_int64_t op2_raw_const = op2_result.result.constant->constant_value.unsigned_long_constant;
			return simulate_flags_from_bitwise_xor_with_constants(op1_raw_const, op2_raw_const, operand_type);
		}

		case SINGLE_AND: {
			/**
			 * Sanity check at this point - no floats allowed
			 */
			if(IS_FLOATING_POINT(operand_type) == TRUE){
				trigger_ice_panic("Invalid floating point operand for & operator");
			}

			u_int64_t op1_raw_const = op1_result.result.constant->constant_value.unsigned_long_constant;
			u_int64_t op2_raw_const = op2_result.result.constant->constant_value.unsigned_long_constant;
			return simulate_flags_from_bitwise_and_with_constants(op1_raw_const, op2_raw_const, operand_type);
		}

		case SINGLE_OR: {
			/**
			 * Sanity check at this point - no floats allowed
			 */
			if(IS_FLOATING_POINT(operand_type) == TRUE){
				trigger_ice_panic("Invalid floating point operand for | operator");
			}

			u_int64_t op1_raw_const = op1_result.result.constant->constant_value.unsigned_long_constant;
			u_int64_t op2_raw_const = op2_result.result.constant->constant_value.unsigned_long_constant;
			return simulate_flags_from_bitwise_or_with_constants(op1_raw_const, op2_raw_const, operand_type);
		}

		case PLUS: {
			/**
			 * Sanity check - we cannot use floats to simulate addition for the purpose of branch
			 */
			if(IS_FLOATING_POINT(operand_type) == TRUE){
				trigger_ice_panic("Invalid floating point operand for + operator");
			}

			u_int64_t op1_raw_const = op1_result.result.constant->constant_value.unsigned_long_constant;
			u_int64_t op2_raw_const = op2_result.result.constant->constant_value.unsigned_long_constant;
			return simulate_flags_from_addition_with_constants(op1_raw_const, op2_raw_const, operand_type);
		}

		case MINUS: {
			/**
			 * Sanity check - we cannot use floats to simulate subtraction for the purpose of branch
			 */
			if(IS_FLOATING_POINT(operand_type) == TRUE){
				trigger_ice_panic("Invalid floating point operand for - operator");
			}

			u_int64_t op1_raw_const = op1_result.result.constant->constant_value.unsigned_long_constant;
			u_int64_t op2_raw_const = op2_result.result.constant->constant_value.unsigned_long_constant;
			return simulate_flags_from_subtraction_with_constants(op1_raw_const, op2_raw_const, operand_type);
		}

		case L_THAN:
		case L_THAN_OR_EQ:
		case G_THAN:
		case G_THAN_OR_EQ:
		case DOUBLE_EQUALS:
		case NOT_EQUALS: {
			/**
			 * For comparison operators over integers, the "cmpX" instruction
			 * is the exact same flag-wise as an integer subtraction under the
			 * hood. As such, we'll use the integer subtraction helper to determine
			 * the flags for all integer comparison operators
			 */
			if(IS_FLOATING_POINT(operand_type) == FALSE){
				u_int64_t op1_raw_const = op1_result.result.constant->constant_value.unsigned_long_constant;
				u_int64_t op2_raw_const = op2_result.result.constant->constant_value.unsigned_long_constant;
				return simulate_flags_from_subtraction_with_constants(op1_raw_const, op2_raw_const, operand_type);

			/**
			 * Otherwise we have a floating point comparison. We will simulate this using the appropriate
			 * f32 or f64 float comparison helpers
			 */
			} else {
				if(operand_type->basic_type_token == F32){
					float op1_raw_const = op1_result.result.constant->constant_value.float_constant;
					float op2_raw_const = op2_result.result.constant->constant_value.float_constant;
					return simulate_flags_from_f32_constant_comparison(op1_raw_const, op2_raw_const);
				} else {
					double op1_raw_const = op1_result.result.constant->constant_value.double_constant;
					double op2_raw_const = op2_result.result.constant->constant_value.double_constant;
					return simulate_flags_from_f64_constant_comparison(op1_raw_const, op2_raw_const);
				}
			}
		}

		/**
		 * We should never get to anything else. If we do then this is a compiler failure
		 */
		default: {
			sprintf(compiler_error_info, "Operator %s does not set branch actionable flags and is therefore invalid for simulation", operator_token_to_string(instruction->op));
			trigger_ice_panic(compiler_error_info);
			return BLANK_SIMULATED_EXECUTION_RESULT;
		}
	}
}


/**
 * Simulate the state of the flags register after a given binary operation with constant has executed. This simulator
 * has been designed to handle one scenario where simulation is possible, that being if the first operand is really
 * a constant somewhere else. All other scenarios lead to nothing
 *
 * NOTE: we will only do this for binary operations that do set flags. If we encounter a binary operation
 * that does not set flags we will panic
 */
static simulated_execution_result_t simulate_binary_operation_with_constant_flags_after_execution(instruction_t* instruction){
	/**
	 * Extract the first operand and determine the overall operand type for this instruction
	 */
	three_addr_var_t* operand1 = instruction->operands.oir.operand1;
	generic_type_t* operand_type = get_destination_type_for_binary_operation_instruction(instruction);

	/**
	 * Attempt to find operand1 inside of the block. If we do not find a *constant* value that it
	 * is equal to, then there is nothing more to be done and we will return a blank result
	 */
	search_result_t op1_result = search_for_variable_assignment_in_block(instruction, operand1);
	if(op1_result.result_type != SEARCH_RESULT_TYPE_CONST){
		return BLANK_SIMULATED_EXECUTION_RESULT;
	}

	/**
	 * Once we get here we know that we have 2 constant operands, so we can begin simulating
	 * accordingly based on them after we extract them
	 */
	three_addr_const_t* constant_operand1 = op1_result.result.constant;
	three_addr_const_t* constant_operand2 = instruction->operands.oir.constant_operand;

	/**
	 * Simulate based on all of the valid operands that we have
	 */
	switch(instruction->op){
		case CARROT: {
			/**
			 * Sanity check at this point - no floats allowed
			 */
			if(IS_FLOATING_POINT(operand_type) == TRUE){
				trigger_ice_panic("Invalid floating point operand for ^ operator");
			}

			u_int64_t op1_raw_const = constant_operand1->constant_value.unsigned_long_constant;
			u_int64_t op2_raw_const = constant_operand2->constant_value.unsigned_long_constant;
			return simulate_flags_from_bitwise_xor_with_constants(op1_raw_const, op2_raw_const, operand_type);
		}

		case SINGLE_AND: {
			/**
			 * Sanity check at this point - no floats allowed
			 */
			if(IS_FLOATING_POINT(operand_type) == TRUE){
				trigger_ice_panic("Invalid floating point operand for & operator");
			}

			u_int64_t op1_raw_const = constant_operand1->constant_value.unsigned_long_constant;
			u_int64_t op2_raw_const = constant_operand2->constant_value.unsigned_long_constant;
			return simulate_flags_from_bitwise_and_with_constants(op1_raw_const, op2_raw_const, operand_type);
		}

		case SINGLE_OR: {
			/**
			 * Sanity check at this point - no floats allowed
			 */
			if(IS_FLOATING_POINT(operand_type) == TRUE){
				trigger_ice_panic("Invalid floating point operand for | operator");
			}

			u_int64_t op1_raw_const = constant_operand1->constant_value.unsigned_long_constant;
			u_int64_t op2_raw_const = constant_operand2->constant_value.unsigned_long_constant;
			return simulate_flags_from_bitwise_or_with_constants(op1_raw_const, op2_raw_const, operand_type);
		}
					
		case PLUS: {
			/**
			 * Sanity check - we cannot use floats to simulate addition for the purpose of branch
			 */
			if(IS_FLOATING_POINT(operand_type) == TRUE){
				trigger_ice_panic("Invalid floating point operand for + operator");
			}

			u_int64_t op1_raw_const = constant_operand1->constant_value.unsigned_long_constant;
			u_int64_t op2_raw_const = constant_operand2->constant_value.unsigned_long_constant;
			return simulate_flags_from_addition_with_constants(op1_raw_const, op2_raw_const, operand_type);
		}

		case MINUS: {
			/**
			 * Sanity check - we cannot use floats to simulate subtraction for the purpose of branch
			 */
			if(IS_FLOATING_POINT(operand_type) == TRUE){
				trigger_ice_panic("Invalid floating point operand for - operator");
			}

			u_int64_t op1_raw_const = constant_operand1->constant_value.unsigned_long_constant;
			u_int64_t op2_raw_const = constant_operand2->constant_value.unsigned_long_constant;
			return simulate_flags_from_subtraction_with_constants(op1_raw_const, op2_raw_const, operand_type);
		}

		case L_THAN:
		case L_THAN_OR_EQ:
		case G_THAN:
		case G_THAN_OR_EQ:
		case DOUBLE_EQUALS:
		case NOT_EQUALS: { 
			/**
			 * For comparison operators over integers, the "cmpX" instruction
			 * is the exact same flag-wise as an integer subtraction under the
			 * hood. As such, we'll use the integer subtraction helper to determine
			 * the flags for all integer comparison operators
			 */
			if(IS_FLOATING_POINT(operand_type) == FALSE){
				u_int64_t op1_raw_const = constant_operand1->constant_value.unsigned_long_constant;
				u_int64_t op2_raw_const = constant_operand2->constant_value.unsigned_long_constant;
				return simulate_flags_from_subtraction_with_constants(op1_raw_const, op2_raw_const, operand_type);

			/**
			 * Otherwise we have a floating point comparison. We will simulate this using the appropriate
			 * f32 or f64 float comparison helpers
			 */
			} else {
				if(operand_type->basic_type_token == F32){
					float op1_raw_const = constant_operand1->constant_value.float_constant;
					float op2_raw_const = constant_operand2->constant_value.float_constant;
					return simulate_flags_from_f32_constant_comparison(op1_raw_const, op2_raw_const);
				} else {
					double op1_raw_const = constant_operand1->constant_value.double_constant;
					double op2_raw_const = constant_operand2->constant_value.double_constant;
					return simulate_flags_from_f64_constant_comparison(op1_raw_const, op2_raw_const);
				}
			}
		}

		/**
		 * We should never get to anything else. If we do then this is a compiler failure
		 */
		default: {
			sprintf(compiler_error_info, "Operator %s does not set branch actionable flags and is therefore invalid for simulation", operator_token_to_string(instruction->op));
			trigger_ice_panic(compiler_error_info);
			return BLANK_SIMULATED_EXECUTION_RESULT;
		}
	}
}


/**
 * Simulate the flags of the given instruction after execution. For this, we really don't care about the actual
 * value of the result variable. Rather, since these instructions are being used for conditional execution, what
 * we really care about is the state of the flags register after they run
 *
 * There is no guarantee that an instruction passed into this simulator can be simulated
 */
simulated_execution_result_t simulate_instruction_flags_after_execution(instruction_t* instruction){
	/**
	 * We will go based on the statement type and make call outs as appropriate. Note
	 * that not all statements can be simulated. If it's not in our predetermined list
	 * we will return a simulation failure
	 */
	switch(instruction->statement_type){
		case THREE_ADDR_CODE_TEST_IF_NOT_ZERO_STMT: {
			return simulate_test_if_not_zero_flags_after_execution(instruction);
		}

		case THREE_ADDR_CODE_LOGICAL_NOT_STMT: {
			return simulate_logical_not_flags_after_execution(instruction);
		}

		case THREE_ADDR_CODE_NEG_STATEMENT: {
			return simulate_negation_flags_after_execution(instruction);
		}

		case THREE_ADDR_CODE_BIN_OP_STMT: {
			return simulate_binary_operation_flags_after_execution(instruction);
		}

		case THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT: {
			return simulate_binary_operation_with_constant_flags_after_execution(instruction);
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

		/**
		 * Signed branch less than - jump to if when the subtraction result's
		 * sign is different from the signed overflow flag. This means that we've
		 * either had no overflow and a negative result or overflow and a positive
		 * result, meaning that the first operand is less than the second
		 *
		 * Condition to jump to physical if block: SF != OF
		 */
		case BRANCH_L: {
			if(get_sign_flag(&flags) != get_overflow_flag(&flags)){
				return if_block;
			} else {
				return else_block;
			}
		}

		/**
		 * Signed branch less than or equal - jump to if when the subtraction result's
		 * sign is different from the signed overflow flag *OR* when the zero flag has
		 * been set. This means that we've  either had no overflow and a negative result
		 * or overflow and a positive result, meaning that the first operand is less than 
		 * the second, or we just have the zero flag set meaning that they're equal
		 *
		 * Condition to jump to physical if block: ZF == 1 || (SF != OF)
		 */
		case BRANCH_LE: {
			if((get_zero_flag(&flags) == 1) || (get_sign_flag(&flags) != get_overflow_flag(&flags))){
				return if_block;
			} else {
				return else_block;
			}
		}

		/**
		 * Unsigned branch if below - jump to if when the carry flag has been set, indicating
		 * an unsigned overflow in the subtraction process which means that the first operand
		 * is smaller than the second
		 *
		 * Condition to jump to physical if block: CF == 1
		 */
		case BRANCH_B: {
			if(get_carry_flag(&flags) == 1){
				return if_block;
			} else {
				return else_block;
			}
		}

		/**
		 * Unsigned branch if below or equal - jump to if when the carry flag has been set, indicating
		 * an unsigned overflow in the subtraction process which means that the first operand
		 * is smaller than the second, *OR* when the zero flag has been set indicating equality
		 *
		 * Condition to jump to physical if block: CF == 1 || ZF == 1
		 */
		case BRANCH_BE: {
			if((get_carry_flag(&flags) == 1) || (get_zero_flag(&flags) == 1)){
				return if_block;
			} else {
				return else_block;
			}
		}

		/**
		 * Signed branch when greater will execute when the sign flag and the overflow
		 * flag are equal *AND* the zero flag is unset. When the sign and overflow flags
		 * are equal, it means that either the result is positive and there was no overflow
		 * *OR* the result was negative and there was overflow, both meaning that the first
		 * operand is more than but not equal to the second
		 *
		 * Condition to jump to physical if block: ZF == 0 && SF == OF
		 */
		case BRANCH_G: {
			if((get_zero_flag(&flags) == 0) && (get_sign_flag(&flags) == get_overflow_flag(&flags))){
				return if_block;
			} else {
				return else_block;
			}
		}

		/**
		 * Signed branch when greater or equal will execute when the sign flag and the overflow
		 * flag are equal. When the sign and overflow flags are equal, it means that either the
		 * result is positive and there was no overflow *OR* the result was negative and there 
		 * was overflow, both meaning that the first operand is more than but not equal to the second.
		 * It could also indicate that the two operands are equal, whcih accounts for our equals
		 * case
		 *
		 * Condition to jump to physical if block: SF == OF
		 */
		case BRANCH_GE: {
			if(get_sign_flag(&flags) == get_overflow_flag(&flags)){
				return if_block;
			} else {
				return else_block;
			}
		}

		/**
		 * Unsigned branch if above occurs when we zero flag is unset and the carry flag
		 * is also unset, indicating that there was no unsigned overflow
		 *
		 * Condition to jump to physical if block: ZF == 0 && CF == 0
		 */
		case BRANCH_A: {
			if((get_zero_flag(&flags) == 0) && (get_carry_flag(&flags) == 0)){
				return if_block;
			} else {
				return else_block;
			}
		}

		/**
		 * Unsigned branch if above occurs when the carry flag is unset, indicating
		 * that there is no unsigned overflow
		 *
		 * Condition to jump to physical if block: CF == 0
		 */
		case BRANCH_AE: {
			if(get_carry_flag(&flags) == 0){
				return if_block;
			} else {
				return else_block;
			}
		}

		/**
		 * Any unrecognized branch requires a panic on our part
		 */
		default: {
			trigger_ice_panic("Invalid branch type encountered in simulator");
			return NULL;
		}
	}
}


/**
 * For branches that derive from some kind of floating point comparsion, we will need to consider the possibility
 * of a NaN oeprator that should cause us to fall through to the logically false condition. Regular OIR to x86 assembly
 * inserts parity jumps(JP) to the appropriate block to handle this. In our case, we'll need to consider the branch
 * category and the flags to determine what the proper block is
 */
static inline basic_block_t* simulate_branch_with_parity_consideration(instruction_t* branch_statement, RFLAGS_t flags){
	/**
	 * Extract the category, type if and else blocks before starting to simulate
	 */
	branch_category_t branch_category = branch_statement->branch_info.branch_category;
	branch_type_t branch_type = branch_statement->branch_info.branch_type;
	basic_block_t* if_block = branch_statement->branch_info.if_block;
	basic_block_t* else_block = branch_statement->branch_info.else_block;

	switch(branch_type){
		/**
		 * BRANCH_A/BRANCH_G behaves differently for floating point based on whether
		 * or not our logical if matches our physical if block or not
		 */
		case BRANCH_A:
		case BRANCH_G: {
			/**
			 * For branching above without an inverted branch, we do *NOT* need to add
			 * parity checking because a NaN will set the CF=1, and the branch will go
			 * to the physical else block, which in this normal case is also the logical
			 * else block
			 * 	
			 * Condition(to jump to phyiscal if block): CF == 0 && ZF == 0
			 */
			if(branch_category == BRANCH_CATEGORY_NORMAL){
				if((get_carry_flag(&flags) == 0) && (get_zero_flag(&flags) == 0)){
					return if_block;
				} else {
					return else_block;
				}
			
			/**
			 * For branching above *WITH* an inverted branch, we *DO* need to add parity
			 * checking becasue a we want to consider the NaN as a false path. NaN is not
			 * ever greater than anything else. However, our "false" path here is actually
			 * the physical if block because it is an inverse branch, so we'll want the parity
			 * to take us to the physical if, which is the logical else
			 *
			 * Condition(to jump to phyiscal if block): PF == 1 || (CF == 0 && ZF == 0)
			 */
			} else {
				if((get_parity_flag(&flags) == 1) || ((get_carry_flag(&flags) == 0) && (get_zero_flag(&flags) == 0))){
					return if_block;
				} else {
					return else_block;
				}
			}
		}

		/**
		 * BRANCH_AE/BRANCH_GE behaves differently for floating point based on whether
		 * or not our logical if matches our physical if block or not
		 */
		case BRANCH_AE:
		case BRANCH_GE: {
			/**
			 * For branching *WITHOUT* an inverted branch, we *DO NOT* need to add
			 * a parity check. We want a NaN to go to the logical else, which is also
			 * the physical else in this case, and since a NaN sets CF=1, this branch
			 * goes to the else block already
			 * 	
			 * Condition(to jump to phyiscal if block): CF == 0 
			 */
			if(branch_category == BRANCH_CATEGORY_NORMAL){
				if(get_carry_flag(&flags) == 0){
					return if_block;
				} else {
					return else_block;
				}

			/**
			 * For branching with an inverted branch, we *DO* need to add a parity
			 * check that will have us jump to the physical if(logical else in this case).
			 * Since NaN sets CF=1, if we do not have this parity check, a NaN will send
			 * us to the physical else(logical if) in this case
			 *
			 * Condition(jump to physical if block): PF == 1 || CF == 0
			 */
			} else {
				if((get_parity_flag(&flags) == 1) || (get_carry_flag(&flags) == 0)){
					return if_block;
				} else {
					return else_block;
				}
			}
		}

		/**
		 * BRANCH_L/BRANCH_B behaves differently for floating point based on whether
		 * or not our logical if matches our physical if block or not
		 */
		case BRANCH_L:
		case BRANCH_B: {
			/**
			 * For branching below without an inverse branch, our logical else matches our
			 * physical else. However, we *DO* still need to add a parity check because
			 * if we have a NaN operand, it will set CF=1, and this would cause us to jump
			 * to the physical if(logical if as well) on a NaN incorrectly. Since the physical
			 * else is our logical else, our parity check will jump to else 
			 *
			 * Condition(to jump to phyiscal if block): PF == 0 && CF == 1
			 */
			if(branch_category == BRANCH_CATEGORY_NORMAL){
				if((get_parity_flag(&flags) == 0) && (get_carry_flag(&flags) == 1)){
					return if_block;
				} else {
					return else_block;
				}

			/**
			 * For branching below with an inverse branch, our logical else is
			 * our physical if. Since NaN should always take the logical false
			 * path, we need to add a parity jump to the physical if block
			 * to account for the different logic
			 *
			 * Condition(to jump to physical if block): PF == 1 || CF == 1
			 */
			} else {
				if((get_parity_flag(&flags) == 1) || (get_carry_flag(&flags) == 1)){
					return if_block;
				} else {
					return else_block;
				}
			}
		}

		/**
		 * BRANCH_L/BRANCH_B behaves differently for floating point based on whether
		 * or not our logical if matches our physical if block or not
		 */
		case BRANCH_LE:
		case BRANCH_BE: {
			/**
			 * For a regular branch, our physical if is also our logical if. For this reason
			 * we'll want any NaN's to jump to the physical else. Since a NaN would set
			 * the CF=1, we will need to add a parity check beforehand that takes us
			 * to the else in the event that the PF is set
			 *
			 * Condition(to jump to phyiscal if block): PF == 0 && (CF == 1 || ZF == 1)
			 */
			if(branch_category == BRANCH_CATEGORY_NORMAL){
				if((get_parity_flag(&flags) == 0) && ((get_carry_flag(&flags) == 1) || (get_zero_flag(&flags) == 1))){
					return if_block;
				} else {
					return else_block;
				}

			/**
			 * For an inverse branch, our physical if is our logical else. For this reason
			 * we'll want any NaN's to jump to the physical if. For this reason, we'll
			 * add a parity check right beforehand to account for the NaN case
			 *
			 * Condition(to jump to phyiscal if block): PF == 1 || (CF == 1 || ZF == 1)
			 */
			} else {
				if((get_parity_flag(&flags) == 1) || ((get_carry_flag(&flags) == 1) || (get_zero_flag(&flags) == 1))){
					return if_block;
				} else {
					return else_block;
				}
			}
		}

		/**
		 * For not equals or not zero branches, we do need to do parity checking because we consider
		 * a NaN to never be equal with anything else. In Ollie, NaN != <anything>, so if the parity
		 * flag is set we'll need to jump to the if block. We'll also jump to true if we're not equal.
		 * Since the parity conditions match up for both inverse branching(jump to physical if, logical else)
		 * and normal branching(jump to physical if, logical if), we don't need any special logic for this
		 *
		 * Condition(to jump to phyiscal if block): PF == 1 || ZF == 0
		 */
		case BRANCH_NE:
		case BRANCH_NZ: {
			if((get_parity_flag(&flags) == 1) || (get_zero_flag(&flags) == 0)){
				return if_block;
			} else {
				return else_block;
			}
		}

		/**
		 * For regular equals branches, we need to do parity checking because like with the
		 * not equals case, NaN != <anything>. In the event that we do see the parity flag
		 * checked we'll need to jump to the physical else condition. We'll jump to true if they are
		 * equal. Since the parity conditions match up for both inverse branching(jump to physical else,
		 * logical if) and normal branching(jump to physical else, logical else), we don't need
		 * to add any special logic here
		 *
		 * Condition(to jump to phyiscal if block): PF == 0 && ZF == 1
		 */
		case BRANCH_E:
		case BRANCH_Z: {
			if((get_parity_flag(&flags) == 0) && (get_zero_flag(&flags) == 1)) {
				return if_block;
			} else {
				return else_block;
			}
		}

		/**
		 * Any unrecognized branch should be a panic on our part
		 */
		default: {
			trigger_ice_panic("Invalid branch type encountered in simulator");
			return NULL;
		}
	}
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
