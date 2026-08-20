/**
 * Author: Jack Robbins
 * This file defines the Ollie IR's three address variable concept. Variables eventually become memory
 * regions or register values in the final compiled program
 */

#ifndef THREE_ADDR_VARIABLE_H
#define THREE_ADDR_VARIABLE_H

#include <sys/types.h>
#include "../type_system/type_system.h"
#include "../local_constant/local_constant.h"
#include "x86_genpurpose_registers.h"
#include "x86_sse_registers.h"
#include "../symtab/symtab.h"

//A struct that holds our three address variables
typedef struct three_addr_var_t three_addr_var_t;
//A struct that stores all of our live ranges
typedef struct live_range_t live_range_t;


/**
 * What type of variable is this? Variables
 * can be temporary, stack variables, or normal
 * vars
 */
typedef enum {
	VARIABLE_TYPE_TEMP,
	VARIABLE_TYPE_NON_TEMP,
	VARIABLE_TYPE_MEMORY_ADDRESS,
	//Specialized memory address for a stack passed parameter
	VARIABLE_TYPE_STACK_PARAM_MEMORY_ADDRESS,
	VARIABLE_TYPE_LOCAL_CONSTANT,
	VARIABLE_TYPE_FUNCTION_ADDRESS, //For rip-relative function pointer loads
	//A return-by-copy address variable designed for functions where we return structs/unions
	VARIABLE_TYPE_RETURN_BY_COPY_ADDRESS,
} variable_type_t;

/**
 * What kind of live range is this? Live ranges can either
 * be part of the normal general purpose class of variables
 * or the SSE(floating point usually) class of variables
 */
typedef enum {
	LIVE_RANGE_CLASS_GEN_PURPOSE,
	LIVE_RANGE_CLASS_SSE
} live_range_class_t;


/**
 * For our live ranges, we'll really only need the name and
 * the variables
 */
struct live_range_t {
	//Hold all the variables that it has
	dynamic_array_t variables;
	//And we'll hold an adjacency list for interference
	dynamic_array_t neighbors;
	//Hold the stack region as well
	stack_region_t* stack_region;
	//What function does this come from?
	symtab_function_record_t* function_defined_in;
	//Store the id of the live range
	u_int32_t live_range_id;
	//Store the heuristic spill cost
	u_int32_t spill_cost;
	/**
	 * ========== IMPORTANT NOTE ===========
	 * We do not arbitrarily clone live
	 * ranges like we do three address variables.
	 * As such, it is completely appropriate for
	 * us to have a use count tracker with them
	 * and not have to worry about it. This core
	 * assumption will never hold for a three_addr_var_t
	 * and anyone dealing with use counts must be aware
	 * of this
	 * ========== IMPORTANT NOTE ===========
	 */
	u_int32_t assignment_count;
	u_int32_t use_count;
	//The degree of this live range
	u_int16_t degree;
	//The interference graph index of it
	u_int16_t interference_graph_index;
	//What is the function parameter order here?
	u_int16_t class_relative_function_parameter_order;
	//Was this live range spilled?
	u_int8_t was_spilled;
	//What class of live range is this?
	live_range_class_t live_range_class;
	//What register is this live range in?
	union {
		general_purpose_register_t gen_purpose;
		sse_register_t sse_reg;
	} reg;
};


/**
 * A three address var may be a temp variable or it may be
 * linked to a non-temp variable. It keeps a generation counter
 * for eventual SSA and type information
*/
struct three_addr_var_t{
	//Link to symtab(NULL if not there)
	symtab_variable_record_t* linked_var;
	//
	//
	//TODO why not just give every single
	//variable a unique ID? If it's a temp var
	//then we just use that ID and if it's not
	//use the symtab variable, but honestly 
	//why reserve this just for temp vars
	//
	//Once we have these numbers(they start at 0), we
	//can have a map that allows O(1) indexing to get
	//the actual counts???
	//
	//Maybe this is just an idea we need to work it through
	//
	//
	//
	//Temp var unique identifier
	u_int32_t temp_var_number;
	//Types will be used for eventual register assignment
	generic_type_t* type;
	//What live range is this variable associate with
	live_range_t* associated_live_range;
	union {
		//What is the stack region associated with this variable?
		stack_region_t* stack_region;
		//What is the local constant associate with this variable
		local_constant_t* local_constant;
		//Rip relative function name for loading function pointers
		symtab_function_record_t* rip_relative_function;
	} associated_memory_region;
	//What is the ssa generation level?
	u_int32_t ssa_generation;
	//Base adjustment for stack passed memory address variables
	u_int32_t memory_address_base_adjustment;
	//What is the parameter number of this var? Used for parameter passing. If
	//it is 0, it's ignored
	u_int32_t class_relative_parameter_order;
	//Does this set condition codes?
	u_int8_t sets_cc;
	//Does this derive from an FP comparison
	u_int8_t comes_from_fp_comparison;
	//Was this variable value named?
	u_int8_t was_value_named;
	//What is the size of this variable
	variable_size_t variable_size;
	//What membership do we have if any
	variable_membership_t membership;
	//What type of variable is this
	variable_type_t variable_type;
};


#endif /* THREE_ADDR_VARIABLE_H */
