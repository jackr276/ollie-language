/**
 * Author: Jack Robbins
 *
 * This is the implementation file for the three_address_code header file
*/

#include "instruction.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "../cfg/cfg.h"
#include "../jump_table/jump_table.h"
#include "../utils/dynamic_string/dynamic_string.h"
#include "../utils/constants.h"


//======================= Utility macros ===================
/**
 * Let's determine if a value is a positive power of 2.
 * Here's how this will work. In binary, powers of 2 look like:
 * 0010
 * 0100
 * 1000
 * ....
 *
 * In other words, they have exactly 1 on bit that is not in the LSB position
 *
 * Here's an example: 5 = 0101, so 5-1 = 0100
 *
 * 0101 & (0100) = 0100 which is 4, not 0
 *
 * How about 8?
 * 8 is 1000
 * 8 - 1 = 0111
 *
 * 1000 & 0111 = 0, so 8 is a power of 2
 *
 * Therefore, the formula we will use is value & (value - 1) == 0
 */
#define IS_SIGNED_POWER_OF_2(value)\
	(((value > 0) && ((value & (value - 1)) == 0)) ? TRUE : FALSE)

#define IS_UNSIGNED_POWER_OF_2(value)\
	(((value & (value - 1)) == 0) ? TRUE : FALSE)

//======================= Utility macros ===================

//The atomically increasing temp name id
static int32_t current_temp_id = 0;

//All created vars
dynamic_array_t emitted_vars;
//All created constants
dynamic_array_t emitted_consts;


/**
 * Initialize the memory management system
 */
void initialize_varible_and_constant_system(){
	emitted_consts = dynamic_array_alloc();
	emitted_vars = dynamic_array_alloc();
}


/**
 * A helper function for our atomically increasing temp id
 */
int32_t increment_and_get_temp_id(){
	current_temp_id++;
	return current_temp_id;
}


/**
 * A helper function that will create a global variable for us
 */
global_variable_t* create_global_variable(symtab_variable_record_t* variable, three_addr_const_t* value){
	//Allocate it
	global_variable_t* var = calloc(1, sizeof(global_variable_t));

	//Add into here for memory management
	dynamic_array_add(&emitted_vars, var);

	//Copy these over
	var->variable = variable;
	//It never hurts to have a quick way to reference this
	var->variable_type = variable->type_defined_as;
	var->initializer_value.constant_value = value;

	//Give the var back
	return var;
}


/**
 * Insert an instruction in a block before the given instruction
 */
void insert_instruction_before_given(instruction_t* insertee, instruction_t* given){
	//Let's first grab out which block we've got
	basic_block_t* block = given->block_contained_in;
	//Mark this while we're here
	insertee->block_contained_in = block;

	//Increment our number here
	block->number_of_instructions++;

	//Grab out what's before the given
	instruction_t* before_given = given->previous_statement;

	//This one's previous statement is always the before given
	insertee->previous_statement = before_given;

	//If this isn't the leader, it will have a before
	if(before_given != NULL){
		//Next statement here is the insertee
		before_given->next_statement = insertee;
	//Otherwise this now is the leader
	} else {
		block->leader_statement = insertee;
	}

	//The insertee is before the given, so its next is the given
	insertee->next_statement = given;
	given->previous_statement = insertee;

	//Save the function as well
	insertee->function = block->function_defined_in;
}


/**
 * Insert an instruction in a block after the given instruction
 */
void insert_instruction_after_given(instruction_t* insertee, instruction_t* given){
	//Let's first grab out which block we've got
	basic_block_t* block = given->block_contained_in;
	//Mark this while we're here
	insertee->block_contained_in = block;
	
	//Increment our number here
	block->number_of_instructions++;

	//Whatever comes after given
	instruction_t* after_given = given->next_statement;

	//Tie the insertee in
	insertee->next_statement = after_given;
	insertee->previous_statement = given;

	//We know that given's next statement will be the insertee
	given->next_statement = insertee;

	//Most common case here
	if(after_given != NULL){
		//Tie this in as the previous
		after_given->previous_statement = insertee;
	//Otherwise this is the exit statement
	} else {
		block->exit_statement = given;
	}

	//Save the function as well
	insertee->function = block->function_defined_in;
}


/**
 * A simple helper function to determine if an operator is a comparison operator
 */
u_int8_t is_operator_relational_operator(ollie_token_t op){
	switch(op){
		case G_THAN:
		case L_THAN:
		case G_THAN_OR_EQ:
		case L_THAN_OR_EQ:
		case DOUBLE_EQUALS:
		case NOT_EQUALS:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Does operation generate truthful byte value
 *
 * This encompasses: >, >=, <, <=, !=, ==, ||, && because
 * they generate either a 0 or a 1
 */
u_int8_t does_operator_generate_truthful_byte_value(ollie_token_t op){
	switch(op){
		case G_THAN:
		case L_THAN:
		case G_THAN_OR_EQ:
		case L_THAN_OR_EQ:
		case DOUBLE_EQUALS:
		case NOT_EQUALS:
		case DOUBLE_AND:
		case DOUBLE_OR:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Helper function to determine if we have a store operation
 */
u_int8_t is_store_operation(instruction_t* statement){
	//Input validation
	if(statement == NULL){
		return FALSE;
	}

	//Only 3 qualifying statements
	switch(statement->statement_type){
		case THREE_ADDR_CODE_STORE_STATEMENT:
		case THREE_ADDR_CODE_STORE_WITH_CONSTANT_OFFSET:
		case THREE_ADDR_CODE_STORE_WITH_VARIABLE_OFFSET:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Helper function to determine if we have a load operation
 */
u_int8_t is_load_operation(instruction_t* statement){
	//Input validation
	if(statement == NULL){
		return FALSE;
	}

	//Only 3 qualifying statements
	switch(statement->statement_type){
		case THREE_ADDR_CODE_LOAD_STATEMENT:
		case THREE_ADDR_CODE_LOAD_WITH_VARIABLE_OFFSET:
		case THREE_ADDR_CODE_LOAD_WITH_CONSTANT_OFFSET:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Is the given instruction a load operation or not?
 */
u_int8_t is_load_instruction(instruction_t* instruction){
	//Just to be safe here
	if(instruction == NULL){
		return FALSE;
	}

	//Run through all of our move operation types
	switch(instruction->instruction_type){
		case MOVQ:
		case MOVL:
		case MOVW:
		case MOVB:
		case MOVSBW:
		case MOVSBL:
		case MOVSBQ:
		case MOVSWL:
		case MOVSWQ:
		case MOVSLQ:
		case MOVZBW:
		case MOVZBL:
		case MOVZBQ:
		case MOVZWL:
		case MOVZWQ:
			//Only if it's a memory read
			if(instruction->memory_access_type == READ_FROM_MEMORY){
				return TRUE;
			}

			//Otherwise no
			return FALSE;

		//Otherwise no
		default:
			return FALSE;
	}
}


/**
 * Helper function to determine if an instruction is a binary operation
 */
u_int8_t is_instruction_binary_operation(instruction_t* instruction){
	//Speedup with NULL processing
	if(instruction == NULL){
		return FALSE;
	}

	//Switch based on class
	switch(instruction->statement_type){
		case THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT:
		case THREE_ADDR_CODE_BIN_OP_STMT:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Helper function to determine if an instruction is an assignment operation
 */
u_int8_t is_instruction_assignment_operation(instruction_t* instruction){
	//Speedup with NULL processing
	if(instruction == NULL){
		return FALSE;
	}

	//Switch based on class
	switch(instruction->statement_type){
		case THREE_ADDR_CODE_ASSN_STMT:
		case THREE_ADDR_CODE_ASSN_CONST_STMT:
			return TRUE;
		default:
			return FALSE;
	}
}




/**
 * Does a given operation overwrite it's source? Think add, subtract, etc
 */
u_int8_t is_destination_also_operand(instruction_t* instruction){
	switch(instruction->instruction_type){
		case ADDB:
		case ADDL:
		case ADDW:
		case ADDQ:
		case ADDSS:
		case ADDSD:
		case SUBB:
		case SUBW:
		case SUBL:
		case SUBQ:
		case SUBSS:
		case SUBSD:
		case IMULB:
		case IMULW:
		case IMULL:
		case IMULQ:
		case MULSS:
		case MULSD:
		case DIVSS:
		case DIVSD:
		case SHRW:
		case SHRB:
		case SHRL:
		case SHRQ:
		case SARB:
		case SARW:
		case SARQ:
		case SARL:
		case SALB:
		case SALW:
		case SALL:
		case SALQ:
		case SHLB:
		case SHLW:
		case SHLQ:
		case SHLL:
		case XORB:
		case XORW:
		case XORL:
		case XORQ:
		case XORPS:
		case XORPD:
		case ANDW:
		case ANDB:
		case ANDL:
		case ANDQ:
		case ORB:
		case ORW:
		case ORL:
		case ORQ:
		//Floating point comparisons do have their destination overwritten
		case CMPSS:
		case CMPSD:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Is the destination actually assigned?
 */
u_int8_t is_move_instruction_destination_assigned(instruction_t* instruction){
	switch(instruction->instruction_type){
		case MOVQ:
		case MOVL:
		case MOVW:
		case MOVB:
		case MOVD:
		case MOVSBW:
		case MOVSBL:
		case MOVSBQ:
		case MOVSWL:
		case MOVSWQ:
		case MOVSLQ:
		case MOVZBW:
		case MOVZBL:
		case MOVZBQ:
		case MOVZWL:
		case MOVZWQ:
			//If we have a move where we are writing to memory, the destination
			//does not count as assigned
			if(instruction->memory_access_type == WRITE_TO_MEMORY){
				return FALSE;
			}

			//Otherwise it is
			return TRUE;

		//By default yes
		default:
			return TRUE;
	}
}


/**
 * Is this an unsigned multiplication instruction?
 */
u_int8_t is_unsigned_multplication_instruction(instruction_t* instruction){
	//Just in case
	if(instruction == NULL){
		return FALSE;
	}

	switch(instruction->instruction_type){
		case MULB:
		case MULW:
		case MULL:
		case MULQ:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Is this constant value 0?
 */
u_int8_t is_constant_value_zero(three_addr_const_t* constant){
	switch(constant->const_type){
		case INT_CONST:
		case INT_CONST_FORCE_U:
			if(constant->constant_value.unsigned_integer_constant == 0){
				return TRUE;
			}
			return FALSE;

		case LONG_CONST:
		case LONG_CONST_FORCE_U:
			if(constant->constant_value.unsigned_long_constant == 0){
				return TRUE;
			}
			return FALSE;

		case SHORT_CONST:
		case SHORT_CONST_FORCE_U:
			if(constant->constant_value.unsigned_short_constant == 0){
				return TRUE;
			}
			return FALSE;

		case BYTE_CONST:
		case BYTE_CONST_FORCE_U:
			if(constant->constant_value.unsigned_byte_constant == 0){
				return TRUE;
			}
			return FALSE;

		case CHAR_CONST:
			if(constant->constant_value.char_constant == 0){
				return TRUE;
			}
			return FALSE;

		//By default just return false
		default:
			return FALSE;
	}
}


/**
 * Is this constant value 1?
 */
u_int8_t is_constant_value_one(three_addr_const_t* constant){
	switch(constant->const_type){
		case INT_CONST:
		case INT_CONST_FORCE_U:
			if(constant->constant_value.unsigned_integer_constant == 1){
				return TRUE;
			}
			return FALSE;

		case LONG_CONST:
		case LONG_CONST_FORCE_U:
			if(constant->constant_value.unsigned_long_constant == 1){
				return TRUE;
			}
			return FALSE;

		case SHORT_CONST:
		case SHORT_CONST_FORCE_U:
			if(constant->constant_value.unsigned_short_constant == 1){
				return TRUE;
			}
			return FALSE;

		case BYTE_CONST:
		case BYTE_CONST_FORCE_U:
			if(constant->constant_value.unsigned_byte_constant == 1){
				return TRUE;
			}
			return FALSE;

		case CHAR_CONST:
			if(constant->constant_value.char_constant == 1){
				return TRUE;
			}
			return FALSE;

		//By default just return false
		default:
			return FALSE;
	}
}

/**
 * Is this constant a power of 2? We mainly rely on the helper above to do this
 */
u_int8_t is_constant_power_of_2(three_addr_const_t* constant){
	switch(constant->const_type){
		case BYTE_CONST:
			return IS_SIGNED_POWER_OF_2(constant->constant_value.signed_byte_constant);

		case BYTE_CONST_FORCE_U:
			return IS_UNSIGNED_POWER_OF_2(constant->constant_value.unsigned_byte_constant);

		case SHORT_CONST:
			return IS_SIGNED_POWER_OF_2(constant->constant_value.signed_short_constant);
			
		case SHORT_CONST_FORCE_U:
			return IS_UNSIGNED_POWER_OF_2(constant->constant_value.unsigned_short_constant);

		case INT_CONST:
			return IS_SIGNED_POWER_OF_2(constant->constant_value.signed_integer_constant);

		case INT_CONST_FORCE_U:
			return IS_UNSIGNED_POWER_OF_2(constant->constant_value.unsigned_integer_constant);

		case LONG_CONST:
			return IS_SIGNED_POWER_OF_2(constant->constant_value.signed_long_constant);

		case LONG_CONST_FORCE_U:
			return IS_UNSIGNED_POWER_OF_2(constant->constant_value.unsigned_long_constant);

		//Chars are always unsigned
		case CHAR_CONST:
			return IS_UNSIGNED_POWER_OF_2(constant->constant_value.char_constant);

		//By default just return false
		default:
			return FALSE;
	}
}


/**
 * Is this constant a power of 2 that is lea compatible(1, 2, 4, 8)?
 *
 * This is used specifically for lea computations and determining whether
 * certain multiplies are eligible
 */
u_int8_t is_constant_lea_compatible_power_of_2(three_addr_const_t* constant){
	//For holding the expanded constant value
	int64_t constant_value_expanded;

	//Extraction
	switch(constant->const_type){
		case BYTE_CONST:
			constant_value_expanded = constant->constant_value.signed_byte_constant; 
			break;

		case BYTE_CONST_FORCE_U:
			constant_value_expanded = constant->constant_value.unsigned_byte_constant;
			break;

		case SHORT_CONST:
			constant_value_expanded = constant->constant_value.signed_short_constant;
			break;

		case SHORT_CONST_FORCE_U:
			constant_value_expanded = constant->constant_value.unsigned_short_constant;
			break;

		case INT_CONST:
			constant_value_expanded = constant->constant_value.signed_integer_constant;
			break;

		case INT_CONST_FORCE_U:
			constant_value_expanded = constant->constant_value.unsigned_integer_constant;
			break;

		case LONG_CONST:
			constant_value_expanded = constant->constant_value.signed_long_constant;
			break;

		case LONG_CONST_FORCE_U:
			constant_value_expanded = constant->constant_value.unsigned_long_constant;
			break;

		//Chars are always unsigned
		case CHAR_CONST:
			constant_value_expanded = constant->constant_value.char_constant;
			break;

		//If we get here it's a definite no
		default:
			return FALSE;
	}

	//In order to work for lea, the constant must be one of: 1, 2, 4, 8. Anything
	//else is incompatible
	switch(constant_value_expanded){
		//The only acceptable values
		case 1:
		case 2:
		case 4:
		case 8:
			return TRUE;
		//Everything else is a no
		default:
			return FALSE;
	}
}


/**
 * Is this operation a pure copy? In other words, is it a move instruction
 * that moves one register to another?
 */
u_int8_t is_instruction_pure_copy(instruction_t* instruction){
	switch(instruction->instruction_type){
		//These are our four candidates
		case MOVB:
		case MOVL:
		case MOVW:
		case MOVQ:
		//Movsd and movss are true copy operations for floating point values
		case MOVSD:
		case MOVSS:
			//If there's a source register we're good
			if(instruction->source_register != NULL
				&& instruction->memory_access_type == NO_MEMORY_ACCESS){
				return TRUE;
			}

			//Otherwise we're assigning a constant so this isn't a pure copy
			return FALSE;

		//By default this isn't
		default:
			return FALSE;
	}
}


/**
 * Is this a pure constant assignment instruction?
 */
u_int8_t is_instruction_constant_assignment(instruction_t* instruction){
	switch(instruction->instruction_type){
		//These are our four candidates
		case MOVB:
		case MOVL:
		case MOVW:
		case MOVQ:
			//Not a pure constant assignment
			if(instruction->memory_access_type != NO_MEMORY_ACCESS){
				return FALSE;
			}

			//If this is NULL, it also doesn't count
			if(instruction->source_immediate == NULL){
				return FALSE;
			}

			//Otherwise if we survive to here, we're good
			return TRUE;

		//By default this isn't
		default:
			return FALSE;
	}
}


/**
 * Dynamically allocate and create a temp var
 *
 * Temp Vars do NOT have their lightstack initialized. If ever you are using the stack of a temp
 * var, you are doing something seriously incorrect
*/
three_addr_var_t* emit_temp_var(generic_type_t* type){
	//Let's first create the temporary variable
	three_addr_var_t* var = calloc(1, sizeof(three_addr_var_t)); 

	//Add into here for memory management
	dynamic_array_add(&emitted_vars, var);

	//Mark this as temporary
	var->variable_type = VARIABLE_TYPE_TEMP;
	//Store the type info
	var->type = type;
	//Store the temp var number
	var->temp_var_number = increment_and_get_temp_id();

	//Select the size of this variable
	var->variable_size = get_type_size(type);

	//Finally we'll bail out
	return var;
}


/**
 * Emit a local constant temp var
 */
three_addr_var_t* emit_local_constant_temp_var(local_constant_t* local_constant){
	//Let's first create the temporary variable
	three_addr_var_t* var = calloc(1, sizeof(three_addr_var_t)); 

	//Add here for memory management
	dynamic_array_add(&emitted_vars, var);

	//This is a special kind of variable that is a local constant variable
	var->variable_type = VARIABLE_TYPE_LOCAL_CONSTANT;

	//Store the local constant inside of the memory region slot
	var->associated_memory_region.local_constant = local_constant;

	//We've used this more than one time
	(local_constant->reference_count)++;

	//Store the type
	var->type = local_constant->type;

	//The size is going to be the size of an address(8 bytes)
	var->variable_size = QUAD_WORD;

	//And give it back
	return var;
}


/**
 * Emit a function pointer temp var
 */
three_addr_var_t* emit_function_pointer_temp_var(symtab_function_record_t* function_record){
	//Let's first create the temporary variable
	three_addr_var_t* var = calloc(1, sizeof(three_addr_var_t)); 

	//Add here for memory management
	dynamic_array_add(&emitted_vars, var);

	//This is a special kind of variable that is a local constant variable
	var->variable_type = VARIABLE_TYPE_FUNCTION_ADDRESS;

	//Store the local constant inside of the memory region slot
	var->associated_memory_region.rip_relative_function = function_record;

	//The type is the signature
	var->type = function_record->signature;

	//The size is going to be the size of an address(8 bytes)
	var->variable_size = QUAD_WORD;

	//And give it back
	return var;
}


/**
 * Dynamically allocate and create a non-temp var. We emit a separate, distinct variable for 
 * each SSA generation. For instance, if we emit x1 and x2, they are distinct. The only thing 
 * that they share is the overall variable that they're linked back to, which stores their type information,
 * etc.
*/
three_addr_var_t* emit_var(symtab_variable_record_t* var){
	//Let's first create the non-temp variable
	three_addr_var_t* emitted_var = calloc(1, sizeof(three_addr_var_t));

	//Add into here for memory management
	dynamic_array_add(&emitted_vars, emitted_var);

	//If we have an aliased variable(almost exclusively function
	//parameters), we will instead emit the alias of that variable instead
	//of the variable itself
	if(var->alias != NULL){
		var = var->alias;
	}

	//This is not temporary
	emitted_var->variable_type = VARIABLE_TYPE_NON_TEMP;
	//We always store the type as the type with which this variable was defined in the CFG
	emitted_var->type = var->type_defined_as;
	//And store the symtab record
	emitted_var->linked_var = var;

	//Store the associate stack region(this is usually null)
	emitted_var->associated_memory_region.stack_region = var->stack_region;

	//The membership is also copied
	emitted_var->membership = var->membership;

	//Copy these over
	emitted_var->class_relative_parameter_order = var->class_relative_function_parameter_order;

	//Select the size of this variable
	emitted_var->variable_size = get_type_size(emitted_var->type);

	//And we're all done
	return emitted_var;
}


/**
 * Create and return a three address var from an existing variable. These special
 * "memory address vars" will represent the memory address of the variable in question
*/
three_addr_var_t* emit_memory_address_var(symtab_variable_record_t* var){
	//Let's first create the non-temp variable
	three_addr_var_t* emitted_var = calloc(1, sizeof(three_addr_var_t));

	//Add into here for memory management
	dynamic_array_add(&emitted_vars, emitted_var);

	//If we have an aliased variable(almost exclusively function
	//parameters), we will instead emit the alias of that variable instead
	//of the variable itself
	if(var->alias != NULL){
		var = var->alias;
	}

	//This is a memory address variable. We will flag this for special
	//printing
	emitted_var->variable_type = VARIABLE_TYPE_MEMORY_ADDRESS;

	//We always store the type as the type with which this variable was defined in the CFG
	emitted_var->type = var->type_defined_as;
	//And store the symtab record
	emitted_var->linked_var = var;

	//Store the associate stack region(this is usually null)
	emitted_var->associated_memory_region.stack_region = var->stack_region;

	//The membership is also copied
	emitted_var->membership = var->membership;

	//Copy these over
	emitted_var->class_relative_parameter_order= var->class_relative_function_parameter_order;

	//Select the size of this variable
	emitted_var->variable_size = get_type_size(emitted_var->type);

	//And we're all done
	return emitted_var;
}


/**
 * Create and return a three address var from an existing variable. These special
 * "memory address vars" will represent the memory address of the variable in question
*/
three_addr_var_t* emit_memory_address_temp_var(generic_type_t* type, stack_region_t* region){
	//Let's first create the non-temp variable
	three_addr_var_t* emitted_var = calloc(1, sizeof(three_addr_var_t));

	//Add into here for memory management
	dynamic_array_add(&emitted_vars, emitted_var);

	//This is a memory address variable. We will flag this for special
	//printing
	emitted_var->variable_type = VARIABLE_TYPE_MEMORY_ADDRESS;

	//We always store the type as the type with which this variable was defined in the CFG
	emitted_var->type = type;

	//Give it a temp var number
	emitted_var->temp_var_number = increment_and_get_temp_id();

	//Store the associate stack region(this is usually null)
	emitted_var->associated_memory_region.stack_region = region;

	//Select the size of this variable
	emitted_var->variable_size = get_type_size(emitted_var->type);

	//And we're all done
	return emitted_var;
}


/**
 * Emit a variable for an identifier node. This rule is designed to account for the fact that
 * some identifiers may have had their types casted / coerced, so we need to keep the actual
 * inferred type here
*/
three_addr_var_t* emit_var_from_identifier(symtab_variable_record_t* var, generic_type_t* inferred_type){
	//Let's first create the non-temp variable
	three_addr_var_t* emitted_var = calloc(1, sizeof(three_addr_var_t));

	//Add into here for memory management
	dynamic_array_add(&emitted_vars, emitted_var);

	//This is not temporary
	emitted_var->variable_type = VARIABLE_TYPE_NON_TEMP;

	//This variable's type will be what the identifier node deemed it as
	emitted_var->type = inferred_type;
	//And store the symtab record
	emitted_var->linked_var = var;

	//Select the size of this variable
	emitted_var->variable_size = get_type_size(emitted_var->type);

	//And we're all done
	return emitted_var;
}



/**
 * Create and return a temporary variable from a live range
*/
three_addr_var_t* emit_temp_var_from_live_range(live_range_t* range){
	//Let's first create the non-temp variable
	three_addr_var_t* emitted_var = calloc(1, sizeof(three_addr_var_t));

	//Add into here for memory management
	dynamic_array_add(&emitted_vars, emitted_var);

	//This is a temp var
	emitted_var->variable_type = VARIABLE_TYPE_TEMP;

	//Link this in with our live range
	emitted_var->associated_live_range = range;
	dynamic_array_add(&(range->variables), emitted_var);

	//These are always quad words
	emitted_var->variable_size = QUAD_WORD;

	//And we're all done
	return emitted_var;
}


/**
 * Emit a copy of this variable
 */
three_addr_var_t* emit_var_copy(three_addr_var_t* var){
	//Let's first create the non-temp variable
	three_addr_var_t* emitted_var = calloc(1, sizeof(three_addr_var_t));

	//Add into here for memory management
	dynamic_array_add(&emitted_vars, emitted_var);

	//Copy the memory
	memcpy(emitted_var, var, sizeof(three_addr_var_t));
	
	//Transfer this status over
	emitted_var->variable_type = var->variable_type;

	//Is this a stack pointer?
	emitted_var->is_stack_pointer = var->is_stack_pointer;

	//Copy the generation level
	emitted_var->ssa_generation = var->ssa_generation;

	return emitted_var;
}


/**
 * Emit a push instruction. We only have one kind of pushing - quadwords - we don't
 * deal with getting granular when pushing
 */
instruction_t* emit_push_instruction(three_addr_var_t* pushee){
	//First we'll allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//Now we set the type
	instruction->instruction_type = PUSH;

	//We only ever have a source
	instruction->source_register = pushee;

	//Finally give it back
	return instruction;
}


/**
 * Sometimes we just want to push a given register. We're able to do this
 * by directly emitting a push instruction with the register in it. This
 * saves us allocation overhead
 *
 * This rule is exclusively for general purpose registers
 */
instruction_t* emit_direct_gp_register_push_instruction(general_purpose_register_t reg){
	//First allocate
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//Set the type
	instruction->instruction_type = PUSH_DIRECT_GP;

	//Now we'll set the register
	instruction->push_or_pop_reg.gen_purpose = reg;

	//Now give it back
	return instruction;
}


/**
 * Sometimes we just want to pop a given register. We're able to do this
 * by directly emitting a pop instruction with the register in it. This
 * saves us allocation overhead
 *
 * This rule is exclusively for general purpose registers
 */
instruction_t* emit_direct_gp_register_pop_instruction(general_purpose_register_t reg){
	//First allocate
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//Set the type
	instruction->instruction_type = POP_DIRECT_GP;

	//Now we'll set the register
	instruction->push_or_pop_reg.gen_purpose = reg;

	//Now give it back
	return instruction;
}


/**
 * Emit a PXOR instruction that's already been instruction selected. This is intended to
 * be used by the instruction selector when we need to insert pxor functions for clearing
 * SSE registers
 */
instruction_t* emit_pxor_instruction(three_addr_var_t* destination, three_addr_var_t* source){
	//First allocate
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//Set the type
	instruction->instruction_type = PXOR;

	//The source and destination are the exact same
	instruction->destination_register = destination;
	instruction->source_register = source;

	//Now give it back
	return instruction;
}


/**
 * Emit a CLEAR instruction that is meant for the FP register to be zeroed out
 * This function only takes an assignee because that's all that we're clearing
 */
instruction_t* emit_floating_point_clear_instruction(three_addr_var_t* assignee){
	//First allocate
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//This is a clear instruction
	instruction->statement_type = THREE_ADDR_CODE_CLEAR_STMT;

	//We've only got an assignee
	instruction->assignee = assignee;

	//And give it back
	return instruction;
}


/**
 * Emit a pop instruction. We only have one kind of popping - quadwords - we don't
 * deal with getting granular when popping 
 */
instruction_t* emit_pop_instruction(three_addr_var_t* popee){
	//First we'll allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//Now we set the type
	instruction->instruction_type = POP;

	//We only ever have a source
	instruction->source_register = popee;

	//Finally give it back
	return instruction;
}


/**
 * Emit a lea statement that has one operand and an offset
 *
 * This would look something like lea 3(t5), t7
 */
instruction_t* emit_lea_offset_only(three_addr_var_t* assignee, three_addr_var_t* op1, three_addr_const_t* op1_const){
	//First we allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Now we'll make our populations
	stmt->statement_type = THREE_ADDR_CODE_LEA_STMT;
	stmt->assignee = assignee;
	stmt->op1 = op1;
	stmt->op1_const = op1_const;

	//This only has registers
	stmt->lea_statement_type = OIR_LEA_TYPE_OFFSET_ONLY;

	//And now we give it back
	return stmt;
}


/**
 * Emit a lea statement with no type size multiplier on it
 *
 * This is designed to emit things like lea (t2, t3), t5
 */
instruction_t* emit_lea_operands_only(three_addr_var_t* assignee, three_addr_var_t* op1, three_addr_var_t* op2){
	//First we allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Now we'll make our populations
	stmt->statement_type = THREE_ADDR_CODE_LEA_STMT;
	stmt->assignee = assignee;
	stmt->op1 = op1;
	stmt->op2 = op2;

	//This only has registers
	stmt->lea_statement_type = OIR_LEA_TYPE_REGISTERS_ONLY;

	//And now we give it back
	return stmt;
}


/**
 * Emit a statement that is in LEA form
 */
instruction_t* emit_lea_multiplier_and_operands(three_addr_var_t* assignee, three_addr_var_t* op1, three_addr_var_t* op2, u_int64_t type_size){
	//First we allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Now we'll make our populations
	stmt->statement_type = THREE_ADDR_CODE_LEA_STMT;
	stmt->assignee = assignee;
	stmt->op1 = op1;
	stmt->op2 = op2;
	stmt->lea_multiplier = type_size;

	//This has registers and a multiplier
	stmt->lea_statement_type = OIR_LEA_TYPE_REGISTERS_AND_SCALE;

	//And now we give it back
	return stmt;
}


/**
 * Emit a lea statement that is used for string calculation(rip relative)
 */
instruction_t* emit_lea_rip_relative_constant(three_addr_var_t* assignee, three_addr_var_t* local_constant, three_addr_var_t* instruction_pointer){
	//First we allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Now we'll make our populations
	stmt->statement_type = THREE_ADDR_CODE_LEA_STMT;
	stmt->assignee = assignee;
	stmt->op1 = instruction_pointer;
	stmt->op2 = local_constant;

	//This is a rip-relative lea
	stmt->lea_statement_type = OIR_LEA_TYPE_RIP_RELATIVE;

	//And now we give it back
	return stmt;
}


/**
 * Emit an indirect jump calculation that includes a block label in three address code form
 */
instruction_t* emit_indir_jump_address_calc_instruction(three_addr_var_t* assignee, void* op1, three_addr_var_t* op2, u_int64_t type_size){
	//First we allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Now we'll make our populations
	stmt->statement_type = THREE_ADDR_CODE_INDIR_JUMP_ADDR_CALC_STMT;
	stmt->assignee = assignee;
	//We store the jumping to block as our operand. It's really a jump table
	stmt->if_block = op1;
	stmt->op2 = op2;
	stmt->lea_multiplier= type_size;

	//And now we'll give it back
	return stmt;
}


/**
 * Directly emit an idle statement
 */
instruction_t* emit_idle_instruction(){
	//First we allocate
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Store the class
	stmt->statement_type = THREE_ADDR_CODE_IDLE_STMT;

	//And we're done
	return stmt;
}


/**
 * Print an 8-bit register out. The names used for these are still
 * 64 bits because 8, 16, 32 and 64 bit uses can't occupy the same register at the 
 * same time
 */
static void print_8_bit_register_name(FILE* fl, general_purpose_register_t reg){
	//One large switch based on what it is
	switch (reg) {
		case NO_REG_GEN_PURPOSE:
			fprintf(fl, "NOREG8");
			break;
		case RAX:
			fprintf(fl, "%%al");
			break;
		case RBX:
			fprintf(fl, "%%bl");
			break;
		case RCX:
			fprintf(fl, "%%cl");
			break;
		case RDX:
			fprintf(fl, "%%dl");
			break;
		case RSI:
			fprintf(fl, "%%sil");
			break;
		case RDI:
			fprintf(fl, "%%dil");
			break;
		case RBP:
			fprintf(fl, "%%bpl");
			break;
		case RSP:
			fprintf(fl, "%%spl");
			break;
		//This one should never happen
		case RIP:
			printf("ERROR");
			break;
		case R8:
			fprintf(fl, "%%r8b");
			break;
		case R9:
			fprintf(fl, "%%r9b");
			break;
		case R10:
			fprintf(fl, "%%r10b");
			break;
		case R11:
			fprintf(fl, "%%r11b");
			break;
		case R12:
			fprintf(fl, "%%r12b");
			break;
		case R13:
			fprintf(fl, "%%r13b");
			break;
		case R14:
			fprintf(fl, "%%r14b");
			break;
		case R15:
			fprintf(fl, "%%r15b");
			break;
	}
}



/**
 * Print a 16-bit register out. The names used for these are still
 * 64 bits because 32 and 64 bit uses can't occupy the same register at the 
 * same time
 */
static void print_16_bit_register_name(FILE* fl, general_purpose_register_t reg){
	//One large switch based on what it is
	switch (reg) {
		case NO_REG_GEN_PURPOSE:
			fprintf(fl, "NOREG16");
			break;
		case RAX:
			fprintf(fl, "%%ax");
			break;
		case RBX:
			fprintf(fl, "%%bx");
			break;
		case RCX:
			fprintf(fl, "%%cx");
			break;
		case RDX:
			fprintf(fl, "%%dx");
			break;
		case RSI:
			fprintf(fl, "%%si");
			break;
		case RDI:
			fprintf(fl, "%%di");
			break;
		case RBP:
			fprintf(fl, "%%bp");
			break;
		case RSP:
			fprintf(fl, "%%sp");
			break;
		//This one should never happen
		case RIP:
			printf("ERROR");
			break;
		case R8:
			fprintf(fl, "%%r8w");
			break;
		case R9:
			fprintf(fl, "%%r9w");
			break;
		case R10:
			fprintf(fl, "%%r10w");
			break;
		case R11:
			fprintf(fl, "%%r11w");
			break;
		case R12:
			fprintf(fl, "%%r12w");
			break;
		case R13:
			fprintf(fl, "%%r13w");
			break;
		case R14:
			fprintf(fl, "%%r14w");
			break;
		case R15:
			fprintf(fl, "%%r15w");
			break;
	}
}


/**
 * Print a 32-bit register out. The names used for these are still
 * 64 bits because 32 and 64 bit uses can't occupy the same register at the 
 * same time
 */
static void print_32_bit_register_name(FILE* fl, general_purpose_register_t reg){
	//One large switch based on what it is
	switch (reg) {
		case NO_REG_GEN_PURPOSE:
			fprintf(fl, "NOREG32");
			break;
		case RAX:
			fprintf(fl, "%%eax");
			break;
		case RBX:
			fprintf(fl, "%%ebx");
			break;
		case RCX:
			fprintf(fl, "%%ecx");
			break;
		case RDX:
			fprintf(fl, "%%edx");
			break;
		case RSI:
			fprintf(fl, "%%esi");
			break;
		case RDI:
			fprintf(fl, "%%edi");
			break;
		case RBP:
			fprintf(fl, "%%ebp");
			break;
		case RSP:
			fprintf(fl, "%%esp");
			break;
		//This one should never happen
		case RIP:
			printf("ERROR");
			break;
		case R8:
			fprintf(fl, "%%r8d");
			break;
		case R9:
			fprintf(fl, "%%r9d");
			break;
		case R10:
			fprintf(fl, "%%r10d");
			break;
		case R11:
			fprintf(fl, "%%r11d");
			break;
		case R12:
			fprintf(fl, "%%r12d");
			break;
		case R13:
			fprintf(fl, "%%r13d");
			break;
		case R14:
			fprintf(fl, "%%r14d");
			break;
		case R15:
			fprintf(fl, "%%r15d");
			break;
	}
}


/**
 * Print a 64 bit register name out
 */
static void print_64_bit_register_name(FILE* fl, general_purpose_register_t reg){
	//One large switch based on what it is
	switch (reg) {
		case NO_REG_GEN_PURPOSE:
			fprintf(fl, "NOREG64");
			break;
		case RAX:
			fprintf(fl, "%%rax");
			break;
		case RBX:
			fprintf(fl, "%%rbx");
			break;
		case RCX:
			fprintf(fl, "%%rcx");
			break;
		case RDX:
			fprintf(fl, "%%rdx");
			break;
		case RSI:
			fprintf(fl, "%%rsi");
			break;
		case RDI:
			fprintf(fl, "%%rdi");
			break;
		case RBP:
			fprintf(fl, "%%rbp");
			break;
		case RSP:
			fprintf(fl, "%%rsp");
			break;
		case RIP:
			fprintf(fl, "%%rip");
			break;
		case R8:
			fprintf(fl, "%%r8");
			break;
		case R9:
			fprintf(fl, "%%r9");
			break;
		case R10:
			fprintf(fl, "%%r10");
			break;
		case R11:
			fprintf(fl, "%%r11");
			break;
		case R12:
			fprintf(fl, "%%r12");
			break;
		case R13:
			fprintf(fl, "%%r13");
			break;
		case R14:
			fprintf(fl, "%%r14");
			break;
		case R15:
			fprintf(fl, "%%r15");
			break;
	}
}


/**
 * Print a single precision SSE register out
 */
void print_single_precision_sse_register(FILE* fl, sse_register_t reg){
	switch(reg){
		//Exclusively for debug purposes. Under normal operation, we shouldn't be hitting this
		case NO_REG_SSE:
			fprintf(fl, "NOREG Single Precision");
			break;
		case XMM0:
			fprintf(fl, "%%xmm0");
			break;
		case XMM1:
			fprintf(fl, "%%xmm1");
			break;
		case XMM2:
			fprintf(fl, "%%xmm2");
			break;
		case XMM3:
			fprintf(fl, "%%xmm3");
			break;
		case XMM4:
			fprintf(fl, "%%xmm4");
			break;
		case XMM5:
			fprintf(fl, "%%xmm5");
			break;
		case XMM6:
			fprintf(fl, "%%xmm6");
			break;
		case XMM7:
			fprintf(fl, "%%xmm7");
			break;
		case XMM8:
			fprintf(fl, "%%xmm8");
			break;
		case XMM9:
			fprintf(fl, "%%xmm9");
			break;
		case XMM10:
			fprintf(fl, "%%xmm10");
			break;
		case XMM11:
			fprintf(fl, "%%xmm11");
			break;
		case XMM12:
			fprintf(fl, "%%xmm12");
			break;
		case XMM13:
			fprintf(fl, "%%xmm13");
			break;
		case XMM14:
			fprintf(fl, "%%xmm14");
			break;
		case XMM15:
			fprintf(fl, "%%xmm15");
			break;
	}
}


/**
 * Print a double precision SSE register out
 */
void print_double_precision_sse_register(FILE* fl, sse_register_t reg){
	switch(reg){
		//Exclusively for debug purposes. Under normal operation, we shouldn't be hitting this
		case NO_REG_SSE:
			fprintf(fl, "NOREG Doulbe Precision");
			break;
		case XMM0:
			fprintf(fl, "%%xmm0");
			break;
		case XMM1:
			fprintf(fl, "%%xmm1");
			break;
		case XMM2:
			fprintf(fl, "%%xmm2");
			break;
		case XMM3:
			fprintf(fl, "%%xmm3");
			break;
		case XMM4:
			fprintf(fl, "%%xmm4");
			break;
		case XMM5:
			fprintf(fl, "%%xmm5");
			break;
		case XMM6:
			fprintf(fl, "%%xmm6");
			break;
		case XMM7:
			fprintf(fl, "%%xmm7");
			break;
		case XMM8:
			fprintf(fl, "%%xmm8");
			break;
		case XMM9:
			fprintf(fl, "%%xmm9");
			break;
		case XMM10:
			fprintf(fl, "%%xmm10");
			break;
		case XMM11:
			fprintf(fl, "%%xmm11");
			break;
		case XMM12:
			fprintf(fl, "%%xmm12");
			break;
		case XMM13:
			fprintf(fl, "%%xmm13");
			break;
		case XMM14:
			fprintf(fl, "%%xmm14");
			break;
		case XMM15:
			fprintf(fl, "%%xmm15");
			break;
	}
}


/**
 * Print a variable in name only. There are no spaces around the variable, and there
 * will be no newline inserted at all. This is meant solely for the use of the "print_three_addr_code_stmt"
 * and nothing more. This function is also designed to take into account the indirection aspected as well
 */
void print_variable(FILE* fl, three_addr_var_t* variable, variable_printing_mode_t mode){
	//Go based on what printing mode we're after
	switch(mode){
		case PRINTING_LIVE_RANGES:
			//Handle this special case
			if(variable->variable_type == VARIABLE_TYPE_LOCAL_CONSTANT){
				fprintf(fl, ".LC%d", variable->associated_memory_region.local_constant->local_constant_id);
				break;
			}

			fprintf(fl, "LR%d", variable->associated_live_range->live_range_id);
			break;

		case PRINTING_REGISTERS:
			//Handle this special case
			if(variable->variable_type == VARIABLE_TYPE_LOCAL_CONSTANT){
				fprintf(fl, ".LC%d", variable->associated_memory_region.local_constant->local_constant_id);
				break;
			}

			//Once we get to this, we need to based on the live range class. There
			//are 2 different classes - SSE and general purpose
			switch(variable->associated_live_range->live_range_class){
				case LIVE_RANGE_CLASS_GEN_PURPOSE:
					//Special edge case
					if(variable->associated_live_range->reg.gen_purpose == NO_REG_GEN_PURPOSE){
						fprintf(fl, "LR%d", variable->associated_live_range->live_range_id);
						break;
					}
					
					//Switch based on the variable's size to print out the register
					switch(variable->variable_size){
						case QUAD_WORD:
							print_64_bit_register_name(fl, variable->associated_live_range->reg.gen_purpose);
							break;
						case DOUBLE_WORD:
							print_32_bit_register_name(fl, variable->associated_live_range->reg.gen_purpose);
							break;
						case WORD:
							print_16_bit_register_name(fl, variable->associated_live_range->reg.gen_purpose);
							break;
						case BYTE:
							print_8_bit_register_name(fl, variable->associated_live_range->reg.gen_purpose);
							break;
						default:
							printf("Fatal internal compiler error: unknown/invalid general purpose variable size encountered\n");
							exit(1);
					}

					break;

				//SSE registers only have the option for single
				//or double precision
				case LIVE_RANGE_CLASS_SSE:
					//Special edge case
					if(variable->associated_live_range->reg.sse_reg == NO_REG_SSE){
						fprintf(fl, "LR%d", variable->associated_live_range->live_range_id);
						break;
					}

					//There are only 2 potential correct sizes here
					switch(variable->variable_size){
						case SINGLE_PRECISION:
							print_single_precision_sse_register(fl, variable->associated_live_range->reg.sse_reg);
							break;
						case DOUBLE_PRECISION:
							print_double_precision_sse_register(fl, variable->associated_live_range->reg.sse_reg);
							break;
						default:
							printf("Fatal internal compiler error: unknown/invalid SSE variable size encountered\n");
							exit(1);
					}

					break;
			}

			break;

		default:
			//Go based on our type of var here
			switch(variable->variable_type){
				case VARIABLE_TYPE_TEMP:
					//Print out it's temp var number
					fprintf(fl, "t%d", variable->temp_var_number);
					break;
				case VARIABLE_TYPE_NON_TEMP:
					//Print out the SSA generation along with the variable
					fprintf(fl, "%s_%d", variable->linked_var->var_name.string, variable->ssa_generation);
					break;
				case VARIABLE_TYPE_LOCAL_CONSTANT:
					fprintf(fl, ".LC%d", variable->associated_memory_region.local_constant->local_constant_id);
					break;
				case VARIABLE_TYPE_FUNCTION_ADDRESS:
					fprintf(fl, "%s", variable->associated_memory_region.rip_relative_function->func_name.string);
					break;
				case VARIABLE_TYPE_MEMORY_ADDRESS:
					if(variable->linked_var != NULL){
						//Print out the normal version, plus the MEM<> wrapper
						fprintf(fl, "MEM<%s_%d>", variable->linked_var->var_name.string, variable->ssa_generation);
					} else {
						fprintf(fl, "MEM<t%d>", variable->temp_var_number);
					}

					break;
			}

			break;
	}
}


/**
 * Print out a global variable string constant. All that this does is point to the internal
 * constant and print it
 */
static inline void print_global_variable_string_constant(FILE* fl, three_addr_const_t* string_constant){
	fprintf(fl, "\t.string \"%s\"\n", string_constant->constant_value.string_constant);
}


/**
 * Specialized printing based on what kind of constant is printed out
 * in a global variable context
 */
static void print_global_variable_constant(FILE* fl, three_addr_const_t* global_variable_constant){
	//For any float constant printing
	int32_t lower_32_bits;
	int32_t upper_32_bits;

	//Go based on the register type, not anything else. We will always print the signed version
	//because at the end of the day we're just trying to write down the bit values, nothing else
	switch(global_variable_constant->const_type){
		case CHAR_CONST:
			fprintf(fl, "\t.byte %d\n", global_variable_constant->constant_value.char_constant);
			break;
		case BYTE_CONST:
			fprintf(fl, "\t.byte %d\n", global_variable_constant->constant_value.signed_byte_constant);
			break;
		case BYTE_CONST_FORCE_U:
			fprintf(fl, "\t.byte %d\n", global_variable_constant->constant_value.unsigned_byte_constant);
			break;
		case SHORT_CONST:
			fprintf(fl, "\t.value %d\n", global_variable_constant->constant_value.signed_short_constant);
			break;
		case SHORT_CONST_FORCE_U:
			fprintf(fl, "\t.value %d\n", global_variable_constant->constant_value.unsigned_short_constant);
			break;
		case INT_CONST:
			fprintf(fl, "\t.long %d\n", global_variable_constant->constant_value.signed_integer_constant);
			break;
		case INT_CONST_FORCE_U:
			fprintf(fl, "\t.long %d\n", global_variable_constant->constant_value.unsigned_integer_constant);
			break;
		case LONG_CONST:
			fprintf(fl, "\t.quad %ld\n", global_variable_constant->constant_value.signed_long_constant);
			break;
		case LONG_CONST_FORCE_U:
			fprintf(fl, "\t.quad %ld\n", global_variable_constant->constant_value.unsigned_long_constant);
			break;
		case FLOAT_CONST:
			//Cast to an int, then dereference. we want the bytes, not an estimation
			lower_32_bits = *((int32_t*)(&(global_variable_constant->constant_value.float_constant)));
			fprintf(fl,  "\t.long %d\n", lower_32_bits);
			break;
		case DOUBLE_CONST:
			//Grab the lower 32 bits out first
			lower_32_bits = *((int64_t*)(&(global_variable_constant->constant_value.double_constant))) & 0xFFFFFFFF;
			//Then the upper 32
			upper_32_bits = (*((int64_t*)(&(global_variable_constant->constant_value.double_constant))) >> 32) & 0xFFFFFFFF;
			fprintf(fl,  "\t.long %d\n\t.long %d\n", lower_32_bits, upper_32_bits);
			break;
		case STR_CONST:
			print_global_variable_string_constant(fl, global_variable_constant);
			break;
		/**
		 * For a relative address constants, we print a quad word pointer to the local constant
		 */
		case REL_ADDRESS_CONST:
			fprintf(fl, "\t.quad .LC%d\n", global_variable_constant->constant_value.local_constant_address->associated_memory_region.local_constant->local_constant_id); 
			break;
		//Catch-all should anything go wrong
		default:
			printf("Fatal internal compiler error: unrecognized global variable type encountered\n");
			exit(1);
	}
}


/**
 * Print all given global variables who's use count is not 0
 */
void print_all_global_variables(FILE* fl, dynamic_array_t* global_variables){
	//Just bail out if this is the case
	if(global_variables == NULL || global_variables->current_index == 0){
		return;
	}

	//If it's needed later on
	dynamic_array_t array_initializer_values;

	//Run through all of them
	for(u_int16_t i = 0; i < global_variables->current_index; i++){
		//Grab the variable out
		global_variable_t* variable = dynamic_array_get_at(global_variables, i);

		//Extract the name
		char* name = variable->variable->var_name.string;

		//Mark that this is global(globl)
		fprintf(fl, "\t.globl %s\n", name);

		//If it's not initialized, it goes to .bss. If it is initialized, it
		//goes to .data
		if(variable->initializer_type == GLOBAL_VAR_INITIALIZER_NONE){
			fprintf(fl, "\t.bss\n");
		//Tell the linker that this is relative writeable data
		} else if(variable->is_relative == TRUE) {
			fprintf(fl, "\t.section .data.rel.local,\"aw\"\n");
		} else {
			fprintf(fl, "\t.data\n");
		}

		//Now print out the alignment
		fprintf(fl, "\t.align %d\n", get_data_section_alignment(variable->variable->type_defined_as));
		
		//Now print out our type, it's always @Object
		fprintf(fl, "\t.type %s, @object\n", name);

		//Now emit the relevant type
		fprintf(fl, "\t.size %s, %d\n", name, variable->variable->type_defined_as->type_size);

		//Now fianlly we'll print the value out
		fprintf(fl, "%s:\n", name);

		//Go based on what kind of initializer we have
		switch(variable->initializer_type){
			//If we have no initializer, we make everything go to zero
			case GLOBAL_VAR_INITIALIZER_NONE:
				fprintf(fl, "\t.zero %d\n", variable->variable->type_defined_as->type_size);
				break;
				
			//For a constant, we print the value out as a .long
			case GLOBAL_VAR_INITIALIZER_CONSTANT:
				//We'll add some special handling here - some constants take special treatment
				print_global_variable_constant(fl, variable->initializer_value.constant_value);
				break;

			//Handle a global var string constant
			case GLOBAL_VAR_INITIALIZER_STRING:
				print_global_variable_string_constant(fl, variable->initializer_value.constant_value);
				break;

			//For an array, we loop through and print them all as constants in order
			case GLOBAL_VAR_INITIALIZER_ARRAY:
				//Extract this
				array_initializer_values = variable->initializer_value.array_initializer_values;

				//Run through all the values
				for(u_int16_t i = 0; i < array_initializer_values.current_index; i++){
					//These will always be constant values
					three_addr_const_t* constant_value = dynamic_array_get_at(&array_initializer_values, i);

					//Emit the constant value here
					print_global_variable_constant(fl, constant_value);
				}

				break;
				
			default:
				printf("Fatal internal compiler error: Unrecognized global variable initializer type\n");
				exit(1);
		}
	}
}


/**
 * Print a live range out
 */
void print_live_range(FILE* fl, live_range_t* live_range){
	fprintf(fl, "LR%d", live_range->live_range_id);
}


/**
 * Print a constant. This is a helper method to avoid excessive code duplication
 */
static void print_three_addr_constant(FILE* fl, three_addr_const_t* constant){
	switch(constant->const_type){
		case BYTE_CONST:
			fprintf(fl, "%d", constant->constant_value.signed_byte_constant);
			break;
		case BYTE_CONST_FORCE_U:
			fprintf(fl, "%d", constant->constant_value.unsigned_byte_constant);
			break;
		case SHORT_CONST:
			fprintf(fl, "%d", constant->constant_value.signed_short_constant);
			break;
		case SHORT_CONST_FORCE_U:
			fprintf(fl, "%d", constant->constant_value.unsigned_short_constant);
			break;
		case INT_CONST:
			fprintf(fl, "%d", constant->constant_value.signed_integer_constant);
			break;
		case INT_CONST_FORCE_U:
			fprintf(fl, "%d", constant->constant_value.unsigned_integer_constant);
			break;
		case LONG_CONST:
			fprintf(fl, "%ld", constant->constant_value.signed_long_constant);
			break;
		case LONG_CONST_FORCE_U:
			fprintf(fl, "%ld", constant->constant_value.unsigned_long_constant);
			break;
		case CHAR_CONST:
			//Special case here to for display reasons
			if(constant->constant_value.char_constant == 0){
				fprintf(fl, "'\\0'");
			} else {
				fprintf(fl, "'%c'", constant->constant_value.char_constant);
			}
			break;
		//To stop compiler warnings
		default:
			printf("Fatal Internal Compiler Error: Attempt to print unrecognized function type");
			exit(1);
	}
}


/**
 * Turn an operand into a string
 */
static char* op_to_string(ollie_token_t op){
	//Whatever we have here
	switch (op) {
		case PLUS:
			return "+";
		case MINUS:
			return "-";
		case STAR:
			return "*";
		case F_SLASH:
			return "/";
		case MOD:
			return "%";
		case G_THAN:
			return ">";
		case L_THAN:
			return "<";
		case L_SHIFT:
			return "<<";
		case R_SHIFT:
			return ">>";
		case SINGLE_AND:
			return "&";
		case SINGLE_OR:
			return "|";
		case CARROT:
			return "^";
		case DOUBLE_OR:
			return "||";
		case DOUBLE_AND:
			return "&&";
		case DOUBLE_EQUALS:
			return "==";
		case NOT_EQUALS:
			return "!=";
		case G_THAN_OR_EQ:
			return ">=";
		case L_THAN_OR_EQ:
			return "<=";
		//Should never happen, but just in case
		default:
			exit(1);
	}
}


/**
 * Convert a jump type to a string
 */
static char* branch_type_to_string(branch_type_t branch_type){
	switch(branch_type){
		case BRANCH_A:
			return "cbranch_a";
		case BRANCH_AE:
			return "cbranch_ae";
		case BRANCH_B:
			return "cbranch_b";
		case BRANCH_BE:
			return "cbranch_be";
		case BRANCH_E:
			return "cbranch_e";
		case BRANCH_NE:
			return "cbranch_ne";
		case BRANCH_Z:
			return "cbranch_z";
		case BRANCH_NZ:
			return "cbranch_nz";
		case BRANCH_GE:
			return "cbranch_ge";
		case BRANCH_G:
			return "cbranch_g";
		case BRANCH_LE:
			return "cbranch_le";
		case BRANCH_L:
			return "cbranch_l";
		//SHould never get here
		default:
			fprintf(stderr, "Fatal internal compiler error: Invalid branch type detected");
			exit(1);
	}
}


/**
 * Pretty print a three address code statement
 *
*/
void print_three_addr_code_stmt(FILE* fl, instruction_t* stmt){
	//For later use
	dynamic_array_t func_params;

	//Go based on what our statatement class is
	switch(stmt->statement_type){
		case THREE_ADDR_CODE_BIN_OP_STMT:
			//This one comes first
			print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);

			//Then the arrow
			fprintf(fl, " <- ");

			//Now we'll do op1, token, op2
			print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
			fprintf(fl, " %s ", op_to_string(stmt->op));
			print_variable(fl, stmt->op2, PRINTING_VAR_INLINE);

			//And end it out here
			fprintf(fl, "\n");
			break;

		case THREE_ADDR_CODE_SETNE_STMT:
			fprintf(fl, "setne ");

			//And then the var
			print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);

			fprintf(fl, "\n");
			break;

		case THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT:
			//This one comes first
			print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);

			//Then the arrow
			fprintf(fl, " <- ");

			//Now we'll do op1, token, op2
			print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
			fprintf(fl, " %s ", op_to_string(stmt->op));

			//Print the constant out
			print_three_addr_constant(fl, stmt->op1_const);

			//We need a newline here
			fprintf(fl, "\n");
			break;

		case THREE_ADDR_CODE_ASSN_STMT:
			//We'll print out the left and right ones here
			print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);
			fprintf(fl, " <- ");
			print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
			fprintf(fl, "\n");
			break;

		//Special kind of statement for things like "if(x)"
		case THREE_ADDR_CODE_TEST_IF_NOT_ZERO_STMT:
			print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);
			fprintf(fl, " <- Test if not zero ");
			print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
			fprintf(fl, "\n");

			break;

		case THREE_ADDR_CODE_ASSN_CONST_STMT:
			//First print out the assignee
			print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);
			fprintf(fl, " <- ");

			//Print the constant out
			print_three_addr_constant(fl, stmt->op1_const);
			//Newline needed
			fprintf(fl, "\n");
			break;

		case THREE_ADDR_CODE_RET_STMT:
			fprintf(fl, "ret ");

			//If it has a returned variable
			if(stmt->op1 != NULL){
				print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
			}
			
			//No matter what, print a newline
			fprintf(fl, "\n");
			break;

		/**
		 * These print out as
		 *
		 * store x <- storee
		 */
		case THREE_ADDR_CODE_STORE_STATEMENT:
			fprintf(fl, "store ");
			print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);
			fprintf(fl, " <- ");
			//Finally the storee(op1 or op1_const)
			if(stmt->op1 != NULL){
				print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
			} else {
				print_three_addr_constant(fl, stmt->op1_const);
			}
			fprintf(fl, "\n");
			break;

		/**
		 * These print out like
		 *
		 * store x[offset] <- storee
		 */
		case THREE_ADDR_CODE_STORE_WITH_CONSTANT_OFFSET:
			//First the base address(assignee)
			fprintf(fl, "store ");
			print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);

			//Then the constant offset
			fprintf(fl, "["); 
			print_three_addr_constant(fl, stmt->offset);
			fprintf(fl, "] <- "); 

			//Finally the storee(op2 or op1_const)
			if(stmt->op2 != NULL){
				print_variable(fl, stmt->op2, PRINTING_VAR_INLINE);
			} else {
				print_three_addr_constant(fl, stmt->op1_const);
			}

			fprintf(fl, "\n");

			break;

		/**
		 * These print out like
		 *
		 * store x[offset] <- storee
		 */
		case THREE_ADDR_CODE_STORE_WITH_VARIABLE_OFFSET:
			//First the base address(assignee)
			fprintf(fl, "store ");
			print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);

			//Then the variable offset(op1)
			fprintf(fl, "["); 
			print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
			fprintf(fl, "] <- "); 

			//Finally the storee(op2 or op1_const)
			if(stmt->op2 != NULL){
				print_variable(fl, stmt->op2, PRINTING_VAR_INLINE);
			} else {
				print_three_addr_constant(fl, stmt->op1_const);
			}

			fprintf(fl, "\n");

			break;


		/**
		 * These print out like
		 *
		 * load assignee <- x_0
		 */
		case THREE_ADDR_CODE_LOAD_STATEMENT:
			fprintf(fl, "load ");
			print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);
			fprintf(fl, " <- "); 
			print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
			fprintf(fl, "\n");
			break;

		/**
		 * These print out like
		 *
		 * load assignee <- x[offset] 
		 */
		case THREE_ADDR_CODE_LOAD_WITH_CONSTANT_OFFSET:
			//First the assignee
			fprintf(fl, "load ");
			print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);
			fprintf(fl, " <- ");

			//Now the base address
			print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);

			//Then the constant offset
			fprintf(fl, "["); 
			print_three_addr_constant(fl, stmt->offset);
			fprintf(fl, "]"); 

			fprintf(fl, "\n");

			break;

		/**
		 * These print out like
		 *
		 * store x[offset] <- storee
		 */
		case THREE_ADDR_CODE_LOAD_WITH_VARIABLE_OFFSET:
			//First the assignee
			fprintf(fl, "load ");
			print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);
			fprintf(fl, " <- ");

			//Now the base address
			print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);

			//Then the constant offset
			fprintf(fl, "["); 
			print_variable(fl, stmt->op2, PRINTING_VAR_INLINE);
			fprintf(fl, "]"); 

			fprintf(fl, "\n");
			break;

		case THREE_ADDR_CODE_JUMP_STMT:
			//Then print out the block label
			fprintf(fl, "jmp .L%d\n", ((basic_block_t*)(stmt->if_block))->block_id);
			break;

		//Branch statements represent the ends of blocks
		case THREE_ADDR_CODE_BRANCH_STMT:
			fprintf(fl, "%s .L%d else .L%d\n", branch_type_to_string(stmt->branch_type), ((basic_block_t*)(stmt->if_block))->block_id, ((basic_block_t*)(stmt->else_block))->block_id);
			break;

		case THREE_ADDR_CODE_FUNC_CALL:
			//First we'll print out the assignment, if one exists
			if(stmt->assignee != NULL){
				//Print the variable and assop out
				print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);
				fprintf(fl, " <- ");
			}

			//No matter what, we'll need to see the "call" keyword, followed
			//by the function name
			fprintf(fl, "call %s(", stmt->called_function->func_name.string);

			//Grab this out
			func_params = stmt->parameters;

			//If we event have any
			if(func_params.internal_array != NULL){
				//Now we can go through and print out all of our parameters here
				for(u_int16_t i = 0; i < func_params.current_index; i++){
					//Grab it out
					three_addr_var_t* func_param = dynamic_array_get_at(&func_params, i);
					
					//Print this out here
					print_variable(fl, func_param, PRINTING_VAR_INLINE);

					//If we need to, print out a comma
					if(i != func_params.current_index - 1){
						fprintf(fl, ", ");
					}
				}
			}

			//Now at the very end, close the whole thing out
			fprintf(fl, ")\n");
			break;

		case THREE_ADDR_CODE_INDIRECT_FUNC_CALL:
			//First we'll print out the assignment, if one exists
			if(stmt->assignee != NULL){
				//Print the variable and assop out
				print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);
				fprintf(fl, " <- ");
			}

			//Print out the call here
			fprintf(fl, "call *");

			//Now we'll use the helper to print the variable name
			print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);

			//Now we can print the opening parenthesis
			fprintf(fl, "(");

			//Grab this out
			func_params = stmt->parameters;

			//If we event have any
			if(func_params.internal_array != NULL){
				//Now we can go through and print out all of our parameters here
				for(u_int16_t i = 0; i < func_params.current_index; i++){
					//Grab it out
					three_addr_var_t* func_param = dynamic_array_get_at(&func_params, i);
					
					//Print this out here
					print_variable(fl, func_param, PRINTING_VAR_INLINE);

					//If we need to, print out a comma
					if(i != func_params.current_index - 1){
						fprintf(fl, ", ");
					}
				}
			}

			//Now at the very end, close the whole thing out
			fprintf(fl, ")\n");
			break;
		
		case THREE_ADDR_CODE_INC_STMT:
			fprintf(fl, "inc ");
			print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);
			fprintf(fl, "\n");
			break;

		case THREE_ADDR_CODE_DEC_STMT:
			fprintf(fl, "dec ");
			print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);
			fprintf(fl, "\n");
			break;

		case THREE_ADDR_CODE_BITWISE_NOT_STMT:
			print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);
			fprintf(fl, " <- not ");
			print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
			fprintf(fl, "\n");
			break;

		case THREE_ADDR_CODE_NEG_STATEMENT:
			print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);
			fprintf(fl, " <- neg ");
			print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
			fprintf(fl, "\n");
			break;

		case THREE_ADDR_CODE_LOGICAL_NOT_STMT:
			print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);
			//We will use a sequence of commands to do this
			fprintf(fl, " <- logical_not ");
			print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
			fprintf(fl, "\n");
			break;

		case THREE_ADDR_CODE_ASM_INLINE_STMT:
			//Should already have a trailing newline
			fprintf(fl, "%s\n", stmt->inlined_assembly.string);
			break;

		case THREE_ADDR_CODE_IDLE_STMT:
			//Just print a nop
			fprintf(fl, "nop\n");
			break;

		case THREE_ADDR_CODE_LEA_STMT:
			//Var name comes first
			print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);

			//Print the assignment operator
			fprintf(fl, " <- ");

			//Go based on what lea statement type 
			//we have
			switch(stmt->lea_statement_type){
				//We have something like t2 <- 3(t3)
				case OIR_LEA_TYPE_OFFSET_ONLY:
					//Print the constant out first
					print_three_addr_constant(fl, stmt->op1_const);

					//Then the variable encased in parenthesis
					fprintf(fl, "(");
					print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
					fprintf(fl, ")");

					break;

				case OIR_LEA_TYPE_REGISTERS_ONLY:
					//Print both variables encase in parenthesis
					fprintf(fl, "(");
					print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
					fprintf(fl, ", ");
					print_variable(fl, stmt->op2, PRINTING_VAR_INLINE);
					fprintf(fl, ")");

					break;

				case OIR_LEA_TYPE_REGISTERS_AND_OFFSET:
					//Print the constant out first
					print_three_addr_constant(fl, stmt->op1_const);
					
					//Print both variables encase in parenthesis
					fprintf(fl, "(");
					print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
					fprintf(fl, ", ");
					print_variable(fl, stmt->op2, PRINTING_VAR_INLINE);
					fprintf(fl, ")");

					break;

				case OIR_LEA_TYPE_REGISTERS_AND_SCALE:
					//Print both variables encase in parenthesis
					fprintf(fl, "(");
					print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
					fprintf(fl, ", ");
					print_variable(fl, stmt->op2, PRINTING_VAR_INLINE);

					//Now print the multiplier
					fprintf(fl, ", %ld)", stmt->lea_multiplier);

					break;

				case OIR_LEA_TYPE_RIP_RELATIVE:
					print_variable(fl, stmt->op2, PRINTING_VAR_INLINE);
					fprintf(fl, "(");
					print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
					fprintf(fl, ")");
					break;

				case OIR_LEA_TYPE_RIP_RELATIVE_WITH_OFFSET:
					print_three_addr_constant(fl, stmt->op1_const);
					fprintf(fl, "+");
					print_variable(fl, stmt->op2, PRINTING_VAR_INLINE);
					fprintf(fl, "(");
					print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
					fprintf(fl, ")");
					break;


				case OIR_LEA_TYPE_REGISTERS_OFFSET_AND_SCALE:
					//Print the constant out first
					print_three_addr_constant(fl, stmt->op1_const);

					//Print both variables encase in parenthesis
					fprintf(fl, "(");
					print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
					fprintf(fl, ", ");
					print_variable(fl, stmt->op2, PRINTING_VAR_INLINE);

					//Now print the multiplier
					fprintf(fl, ", %ld)", stmt->lea_multiplier);

				case OIR_LEA_TYPE_INDEX_AND_SCALE:
					//Print out the scale and multiplier
					fprintf(fl, "( , ");
					print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
					fprintf(fl, ", %ld)", stmt->lea_multiplier);

					break;

				case OIR_LEA_TYPE_INDEX_OFFSET_AND_SCALE:
					//Print the offset first
					print_three_addr_constant(fl, stmt->op1_const);
					//Print out the scale and multiplier
					fprintf(fl, "( , ");
					print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
					fprintf(fl, ", %ld)", stmt->lea_multiplier);

					break;

				//Should be unreachable
				default:
					printf("Fatal internal compiler error: unknown lea statement type hit\n");
					exit(1);
			}

			fprintf(fl, "\n");
			break;

		case THREE_ADDR_CODE_PHI_FUNC:
			//Print it in block header mode
			print_variable(fl, stmt->assignee, PRINTING_VAR_BLOCK_HEADER);
			fprintf(fl, " <- PHI(");

			//For convenience
			dynamic_array_t phi_func_params = stmt->parameters;

			//Now run through all of the parameters if we have any
			if(phi_func_params.internal_array != NULL){
				for(u_int16_t _ = 0; _ < phi_func_params.current_index; _++){
					//Print out the variable
					print_variable(fl, dynamic_array_get_at(&phi_func_params, _), PRINTING_VAR_BLOCK_HEADER);

					//If it isn't the very last one, add a comma space
					if(_ != phi_func_params.current_index - 1){
						fprintf(fl, ", ");
					}
				}
			}

			fprintf(fl, ")\n");
			break;

		case THREE_ADDR_CODE_INDIR_JUMP_ADDR_CALC_STMT:
			print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);

			//Print out the jump block ID
			fprintf(fl, " <- .JT%d + ", ((jump_table_t*)(stmt->if_block))->jump_table_id);
			
			//Now print out the variable
			print_variable(fl, stmt->op2, PRINTING_VAR_INLINE);

			//Finally the multiplicator
			fprintf(fl, " * %ld\n", stmt->lea_multiplier);
			break;

		case THREE_ADDR_CODE_INDIRECT_JUMP_STMT:
			//Indirection
			fprintf(fl, "jmp *");

			//Now the variable
			print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
			fprintf(fl, "\n");
			break;

		case THREE_ADDR_CODE_CLEAR_STMT:
			fprintf(fl, "clear_sse ");
			print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);
			fprintf(fl, "\n");

			break;

		case THREE_ADDR_CODE_STACK_ALLOCATION_STMT:
			fprintf(fl, "Stack Allocate <- ");
			print_three_addr_constant(fl, stmt->op1_const);
			fprintf(fl, " bytes\n");

			break;

		case THREE_ADDR_CODE_STACK_DEALLOCATION_STMT:
			fprintf(fl, "Stack Deallocate <- ");
			print_three_addr_constant(fl, stmt->op1_const);
			fprintf(fl, " bytes\n");

			break;

		default:
			printf("UNKNOWN TYPE");
			break;
	}
}


/**
 * Print a constant as an immediate($ prefixed) value
 */
static void print_immediate_value(FILE* fl, three_addr_const_t* constant){
	switch(constant->const_type){
		case BYTE_CONST:
			fprintf(fl, "$%d", constant->constant_value.signed_byte_constant);
			break;
		case BYTE_CONST_FORCE_U:
			fprintf(fl, "$%d", constant->constant_value.unsigned_byte_constant);
			break;
		case SHORT_CONST:
			fprintf(fl, "$%d", constant->constant_value.signed_short_constant);
			break;
		case SHORT_CONST_FORCE_U:
			fprintf(fl, "$%d", constant->constant_value.unsigned_short_constant);
			break;
		case INT_CONST:
			fprintf(fl, "$%d", constant->constant_value.signed_integer_constant);
			break;
		case INT_CONST_FORCE_U:
			fprintf(fl, "$%d", constant->constant_value.unsigned_integer_constant);
			break;
		case LONG_CONST:
			fprintf(fl, "$%ld", constant->constant_value.signed_long_constant);
			break;
		case LONG_CONST_FORCE_U:
			fprintf(fl, "$%ld", constant->constant_value.unsigned_long_constant);
			break;
		case CHAR_CONST:
			fprintf(fl, "$%d", constant->constant_value.char_constant);
			break;
		//To avoid compiler complaints
		default:
			printf("Fatal internal compiler error: unreachable immediate value type hit\n");
			exit(1);
	}
}


/**
 * Print a constant as an immediate(not $ prefixed) value
 */
static void print_immediate_value_no_prefix(FILE* fl, three_addr_const_t* constant){
	switch(constant->const_type){
		case BYTE_CONST:
			if(constant->constant_value.signed_byte_constant != 0){
				fprintf(fl, "%d", constant->constant_value.signed_byte_constant);
			}
			break;
		case BYTE_CONST_FORCE_U:
			if(constant->constant_value.unsigned_byte_constant != 0){
				fprintf(fl, "%d", constant->constant_value.unsigned_byte_constant);
			}
			break;
		case SHORT_CONST:
			if(constant->constant_value.signed_short_constant != 0){
				fprintf(fl, "%d", constant->constant_value.signed_short_constant);
			}
			break;
		case SHORT_CONST_FORCE_U:
			if(constant->constant_value.unsigned_short_constant != 0){
				fprintf(fl, "%d", constant->constant_value.unsigned_short_constant);
			}
			break;
		case INT_CONST:
			if(constant->constant_value.signed_integer_constant != 0){
				fprintf(fl, "%d", constant->constant_value.signed_integer_constant);
			}
			break;
		case INT_CONST_FORCE_U:
			if(constant->constant_value.unsigned_integer_constant != 0){
				fprintf(fl, "%d", constant->constant_value.unsigned_integer_constant);
			}
			break;
		case LONG_CONST:
			if(constant->constant_value.signed_long_constant != 0){
				fprintf(fl, "%ld", constant->constant_value.signed_long_constant);
			}
			break;
		case LONG_CONST_FORCE_U:
			if(constant->constant_value.unsigned_long_constant != 0){
				fprintf(fl, "%ld", constant->constant_value.unsigned_long_constant);
			}
			break;
		case CHAR_CONST:
			if(constant->constant_value.char_constant != 0){
				fprintf(fl, "%d", constant->constant_value.char_constant);
			}
			break;
		//To avoid compiler complaints
		default:
			printf("Fatal internal compiler error: unreachable immediate value type hit\n");
			exit(1);
	}
}


/**
 * Print out a complex addressing mode expression
 */
static void print_addressing_mode_expression(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	switch (instruction->calculation_mode) {
		/**
		 * This is the case where we only have a deref
		 */
		case ADDRESS_CALCULATION_MODE_DEREF_ONLY_SOURCE:
		case ADDRESS_CALCULATION_MODE_DEREF_ONLY_DEST:
			fprintf(fl, "(");

			if(instruction->calculation_mode == ADDRESS_CALCULATION_MODE_DEREF_ONLY_SOURCE){
				print_variable(fl, instruction->source_register, mode);
			} else {
				print_variable(fl, instruction->destination_register, mode);
			}
			fprintf(fl, ")");

			break;

		/**
		 * Global var address calculation
		 */
		case ADDRESS_CALCULATION_MODE_RIP_RELATIVE:
			//There are different ways that this can go
			switch(instruction->rip_offset_variable->variable_type){
				case VARIABLE_TYPE_LOCAL_CONSTANT:
					fprintf(fl, ".LC%d", instruction->rip_offset_variable->associated_memory_region.local_constant->local_constant_id);
					break;
				case VARIABLE_TYPE_FUNCTION_ADDRESS:
					fprintf(fl, "%s", instruction->rip_offset_variable->associated_memory_region.rip_relative_function->func_name.string);
					break;
				default:
					fprintf(fl, "%s", instruction->rip_offset_variable->linked_var->var_name.string);
					break;
			}

			//Print the actual string name of the variable - no SSA and no registers
			fprintf(fl, "(");
			//This will be the instruction pointer
			print_variable(fl, instruction->address_calc_reg1, mode);
			fprintf(fl, ")");

		   	break;

		/**
		 * Global var address calculation with offset
		 */
		case ADDRESS_CALCULATION_MODE_RIP_RELATIVE_WITH_OFFSET:
			print_immediate_value_no_prefix(fl, instruction->offset);
			//There are different ways that this can go
			switch(instruction->rip_offset_variable->variable_type){
				case VARIABLE_TYPE_LOCAL_CONSTANT:
					fprintf(fl, "+.LC%d", instruction->rip_offset_variable->associated_memory_region.local_constant->local_constant_id);
					break;
				case VARIABLE_TYPE_FUNCTION_ADDRESS:
					fprintf(fl, "%s", instruction->rip_offset_variable->associated_memory_region.rip_relative_function->func_name.string);
					break;
				default:
					fprintf(fl, "+%s", instruction->rip_offset_variable->linked_var->var_name.string);
					break;
			}
			fprintf(fl, "(");
			//This will be the instruction pointer
			print_variable(fl, instruction->address_calc_reg1, mode);
			fprintf(fl, ")");

		   	break;

		/**
		 * If we get here, that means we have this kind
		 * of address mode
		 *
		 * (%rax, %rbx, 2)
		 * (address_calc_reg1, address_calc_reg2, lea_mult)
		 */
		case ADDRESS_CALCULATION_MODE_REGISTERS_AND_SCALE:
			fprintf(fl, "(");
			print_variable(fl, instruction->address_calc_reg1, mode);
			fprintf(fl, ", ");
			print_variable(fl, instruction->address_calc_reg2, mode);
			fprintf(fl, ", ");
			fprintf(fl, "%ld", instruction->lea_multiplier);
			fprintf(fl, ")");
			break;

		case ADDRESS_CALCULATION_MODE_OFFSET_ONLY:
			//Only print this if it's not 0
			print_immediate_value_no_prefix(fl, instruction->offset);
			fprintf(fl, "(");
			print_variable(fl, instruction->address_calc_reg1, mode);
			fprintf(fl, ")");
			break;

		case ADDRESS_CALCULATION_MODE_REGISTERS_ONLY:
			fprintf(fl, "(");
			print_variable(fl, instruction->address_calc_reg1, mode);
			fprintf(fl, ", ");
			print_variable(fl, instruction->address_calc_reg2, mode);
			fprintf(fl, ")");
			break;

		case ADDRESS_CALCULATION_MODE_REGISTERS_AND_OFFSET:
			//Only print this if it's not 0
			print_immediate_value_no_prefix(fl, instruction->offset);
			fprintf(fl, "(");
			print_variable(fl, instruction->address_calc_reg1, mode);
			fprintf(fl, ", ");
			print_variable(fl, instruction->address_calc_reg2, mode);
			fprintf(fl, ")");
			break;

		case ADDRESS_CALCULATION_MODE_REGISTERS_OFFSET_AND_SCALE:
			//Only print this if it's not 0
			print_immediate_value_no_prefix(fl, instruction->offset);
			fprintf(fl, "(");
			print_variable(fl, instruction->address_calc_reg1, mode);
			fprintf(fl, ", ");
			print_variable(fl, instruction->address_calc_reg2, mode);
			fprintf(fl, ", %ld)", instruction->lea_multiplier);
			break;

		//Index is in address calc reg 1 for this
		case ADDRESS_CALCULATION_MODE_INDEX_AND_SCALE:
			fprintf(fl, "( , ");
			print_variable(fl, instruction->address_calc_reg1, mode);
			fprintf(fl, ", %ld)", instruction->lea_multiplier);
			break;
			
		//Index is in address calc reg 1 for this
		case ADDRESS_CALCULATION_MODE_INDEX_OFFSET_AND_SCALE:
			print_immediate_value_no_prefix(fl, instruction->offset);
			fprintf(fl, "( , ");
			print_variable(fl, instruction->address_calc_reg1, mode);
			fprintf(fl, ", %ld)", instruction->lea_multiplier);
			break;

		//Do nothing
		default:
			break;
	}
}


/**
 * Print the move instruction corresponding to the code given to the file pointer given
 */
static inline void print_move_instruction(FILE* fl, instruction_type_t instruction_type){
	//What we need to print out here
	switch(instruction_type){
		case MOVQ:
			fprintf(fl, "movq ");
			break;
		case MOVL:
			fprintf(fl, "movl ");
			break;
		case MOVW:
			fprintf(fl, "movw ");
			break;
		case MOVD:
			fprintf(fl, "movd ");
			break;
		case MOVB:
			fprintf(fl, "movb ");
			break;
		case MOVSBW:
			fprintf(fl, "movsbw ");
			break;
		case MOVSBL:
			fprintf(fl, "movsbl ");
			break;
		case MOVSBQ:
			fprintf(fl, "movsbq ");
			break;
		case MOVSWL:
			fprintf(fl, "movswl ");
			break;
		case MOVSWQ:
			fprintf(fl, "movswq ");
			break;
		case MOVSLQ:
			fprintf(fl, "movslq ");
			break;
		case MOVZBW:
			fprintf(fl, "movzbw ");
			break;
		case MOVZBL:
			fprintf(fl, "movzbl ");
			break;
		case MOVZBQ:
			fprintf(fl, "movzbq ");
			break;
		case MOVZWL:
			fprintf(fl, "movzwl ");
			break;
		case MOVZWQ:
			fprintf(fl, "movzwq ");
			break;
		case CMOVE:
			fprintf(fl, "cmove ");
			break;
		case CMOVNE:
			fprintf(fl, "cmovne ");
			break;
		case CMOVG:
			fprintf(fl, "cmovg ");
			break;
		case CMOVL:
			fprintf(fl, "cmovl ");
			break;
		case CMOVGE:
			fprintf(fl, "cmovge ");
			break;
		case CMOVLE:
			fprintf(fl, "cmovle ");
			break;
		case CMOVZ:
			fprintf(fl, "cmovz ");
			break;
		case CMOVNZ:
			fprintf(fl, "cmovnz ");
			break;
		case CMOVA:
			fprintf(fl, "cmova ");
			break;
		case CMOVAE:
			fprintf(fl, "cmovae ");
			break;
		case CMOVB:
			fprintf(fl, "cmovb ");
			break;
		case CMOVBE:
			fprintf(fl, "cmovbe ");
			break;
		case CMOVNP:
			fprintf(fl, "cmovnp ");
			break;
		case CMOVP:
			fprintf(fl, "cmovp ");
			break;
		//We should never hit this
		default:
			printf("Fatal internal compiler error: unreachable path hit\n");
			exit(1);
	}
}


/**
 * Handle a simple register to register or immediate to register move
 */
static void print_general_purpose_register_to_register_move(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	//First thing - print the move instruciton
	print_move_instruction(fl, instruction->instruction_type);

	//Print the appropriate variable here
	if(instruction->source_register != NULL){
		print_variable(fl, instruction->source_register, mode);
	} else {
		print_immediate_value(fl, instruction->source_immediate);
	}

	//Needed comma
	fprintf(fl, ", ");

	//Finally we print the destination
	print_variable(fl, instruction->destination_register, mode);

	//A final newline is needed for all instructions
	fprintf(fl, "\n");
}


/**
 * Handle a complex register(or immediate) to memory move with a complex
 * address offset calculation
 */
static void print_general_purpose_register_to_memory_move(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	//First thing - print the move instruciton
	print_move_instruction(fl, instruction->instruction_type);

	//First we'll print out the source
	if(instruction->source_register != NULL){
		print_variable(fl, instruction->source_register, mode);
	} else {
		//Otherwise we have an immediate value source
		print_immediate_value(fl, instruction->source_immediate);
	}
	
	fprintf(fl, ", ");
	//Let this handle it now
	print_addressing_mode_expression(fl, instruction, mode);
	fprintf(fl, "\n");
}


/**
 * Handle a complex memory to register move with a complex address offset calculation
 */
static void print_general_purpose_memory_to_register_move(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	//First thing - print the move instruciton
	print_move_instruction(fl, instruction->instruction_type);
	
	//The address mode expression comes firsj
	print_addressing_mode_expression(fl, instruction, mode);
	fprintf(fl, ", ");
	print_variable(fl, instruction->destination_register, mode);
	fprintf(fl, "\n");
}


/**
 * Handle a simple register to register or immediate to register move using SSE instructions
 */
static void print_sse_register_to_register_move(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	//What we need to print out here
	switch(instruction->instruction_type){
		case MOVSS:
			fprintf(fl, "movss ");
			break;
		case MOVSD:
			fprintf(fl, "movsd ");
			break;
		case MOVAPS:
			fprintf(fl, "movaps ");
			break;
		case MOVAPD:
			fprintf(fl, "movapd ");
			break;
		case CVTSS2SD:
			fprintf(fl, "cvtss2sd ");
			break;
		case CVTSD2SS:
			fprintf(fl, "cvtsd2ss ");
			break;
		case CVTTSD2SIL:
			fprintf(fl, "cvttsd2sil ");
			break;
		case CVTTSD2SIQ:
			fprintf(fl, "cvttsd2siq ");
			break;
		case CVTTSS2SIL:
			fprintf(fl, "cvttss2sil ");
			break;
		case CVTTSS2SIQ:
			fprintf(fl, "cvttss2siq ");
			break;
		case CVTSI2SSL:
			fprintf(fl, "cvtsi2ssl ");
			break;
		case CVTSI2SSQ:
			fprintf(fl, "cvtsi2ssq ");
			break;
		case CVTSI2SDL:
			fprintf(fl, "cvtsi2sdl ");
			break;
		case CVTSI2SDQ:
			fprintf(fl, "cvtsi2sdq ");
			break;
		//We should never hit this
		default:
			printf("Fatal internal compiler error: unreachable path hit\n");
			exit(1);
	}

	//Print the appropriate variable here. There are no immediate values
	//that may be produced by SSE, but we'll keep the optionality here
	if(instruction->source_register != NULL){
		print_variable(fl, instruction->source_register, mode);
	} else {
		print_immediate_value(fl, instruction->source_immediate);
	}

	//Needed comma
	fprintf(fl, ", ");

	//Finally we print the destination
	print_variable(fl, instruction->destination_register, mode);

	//A final newline is needed for all instructions
	fprintf(fl, "\n");
}


/**
 * Handle a complex register(or immediate) to memory move with a complex
 * address offset calculation for SSE instructions
 */
static void print_sse_register_to_memory_move(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	//What we need to print out here
	switch(instruction->instruction_type){
		case MOVSS:
			fprintf(fl, "movss ");
			break;
		case MOVSD:
			fprintf(fl, "movsd ");
			break;
		case MOVAPS:
			fprintf(fl, "movaps ");
			break;
		case MOVAPD:
			fprintf(fl, "movapd ");
			break;
		case CVTSS2SD:
			fprintf(fl, "cvtss2sd ");
			break;
		case CVTSD2SS:
			fprintf(fl, "cvtsd2ss ");
			break;
		case CVTTSD2SIL:
			fprintf(fl, "cvttsd2sil ");
			break;
		case CVTTSD2SIQ:
			fprintf(fl, "cvttsd2siq ");
			break;
		case CVTTSS2SIL:
			fprintf(fl, "cvttss2sil ");
			break;
		case CVTTSS2SIQ:
			fprintf(fl, "cvttss2siq ");
			break;
		case CVTSI2SSL:
			fprintf(fl, "cvtsi2ssl ");
			break;
		case CVTSI2SSQ:
			fprintf(fl, "cvtsi2ssq ");
			break;
		case CVTSI2SDL:
			fprintf(fl, "cvtsi2sdl ");
			break;
		case CVTSI2SDQ:
			fprintf(fl, "cvtsi2sdq ");
			break;
		//We should never hit this
		default:
			printf("Fatal internal compiler error: unreachable path hit\n");
			exit(1);
	}

	//First we'll print out the source
	if(instruction->source_register != NULL){
		print_variable(fl, instruction->source_register, mode);
	} else {
		//Otherwise we have an immediate value source
		print_immediate_value(fl, instruction->source_immediate);
	}
	
	fprintf(fl, ", ");
	//Let this handle it now
	print_addressing_mode_expression(fl, instruction, mode);
	fprintf(fl, "\n");
}


/**
 * Handle a complex memory to register move with a complex address offset calculation for SSE
 * instructions
 */
static void print_sse_memory_to_register_move(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	//What we need to print out here
	switch(instruction->instruction_type){
		case MOVSS:
			fprintf(fl, "movss ");
			break;
		case MOVSD:
			fprintf(fl, "movsd ");
			break;
		case MOVAPS:
			fprintf(fl, "movaps ");
			break;
		case MOVAPD:
			fprintf(fl, "movapd ");
			break;
		case CVTSS2SD:
			fprintf(fl, "cvtss2sd ");
			break;
		case CVTSD2SS:
			fprintf(fl, "cvtsd2ss ");
			break;
		case CVTTSD2SIL:
			fprintf(fl, "cvttsd2sil ");
			break;
		case CVTTSD2SIQ:
			fprintf(fl, "cvttsd2siq ");
			break;
		case CVTTSS2SIL:
			fprintf(fl, "cvttss2sil ");
			break;
		case CVTTSS2SIQ:
			fprintf(fl, "cvttss2siq ");
			break;
		case CVTSI2SSL:
			fprintf(fl, "cvtsi2ssl ");
			break;
		case CVTSI2SSQ:
			fprintf(fl, "cvtsi2ssq ");
			break;
		case CVTSI2SDL:
			fprintf(fl, "cvtsi2sdl ");
			break;
		case CVTSI2SDQ:
			fprintf(fl, "cvtsi2sdq ");
			break;
		//We should never hit this
		default:
			printf("Fatal internal compiler error: unreachable path hit\n");
			exit(1);
	}
	
	//The address mode expression comes firsj
	print_addressing_mode_expression(fl, instruction, mode);
	fprintf(fl, ", ");
	print_variable(fl, instruction->destination_register, mode);
	fprintf(fl, "\n");
}


/**
 * Print out an inc instruction
 */
static void print_inc_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	switch(instruction->instruction_type){
		case INCQ:
			fprintf(fl, "incq ");
			break;
		case INCL:
			fprintf(fl, "incl ");
			break;
		case INCW:
			fprintf(fl, "incw ");
			break;
		case INCB:
			fprintf(fl, "incb ");
			break;
		default:
			break;
	}

	print_variable(fl, instruction->destination_register, mode);
	fprintf(fl, "\n");
}


/**
 * Print out a conversion instruction
 *
 * Always goes RAX := sign extend RDX:RAX
 */
static void print_conversion_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	switch(instruction->instruction_type){
		case CQTO:
			fprintf(fl, "cqto /* Source: ");
			break;
		case CLTD:
			fprintf(fl, "cltd /* Source: ");
			break;
		case CWTL:
			fprintf(fl, "cwtl /* Source: ");
			break;
		case CBTW:
			fprintf(fl, "cbtw /* Source: ");
			break;
		default:
			break;
	}

	print_variable(fl, instruction->source_register, mode);
	fprintf(fl, "--> ");
	//Print the appropriate bitfield mapping for the destination
	print_variable(fl, instruction->destination_register2, mode);
	fprintf(fl, ":");
	print_variable(fl, instruction->destination_register, mode);
	fprintf(fl, "*/\n");
}



/**
 * Print out an dec instruction
 */
static void print_dec_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	switch(instruction->instruction_type){
		case DECQ:
			fprintf(fl, "decq ");
			break;
		case DECL:
			fprintf(fl, "decl ");
			break;
		case DECW:
			fprintf(fl, "decw ");
			break;
		case DECB:
			fprintf(fl, "decb ");
			break;
		default:
			break;
	}

	print_variable(fl, instruction->destination_register, mode);
	fprintf(fl, "\n");
}


/**
 * Print an usigned multiplication instruction, in all the forms it can take
 */
static void print_unsigned_multiplication_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	//First we'll print out the appropriate variety of addition
	switch(instruction->instruction_type){
		case MULB:
			fprintf(fl, "mulb ");
			break;
		case MULW:
			fprintf(fl, "mulw ");
			break;
		case MULL:
			fprintf(fl, "mull ");
			break;
		case MULQ:
			fprintf(fl, "mulq ");
			break;
		//We'll never get here, just to stop the compiler from complaining
		default:
			break;
	}

	//We'll only print the source register, there is no explicit destination
	//register
	print_variable(fl, instruction->source_register, mode);

	//Print where this went
	fprintf(fl, " /* Implicit Source: ");
	//Print out the implied source
	print_variable(fl, instruction->source_register2, mode);

	//Print where this went
	fprintf(fl, " -->  ");
	//Print this mode
	print_variable(fl, instruction->destination_register, mode);

	fprintf(fl, " */\n");
}


/**
 * Print a signed multiplication instruction, in all the forms it can take
 */
static void print_signed_multiplication_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	//First we'll print out the appropriate variety of addition
	switch(instruction->instruction_type){
		case IMULB:
			fprintf(fl, "imulb ");
			break;
		case IMULW:
			fprintf(fl, "imulw ");
			break;
		case IMULL:
			fprintf(fl, "imull ");
			break;
		case IMULQ:
			fprintf(fl, "imulq ");
			break;
		//We'll never get here, just to stop the compiler from complaining
		default:
			break;
	}

	//Print the appropriate variable here
	if(instruction->source_register != NULL){
		print_variable(fl, instruction->source_register, mode);
	} else {
		print_immediate_value(fl, instruction->source_immediate);
	}

	//Needed comma
	fprintf(fl, ", ");

	//Now print our destination
	print_variable(fl, instruction->destination_register, mode);
	fprintf(fl, "\n");
}


/**
 * Print a division instruction, in all the forms it can take
 */
static void print_division_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	//First we'll print out the appropriate variety of addition
	switch(instruction->instruction_type){
		case DIVB:
			fprintf(fl, "divb ");
			break;
		case DIVW:
			fprintf(fl, "divw ");
			break;
		case DIVL:
			fprintf(fl, "divl ");
			break;
		case DIVQ:
			fprintf(fl, "divq ");
			break;
		case IDIVB:
			fprintf(fl, "idivb ");
			break;
		case IDIVW:
			fprintf(fl, "idivw ");
			break;
		case IDIVL:
			fprintf(fl, "idivl ");
			break;
		case IDIVQ:
			fprintf(fl, "idivq ");
			break;
		//We'll never get here, just to stop the compiler from complaining
		default:
			break;
	}

	//We'll only have a source register here
	print_variable(fl, instruction->source_register, mode);

	//Print the implied source
	fprintf(fl, " /* Dividend: ");
	
	//Print out the higher order bit source if need be
	if(instruction->address_calc_reg1 != NULL){
		print_variable(fl, instruction->address_calc_reg1, mode);
		fprintf(fl, ":");
	}

	print_variable(fl, instruction->source_register2, mode);
	//Print out both the quotient and the remainder
	fprintf(fl, " --> Quotient: ");
	print_variable(fl, instruction->destination_register, mode);
	fprintf(fl, ", Remainder: ");
	print_variable(fl, instruction->destination_register2, mode);
	fprintf(fl, " */\n");
}


/**
 * Print an addition instruction, in all the forms it can take
 */
static void print_addition_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	//First we'll print out the appropriate variety of addition
	switch(instruction->instruction_type){
		case ADDB:
			fprintf(fl, "addb ");
			break;
		case ADDW:
			fprintf(fl, "addw ");
			break;
		case ADDL:
			fprintf(fl, "addl ");
			break;
		case ADDQ:
			fprintf(fl, "addq ");
			break;
		//We'll never get here, just to stop the compiler from complaining
		default:
			break;
	}

	//Print the appropriate variable here
	if(instruction->source_register != NULL){
		print_variable(fl, instruction->source_register, mode);
	} else {
		print_immediate_value(fl, instruction->source_immediate);
	}

	//Needed comma
	fprintf(fl, ", ");

	//Now print our destination
	print_variable(fl, instruction->destination_register, mode);
	fprintf(fl, "\n");
}


/**
 * Print a subtraction instruction, in all the forms it can take
 */
static void print_subtraction_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	//First we'll print out the appropriate variety of subtraction 
	switch(instruction->instruction_type){
		case SUBB:
			fprintf(fl, "subb ");
			break;
		case SUBW:
			fprintf(fl, "subw ");
			break;
		case SUBL:
			fprintf(fl, "subl ");
			break;
		case SUBQ:
			fprintf(fl, "subq ");
			break;
		//We'll never get here, just to stop the compiler from complaining
		default:
			break;
	}

	//Print the appropriate variable here
	if(instruction->source_register != NULL){
		print_variable(fl, instruction->source_register, mode);
	} else {
		print_immediate_value(fl, instruction->source_immediate);
	}

	//Needed comma
	fprintf(fl, ", ");

	//Now print our destination
	print_variable(fl, instruction->destination_register, mode);
	fprintf(fl, "\n");
}


/**
 * Print a lea instruction. This will also handle all the complexities around
 * complex addressing modes
 */
static void print_lea_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	//We'll always print out the lea value and the destination first
	switch(instruction->instruction_type){
		case LEAQ:
			fprintf(fl, "leaq ");
			break;
		case LEAL:
			fprintf(fl, "leal ");
			break;
		case LEAW:
			fprintf(fl, "leaw ");
			break;
		default:
			break;
	}

	//Now we'll print out one of the various complex addressing modes
	print_addressing_mode_expression(fl, instruction, mode);

	fprintf(fl, ", ");

	//Now we print out the destination
	print_variable(fl, instruction->destination_register, mode);

	fprintf(fl, "\n");
}


/**
 * Print a neg instruction
 */
static void print_neg_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	switch(instruction->instruction_type){
		case NEGQ:
			fprintf(fl, "negq ");
			break;
		case NEGL:
			fprintf(fl, "negl ");
			break;
		case NEGW:
			fprintf(fl, "negw ");
			break;
		case NEGB:
			fprintf(fl, "negb ");
			break;
		default:
			break;
	}

	//Now we'll print out the destination register
	print_variable(fl, instruction->destination_register, mode);

	//And give it a newlinw and we're done
	fprintf(fl, "\n");
}


/**
 * Print a not instruction
 */
static void print_not_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	switch(instruction->instruction_type){
		case NOTQ:
			fprintf(fl, "notq ");
			break;
		case NOTL:
			fprintf(fl, "notl ");
			break;
		case NOTW:
			fprintf(fl, "notw ");
			break;
		case NOTB:
			fprintf(fl, "notb ");
			break;
		default:
			break;
	}

	//Now we'll print out the destination register
	print_variable(fl, instruction->destination_register, mode);

	//And give it a newlinw and we're done
	fprintf(fl, "\n");
}


/**
 * Print a gp cmp instruction. These instructions can have two registers or
 * one register and one immediate value
 */
static inline void print_general_purpose_cmp_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	switch(instruction->instruction_type){
		case CMPQ:
			fprintf(fl, "cmpq ");
			break;
		case CMPL:
			fprintf(fl, "cmpl ");
			break;
		case CMPW:
			fprintf(fl, "cmpw ");
			break;
		case CMPB:
			fprintf(fl, "cmpb ");
			break;
		default:
			break;
	}

	//If we have an immediate value, print it
	if(instruction->source_immediate != NULL){
		print_immediate_value(fl, instruction->source_immediate);
	} else {
		print_variable(fl, instruction->source_register2, mode);
	}

	fprintf(fl, ",");

	//Now we'll need the source register. This may never be null
	print_variable(fl, instruction->source_register, mode);

	//And give it a newline and we're done
	fprintf(fl, "\n");
}


/**
 * Print an sse cmp instruction. These instructions can have two registers or
 * one register and one immediate value
 */
static inline void print_sse_cmp_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	switch(instruction->instruction_type){
		case COMISS:
			fprintf(fl, "comiss ");
			break;
		case UCOMISS:
			fprintf(fl, "ucomiss ");
			break;
		case COMISD:
			fprintf(fl, "comisd ");
			break;
		case UCOMISD:
			fprintf(fl, "ucomisd ");
			break;
		default:
			break;
	}

	//No immediate values here, only ever a register
	print_variable(fl, instruction->source_register2, mode);

	fprintf(fl, ",");

	//Now we'll need the source register. This may never be null
	print_variable(fl, instruction->source_register, mode);

	//And give it a newline and we're done
	fprintf(fl, "\n");
}


/**
 * Print the specialized CMPSS or CMPSD instructions along with their comparison
 * selectors for SSE. We will handle all of the comparions selector logic here
 *
 * These instructions go like cmpss <option_code>, source register, destination
 */
static inline void print_sse_scalar_cmp_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	switch (instruction->instruction_type){
		case CMPSS:
			fprintf(fl, "cmpss ");
			break;
		case CMPSD:
			fprintf(fl, "cmpsd ");
			break;
		default:
			printf("Fatal internal compiler error: unreachable path hit\n");
			exit(1);
	}

	//Depending on the operand, we print out the needed comparison code immediate value
	switch(instruction->op){
		case L_THAN:
			fprintf(fl, "$1, "); //CMPLT
			break;

		case L_THAN_OR_EQ:
			fprintf(fl, "$2, "); //CMPLE
			break;

		case G_THAN:
			fprintf(fl, "$6, "); //CMPNLE
			break;

		case G_THAN_OR_EQ:
			fprintf(fl, "$5, "); //CMPNLT
			break;

		case DOUBLE_EQUALS:
			fprintf(fl, "$0, "); //CMPEQ
			break;
		
		case NOT_EQUALS:
			fprintf(fl, "$4, "); //CMPNEQ
			break;

		default:
			printf("Fatal internal compiler error: unreachable path hit\n");
			exit(1);
	}

	//Now print out the source register
	print_variable(fl, instruction->source_register, mode);
	fprintf(fl, ", ");

	//Finally the second source which also doubles as the destination
	print_variable(fl, instruction->destination_register, mode);
	fprintf(fl, "\n");
}


/**
 * Print out a setX instruction
 */
static void print_set_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	switch(instruction->instruction_type){
		case SETE:
			fprintf(fl, "sete ");
			break;
		case SETNE:
			fprintf(fl, "setne ");
			break;
		case SETGE:
			fprintf(fl, "setge ");
			break;
		case SETLE:
			fprintf(fl, "setle ");
			break;
		case SETL:
			fprintf(fl, "setl ");
			break;
		case SETG:
			fprintf(fl, "setg ");
			break;
		case SETAE:
			fprintf(fl, "setae ");
			break;
		case SETA:
			fprintf(fl, "seta ");
			break;
		case SETP:
			fprintf(fl, "setp ");
			break;
		case SETNP:
			fprintf(fl, "setnp ");
			break;
		case SETBE:
			fprintf(fl, "setbe ");
			break;
		case SETB:
			fprintf(fl, "setb ");
			break;
		//We should never get here
		default:
			break;
	}

	//Now we'll print the destination register
	print_variable(fl, instruction->destination_register, mode);
	fprintf(fl, "\n");
}


/**
 * Print out a standard test instruction
 */
static void print_test_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	switch(instruction->instruction_type){
		case TESTQ:
			fprintf(fl, "testq ");
			break;
		case TESTL:
			fprintf(fl, "testl ");
			break;
		case TESTW:
			fprintf(fl, "testw ");
			break;
		case TESTB:
			fprintf(fl, "testb ");
			break;
		default:
			break;
	}

	//Now we'll print out the source and source2 registers. Test instruction
	//has no destination
	print_variable(fl, instruction->source_register, mode);
	fprintf(fl, ",");
	print_variable(fl, instruction->source_register2, mode);

	//And give it a newline
	fprintf(fl, "\n");
}


/**
 * Print out an arithmetic left shift instruction
 */
static void print_sal_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	switch(instruction->instruction_type){
		case SALQ:
			fprintf(fl, "salq ");
			break;
		case SALL:
			fprintf(fl, "sall ");
			break;
		case SALW:
			fprintf(fl, "salw ");
			break;
		case SALB:
			fprintf(fl, "salb ");
			break;
		default:
			break;
	}

	//Now we'll need the source immediate/source
	if(instruction->source_register != NULL){
		print_variable(fl, instruction->source_register, mode);
	} else {
		print_immediate_value(fl, instruction->source_immediate);
	}

	//Now our comma and the destination
	fprintf(fl, ",");
	print_variable(fl, instruction->destination_register, mode);
	fprintf(fl, "\n");
}


/**
 * Print out a logical left shift instruction
 */
static void print_shl_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	switch(instruction->instruction_type){
		case SHLQ:
			fprintf(fl, "shlq ");
			break;
		case SHLL:
			fprintf(fl, "shll ");
			break;
		case SHLW:
			fprintf(fl, "shlw ");
			break;
		case SHLB:
			fprintf(fl, "shlb ");
			break;
		default:
			break;
	}

	//Now we'll need the source immediate/source
	if(instruction->source_register != NULL){
		print_variable(fl, instruction->source_register, mode);
	} else {
		print_immediate_value(fl, instruction->source_immediate);
	}

	//Now our comma and the destination
	fprintf(fl, ",");
	print_variable(fl, instruction->destination_register, mode);
	fprintf(fl, "\n");
}


/**
 * Print out an arithmetic right shift instruction
 */
static void print_sar_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	switch(instruction->instruction_type){
		case SARQ:
			fprintf(fl, "sarq ");
			break;
		case SARL:
			fprintf(fl, "sarl ");
			break;
		case SARW:
			fprintf(fl, "sarw ");
			break;
		case SARB:
			fprintf(fl, "sarb ");
			break;
		default:
			break;
	}

	//Now we'll need the source immediate/source
	if(instruction->source_register != NULL){
		print_variable(fl, instruction->source_register, mode);
	} else {
		print_immediate_value(fl, instruction->source_immediate);
	}

	//Now our comma and the destination
	fprintf(fl, ",");
	print_variable(fl, instruction->destination_register, mode);
	fprintf(fl, "\n");
}


/**
 * Print out a logical right shift instruction
 */
static void print_shr_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	switch(instruction->instruction_type){
		case SHRQ:
			fprintf(fl, "shrq ");
			break;
		case SHRL:
			fprintf(fl, "shrl ");
			break;
		case SHRW:
			fprintf(fl, "shrw ");
			break;
		case SHRB:
			fprintf(fl, "shrb ");
			break;
		default:
			break;
	}

	//Now we'll need the source immediate/source
	if(instruction->source_register != NULL){
		print_variable(fl, instruction->source_register, mode);
	} else {
		print_immediate_value(fl, instruction->source_immediate);
	}

	//Now our comma and the destination
	fprintf(fl, ",");
	print_variable(fl, instruction->destination_register, mode);
	fprintf(fl, "\n");
}


/**
 * Print out a bitwise AND instruction
 */
static void print_and_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	switch(instruction->instruction_type){
		case ANDQ:
			fprintf(fl, "andq ");
			break;
		case ANDL:
			fprintf(fl, "andl ");
			break;
		case ANDW:
			fprintf(fl, "andw ");
			break;
		case ANDB:
			fprintf(fl, "andb ");
			break;
		default:
			break;
	}

	//Now we'll need the source immediate/source
	if(instruction->source_register != NULL){
		print_variable(fl, instruction->source_register, mode);
	} else {
		print_immediate_value(fl, instruction->source_immediate);
	}

	//Now our comma and the destination
	fprintf(fl, ",");
	print_variable(fl, instruction->destination_register, mode);
	fprintf(fl, "\n");
}


/**
 * Print out a bitwise OR instruction
 */
static void print_or_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	switch(instruction->instruction_type){
		case ORQ:
			fprintf(fl, "orq ");
			break;
		case ORL:
			fprintf(fl, "orl ");
			break;
		case ORW:
			fprintf(fl, "orw ");
			break;
		case ORB:
			fprintf(fl, "orb ");
			break;
		default:
			break;
	}

	//Now we'll need the source immediate/source
	if(instruction->source_register != NULL){
		print_variable(fl, instruction->source_register, mode);
	} else {
		print_immediate_value(fl, instruction->source_immediate);
	}

	//Now our comma and the destination
	fprintf(fl, ",");
	print_variable(fl, instruction->destination_register, mode);
	fprintf(fl, "\n");
}


/**
 * Print out a bitwise XOR instruction
 */
static inline void print_xor_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	switch(instruction->instruction_type){
		case XORQ:
			fprintf(fl, "xorq ");
			break;
		case XORL:
			fprintf(fl, "xorl ");
			break;
		case XORW:
			fprintf(fl, "xorw ");
			break;
		case XORB:
			fprintf(fl, "xorb ");
			break;
		case XORPS:
			fprintf(fl, "xorps ");
			break;
		case XORPD:
			fprintf(fl, "xorpd ");
			break;
		default:
			break;
	}

	//Now we'll need the source immediate/source
	if(instruction->source_register != NULL){
		print_variable(fl, instruction->source_register, mode);
	} else {
		print_immediate_value(fl, instruction->source_immediate);
	}

	//Now our comma and the destination
	fprintf(fl, ",");
	print_variable(fl, instruction->destination_register, mode);
	fprintf(fl, "\n");
}



/**
 * Print an instruction that has not yet been given registers
 */
void print_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	//This will be null often, but if we need it it'll be here
	basic_block_t* jumping_to_block = instruction->if_block;

	//Switch based on what type we have
	switch (instruction->instruction_type) {
		//These first ones are very simple - no real variations here
		case RET:
			fprintf(fl, "ret");
			if(instruction->source_register != NULL){
				fprintf(fl, " /* --> ");
				print_variable(fl, instruction->source_register, mode);
				fprintf(fl, " */");
			}
			fprintf(fl, "\n");
			break;
		case NOP:
			fprintf(fl, "nop\n");
			break;
		case CQTO:
		case CLTD:
		case CWTL:
		case CBTW:
			print_conversion_instruction(fl, instruction, mode);
			break;
		case JMP:
			fprintf(fl, "jmp .L%d\n", jumping_to_block->block_id);
			break;
		case JE:
			fprintf(fl, "je .L%d\n", jumping_to_block->block_id);
			break;
		case JNE:
			fprintf(fl, "jne .L%d\n", jumping_to_block->block_id);
			break;
		case JZ:
			fprintf(fl, "jz .L%d\n", jumping_to_block->block_id);
			break;
		case JNZ:
			fprintf(fl, "jnz .L%d\n", jumping_to_block->block_id);
			break;
		case JG:
			fprintf(fl, "jg .L%d\n", jumping_to_block->block_id);
			break;
		case JL:
			fprintf(fl, "jl .L%d\n", jumping_to_block->block_id);
			break;
		case JGE:
			fprintf(fl, "jge .L%d\n", jumping_to_block->block_id);
			break;
		case JLE:
			fprintf(fl, "jle .L%d\n", jumping_to_block->block_id);
			break;
		case JA:
			fprintf(fl, "ja .L%d\n", jumping_to_block->block_id);
			break;
		case JB:
			fprintf(fl, "jb .L%d\n", jumping_to_block->block_id);
			break;
		case JAE:
			fprintf(fl, "jae .L%d\n", jumping_to_block->block_id);
			break;
		case JBE:
			fprintf(fl, "jbe .L%d\n", jumping_to_block->block_id);
			break;
		case JP:
			fprintf(fl, "jp .L%d\n", jumping_to_block->block_id);
			break;
		case ASM_INLINE:
			fprintf(fl, "%s\n", instruction->inlined_assembly.string);
			break;
		case CALL:
			fprintf(fl, "call %s", instruction->called_function->func_name.string);

			//This could be NULL
			if(instruction->destination_register != NULL){
				fprintf(fl, " /* --> ");
				print_variable(fl, instruction->destination_register, mode);
				fprintf(fl, " */");
			} else {
				fprintf(fl, " /* --> void */");
			}

			//Final newline
			fprintf(fl, "\n");
			break;
		case INDIRECT_CALL:
			//Indirect function calls store the location of the call in op1
			fprintf(fl, "call *");
			print_variable(fl, instruction->op1, mode);

			if(instruction->destination_register != NULL){
				fprintf(fl, " /* --> ");
				print_variable(fl, instruction->destination_register, mode);
				fprintf(fl, " */");
			} else {
				fprintf(fl, " /* --> void */");
			}

			//Final newline
			fprintf(fl, "\n");
			break;

		case PUSH:
			fprintf(fl, "push ");
			print_variable(fl, instruction->source_register, mode);
			fprintf(fl, "\n");
			break;

		case PUSH_DIRECT_GP:
			fprintf(fl, "push ");
			//We have to print a register here, there's no choice
			print_64_bit_register_name(fl, instruction->push_or_pop_reg.gen_purpose);
			fprintf(fl, "\n");
			break;

		case PUSH_DIRECT_SSE:
			fprintf(fl, "push ");
			//We have to print a register here, there's no choice
			print_double_precision_sse_register(fl, instruction->push_or_pop_reg.sse_register);
			fprintf(fl, "\n");
			break;

		case POP:
			fprintf(fl, "pop ");
			print_variable(fl, instruction->source_register, mode);
			fprintf(fl, "\n");
			break;

		case POP_DIRECT_GP:
			fprintf(fl, "pop ");
			//We have to print a register here, there's no choice
			print_64_bit_register_name(fl, instruction->push_or_pop_reg.gen_purpose);
			fprintf(fl, "\n");
			break;

		case POP_DIRECT_SSE:
			fprintf(fl, "pop ");
			//We have to print a register here, there's no choice
			print_double_precision_sse_register(fl, instruction->push_or_pop_reg.sse_register);
			fprintf(fl, "\n");
			break;

		case INCL:
		case INCQ:
		case INCW:
		case INCB:
			print_inc_instruction(fl, instruction, mode);
			break;

		case DECL:
		case DECQ:
		case DECW:
		case DECB:
			print_dec_instruction(fl, instruction, mode);
			break;

		case MULW:
		case MULB:
		case MULL:
		case MULQ:
			print_unsigned_multiplication_instruction(fl, instruction, mode);
			break;
		case IMULW:
		case IMULB:
		case IMULQ:
		case IMULL:
			print_signed_multiplication_instruction(fl, instruction, mode);
			break;

		case DIVB:
		case DIVW:
		case DIVL:
		case DIVQ:
		case IDIVB:
		case IDIVW:
		case IDIVL:
		case IDIVQ:
			print_division_instruction(fl, instruction, mode);
			break;

		//Handle addition instructions
		case ADDB:
		case ADDW:
		case ADDL:
		case ADDQ:
			print_addition_instruction(fl, instruction, mode);
			break;

		//Handle subtraction instruction
		case SUBB:
		case SUBW:
		case SUBL:
		case SUBQ:
			print_subtraction_instruction(fl, instruction, mode);
			break;

		//Handle movement instructions
		case MOVB:
		case MOVW:
		case MOVL:
		case MOVQ:
		case MOVD:
		case MOVSBW:
		case MOVSBL:
		case MOVSBQ:
		case MOVSWL:
		case MOVSWQ:
		case MOVSLQ:
		case MOVZBW:
		case MOVZBL:
		case MOVZBQ:
		case MOVZWL:
		case MOVZWQ:
		case CMOVE:
		case CMOVNE:
		case CMOVG:
		case CMOVL:
		case CMOVGE:
		case CMOVLE:
		case CMOVZ:
		case CMOVNZ:
		case CMOVA:
		case CMOVAE:
		case CMOVB:
		case CMOVBE:
		case CMOVNP:
		case CMOVP:
			/**
			 * Now we go based on what kind of memory
			 * access we're doing here. This will determine
			 * the final output of our move
			 */
			switch(instruction->memory_access_type){
				case NO_MEMORY_ACCESS:
					print_general_purpose_register_to_register_move(fl, instruction, mode);
					break;

				case WRITE_TO_MEMORY:
					print_general_purpose_register_to_memory_move(fl, instruction, mode);
					break;

				case READ_FROM_MEMORY:
					print_general_purpose_memory_to_register_move(fl, instruction, mode);
					break;
			}

			break;

		//Handle lea printing
		case LEAL:
		case LEAQ:
			//Invoke the helper
			print_lea_instruction(fl, instruction, mode);
			break;

		//Handle neg printing
		case NEGB:
		case NEGW:
		case NEGL:
		case NEGQ:
			print_neg_instruction(fl, instruction, mode);
			break;

		//Handle not(one's complement) printing
		case NOTB:
		case NOTW:
		case NOTL:
		case NOTQ:
			print_not_instruction(fl, instruction, mode);
			break;

		//Handle our CMP instructions
		case CMPB:
		case CMPW:
		case CMPL:
		case CMPQ:
			print_general_purpose_cmp_instruction(fl, instruction, mode);
			break;
		
		case CMPSS:
		case CMPSD:
			print_sse_scalar_cmp_instruction(fl, instruction, mode);
			break;

		case UCOMISD:
		case UCOMISS:
		case COMISS:
		case COMISD:
			print_sse_cmp_instruction(fl, instruction, mode);
			break;

		//Handle a simple sete instruction
		case SETE:
		case SETNE:
		case SETGE:
		case SETLE:
		case SETL:
		case SETG:
		case SETAE:
		case SETA:
		case SETBE:
		case SETB:
		case SETNP:
		case SETP:
			print_set_instruction(fl, instruction, mode);
			break;
		
		//Handle a test instruction
		case TESTB:
		case TESTL:
		case TESTW:
		case TESTQ:
			print_test_instruction(fl, instruction, mode);
			break;

		//Handle an arithmetic left shift instruction
		case SALB:
		case SALW:
		case SALL:
		case SALQ:
			print_sal_instruction(fl, instruction, mode);
			break;

		//Handle a logical left shift instruction
		case SHLB:
		case SHLW:
		case SHLL:
		case SHLQ:
			print_shl_instruction(fl, instruction, mode);
			break;

		//Handle a logical right shift instruction
		case SHRB:
		case SHRW:
		case SHRL:
		case SHRQ:
			print_shr_instruction(fl, instruction, mode);
			break;

		//Handle an arithmentic right shift instruction
		case SARW:
		case SARB:
		case SARL:
		case SARQ:
			print_sar_instruction(fl, instruction, mode);
			break;

		//Handle a bitwise and instruction
		case ANDL:
		case ANDQ:
		case ANDB:
		case ANDW:
			print_and_instruction(fl, instruction, mode);
			break;

		//Handle a bitwise inclusive or instruction
		case ORB:
		case ORW:
		case ORL:
		case ORQ:
			print_or_instruction(fl, instruction, mode);
			break;

		//Handle a bitwise exclusive or instruction
		case XORB:
		case XORW:
		case XORL:
		case XORQ:
		case XORPS:
		case XORPD:
			print_xor_instruction(fl, instruction, mode);
			break;

		//Handle the very rare case of an indirect jump. This will only appear
		//in case statements
		case INDIRECT_JMP:
			//The star makes this indirect
			fprintf(fl, "jmp *");

			//Grab this out for convenience
			jump_table_t* jumping_to_block = instruction->if_block;

			//We first print out the jumping to block
			fprintf(fl, ".JT%d(,", jumping_to_block->jump_table_id);

			//Now we print out the source register
			print_variable(fl, instruction->source_register, mode);

			//And then a comma and the multplicator
			fprintf(fl, ",%ld)\n", instruction->lea_multiplier);

			break;

		//PHI functions are printed in the exact same manner they are for instructions. These
		//will be dealt with after we perform register allocation
		case PHI_FUNCTION:
			//Print it in block header mode
			print_variable(fl, instruction->assignee, PRINTING_VAR_BLOCK_HEADER);
			fprintf(fl, " <- PHI(");

			//For convenience
			dynamic_array_t phi_func_params = instruction->parameters;

			//Now run through all of the parameters if we have any
			if(phi_func_params.internal_array != NULL){
				for(u_int16_t _ = 0; _ < phi_func_params.current_index; _++){
					//Print out the variable
					print_variable(fl, dynamic_array_get_at(&phi_func_params, _), PRINTING_VAR_BLOCK_HEADER);

					//If it isn't the very last one, add a comma space
					if(_ != phi_func_params.current_index - 1){
						fprintf(fl, ", ");
					}
				}
			}

			fprintf(fl, ")\n");

			break;

		// ============================ Begin floating point area ==============================
		// The instructions below operate either exclusively with xmm registers or with a mix
		// of xmm and general purpose registers or memory operations. These handle basic movement
		// and conversion
		case MOVAPD:
		case MOVAPS:
		case MOVSD:
		case MOVSS:
		case CVTTSS2SIL:
		case CVTTSS2SIQ:
		case CVTTSD2SIL:
		case CVTTSD2SIQ:
		case CVTSD2SS:
		case CVTSS2SD:
		case CVTSI2SSL:
		case CVTSI2SSQ:
		case CVTSI2SDL:
		case CVTSI2SDQ:
			/**
			 * Now we go based on what kind of memory
			 * access we're doing here. This will determine
			 * the final output of our move
			 */
			switch(instruction->memory_access_type){
				case NO_MEMORY_ACCESS:
					print_sse_register_to_register_move(fl, instruction, mode);
					break;

				case WRITE_TO_MEMORY:
					print_sse_register_to_memory_move(fl, instruction, mode);
					break;

				case READ_FROM_MEMORY:
					print_sse_memory_to_register_move(fl, instruction, mode);
					break;
			}

			break;

		case ADDSS:
			fprintf(fl, "addss ");
			print_variable(fl, instruction->source_register, mode);
			fprintf(fl, ", ");
			print_variable(fl, instruction->destination_register, mode);
			fprintf(fl, "\n");

			break;

		case ADDSD:
			fprintf(fl, "addsd ");
			print_variable(fl, instruction->source_register, mode);
			fprintf(fl, ", ");
			print_variable(fl, instruction->destination_register, mode);
			fprintf(fl, "\n");

			break;

		case SUBSS:
			fprintf(fl, "subss ");
			print_variable(fl, instruction->source_register, mode);
			fprintf(fl, ", ");
			print_variable(fl, instruction->destination_register, mode);
			fprintf(fl, "\n");

			break;

		case SUBSD:
			fprintf(fl, "subsd ");
			print_variable(fl, instruction->source_register, mode);
			fprintf(fl, ", ");
			print_variable(fl, instruction->destination_register, mode);
			fprintf(fl, "\n");

			break;

		case MULSS:
			fprintf(fl, "mulss ");
			print_variable(fl, instruction->source_register, mode);
			fprintf(fl, ", ");
			print_variable(fl, instruction->destination_register, mode);
			fprintf(fl, "\n");

			break;

		case MULSD:
			fprintf(fl, "mulsd ");
			print_variable(fl, instruction->source_register, mode);
			fprintf(fl, ", ");
			print_variable(fl, instruction->destination_register, mode);
			fprintf(fl, "\n");

			break;

		case DIVSS:
			fprintf(fl, "divss ");
			print_variable(fl, instruction->source_register, mode);
			fprintf(fl, ", ");
			print_variable(fl, instruction->destination_register, mode);
			fprintf(fl, "\n");

			break;

		case DIVSD:
			fprintf(fl, "DIVSD ");
			print_variable(fl, instruction->source_register, mode);
			fprintf(fl, ", ");
			print_variable(fl, instruction->destination_register, mode);
			fprintf(fl, "\n");

			break;

		case PAND:
			fprintf(fl, "pand ");
			print_variable(fl, instruction->source_register, mode);
			fprintf(fl, ", ");
			print_variable(fl, instruction->destination_register, mode);
			fprintf(fl, "\n");

			break;

		case PANDN:
			fprintf(fl, "pandn ");
			print_variable(fl, instruction->source_register, mode);
			fprintf(fl, ", ");
			print_variable(fl, instruction->destination_register, mode);
			fprintf(fl, "\n");

			break;

		case POR:
			fprintf(fl, "por ");
			print_variable(fl, instruction->source_register, mode);
			fprintf(fl, ", ");
			print_variable(fl, instruction->destination_register, mode);
			fprintf(fl, "\n");

			break;
		
		//Generic PXOR
		case PXOR:
			fprintf(fl, "pxor ");
			print_variable(fl, instruction->source_register, mode);
			fprintf(fl, ", ");
			print_variable(fl, instruction->destination_register, mode);
			fprintf(fl, "\n");

			break;
		
		//PXOR instruction that is used for the express purpose of wiping out a register,
		//and nothing more
		case PXOR_CLEAR:
			fprintf(fl, "pxor ");
			print_variable(fl, instruction->destination_register, mode);
			fprintf(fl, ", ");
			print_variable(fl, instruction->destination_register, mode);
			fprintf(fl, "\n");

			break;

		//Show a default error message. This is for the Dev's use only
		default:
			fprintf(fl, "Not yet selected. Statement code is: %d\n", instruction->statement_type);
			break;
	}
}


/**
 * Emit a decrement instruction
 */
instruction_t* emit_dec_instruction(three_addr_var_t* decrementee){
	//First allocate it
	instruction_t* dec_stmt = calloc(1, sizeof(instruction_t));

	//Now we populate
	dec_stmt->statement_type = THREE_ADDR_CODE_DEC_STMT;

	//If this is not a temporary variable, then we'll
	//emit an exact copy and let the SSA system handle it
	if(decrementee->variable_type != VARIABLE_TYPE_TEMP){
		dec_stmt->assignee = emit_var_copy(decrementee);

	//Otherwise, we'll need to spawn a new temporary variable
	} else {
		dec_stmt->assignee = emit_temp_var(decrementee->type);
	}

	//This is always our input variable
	dec_stmt->op1 = decrementee;

	//And give it back
	return dec_stmt;
}


/**
 * Emit an increment instruction
 */
instruction_t* emit_inc_instruction(three_addr_var_t* incrementee){
	//First allocate it
	instruction_t* inc_stmt = calloc(1, sizeof(instruction_t));

	//Now we populate
	inc_stmt->statement_type = THREE_ADDR_CODE_INC_STMT;

	//If this is not a temporary variable, then we'll
	//emit an exact copy and let the SSA system handle it
	if(incrementee->variable_type != VARIABLE_TYPE_TEMP){
		inc_stmt->assignee = emit_var_copy(incrementee);

	//Otherwise, we'll need to spawn a new temporary variable
	} else {
		inc_stmt->assignee = emit_temp_var(incrementee->type);
	}

	//No matter what this is the op1
	inc_stmt->op1 = incrementee;

	//And give it back
	return inc_stmt;
}


/**
 * Create and return a constant three address var
 */
three_addr_const_t* emit_constant(generic_ast_node_t* const_node){
	//First we'll dynamically allocate the constant
	three_addr_const_t* constant = calloc(1, sizeof(three_addr_const_t));

	//Add into here for memory management
	dynamic_array_add(&emitted_consts, constant);

	//Now we'll assign the appropriate values
	constant->const_type = const_node->constant_type; 
	constant->type = const_node->inferred_type;

	//Now based on what type we have we'll make assignments
	switch(constant->const_type){
		case CHAR_CONST:
			constant->constant_value.char_constant = const_node->constant_value.char_value;
			break;
		case BYTE_CONST:
			constant->constant_value.signed_byte_constant = const_node->constant_value.signed_byte_value;
			break;
		case BYTE_CONST_FORCE_U:
			constant->constant_value.unsigned_byte_constant = const_node->constant_value.unsigned_byte_value;
			break;
		case SHORT_CONST:
			constant->constant_value.signed_short_constant = const_node->constant_value.signed_short_value;
			break;
		case SHORT_CONST_FORCE_U:
			constant->constant_value.unsigned_short_constant = const_node->constant_value.unsigned_short_value;
			break;
		case INT_CONST:
			constant->constant_value.signed_integer_constant = const_node->constant_value.signed_int_value;
			break;
		case INT_CONST_FORCE_U:
			constant->constant_value.unsigned_integer_constant = const_node->constant_value.unsigned_int_value;
			break;
		case LONG_CONST:
			constant->constant_value.signed_long_constant = const_node->constant_value.signed_long_value;
			break;
		case LONG_CONST_FORCE_U:
			constant->constant_value.unsigned_long_constant = const_node->constant_value.unsigned_long_value;
			break;
		//These need to be emitted via the local constant(.LC) system, so any attempt to call this from here is
		//an error
		case DOUBLE_CONST:
		case FLOAT_CONST:
		case STR_CONST:
		case FUNC_CONST:
			printf("Fatal internal compiler error: string, function pointer, f32 and f64 constants may not be emitted directly\n");
			exit(1);
		//Some very weird error here
		default:
			printf("Fatal internal compiler error: unrecognizable constant type found in constant\n");
			exit(1);
	}
	
	//Once all that is done, we can leave
	return constant;
}


/**
 * Emit a return statement. The returnee variable may or may not be null
 */
instruction_t* emit_ret_instruction(three_addr_var_t* returnee){
	//First allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Let's now populate it appropriately
	stmt->statement_type = THREE_ADDR_CODE_RET_STMT;
	//Set op1 to be the returnee
	stmt->op1 = returnee;

	//And that's all, so we'll hop out
	return stmt;
}


/**
 * Emit a binary operator three address code statement. Once we're here, we expect that the caller has created and 
 * supplied the appropriate variables
 */
instruction_t* emit_binary_operation_instruction(three_addr_var_t* assignee, three_addr_var_t* op1, ollie_token_t op, three_addr_var_t* op2){
	//First allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Let's now populate it with the appropriate values
	stmt->statement_type = THREE_ADDR_CODE_BIN_OP_STMT;
	stmt->assignee = assignee;
	stmt->op1 = op1;
	stmt->op = op;
	stmt->op2 = op2;

	//Give back the newly allocated statement
	return stmt;
}


/**
 * Emit a binary operation with a constant three address code statement
 */
instruction_t* emit_binary_operation_with_const_instruction(three_addr_var_t* assignee, three_addr_var_t* op1, ollie_token_t op, three_addr_const_t* op2){
	//First allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Let's now populate it with the appropriate values
	stmt->statement_type = THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT;
	stmt->assignee = assignee;
	stmt->op1 = op1;
	stmt->op = op;
	stmt->op1_const = op2;

	//Give back the newly allocated statement
	return stmt;
}


/**
 * Emit an assignment three address code statement. Once we're here, we expect that the caller has created and supplied the
 * appropriate variables
 */
instruction_t* emit_assignment_instruction(three_addr_var_t* assignee, three_addr_var_t* op1){
	//First allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Define the class
	stmt->statement_type = THREE_ADDR_CODE_ASSN_STMT;
	//Let's now populate it with values
	stmt->assignee = assignee;
	stmt->op1 = op1;

	//And that's it, we'll just leave our now
	return stmt;
}


/**
 * Emit a memory access statement
 */
instruction_t* emit_memory_access_instruction(three_addr_var_t* assignee, three_addr_var_t* op1){
	//First we allocate
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Let's now populate it with values
	stmt->statement_type = THREE_ADDR_CODE_MEM_ACCESS_STMT;
	stmt->assignee = assignee;
	stmt->op1 = op1;

	return stmt;
}


/**
 * Emit a load statement directly. This should only be used during spilling
 */
instruction_t* emit_load_instruction(three_addr_var_t* assignee, three_addr_var_t* stack_pointer, type_symtab_t* symtab, u_int64_t offset){
	//Allocate the instruction
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Select the size
	variable_size_t size = get_type_size(assignee->type);

	//Select the appropriate register
	switch(size){
		case BYTE:
			stmt->instruction_type = MOVB;
			break;
		case WORD:
			stmt->instruction_type = MOVW;
			break;
		case DOUBLE_WORD:
			stmt->instruction_type = MOVL;
			break;
		case QUAD_WORD:
			stmt->instruction_type = MOVQ;
			break;
		case SINGLE_PRECISION:
			stmt->instruction_type = MOVSS;
			break;
		case DOUBLE_PRECISION:
			stmt->instruction_type = MOVSD;
			break;
		default:
			break;
	}

	stmt->destination_register = assignee;
	//Stack pointer is source 1
	stmt->address_calc_reg1 = stack_pointer;
	stmt->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;
	//Loading is reading from memory
	stmt->memory_access_type = READ_FROM_MEMORY;

	//Emit an integer constant for this offset
	stmt->offset= emit_direct_integer_or_char_constant(offset, lookup_type_name_only(symtab, "u64", NOT_MUTABLE)->type);

	//And we're done, we can return it
	return stmt;
}


/**
 * Emit a store statement directly. This should only be used during spilling in the register allocator
 */
instruction_t* emit_store_instruction(three_addr_var_t* source, three_addr_var_t* stack_pointer, type_symtab_t* symtab, u_int64_t offset){
	//Allocate the instruction
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Select the size
	variable_size_t size = get_type_size(source->type);

	//Select the appropriate register
	switch(size){
		case BYTE:
			stmt->instruction_type = MOVB;
			break;
		case WORD:
			stmt->instruction_type = MOVW;
			break;
		case DOUBLE_WORD:
			stmt->instruction_type = MOVL;
			break;
		case QUAD_WORD:
			stmt->instruction_type = MOVQ;
			break;
		case SINGLE_PRECISION:
			stmt->instruction_type = MOVSS;
			break;
		case DOUBLE_PRECISION:
			stmt->instruction_type = MOVSD;
			break;
		default:
			break;
	}

	//We'll have the stored variable as our source
	stmt->source_register = source;
	
	//Stack pointer our base address
	stmt->address_calc_reg1 = stack_pointer;
	stmt->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;
	//Storing is writing to memory
	stmt->memory_access_type = WRITE_TO_MEMORY;

	//Emit an integer constant for this offset
	stmt->offset= emit_direct_integer_or_char_constant(offset, lookup_type_name_only(symtab, "u64", NOT_MUTABLE)->type);

	//And we're done, we can return it
	return stmt;
}


/**
 * Emit an assignment "three" address code statement
 */
instruction_t* emit_assignment_with_const_instruction(three_addr_var_t* assignee, three_addr_const_t* constant){
	//First allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Let's now populate it with values
	stmt->statement_type = THREE_ADDR_CODE_ASSN_CONST_STMT;
	stmt->assignee = assignee;
	stmt->op1_const = constant;

	//And that's it, we'll now just give it back
	return stmt;
}


/**
 * Emit a store statement. This is like an assignment instruction, but we're explicitly
 * using stack memory here
 */
instruction_t* emit_store_ir_code(three_addr_var_t* assignee, three_addr_var_t* op1, generic_type_t* memory_write_type){
	//First allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Let's now populate it with values
	stmt->statement_type = THREE_ADDR_CODE_STORE_STATEMENT;
	stmt->assignee = assignee;

	//This is being dereferenced
	stmt->assignee->is_dereferenced = TRUE;

	stmt->op1 = op1;

	//Important - add the type that we expect to be writing to in memory
	stmt->memory_read_write_type = memory_write_type;

	//And that's it, we'll now just give it back
	return stmt;
}


/**
 * Emit a store with offset ir code. We take in a base address(assignee), 
 * a variable offset(op1), and the value we're storing(op2)
 */
instruction_t* emit_store_with_variable_offset_ir_code(three_addr_var_t* base_address, three_addr_var_t* offset, three_addr_var_t* storee, generic_type_t* memory_write_type){
	//First allocate
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Now populate with values
	stmt->statement_type = THREE_ADDR_CODE_STORE_WITH_VARIABLE_OFFSET;
	//The base address that we're assigning to
	stmt->assignee = base_address;

	//This is being dereferenced
	stmt->assignee->is_dereferenced = TRUE;

	//The op1 is our offset
	stmt->op1 = offset;

	//What we're storing
	stmt->op2 = storee;

	//Important - add the type that we expect to be writing to in memory
	stmt->memory_read_write_type = memory_write_type;

	//And give it back
	return stmt;
}


/**
 * Emit a store with offset ir code. We take in a base address(assignee), 
 * a constant offset(offset), and the value we're storing(op2)
 */
instruction_t* emit_store_with_constant_offset_ir_code(three_addr_var_t* base_address, three_addr_const_t* offset, three_addr_var_t* storee, generic_type_t* memory_write_type){
	//First allocate
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Now populate with values
	stmt->statement_type = THREE_ADDR_CODE_STORE_WITH_CONSTANT_OFFSET;
	//The base address that we're assigning to
	stmt->assignee = base_address;

	//This is being dereferenced
	stmt->assignee->is_dereferenced = TRUE;

	//The offset placeholder is used for our offset, not op1_const 
	stmt->offset = offset;

	//What we're storing
	stmt->op2 = storee;

	//Important - add the type that we expect to be writing to in memory
	stmt->memory_read_write_type = memory_write_type;

	//And give it back
	return stmt;
}


/**
 * Emit a load statement. This is like an assignment instruction, but we're explicitly
 * using stack memory here
 */
instruction_t* emit_load_ir_code(three_addr_var_t* assignee, three_addr_var_t* op1, generic_type_t* memory_read_type){
	//First allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Let's now populate it with values
	stmt->statement_type = THREE_ADDR_CODE_LOAD_STATEMENT;
	stmt->assignee = assignee;
	stmt->op1 = op1;

	//Important - store the type that we expect to be getting out of memory
	stmt->memory_read_write_type = memory_read_type;
	
	//And that's it, we'll now just give it back
	return stmt;
}


/**
 * Emit a load with offset ir code. We take in a base address(op1), 
 * an offset(op2), and the value we're loading into(assignee)
 */
instruction_t* emit_load_with_variable_offset_ir_code(three_addr_var_t* assignee, three_addr_var_t* base_address, three_addr_var_t* offset, generic_type_t* memory_read_type){
	//First allocate
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Now populate with values
	stmt->statement_type = THREE_ADDR_CODE_LOAD_WITH_VARIABLE_OFFSET;
	//The base address that we're assigning to
	stmt->assignee = assignee;
	//The op1 is our base address
	stmt->op1 = base_address;

	//And op2 is our offset
	stmt->op2 = offset;

	//Important - store the type that we expect to be getting out of memory
	stmt->memory_read_write_type = memory_read_type;

	//And give it back
	return stmt;
}


/**
 * Emit a load with constant offset ir code. We take in a base address(op1), 
 * an offset(offset), and the value we're loading into(assignee)
 */
instruction_t* emit_load_with_constant_offset_ir_code(three_addr_var_t* assignee, three_addr_var_t* base_address, three_addr_const_t* offset, generic_type_t* memory_read_type){
	//First allocate
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Now populate with values
	stmt->statement_type = THREE_ADDR_CODE_LOAD_WITH_CONSTANT_OFFSET;
	//The assignee that we're loading into
	stmt->assignee = assignee;
	//The op1 is our base address
	stmt->op1 = base_address;

	//Our offset is stored in "offset", not op1_const
	stmt->offset = offset;

	//Important - store the type that we expect to be getting out of memory
	stmt->memory_read_write_type = memory_read_type;

	//And give it back
	return stmt;
}


/**
 * Emit a jump statement where we jump to the block with the ID provided
 */
instruction_t* emit_jmp_instruction(void* jumping_to_block){
	//First allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Let's now populate it with values
	stmt->statement_type = THREE_ADDR_CODE_JUMP_STMT;
	stmt->if_block = jumping_to_block;
	//Give the statement back
	return stmt;
}


/**
 * Emit a jump instruction directly
 */
instruction_t* emit_jump_instruction_directly(void* jumping_to_block, instruction_type_t jump_instruction_type){
	//Allocate
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//Directly assign here
	instruction->instruction_type = jump_instruction_type;

	//Point to the jumping to block
	instruction->if_block = jumping_to_block;

	return instruction;
}


/**
 * Emit a stack allocation statement
 */
instruction_t* emit_stack_allocation_ir_statement(three_addr_const_t* bytes_to_allocate){
	//Allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//Directly assign the type here
	instruction->statement_type = THREE_ADDR_CODE_STACK_ALLOCATION_STMT;
	
	//Store the constant
	instruction->op1_const = bytes_to_allocate;

	//And give it back
	return instruction;
}


/**
 * Emit a stack deallocation statement
 */
instruction_t* emit_stack_deallocation_ir_statement(three_addr_const_t* bytes_to_deallocate){
	//Allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//Directly assign the type here
	instruction->statement_type = THREE_ADDR_CODE_STACK_DEALLOCATION_STMT;
	
	//Store the constant
	instruction->op1_const = bytes_to_deallocate;

	//And give it back
	return instruction;
}


/**
 * Emit a branch statement
 */
instruction_t* emit_branch_statement(void* if_block, void* else_block, three_addr_var_t* relies_on, branch_type_t branch_type){
	//First allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Let's now populate it with values
	stmt->statement_type = THREE_ADDR_CODE_BRANCH_STMT;
	
	//If and else block storage
	stmt->if_block = if_block;
	stmt->else_block = else_block;

	//Branch type
	stmt->branch_type = branch_type;

	//And we'll store the variable that we're making a decision based on here
	stmt->op1 = relies_on;

	//Give the statement back
	return stmt;
}


/**
 * Emit an indirect jump statement. The jump statement can take on several different types of jump
 */
instruction_t* emit_indirect_jmp_instruction(three_addr_var_t* address){
	//First we allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Let's now populate it with values
	stmt->statement_type = THREE_ADDR_CODE_INDIRECT_JUMP_STMT;
	//The address we're jumping to is in op1
	stmt->op1 = address;
	//And give it back
	return stmt;
}


/**
 * Emit a function call statement where we're calling the function record provided
 */
instruction_t* emit_function_call_instruction(symtab_function_record_t* func_record, three_addr_var_t* assigned_to){
	//First allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Let's now populate it with values
	stmt->statement_type = THREE_ADDR_CODE_FUNC_CALL;
	stmt->called_function = func_record;
	stmt->assignee = assigned_to;

	//We do NOT add parameters here, instead we had them in the CFG function
	//Just give back the result
	return stmt;
}


/**
 * Emit an indirect function call statement. Once emitted, no paramters will have been added in
 */
instruction_t* emit_indirect_function_call_instruction(three_addr_var_t* function_pointer, three_addr_var_t* assigned_to){
	//First allocate the statement
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Populate it with the appropriate values
	stmt->statement_type = THREE_ADDR_CODE_INDIRECT_FUNC_CALL;
	//We will store the variable for the function that we're calling indirectly in op1
	stmt->op1 = function_pointer;
	//Mark the assignee
	stmt->assignee = assigned_to;

	return stmt;
}


/**
 * Emit a constant directly based on whatever the type given is
 */
three_addr_const_t* emit_direct_integer_or_char_constant(int64_t value, generic_type_t* type){
	//First allocate it
	three_addr_const_t* constant = calloc(1, sizeof(three_addr_const_t));

	//Add into here for memory management
	dynamic_array_add(&emitted_consts, constant);

	//Store the type here
	constant->type = type;

	//If the type is not a basic type, we leave
	if(type->type_class != TYPE_CLASS_BASIC){
		fprintf(stderr, "Please use a basic type for integer constant emittal\n");
		exit(1);
	}

	//Extract this
	ollie_token_t basic_type_token = type->basic_type_token;
	
	switch(basic_type_token){
		case I64:
			constant->const_type = LONG_CONST;
			constant->constant_value.signed_long_constant = value;
			break;
		case U64:
			constant->const_type = LONG_CONST_FORCE_U;
			constant->constant_value.unsigned_long_constant = value;
			break;
		case I32:
			constant->const_type = INT_CONST;
			constant->constant_value.signed_integer_constant = value;
			break;
		case I16:
			constant->const_type = SHORT_CONST;
			constant->constant_value.signed_short_constant = value;
			break;
		case I8:
			constant->const_type = SHORT_CONST;
			constant->constant_value.signed_byte_constant = value;
			break;
		case U32:
			constant->const_type = INT_CONST_FORCE_U;
			constant->constant_value.unsigned_integer_constant = value;
			break;
		case U16:
			constant->const_type = SHORT_CONST_FORCE_U;
			constant->constant_value.unsigned_short_constant = value;
			break;
		case U8:
			constant->const_type = BYTE_CONST_FORCE_U;
			constant->constant_value.unsigned_byte_constant = value;
			break;
		case CHAR:
			constant->const_type = CHAR_CONST;
			constant->constant_value.char_constant = value;
			break;
		default:
			fprintf(stderr, "Please use an integer or char type for constant emittal");
			exit(1);
	}

	//Give back the constant
	return constant;
}


/**
 * Emit a negation statement
 */
instruction_t* emit_neg_instruction(three_addr_var_t* negatee){
	//First we'll create the negation
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Now we populate
	stmt->statement_type = THREE_ADDR_CODE_NEG_STATEMENT;

	//If this is not a temporary variable, then we'll
	//emit an exact copy and let the SSA system handle it
	if(negatee->variable_type != VARIABLE_TYPE_TEMP){
		stmt->assignee = emit_var_copy(negatee);

	//Otherwise, we'll need to spawn a new temporary variable
	} else {
		stmt->assignee = emit_temp_var(negatee->type);
	}

	//No matter what this is the op1
	stmt->op1 = negatee;

	//Give it back
	return stmt;
}


/**
 * Emit a not instruction 
 */
instruction_t* emit_not_instruction(three_addr_var_t* var){
	//First allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Let's make it a not stmt
	stmt->statement_type = THREE_ADDR_CODE_BITWISE_NOT_STMT;
	//The only var here is the assignee
	stmt->assignee = var;
	//For the potential of temp variables
	stmt->op1 = var;

	//Give the statement back
	return stmt;
}


/**
 * Emit a logical not statement
 */
instruction_t* emit_logical_not_instruction(three_addr_var_t* assignee, three_addr_var_t* op1){
	//First allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Let's make it a logical not stmt
	stmt->statement_type = THREE_ADDR_CODE_LOGICAL_NOT_STMT;
	stmt->assignee = assignee;
	//Leave it in here
	stmt->op1 = op1;

	//Flag that this does have an operator, even though we aren't strictly using it
	stmt->op = L_NOT;

	//Give the stmt back
	return stmt;
}


/**
 * Emit an assembly inline statement. Once emitted, these statements are final and are ignored
 * by any future optimizations
 */
instruction_t* emit_asm_inline_instruction(generic_ast_node_t* asm_inline_node){
	//First we allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Store the class
	stmt->statement_type = THREE_ADDR_CODE_ASM_INLINE_STMT;

	//Copy this over
	stmt->inlined_assembly = clone_dynamic_string(&(asm_inline_node->string_value));

	//And we're done, now we'll bail out
	return stmt;
}


/**
 * Emit a phi function for a given variable. Once emitted, these statements are compiler exclusive,
 * but they are needed for our optimization
 */
instruction_t* emit_phi_function(symtab_variable_record_t* variable){
	//First we allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//We'll just store the assignee here, no need for anything else
	stmt->assignee = emit_var(variable);

	//Note what kind of node this is
	stmt->statement_type = THREE_ADDR_CODE_PHI_FUNC;

	//And give the statement back
	return stmt;
}


/**
 * Emit a "test if not 0 three address code statement"
 */
instruction_t* emit_test_if_not_zero_statement(three_addr_var_t* destination_variable, three_addr_var_t* being_tested){
	//First we allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//The assignee/op1 is passed through
	stmt->assignee = destination_variable;
	stmt->op1 = being_tested;

	//Note what kind of node this is
	stmt->statement_type = THREE_ADDR_CODE_TEST_IF_NOT_ZERO_STMT;

	//And give the statement back
	return stmt;
}


/**
 * Emit a fully formed global variable OIR address calculation lea
 *
 * This will always produce instructions like: t8 <- global_var(%rip)
 */
instruction_t* emit_global_variable_address_calculation_oir(three_addr_var_t* assignee, three_addr_var_t* global_variable, three_addr_var_t* instruction_pointer){
	//Get the intstruction out
	instruction_t* lea = calloc(1, sizeof(instruction_t));

	//This will be leaq always
	lea->statement_type = THREE_ADDR_CODE_LEA_STMT;

	//Global var address calc mode
	lea->lea_statement_type = OIR_LEA_TYPE_RIP_RELATIVE;

	//We already know what the destination will be
	lea->assignee = assignee;

	//Copy the global var and give a non-memory address version of it
	three_addr_var_t* remediated_version = emit_var_copy(global_variable);
	remediated_version->variable_type = VARIABLE_TYPE_NON_TEMP;

	//Op1 is the instruction pointer(relative addressing)
	lea->op1 = instruction_pointer;

	//The op2 is always the global var itself
	lea->op2 = remediated_version;

	//And give it back
	return lea;
}


/**
 * Emit a fully formed global variable OIR address calculation with offset lea
 *
 * This will always produce instructions like: t8 <- global_var(%rip)
 */
instruction_t* emit_global_variable_address_calculation_with_offset_oir(three_addr_var_t* assignee, three_addr_var_t* global_variable, three_addr_var_t* instruction_pointer, three_addr_const_t* constant){
	//Get the intstruction out
	instruction_t* lea = calloc(1, sizeof(instruction_t));

	//This will be leaq always
	lea->statement_type = THREE_ADDR_CODE_LEA_STMT;

	//Global var address calc mode
	lea->lea_statement_type = OIR_LEA_TYPE_RIP_RELATIVE_WITH_OFFSET;

	//We already know what the destination will be
	lea->assignee = assignee;

	//Copy the global var and give a non-memory address version of it
	three_addr_var_t* remediated_version = emit_var_copy(global_variable);
	remediated_version->variable_type = VARIABLE_TYPE_NON_TEMP;

	//Op1 is the instruction pointer(relative addressing)
	lea->op1 = instruction_pointer;

	//The op2 is always the global var itself
	lea->op2 = remediated_version;

	//Store the constant offset here as well
	lea->op1_const = constant;

	//And give it back
	return lea;
}


/**
 * Emit a fully formed global variable x86 address calculation lea
 *
 * This will always produce instructions like: leaq global_var(%rip), t8
 */
instruction_t* emit_global_variable_address_calculation_x86(three_addr_var_t* global_variable, three_addr_var_t* instruction_pointer, generic_type_t* u64){
	//Emit a temp var that is always a u64(memory address)
	three_addr_var_t* destination = emit_temp_var(u64);

	//Get the intstruction out
	instruction_t* lea = calloc(1, sizeof(instruction_t));

	//This will be leaq always
	lea->instruction_type = LEAQ;

	//Global var address calc mode
	lea->calculation_mode = ADDRESS_CALCULATION_MODE_RIP_RELATIVE;

	//We already know what the destination will be
	lea->destination_register = destination;

	//Address calc reg 1 is the instruction pointer(relative addressing)
	lea->address_calc_reg1 = instruction_pointer;

	//The offset is the global variable(unique case)
	lea->rip_offset_variable = global_variable;

	//And give it back
	return lea;
}


/**
 * Emit a complete copy of whatever was in here previously
 */
instruction_t* copy_instruction(instruction_t* copied){
	//First we allocate
	instruction_t* copy = calloc(1, sizeof(instruction_t));

	//Perform a complete memory copy
	memcpy(copy, copied, sizeof(instruction_t));

	//Now we'll check for special values. NOTE: if we're using this, we should NOT have
	//any phi functions OR assembly in here. The only thing that we might have are
	//function calls
	
	//Null these out, better safe than sorry
	copy->inlined_assembly = copied->inlined_assembly;
	copy->next_statement = NULL;
	copy->previous_statement = NULL;
	
	//If we have function call parameters, emit a copy of them
	if(copied->parameters.internal_array != NULL){
		copy->parameters = clone_dynamic_array(&(copied->parameters));
	}

	//Give back the copied one
	return copied;
}


/**
 * Sum a constant by a raw int64_t value
 * 
 * NOTE: The result is always stored in the first one, and the first one will become 
 * a long constant. This is specifically designed for lea simplification/address computation
 */
three_addr_const_t* sum_constant_with_raw_int64_value(three_addr_const_t* constant, generic_type_t* i64_type, int64_t raw_constant){
	//Go based on the first one's type
	switch(constant->const_type){
		case INT_CONST_FORCE_U:
			//Reassign
			constant->constant_value.signed_long_constant = constant->constant_value.unsigned_integer_constant;

			//Multiply
			constant->constant_value.signed_long_constant += raw_constant;

			break;

		case INT_CONST:
			//Reassign
			constant->constant_value.signed_long_constant = constant->constant_value.signed_integer_constant;

			//Multiply
			constant->constant_value.signed_long_constant += raw_constant;

			break;

		case LONG_CONST_FORCE_U:
			//Reassign
			constant->constant_value.signed_long_constant = constant->constant_value.unsigned_long_constant;

			//Multiply
			constant->constant_value.signed_long_constant += raw_constant;

			break;

		case LONG_CONST:
			//Multiply
			constant->constant_value.signed_long_constant += raw_constant;

			break;

		case CHAR_CONST:
			//Reassign
			constant->constant_value.signed_long_constant = constant->constant_value.char_constant;

			//Multiply
			constant->constant_value.signed_long_constant += raw_constant;

			break;

		//This should never happen
		default:
			printf("Fatal internal compiler error: Unsupported constant addition operation\n");
			exit(1);
	}

	//This will always be forced to be an i64
	constant->type = i64_type;
	constant->const_type = LONG_CONST;

	//Give it back for clarity
	return constant;
}


/**
 * Multiply a constant by a raw int64_t value
 * 
 * NOTE: The result is always stored in the first one, and the first one will become 
 * a long constant. This is specifically designed for lea simplification
 */
three_addr_const_t* multiply_constant_by_raw_int64_value(three_addr_const_t* constant, generic_type_t* i64_type, int64_t raw_constant){
	//Go based on the first one's type
	switch(constant->const_type){
		case SHORT_CONST:
			//Reassign
			constant->constant_value.signed_long_constant = constant->constant_value.signed_short_constant;

			//Multiply
			constant->constant_value.signed_long_constant *= raw_constant;

			break;

		case SHORT_CONST_FORCE_U:
			//Reassign
			constant->constant_value.signed_long_constant = constant->constant_value.unsigned_short_constant;

			//Multiply
			constant->constant_value.signed_long_constant *= raw_constant;

			break;

		case INT_CONST_FORCE_U:
			//Reassign
			constant->constant_value.signed_long_constant = constant->constant_value.unsigned_integer_constant;

			//Multiply
			constant->constant_value.signed_long_constant *= raw_constant;

			break;

		case INT_CONST:
			//Reassign
			constant->constant_value.signed_long_constant = constant->constant_value.signed_integer_constant;

			//Multiply
			constant->constant_value.signed_long_constant *= raw_constant;

			break;

		case LONG_CONST_FORCE_U:
			//Reassign
			constant->constant_value.signed_long_constant = constant->constant_value.unsigned_long_constant;

			//Multiply
			constant->constant_value.signed_long_constant *= raw_constant;

			break;

		case LONG_CONST:
			//Multiply
			constant->constant_value.signed_long_constant *= raw_constant;

			break;

		case CHAR_CONST:
			//Reassign
			constant->constant_value.signed_long_constant = constant->constant_value.char_constant;

			//Multiply
			constant->constant_value.signed_long_constant *= raw_constant;

			break;

		//This should never happen
		default:
			printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
			exit(1);
	}

	//This will always be forced to be an i64
	constant->type = i64_type;
	constant->const_type = LONG_CONST;

	//Give it back for clarity
	return constant;
}


/**
 * Multiply two constants together
 * 
 * NOTE: The result is always stored in the first one
 */
void multiply_constants(three_addr_const_t* constant1, three_addr_const_t* constant2){
	switch(constant1->const_type){
		case INT_CONST_FORCE_U:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.unsigned_integer_constant *= constant2->constant_value.unsigned_long_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.unsigned_integer_constant *= constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.unsigned_integer_constant *= constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.unsigned_integer_constant *= constant2->constant_value.signed_integer_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.unsigned_integer_constant *= constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.unsigned_integer_constant *= constant2->constant_value.unsigned_short_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.unsigned_integer_constant *= constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.unsigned_integer_constant *= constant2->constant_value.unsigned_byte_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.unsigned_integer_constant *= constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
					exit(1);
			}

			break;

		case INT_CONST:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.signed_integer_constant *= constant2->constant_value.unsigned_long_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.signed_integer_constant *= constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.signed_integer_constant *= constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.signed_integer_constant *= constant2->constant_value.signed_integer_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.signed_integer_constant *= constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.signed_integer_constant *= constant2->constant_value.unsigned_byte_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.signed_integer_constant *= constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.signed_integer_constant *= constant2->constant_value.unsigned_short_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.signed_integer_constant *= constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.signed_byte_constant *= constant2->constant_value.unsigned_integer_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.signed_byte_constant *= constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.signed_byte_constant *= constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.signed_byte_constant *= constant2->constant_value.signed_integer_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.signed_byte_constant *= constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.signed_byte_constant *= constant2->constant_value.unsigned_short_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.signed_byte_constant *= constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.signed_byte_constant *= constant2->constant_value.unsigned_byte_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.signed_byte_constant *= constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
					exit(1);
			}
			
			break;

		case BYTE_CONST_FORCE_U:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.unsigned_byte_constant *= constant2->constant_value.unsigned_integer_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.unsigned_byte_constant *= constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.unsigned_byte_constant *= constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.unsigned_byte_constant *= constant2->constant_value.signed_integer_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.unsigned_byte_constant *= constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.unsigned_byte_constant *= constant2->constant_value.unsigned_short_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.unsigned_byte_constant *= constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.unsigned_byte_constant *= constant2->constant_value.unsigned_byte_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.unsigned_byte_constant *= constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
					exit(1);
			}
			
			break;

		case LONG_CONST_FORCE_U:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.unsigned_long_constant *= constant2->constant_value.unsigned_integer_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.unsigned_long_constant *= constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.unsigned_long_constant *= constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.unsigned_long_constant *= constant2->constant_value.signed_integer_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.unsigned_long_constant *= constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.unsigned_long_constant *= constant2->constant_value.unsigned_short_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.unsigned_long_constant *= constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.unsigned_long_constant *= constant2->constant_value.unsigned_byte_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.unsigned_long_constant *= constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
					exit(1);
			}
			
			break;

		case LONG_CONST:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.signed_long_constant *= constant2->constant_value.unsigned_long_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.signed_long_constant *= constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.signed_long_constant *= constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.signed_long_constant *= constant2->constant_value.signed_integer_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.signed_long_constant *= constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.signed_long_constant *= constant2->constant_value.unsigned_byte_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.signed_long_constant *= constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.signed_long_constant *= constant2->constant_value.unsigned_short_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.signed_long_constant *= constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.signed_short_constant *= constant2->constant_value.unsigned_long_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.signed_short_constant *= constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.signed_short_constant *= constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.signed_short_constant *= constant2->constant_value.signed_integer_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.signed_short_constant *= constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.signed_short_constant *= constant2->constant_value.unsigned_byte_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.signed_short_constant *= constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.signed_short_constant *= constant2->constant_value.unsigned_short_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.signed_short_constant *= constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
					exit(1);
			}

			break;
			

		case SHORT_CONST_FORCE_U:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.unsigned_short_constant *= constant2->constant_value.unsigned_long_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.unsigned_short_constant *= constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.unsigned_short_constant *= constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.unsigned_short_constant *= constant2->constant_value.signed_integer_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.unsigned_short_constant *= constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.unsigned_short_constant *= constant2->constant_value.unsigned_byte_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.unsigned_short_constant *= constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.unsigned_short_constant *= constant2->constant_value.unsigned_short_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.unsigned_short_constant *= constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
					exit(1);
			}

			break;

		case CHAR_CONST:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.char_constant *= constant2->constant_value.unsigned_long_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.char_constant *= constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.char_constant *= constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.char_constant *= constant2->constant_value.signed_integer_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.char_constant *= constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.char_constant *= constant2->constant_value.unsigned_byte_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.char_constant *= constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.char_constant *= constant2->constant_value.unsigned_short_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.char_constant *= constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
					exit(1);
			}

			break;

		default:
			printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
			exit(1);
	}
}


/**
 * Emit the sum of two given constants. The result will overwrite the first constant given
 *
 * NOTE: The result is always stored in the first one
 */
void add_constants(three_addr_const_t* constant1, three_addr_const_t* constant2){
	switch(constant1->const_type){
		case INT_CONST_FORCE_U:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.unsigned_integer_constant += constant2->constant_value.unsigned_long_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.unsigned_integer_constant += constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.unsigned_integer_constant += constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.unsigned_integer_constant += constant2->constant_value.signed_integer_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.unsigned_integer_constant += constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.unsigned_integer_constant += constant2->constant_value.unsigned_short_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.unsigned_integer_constant += constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.unsigned_integer_constant += constant2->constant_value.unsigned_byte_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.unsigned_integer_constant += constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant addition operation\n");
					exit(1);
			}

			break;

		case INT_CONST:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.signed_integer_constant += constant2->constant_value.unsigned_long_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.signed_integer_constant += constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.signed_integer_constant += constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.signed_integer_constant += constant2->constant_value.signed_integer_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.signed_integer_constant += constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.signed_integer_constant += constant2->constant_value.unsigned_byte_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.signed_integer_constant += constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.signed_integer_constant += constant2->constant_value.unsigned_short_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.signed_integer_constant += constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant addition operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.signed_byte_constant += constant2->constant_value.unsigned_integer_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.signed_byte_constant += constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.signed_byte_constant += constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.signed_byte_constant += constant2->constant_value.signed_integer_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.signed_byte_constant += constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.signed_byte_constant += constant2->constant_value.unsigned_short_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.signed_byte_constant += constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.signed_byte_constant += constant2->constant_value.unsigned_byte_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.signed_byte_constant += constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant addition operation\n");
					exit(1);
			}
			
			break;

		case BYTE_CONST_FORCE_U:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.unsigned_byte_constant += constant2->constant_value.unsigned_integer_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.unsigned_byte_constant += constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.unsigned_byte_constant += constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.unsigned_byte_constant += constant2->constant_value.signed_integer_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.unsigned_byte_constant += constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.unsigned_byte_constant += constant2->constant_value.unsigned_short_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.unsigned_byte_constant += constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.unsigned_byte_constant += constant2->constant_value.unsigned_byte_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.unsigned_byte_constant += constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant addition operation\n");
					exit(1);
			}
			
			break;

		case LONG_CONST_FORCE_U:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.unsigned_long_constant += constant2->constant_value.unsigned_integer_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.unsigned_long_constant += constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.unsigned_long_constant += constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.unsigned_long_constant += constant2->constant_value.signed_integer_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.unsigned_long_constant += constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.unsigned_long_constant += constant2->constant_value.unsigned_short_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.unsigned_long_constant += constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.unsigned_long_constant += constant2->constant_value.unsigned_byte_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.unsigned_long_constant += constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant addition operation\n");
					exit(1);
			}
			
			break;

		case LONG_CONST:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.signed_long_constant += constant2->constant_value.unsigned_long_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.signed_long_constant += constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.signed_long_constant += constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.signed_long_constant += constant2->constant_value.signed_integer_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.signed_long_constant += constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.signed_long_constant += constant2->constant_value.unsigned_byte_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.signed_long_constant += constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.signed_long_constant += constant2->constant_value.unsigned_short_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.signed_long_constant += constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant addition operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.signed_short_constant += constant2->constant_value.unsigned_long_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.signed_short_constant += constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.signed_short_constant += constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.signed_short_constant += constant2->constant_value.signed_integer_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.signed_short_constant += constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.signed_short_constant += constant2->constant_value.unsigned_byte_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.signed_short_constant += constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.signed_short_constant += constant2->constant_value.unsigned_short_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.signed_short_constant += constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant addition operation\n");
					exit(1);
			}

			break;
			

		case SHORT_CONST_FORCE_U:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.unsigned_short_constant += constant2->constant_value.unsigned_long_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.unsigned_short_constant += constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.unsigned_short_constant += constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.unsigned_short_constant += constant2->constant_value.signed_integer_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.unsigned_short_constant += constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.unsigned_short_constant += constant2->constant_value.unsigned_byte_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.unsigned_short_constant += constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.unsigned_short_constant += constant2->constant_value.unsigned_short_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.unsigned_short_constant += constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant addition operation\n");
					exit(1);
			}

			break;

		case CHAR_CONST:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.char_constant += constant2->constant_value.unsigned_long_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.char_constant += constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.char_constant += constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.char_constant += constant2->constant_value.signed_integer_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.char_constant += constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.char_constant += constant2->constant_value.unsigned_byte_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.char_constant += constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.char_constant += constant2->constant_value.unsigned_short_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.char_constant += constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant addition operation\n");
					exit(1);
			}

			break;

		default:
			printf("Fatal internal compiler error: Unsupported constant addition operation\n");
			exit(1);
	}
}


/**
 * Emit the difference of two given constants. The result will overwrite the first constant given
 *
 * NOTE: The result is always stored in the first one
 */
void subtract_constants(three_addr_const_t* constant1, three_addr_const_t* constant2){
	switch(constant1->const_type){
		case INT_CONST_FORCE_U:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.unsigned_integer_constant -= constant2->constant_value.unsigned_long_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.unsigned_integer_constant -= constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.unsigned_integer_constant -= constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.unsigned_integer_constant -= constant2->constant_value.signed_integer_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.unsigned_integer_constant -= constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.unsigned_integer_constant -= constant2->constant_value.unsigned_short_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.unsigned_integer_constant -= constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.unsigned_integer_constant -= constant2->constant_value.unsigned_byte_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.unsigned_integer_constant -= constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
					exit(1);
			}

			break;

		case INT_CONST:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.signed_integer_constant -= constant2->constant_value.unsigned_long_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.signed_integer_constant -= constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.signed_integer_constant -= constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.signed_integer_constant -= constant2->constant_value.signed_integer_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.signed_integer_constant -= constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.signed_integer_constant -= constant2->constant_value.unsigned_byte_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.signed_integer_constant -= constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.signed_integer_constant -= constant2->constant_value.unsigned_short_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.signed_integer_constant -= constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
					exit(1);
			}

			break;

		case BYTE_CONST:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.signed_byte_constant -= constant2->constant_value.unsigned_integer_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.signed_byte_constant -= constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.signed_byte_constant -= constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.signed_byte_constant -= constant2->constant_value.signed_integer_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.signed_byte_constant -= constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.signed_byte_constant -= constant2->constant_value.unsigned_short_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.signed_byte_constant -= constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.signed_byte_constant -= constant2->constant_value.unsigned_byte_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.signed_byte_constant -= constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
					exit(1);
			}
			
			break;

		case BYTE_CONST_FORCE_U:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.unsigned_byte_constant -= constant2->constant_value.unsigned_integer_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.unsigned_byte_constant -= constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.unsigned_byte_constant -= constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.unsigned_byte_constant -= constant2->constant_value.signed_integer_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.unsigned_byte_constant -= constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.unsigned_byte_constant -= constant2->constant_value.unsigned_short_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.unsigned_byte_constant -= constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.unsigned_byte_constant -= constant2->constant_value.unsigned_byte_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.unsigned_byte_constant -= constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
					exit(1);
			}
			
			break;

		case LONG_CONST_FORCE_U:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.unsigned_long_constant -= constant2->constant_value.unsigned_integer_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.unsigned_long_constant -= constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.unsigned_long_constant -= constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.unsigned_long_constant -= constant2->constant_value.signed_integer_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.unsigned_long_constant -= constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.unsigned_long_constant -= constant2->constant_value.unsigned_short_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.unsigned_long_constant -= constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.unsigned_long_constant -= constant2->constant_value.unsigned_byte_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.unsigned_long_constant -= constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
					exit(1);
			}
			
			break;

		case LONG_CONST:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.signed_long_constant -= constant2->constant_value.unsigned_long_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.signed_long_constant -= constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.signed_long_constant -= constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.signed_long_constant -= constant2->constant_value.signed_integer_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.signed_long_constant -= constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.signed_long_constant -= constant2->constant_value.unsigned_byte_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.signed_long_constant -= constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.signed_long_constant -= constant2->constant_value.unsigned_short_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.signed_long_constant -= constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
					exit(1);
			}

			break;

		case SHORT_CONST:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.signed_short_constant -= constant2->constant_value.unsigned_long_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.signed_short_constant -= constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.signed_short_constant -= constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.signed_short_constant -= constant2->constant_value.signed_integer_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.signed_short_constant -= constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.signed_short_constant -= constant2->constant_value.unsigned_byte_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.signed_short_constant -= constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.signed_short_constant -= constant2->constant_value.unsigned_short_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.signed_short_constant -= constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
					exit(1);
			}

			break;
			

		case SHORT_CONST_FORCE_U:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.unsigned_short_constant -= constant2->constant_value.unsigned_long_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.unsigned_short_constant -= constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.unsigned_short_constant -= constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.unsigned_short_constant -= constant2->constant_value.signed_integer_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.unsigned_short_constant -= constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.unsigned_short_constant -= constant2->constant_value.unsigned_byte_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.unsigned_short_constant -= constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.unsigned_short_constant -= constant2->constant_value.unsigned_short_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.unsigned_short_constant -= constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
					exit(1);
			}

			break;

		case CHAR_CONST:
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.char_constant -= constant2->constant_value.unsigned_long_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.char_constant -= constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.char_constant -= constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.char_constant -= constant2->constant_value.signed_integer_constant;
					break;
				case BYTE_CONST:
					constant1->constant_value.char_constant -= constant2->constant_value.signed_byte_constant;
					break;
				case BYTE_CONST_FORCE_U:
					constant1->constant_value.char_constant -= constant2->constant_value.unsigned_byte_constant;
					break;
				case SHORT_CONST:
					constant1->constant_value.char_constant -= constant2->constant_value.signed_short_constant;
					break;
				case SHORT_CONST_FORCE_U:
					constant1->constant_value.char_constant -= constant2->constant_value.unsigned_short_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.char_constant -= constant2->constant_value.char_constant;
					break;
				default:
					printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
					exit(1);
			}

			break;

		default:
			printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
			exit(1);
	}
}


/**
 * Logical or two constants. The result is always stored in constant1
 */
void logical_or_constants(three_addr_const_t* constant1, three_addr_const_t* constant2){
	//Determine if they are 0 or not
	u_int8_t const_1_0 = is_constant_value_zero(constant1);
	u_int8_t const_2_0 = is_constant_value_zero(constant2);

	//Go through the 4 cases in the truth table
	if(const_1_0 == TRUE){
		/* 0 || (non-zero) = 1 */
		if(const_2_0 == FALSE){
			constant1->constant_value.unsigned_long_constant = 1;
		/* 0 || 0 = 0 */
		} else {
			constant1->constant_value.unsigned_long_constant = 0;
		}

	//This is non-zero, the other one is irrelevant
	} else {
		constant1->constant_value.unsigned_long_constant = 1;
	}
}


/**
 * Logical and two constants. The result is always stored in constant1
 */
void logical_and_constants(three_addr_const_t* constant1, three_addr_const_t* constant2){
	//Determine if they are 0 or not
	u_int8_t const_1_0 = is_constant_value_zero(constant1);
	u_int8_t const_2_0 = is_constant_value_zero(constant2);

	//If this one is 0, the other one's result is irrelevant
	if(const_1_0 == TRUE){
		constant1->constant_value.unsigned_long_constant = 0;

	//Nonzero
	} else {
		/* (non-zero) && (non-zero) = 1 */
		if(const_2_0 == FALSE){
			constant1->constant_value.unsigned_long_constant = 1;
		/* (non-zero) && 0 = 0 */
		} else {
			constant1->constant_value.unsigned_long_constant = 0;
		}
	}
}


/**
 * select the appropriate branch statement given the circumstances, including operand and signedness
 */
branch_type_t select_appropriate_branch_statement(ollie_token_t op, branch_category_t branch_type, u_int8_t is_signed){
	//Let's see what we have here
	switch(op){
		case G_THAN:
			if(branch_type == BRANCH_CATEGORY_INVERSE){
				if(is_signed == TRUE){
					//Signed version
					return BRANCH_LE;
				} else {
					//Unsigned version
					return BRANCH_BE;
				}
			} else {
				if(is_signed == TRUE){
					//Signed version
					return BRANCH_G;
				} else {
					//Unsigned version
					return BRANCH_A;
				}
			}
		case L_THAN:
			if(branch_type == BRANCH_CATEGORY_INVERSE){
				if(is_signed == TRUE){
					//Signed version
					return BRANCH_GE;
				} else {
					//Unsigned version
					return BRANCH_AE;
				}
			} else {
				if(is_signed == TRUE){
					//Signed version
					return BRANCH_L;
				} else {
					//Unsigned version
					return BRANCH_B;
				}
			}
		case L_THAN_OR_EQ:
			if(branch_type == BRANCH_CATEGORY_INVERSE){
				if(is_signed == TRUE){
					//Signed version
					return BRANCH_G;
				} else {
					//Unsigned version
					return BRANCH_A;
				}
			} else {
				if(is_signed == TRUE){
					//Signed version
					return BRANCH_LE;
				} else {
					//Unsigned version
					return BRANCH_BE;
				}
			}
		case G_THAN_OR_EQ:
			if(branch_type == BRANCH_CATEGORY_INVERSE){
				if(is_signed == TRUE){
					//Signed version
					return BRANCH_L;
				} else {
					//Unsigned version
					return BRANCH_B;
				}
			} else {
				if(is_signed == TRUE){
					//Signed version
					return BRANCH_GE;
				} else {
					//Unsigned version
					return BRANCH_AE;
				}
			}
		case DOUBLE_EQUALS:
			if(branch_type == BRANCH_CATEGORY_INVERSE){
				return BRANCH_NE;
			} else {
				return BRANCH_E;
			}
		case NOT_EQUALS:
			if(branch_type == BRANCH_CATEGORY_INVERSE){
				return BRANCH_E;
			} else {
				return BRANCH_NE;
			}

		//Logical not is *TRUE* when the value is zero, and not
		//true when the value isn't zero
		case L_NOT:
			if(branch_type == BRANCH_CATEGORY_INVERSE){
				return BRANCH_NZ;
			} else {
				return BRANCH_Z;
			}

		//If we get here, it was some kind of
		//non relational operator. In this case,
		//we default to 0 = false non zero = true
		default:
			if(branch_type == BRANCH_CATEGORY_INVERSE){
				return BRANCH_Z;
			} else {
				return BRANCH_NZ;
			}
	}
}


/**
 * Get the estimated cycle count for a given instruction. This count
 * is of course estimated, we cannot know for sure
 */
u_int32_t get_estimated_cycle_count(instruction_t* instruction){
	switch(instruction->instruction_type){
		case MULQ:
		case MULL:
		case MULW:
		case MULB:
			return UNSIGNED_INT_MULTIPLY_CYCLE_COUNT;
		case IMULQ:
		case IMULW:
		case IMULL:
		case IMULB:
			return SIGNED_INT_MULTIPLY_CYCLE_COUNT;
		case DIVQ:
		case DIVL:
		case DIVW:
		case DIVB:
			return UNSIGNED_INT_DIVIDE_CYCLE_COUNT;
		case IDIVQ:
		case IDIVL:
		case IDIVW:
		case IDIVB:
			return SIGNED_INT_DIVIDE_CYCLE_COUNT;
		/**
		 * For moves, we have to account for the differences
		 * in expense between loading and storing and just regular
		 * moves
		 */
		case MOVL:
		case MOVQ:
		case MOVB:
		case MOVW:
		case MOVSBL:
		case MOVSBW:
		case MOVSBQ:
		case MOVZBL:
		case MOVZBW:
		case MOVZBQ:
		case MOVSWL:
		case MOVSWQ:
		case MOVZWL:
		case MOVZWQ:
		case MOVSLQ:
			//Now go based on how we are hitting memory
			switch(instruction->memory_access_type){
				//Loads are typically more expensive than
				//stores
				case READ_FROM_MEMORY:
					return LOAD_CYCLE_COUNT;
				case WRITE_TO_MEMORY:
					return STORE_CYCLE_COUNT;
				//Register moves are the cheapest of all
				default:
					return DEFAULT_CYCLE_COUNT;
			}

		//By default we assume the default
		default:
			return DEFAULT_CYCLE_COUNT;
	}

}


/**
 * Are two variables equal? A helper method for searching
 */
u_int8_t variables_equal(three_addr_var_t* a, three_addr_var_t* b, u_int8_t ignore_indirection){
	//Easy way to tell here
	if(a == NULL || b == NULL){
		return FALSE;
	}

	//Are we ignoring indirection? If not, we need to compare the dereference here
	if(ignore_indirection == FALSE && a->is_dereferenced != b->is_dereferenced){
		return FALSE;
	}

	//Another easy way to tell
	if(a->variable_type != b->variable_type){
		return FALSE;
	}

	//For temporary variables, the comparison is very easy
	if(a->variable_type == VARIABLE_TYPE_TEMP){
		if(a->temp_var_number == b->temp_var_number){
			return TRUE;
		} else {
			return FALSE;
		}

	//Otherwise, we're comparing two non-temp variables
	} else {
		//Do they reference the same overall variable?
		if(a->linked_var != b->linked_var){
			return FALSE;
		}

		//Finally check their SSA levels
		if(a->ssa_generation == b->ssa_generation){
			return TRUE;
		}
	}

	//If we get here it's a no go
	return FALSE;
}


/**
 * Are two variables equal regardless of their SSA level? A helper method for searching
 */
u_int8_t variables_equal_no_ssa(three_addr_var_t* a, three_addr_var_t* b, u_int8_t ignore_indirection){
	//Easy way to tell here
	if(a == NULL || b == NULL){
		return FALSE;
	}

	//Are we ignoring indirection? If not, we need to compare the dereference here
	if(ignore_indirection == FALSE && a->is_dereferenced != b->is_dereferenced){
		return FALSE;
	}

	//Another easy way to tell
	if(a->variable_type != b->variable_type){
		return FALSE;
	}

	//For temporary variables, the comparison is very easy
	if(a->variable_type == VARIABLE_TYPE_TEMP){
		if(a->temp_var_number == b->temp_var_number){
			return TRUE;
		} else {
			return FALSE;
		}
	//Otherwise, we're comparing two non-temp variables
	} else if(a->linked_var == b->linked_var){
			return TRUE;
	}

	//If we get here it's a no go
	return FALSE;
}


/**
 * Deallocate the variable portion of a three address code
*/
void three_addr_var_dealloc(three_addr_var_t* var){
	//Null check as appropriate
	if(var != NULL){
		free(var);
	}
}

/**
 * Dellocate the constant portion of a three address code
 */
void three_addr_const_dealloc(three_addr_const_t* constant){
	//Null check as appropriate
	if(constant != NULL){
		free(constant);
	}
}


/**
 * Deallocate the entire three address code statement
*/
void instruction_dealloc(instruction_t* stmt){
	//If the statement is null we bail out
	if(stmt == NULL){
		return;
	}

	//If we have a phi function, deallocate the dynamic array
	if(stmt->parameters.internal_array != NULL){
		dynamic_array_dealloc(&(stmt->parameters));
	}
	
	//Free the overall stmt -- variables handled elsewhere
	free(stmt);
}


/**
 * Deallocate all variables using our global list strategy
*/
void deallocate_all_vars(){
	//Until we're empty
	while(dynamic_array_is_empty(&emitted_vars) == FALSE){
		//O(1) removal
		three_addr_var_t* variable = dynamic_array_delete_from_back(&emitted_vars);

		//Free it
		free(variable);
	}

	//Finally scrap the array
	dynamic_array_dealloc(&emitted_vars);
}


/**
 * Deallocate all constants using our global list strategy
*/
void deallocate_all_consts(){
	//Until we're empty
	while(dynamic_array_is_empty(&emitted_consts) == FALSE){
		//O(1) removal
		three_addr_const_t* constant = dynamic_array_delete_from_back(&emitted_consts);

		//Free it
		free(constant);
	}

	//Finally scrap the array
	dynamic_array_dealloc(&emitted_consts);
}
