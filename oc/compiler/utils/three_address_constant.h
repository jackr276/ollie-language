/**
*/

#ifndef THREE_ADDR_CONSTANT_H
#define THREE_ADDR_CONSTANT_H

#include "../type_system/type_system.h"
#include "three_address_variable.h"
#include "stack_management_structs.h"

//A struct that holds our three address constants
typedef struct three_addr_const_t three_addr_const_t;

/**
 * A three address constant always holds the value of the constant
 */
struct three_addr_const_t{
	//We hold the type info
	generic_type_t* type;

	//Store the constant value in a union
	union {
		int64_t signed_long_constant;
		u_int64_t unsigned_long_constant;
		int32_t signed_integer_constant;
		u_int32_t unsigned_integer_constant;
		int16_t signed_short_constant;
		u_int16_t unsigned_short_constant;
		int8_t signed_byte_constant;
		u_int8_t unsigned_byte_constant;
		char char_constant;
		/**
		 * We should note that these will only be used inside of a global
		 * variable context. If a user is regularly using a float or double
		 * constant, it would be loaded in via the Local Constant(.LC) subsystem
		 */
		double double_constant;
		float float_constant;
		/**
		 * There are special cases in the global context where we can
		 * use a string constant. This is exclusively for the global context
		 * in that case however, and will not be used anywhere else
		 */
		char* string_constant;
		/**
		 * There are other special cases where we can hold a relative pointer to
		 * a local constant. This is done exlcusively for declaring char* values
		 * These pointers are always 8 bytes
		 */
		three_addr_var_t* local_constant_address;
		/**
		 * This is a stack-param passed offset constant. We will only act upon it inside of the regsiter allocator/postprocessor
		 * once we are done with all spilling/stack maanagement logic. There should be a "const type" to do this. In fact, we should
		 * probably make the "const_type" an actual type and not just some tacked on token extension
		 */
		stack_region_t* parameter_passed_stack_region;

	} constant_value;

	/**
	 * We want the ability to use all of our fancy simplification tricks, but we also need to account for the ambiguity in
	 * how stack passed parameter constants work. This is our middle ground. We can do any constant manipulation on this
	 * adjustment integer here, then at the register allocation step, when we translate everything out of stack parameters,
	 * we will add this adjustment to the offset that we get to maintain accuracy and keep our instruction selector simplification
	 * viable
	 */
	int64_t constant_adjustment;

	//What kind of constant is it
	ollie_token_t const_type;
};

#endif /* THREE_ADDR_CONSTANT_H */
