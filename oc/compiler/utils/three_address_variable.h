

#ifndef THREE_ADDR_VARIABLE_H
#define THREE_ADDR_VARIABLE_H

/**
 * A three address var may be a temp variable or it may be
 * linked to a non-temp variable. It keeps a generation counter
 * for eventual SSA and type information
*/
struct three_addr_var_t{
	//Link to symtab(NULL if not there)
	symtab_variable_record_t* linked_var;
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
	//What's the temp var number
	u_int32_t temp_var_number;
	//What's the reference count of this variable.
	//This will be needed later on down the line in 
	//the instruction selector
	u_int32_t use_count;
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
