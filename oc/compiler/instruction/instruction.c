/**
 * Author: Jack Robbins
 *
 * This is the implementation file for the three_address_code header file
*/

#include "instruction.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "../cfg/cfg.h"
#include "../jump_table/jump_table.h"
#include "../utils/dynamic_string/dynamic_string.h"
#include "../utils/constants.h"

//The atomically increasing temp name id
static int32_t current_temp_id = 0;
//The current function
static symtab_function_record_t* current_function = NULL;

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
	var->initializer_value.constant_value = value;

	//Give the var back
	return var;
}


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
static u_int8_t is_signed_power_of_2(int64_t value){
	//If it's negative or 0, we're done here
	if(value <= 0){
		return FALSE;
	}

	//Using the bitwise formula described above
	if((value & (value - 1)) == 0){
		return TRUE;
	} else {
		return FALSE;
	}
}


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
static u_int8_t is_unsigned_power_of_2(u_int64_t value){
	//Using the bitwise formula described above
	if((value & (value - 1)) == 0){
		return TRUE;
	} else {
		return FALSE;
	}
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
}


/**
 * Declare that we are in a new function
 */
void set_new_function(symtab_function_record_t* func){
	//We'll save this up top
	current_function = func;
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
 * Helper function to determine if an operator is can be constant folded
 */
u_int8_t is_operation_valid_for_op1_assignment_folding(ollie_token_t op){
	switch(op){
		case G_THAN:
		case L_THAN:
		case G_THAN_OR_EQ:
		case L_THAN_OR_EQ:
		case DOUBLE_EQUALS:
		case NOT_EQUALS:
		//Note that this is valid only for logical and. Logical or
		//requires the use of the "orX" instruction, which does modify
		//its assignee unlike logical and
		case DOUBLE_AND:
			return TRUE;
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
		case SUBB:
		case SUBW:
		case SUBL:
		case SUBQ:
		case IMULB:
		case IMULW:
		case IMULL:
		case IMULQ:
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
		case ANDW:
		case ANDB:
		case ANDL:
		case ANDQ:
		case ORB:
		case ORW:
		case ORL:
		case ORQ:
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
		case INT_CONST:
			return is_signed_power_of_2(constant->constant_value.signed_integer_constant);

		case INT_CONST_FORCE_U:
			return is_unsigned_power_of_2(constant->constant_value.unsigned_integer_constant);

		case LONG_CONST:
			return is_signed_power_of_2(constant->constant_value.signed_long_constant);

		case LONG_CONST_FORCE_U:
			return is_unsigned_power_of_2(constant->constant_value.unsigned_long_constant);

		//Chars are always unsigned
		case CHAR_CONST:
			return is_unsigned_power_of_2(constant->constant_value.char_constant);

		//By default just return false
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
			//If there's a source register we're good
			if(instruction->source_register != NULL
				//It's only a copy if we're not accessing memory
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
	emitted_var->stack_region = var->stack_region;

	//The membership is also copied
	emitted_var->membership = var->membership;

	//Copy these over
	emitted_var->parameter_number = var->function_parameter_order;

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
	emitted_var->stack_region = var->stack_region;

	//The membership is also copied
	emitted_var->membership = var->membership;

	//Copy these over
	emitted_var->parameter_number = var->function_parameter_order;

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
 */
instruction_t* emit_direct_register_push_instruction(general_purpose_register_t reg){
	//First allocate
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//Set the type
	instruction->instruction_type = PUSH_DIRECT;

	//Now we'll set the register
	instruction->push_or_pop_reg = reg;

	//Now give it back
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
 * Sometimes we just want to pop a given register. We're able to do this
 * by directly emitting a pop instruction with the register in it. This
 * saves us allocation overhead
 */
instruction_t* emit_direct_register_pop_instruction(general_purpose_register_t reg){
	//First allocate
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//Set the type
	instruction->instruction_type = POP_DIRECT;

	//Now we'll set the register
	instruction->push_or_pop_reg = reg;

	//Now give it back
	return instruction;
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
	//What function are we in
	stmt->function = current_function;

	//This only has registers
	stmt->lea_statement_type = OIR_LEA_TYPE_REGISTERS_ONLY;

	//And now we give it back
	return stmt;
}


/**
 * Emit a statement that is in LEA form
 */
instruction_t* emit_lea_instruction_multiplier_and_operands(three_addr_var_t* assignee, three_addr_var_t* op1, three_addr_var_t* op2, u_int64_t type_size){
	//First we allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Now we'll make our populations
	stmt->statement_type = THREE_ADDR_CODE_LEA_STMT;
	stmt->assignee = assignee;
	stmt->op1 = op1;
	stmt->op2 = op2;
	stmt->lea_multiplicator = type_size;
	//What function are we in
	stmt->function = current_function;

	//This has registers and a multiplier
	stmt->lea_statement_type = OIR_LEA_TYPE_REGISTERS_AND_SCALE;

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
	stmt->lea_multiplicator = type_size;

	//Mark the current function
	stmt->function = current_function;

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
	//What function are we in
	stmt->function = current_function;
	//And we're done
	return stmt;
}


/**
 * Emit a setX instruction
 */
instruction_t* emit_setX_instruction(ollie_token_t op, three_addr_var_t* destination_register, three_addr_var_t* relies_on, u_int8_t is_signed){
	//First allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//We'll need to give it the assignee
	stmt->destination_register = destination_register;

	//What do we relie on
	stmt->op1 = relies_on;

	//We'll determine the actual instruction type using the helper
	stmt->instruction_type = select_appropriate_set_stmt(op, is_signed);

	//Once that's done, we'll return
	return stmt;
}


/**
 * Emit a setne three address code statement
 */
instruction_t* emit_setne_code(three_addr_var_t* assignee, three_addr_var_t* relies_on){
	//First allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Save the assignee
	stmt->assignee = assignee;

	stmt->op1 = relies_on;

	//We'll determine the actual instruction type using the helper
	stmt->statement_type = THREE_ADDR_CODE_SETNE_STMT;

	//Once that's done, we'll return
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
		case NO_REG:
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
		case NO_REG:
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
		case NO_REG:
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
		case NO_REG:
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
 * Print a variable in name only. There are no spaces around the variable, and there
 * will be no newline inserted at all. This is meant solely for the use of the "print_three_addr_code_stmt"
 * and nothing more. This function is also designed to take into account the indirection aspected as well
 */
void print_variable(FILE* fl, three_addr_var_t* variable, variable_printing_mode_t mode){
	//Go based on what printing mode we're after
	switch(mode){
		case PRINTING_LIVE_RANGES:
			fprintf(fl, "LR%d", variable->associated_live_range->live_range_id);
			break;

		case PRINTING_REGISTERS:
			//Special edge case
			if(variable->associated_live_range->reg == NO_REG){
				fprintf(fl, "LR%d", variable->associated_live_range->live_range_id);
				break;
			}
			
			//Switch based on the variable's size to print out the register
			switch(variable->variable_size){
				case QUAD_WORD:
					print_64_bit_register_name(fl, variable->associated_live_range->reg);
					break;
				case DOUBLE_WORD:
					print_32_bit_register_name(fl, variable->associated_live_range->reg);
					break;
				case WORD:
					print_16_bit_register_name(fl, variable->associated_live_range->reg);
					break;
				case BYTE:
					print_8_bit_register_name(fl, variable->associated_live_range->reg);
					break;
				default:
					print_64_bit_register_name(fl, variable->associated_live_range->reg);
					printf("DEFAULTED\n");
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
				case VARIABLE_TYPE_MEMORY_ADDRESS:
					//Print out the normal version, plus the MEM<> wrapper
					fprintf(fl, "MEM<%s_%d>", variable->linked_var->var_name.string, variable->ssa_generation);
					break;
			}

			break;
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
		} else {
			fprintf(fl, "\t.data\n");
		}

		//Now print out the alignment
		fprintf(fl, "\t.align %d\n", get_base_alignment_type(variable->variable->type_defined_as)->type_size);
		
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
				fprintf(fl, "\t.long %ld\n", variable->initializer_value.constant_value->constant_value.signed_long_constant);
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
					fprintf(fl, "\t.long %ld\n", constant_value->constant_value.signed_long_constant);
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
		//We do not print out string constants directly. Instead, we print
		//out the local constant ID that is associated with them
		case STR_CONST:
			fprintf(fl, ".LC%d", constant->local_constant->local_constant_id);
			break;
		case FLOAT_CONST:
			fprintf(fl, "%f", constant->constant_value.float_constant);
			break;
		case DOUBLE_CONST:
			fprintf(fl, "%f", constant->constant_value.double_constant);
			break;
		case FUNC_CONST:
			fprintf(fl, "%s", constant->constant_value.function_name->func_name.string);
			break;
		//To stop compiler warnings
		default:
			break;
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

		case THREE_ADDR_CODE_TEST_STMT:
			//First print the assignee
			print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);
			fprintf(fl, " <- test ");
			print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
			fprintf(fl, ", ");
			print_variable(fl, stmt->op2, PRINTING_VAR_INLINE);
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
			fprintf(fl, "%s", stmt->inlined_assembly.string);
			break;

		case THREE_ADDR_CODE_IDLE_STMT:
			//Just print a nop
			fprintf(fl, "nop\n");
			break;

		//
		//
		//
		//
		//
		//
		//
		//
		//TODO COMPLETE REWRITE NEEDED
		//
		//
		//
		//
		//
		//
		//
		//
		case THREE_ADDR_CODE_LEA_STMT:
			//Var name comes first
			print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);

			//Print the assignment operator
			fprintf(fl, " <- ");

			//Now print out the rest in order
			print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);

			//If we have a constant, we'll print that. Otherwise, print op2
			if(stmt->op1_const != NULL){
				//Then we have a plus
				fprintf(fl, " + ");

				//Print the constant out
				print_three_addr_constant(fl, stmt->op1_const);
			}

			//If we have an op2, we must print that as well
			if(stmt->op2 != NULL){
				//Then we have a plus
				fprintf(fl, " + ");

				//Then we have the third one, times some multiplier
				print_variable(fl, stmt->op2, PRINTING_VAR_INLINE);

				//If we have a multiplicator, then we can print it
				if(stmt->has_multiplicator == TRUE){
					//And the finishing sequence
					fprintf(fl, " * %ld", stmt->lea_multiplicator);
				}
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
			fprintf(fl, " * %ld\n", stmt->lea_multiplicator);
			break;

		case THREE_ADDR_CODE_INDIRECT_JUMP_STMT:
			//Indirection
			fprintf(fl, "jmp *");

			//Now the variable
			print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
			fprintf(fl, "\n");
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
		case FLOAT_CONST:
			fprintf(fl, "$%f", constant->constant_value.float_constant);
			break;
		case DOUBLE_CONST:
			fprintf(fl, "$%f", constant->constant_value.double_constant);
			break;
		case FUNC_CONST:
			fprintf(fl, "%s", constant->constant_value.function_name->func_name.string);
			break;
		//String constants are a special case because they are represented by
		//local constants, not immediate values
		case STR_CONST:
			fprintf(fl, ".LC%d", constant->local_constant->local_constant_id);
			break;
		//To avoid compiler complaints
		default:
			break;
	}
}


/**
 * Print a constant as an immediate(not $ prefixed) value
 */
static void print_immediate_value_no_prefix(FILE* fl, three_addr_const_t* constant){
	switch(constant->const_type){
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
		case FLOAT_CONST:
			fprintf(fl, "%f", constant->constant_value.float_constant);
			break;
		case DOUBLE_CONST:
			fprintf(fl, "%f", constant->constant_value.double_constant);
			break;
		case FUNC_CONST:
			fprintf(fl, "%s", constant->constant_value.function_name->func_name.string);
			break;
		//String constants are a special case because they are represented by
		//local constants, not immediate values
		case STR_CONST:
			fprintf(fl, ".LC%d", constant->local_constant->local_constant_id);
			break;

		//To avoid compiler complaints
		default:
			break;
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
		case ADDRESS_CALCULATION_MODE_GLOBAL_VAR:
			//Print the actual string name of the variable - no SSA and no registers
			fprintf(fl, "%s", instruction->op2->linked_var->var_name.string);
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
			fprintf(fl, "%ld", instruction->lea_multiplicator);
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
			fprintf(fl, ", %ld)", instruction->lea_multiplicator);
			
		//Do nothing
		default:
			break;
	}
}


/**
 * Handle a simple register to register or immediate to register move
 */
static void print_register_to_register_move(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	//What we need to print out here
	switch(instruction->instruction_type){
		case MOVQ:
			fprintf(fl, "movq ");
			break;
		case MOVL:
			fprintf(fl, "movl ");
			break;
		case MOVW:
			fprintf(fl, "movw ");
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
		//We should never hit this
		default:
			printf("Fatal internal compiler error: unreachable path hit\n");
			exit(1);
	}

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
static void print_register_to_memory_move(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	//What we need to print out here
	switch(instruction->instruction_type){
		case MOVQ:
			fprintf(fl, "movq ");
			break;
		case MOVL:
			fprintf(fl, "movl ");
			break;
		case MOVW:
			fprintf(fl, "movw ");
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
 * Handle a complex memory to register move with a complex address offset calculation
 */
static void print_memory_to_register_move(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	//What we need to print out here
	switch(instruction->instruction_type){
		case MOVQ:
			fprintf(fl, "movq ");
			break;
		case MOVL:
			fprintf(fl, "movl ");
			break;
		case MOVW:
			fprintf(fl, "movw ");
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
 * Print a cmp instruction. These instructions can have two registers or
 * one register and one immediate value
 */
static void print_cmp_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
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
static void print_xor_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
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
		case ASM_INLINE:
			fprintf(fl, "%s", instruction->inlined_assembly.string);
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

		case PUSH_DIRECT:
			fprintf(fl, "push ");
			//We have to print a register here, there's no choice
			print_64_bit_register_name(fl, instruction->push_or_pop_reg);
			fprintf(fl, "\n");
			break;

		case POP:
			fprintf(fl, "pop ");
			print_variable(fl, instruction->source_register, mode);
			fprintf(fl, "\n");
			break;

		case POP_DIRECT:
			fprintf(fl, "pop ");
			//We have to print a register here, there's no choice
			print_64_bit_register_name(fl, instruction->push_or_pop_reg);
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
			/**
			 * Now we go based on what kind of memory
			 * access we're doing here. This will determine
			 * the final output of our move
			 */
			switch(instruction->memory_access_type){
				case NO_MEMORY_ACCESS:
					print_register_to_register_move(fl, instruction, mode);
					break;

				case WRITE_TO_MEMORY:
					print_register_to_memory_move(fl, instruction, mode);
					break;

				case READ_FROM_MEMORY:
					print_memory_to_register_move(fl, instruction, mode);
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
			print_cmp_instruction(fl, instruction, mode);
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
			fprintf(fl, ",%ld)\n", instruction->lea_multiplicator);

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
	//What function are we in
	dec_stmt->function = current_function;
	//And give it back
	return dec_stmt;
}


/**
 * Emit a test instruction
 *
 * Test instructions inherently have no assignee as they don't modify registers
 */
instruction_t* emit_test_statement(three_addr_var_t* assignee, three_addr_var_t* op1, three_addr_var_t* op2){
	//First we'll allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//We'll now set the type
	stmt->statement_type = THREE_ADDR_CODE_TEST_STMT;

	//Assign the assignee and op1
	stmt->assignee = assignee;
	stmt->op1 = op1;
	stmt->op2 = op2;

	op1->use_count++;
	op2->use_count++;

	//Assign the function too
	stmt->function = current_function;

	//And now we'll give it back
	return stmt;
}


/**
 * Emit a test instruction directly - bypassing the instruction selection step
 *
 * Test instructions inherently have no assignee as they don't modify registers
 *
 * NOTE: This may only be used DURING the process of register selection
 */
instruction_t* emit_direct_test_instruction(three_addr_var_t* op1, three_addr_var_t* op2){
	//First we'll allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//We'll need the size to select the appropriate instruction
	variable_size_t size = get_type_size(op1->type);

	//Select the size appropriately
	switch(size){
		case QUAD_WORD:
			instruction->instruction_type = TESTQ;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = TESTL;
			break;
		case WORD:
			instruction->instruction_type = TESTW;
			break;
		case BYTE:
			instruction->instruction_type = TESTB;
			break;
		default:
			break;
	}

	//Then we'll set op1 and op2 to be the source registers
	instruction->source_register = op1;
	instruction->source_register2 = op2;

	//And now we'll give it back
	return instruction;
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
	//What function are we in
	inc_stmt->function = current_function;
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
		case INT_CONST:
			constant->constant_value.signed_integer_constant = const_node->constant_value.signed_int_value;
			break;
		case INT_CONST_FORCE_U:
			constant->constant_value.unsigned_integer_constant = const_node->constant_value.unsigned_int_value;
			break;
		case FLOAT_CONST:
			constant->constant_value.float_constant = const_node->constant_value.float_value;
			break;
		case DOUBLE_CONST:
			constant->constant_value.double_constant = const_node->constant_value.double_value;
			break;
		case STR_CONST:
			fprintf(stderr, "String constants may not be emitted directly\n");
			exit(0);
		case LONG_CONST:
			constant->constant_value.signed_long_constant = const_node->constant_value.signed_long_value;
			break;
		case LONG_CONST_FORCE_U:
			constant->constant_value.unsigned_long_constant = const_node->constant_value.unsigned_long_value;
			break;

			
		//If we have a function constant, we'll add the function record in
		//as a value
		case FUNC_CONST:
			//Store the function name
			constant->constant_value.function_name = const_node->func_record;
			break;

		//Some very weird error here
		default:
			fprintf(stderr, "Unrecognizable constant type found in constant\n");
			exit(0);
	}
	
	//Once all that is done, we can leave
	return constant;
}


/**
 * Emit a three_addr_const_t value that is a local constant(.LCx) reference
 */
three_addr_const_t* emit_string_constant(symtab_function_record_t* function, generic_ast_node_t* const_node){
	//Let's create the local constant first
	local_constant_t* local_constant = local_constant_alloc(&(const_node->string_value));

	//Once this has been made, we can add it to the function
	add_local_constant_to_function(function, local_constant);

	//Let's allocate it first
	three_addr_const_t* constant = calloc(1, sizeof(three_addr_const_t));

	//Add into here for memory management
	dynamic_array_add(&emitted_consts, constant);

	//Now we'll assign the appropriate values
	constant->const_type = const_node->constant_type; 
	constant->type = const_node->inferred_type;

	//Increment the reference count
	(local_constant->reference_count)++;

	//Add this value in
	constant->local_constant = local_constant;

	//And give this back
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
	//What function are we in
	stmt->function = current_function;
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
	//What function are we in
	stmt->function = current_function;

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
	//What function are we in
	stmt->function = current_function;
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
	//What function are we in
	stmt->function = current_function;
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
	//Record the function that we're in
	stmt->function = current_function;
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
	stmt->offset = emit_direct_integer_or_char_constant(offset, lookup_type_name_only(symtab, "u64", NOT_MUTABLE)->type);

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
	stmt->offset = emit_direct_integer_or_char_constant(offset, lookup_type_name_only(symtab, "u64", NOT_MUTABLE)->type);

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
	//What function are we in
	stmt->function = current_function;
	//And that's it, we'll now just give it back
	return stmt;
}


/**
 * Emit a store statement. This is like an assignment instruction, but we're explicitly
 * using stack memory here
 */
instruction_t* emit_store_ir_code(three_addr_var_t* assignee, three_addr_var_t* op1){
	//First allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Let's now populate it with values
	stmt->statement_type = THREE_ADDR_CODE_STORE_STATEMENT;
	stmt->assignee = assignee;

	//This is being dereferenced
	stmt->assignee->is_dereferenced = TRUE;

	stmt->op1 = op1;
	//What function are we in
	stmt->function = current_function;
	//And that's it, we'll now just give it back
	return stmt;
}


/**
 * Emit a store with offset ir code. We take in a base address(assignee), 
 * a variable offset(op1), and the value we're storing(op2)
 */
instruction_t* emit_store_with_variable_offset_ir_code(three_addr_var_t* base_address, three_addr_var_t* offset, three_addr_var_t* storee){
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

	//Save our current function
	stmt->function = current_function;

	//And give it back
	return stmt;
}


/**
 * Emit a store with offset ir code. We take in a base address(assignee), 
 * a constant offset(offset), and the value we're storing(op2)
 */
instruction_t* emit_store_with_constant_offset_ir_code(three_addr_var_t* base_address, three_addr_const_t* offset, three_addr_var_t* storee){
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

	//Save our current function
	stmt->function = current_function;

	//And give it back
	return stmt;
}


/**
 * Emit a load statement. This is like an assignment instruction, but we're explicitly
 * using stack memory here
 */
instruction_t* emit_load_ir_code(three_addr_var_t* assignee, three_addr_var_t* op1){
	//First allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Let's now populate it with values
	stmt->statement_type = THREE_ADDR_CODE_LOAD_STATEMENT;
	stmt->assignee = assignee;
	stmt->op1 = op1;
	//What function are we in
	stmt->function = current_function;
	//And that's it, we'll now just give it back
	return stmt;
}


/**
 * Emit a load with offset ir code. We take in a base address(op1), 
 * an offset(op2), and the value we're loading into(assignee)
 */
instruction_t* emit_load_with_variable_offset_ir_code(three_addr_var_t* assignee, three_addr_var_t* base_address, three_addr_var_t* offset){
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

	//Save our current function
	stmt->function = current_function;

	//And give it back
	return stmt;
}


/**
 * Emit a load with constant offset ir code. We take in a base address(op1), 
 * an offset(offset), and the value we're loading into(assignee)
 */
instruction_t* emit_load_with_constant_offset_ir_code(three_addr_var_t* assignee, three_addr_var_t* base_address, three_addr_const_t* offset){
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

	//Save our current function
	stmt->function = current_function;

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
	//What function are we in
	stmt->function = current_function;
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

	//What function are we in
	stmt->function = current_function;

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
	//What function we're in
	stmt->function = current_function;
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
	//What function are we in
	stmt->function = current_function;

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
	//Mark what function we're in
	stmt->function = current_function;

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
		case I16:
		case I8:
			constant->const_type = INT_CONST;
			constant->constant_value.signed_integer_constant = value;
			break;
		case U32:
		case U16:
		case U8:
			constant->const_type = INT_CONST_FORCE_U;
			constant->constant_value.unsigned_integer_constant = value;
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
instruction_t* emit_neg_instruction(three_addr_var_t* assignee, three_addr_var_t* negatee){
	//First we'll create the negation
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Now we'll assign whatever we need
	stmt->statement_type = THREE_ADDR_CODE_NEG_STATEMENT;
	stmt->assignee = assignee;
	stmt->op1 = negatee;
	//What function are we in
	stmt->function = current_function;

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
	//What function are we in
	stmt->function = current_function;

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
	//What function are we in
	stmt->function = current_function;

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

	//What function are we in
	stmt->function = current_function;

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
	//What function are we in
	stmt->function = current_function;

	//And give the statement back
	return stmt;
}


/**
 * Emit a stack allocation statement
 */
instruction_t* emit_stack_allocation_statement(three_addr_var_t* stack_pointer, type_symtab_t* type_symtab, u_int64_t offset){
	//Allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//This is always a subq statement
	stmt->instruction_type = SUBQ;

	//Store the destination as the stack pointer
	stmt->destination_register = stack_pointer;

	//Emit this directly
	stmt->source_immediate = emit_direct_integer_or_char_constant(offset, lookup_type_name_only(type_symtab, "u64", NOT_MUTABLE)->type);

	//Just give this back
	return stmt;
}


/**
 * Emit a stack deallocation statement
 */
instruction_t* emit_stack_deallocation_statement(three_addr_var_t* stack_pointer, type_symtab_t* type_symtab, u_int64_t offset){
	//Allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//This is always an addq statement
	stmt->instruction_type = ADDQ;

	//Destination is always the stack pointer
	stmt->destination_register = stack_pointer;

	//Emit this directly
	stmt->source_immediate = emit_direct_integer_or_char_constant(offset, lookup_type_name_only(type_symtab, "u64", NOT_MUTABLE)->type);

	//Just give this back
	return stmt;
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
 * Multiply two constants together
 * 
 * NOTE: The result is always stored in the first one
 */
void multiply_constants(three_addr_const_t* constant1, three_addr_const_t* constant2){
	//Go based on the first one's type
	switch(constant1->const_type){
		case INT_CONST_FORCE_U:
			//Now go based on the second one's type
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.unsigned_long_constant *= constant2->constant_value.unsigned_long_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.unsigned_long_constant *= constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.unsigned_integer_constant *= constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.unsigned_integer_constant *= constant2->constant_value.signed_integer_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.unsigned_integer_constant *= constant2->constant_value.char_constant;
					break;
				//This should never happen
				default:
					printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
					exit(1);
			}

			break;

		case INT_CONST:
			//Now go based on the second one's type
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.unsigned_long_constant *= constant2->constant_value.unsigned_long_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.unsigned_integer_constant *= constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.signed_integer_constant *= constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.signed_integer_constant *= constant2->constant_value.signed_integer_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.signed_integer_constant *= constant2->constant_value.char_constant;
					break;
				//This should never happen
				default:
					printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
					exit(1);
			}

			break;

		case LONG_CONST_FORCE_U:
			//Now go based on the second one's type
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
				case CHAR_CONST:
					constant1->constant_value.unsigned_long_constant *= constant2->constant_value.char_constant;
					break;
				//This should never happen
				default:
					printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
					exit(1);
			}
			
			break;

		case LONG_CONST:
			//Now go based on the second one's type
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
				case CHAR_CONST:
					constant1->constant_value.signed_long_constant *= constant2->constant_value.char_constant;
					break;
				//This should never happen
				default:
					printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
					exit(1);
			}

			break;

		case CHAR_CONST:
			//Now go based on the second one's type
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
				case CHAR_CONST:
					constant1->constant_value.char_constant *= constant2->constant_value.char_constant;
					break;
				//This should never happen
				default:
					printf("Fatal internal compiler error: Unsupported constant multiplication operation\n");
					exit(1);
			}

			break;

		//This should never happen
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
	//Go based on the first one's type
	switch(constant1->const_type){
		case INT_CONST_FORCE_U:
			//Now go based on the second one's type
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.unsigned_long_constant += constant2->constant_value.unsigned_long_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.unsigned_long_constant += constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.unsigned_integer_constant += constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.unsigned_integer_constant += constant2->constant_value.signed_integer_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.unsigned_integer_constant += constant2->constant_value.char_constant;
					break;
				//This should never happen
				default:
					printf("Fatal internal compiler error: Unsupported constant addition operation\n");
					exit(1);
			}

			break;

		case INT_CONST:
			//Now go based on the second one's type
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.unsigned_long_constant += constant2->constant_value.unsigned_long_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.unsigned_integer_constant += constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.signed_integer_constant += constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.signed_integer_constant += constant2->constant_value.signed_integer_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.signed_integer_constant += constant2->constant_value.char_constant;
					break;
				//This should never happen
				default:
					printf("Fatal internal compiler error: Unsupported constant addition operation\n");
					exit(1);
			}

			break;

		case LONG_CONST_FORCE_U:
			//Now go based on the second one's type
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
				case CHAR_CONST:
					constant1->constant_value.unsigned_long_constant += constant2->constant_value.char_constant;
					break;
				//This should never happen
				default:
					printf("Fatal internal compiler error: Unsupported constant addition operation\n");
					exit(1);
			}
			
			break;

		case LONG_CONST:
			//Now go based on the second one's type
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
				case CHAR_CONST:
					constant1->constant_value.signed_long_constant += constant2->constant_value.char_constant;
					break;
				//This should never happen
				default:
					printf("Fatal internal compiler error: Unsupported constant addition operation\n");
					exit(1);
			}

			break;

		case CHAR_CONST:
			//Now go based on the second one's type
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
				case CHAR_CONST:
					constant1->constant_value.char_constant += constant2->constant_value.char_constant;
					break;
				//This should never happen
				default:
					printf("Fatal internal compiler error: Unsupported constant addition operation\n");
					exit(1);
			}

			break;

		//This should never happen
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
	//Go based on the first one's type
	switch(constant1->const_type){
		case INT_CONST_FORCE_U:
			//Now go based on the second one's type
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.unsigned_long_constant -= constant2->constant_value.unsigned_long_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.unsigned_long_constant -= constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.unsigned_integer_constant -= constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.unsigned_integer_constant -= constant2->constant_value.signed_integer_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.unsigned_integer_constant -= constant2->constant_value.char_constant;
					break;
				//This should never happen
				default:
					printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
					exit(1);
			}

			break;

		case INT_CONST:
			//Now go based on the second one's type
			switch(constant2->const_type){
				case LONG_CONST_FORCE_U:
					constant1->constant_value.unsigned_long_constant -= constant2->constant_value.unsigned_long_constant;
					break;
				case LONG_CONST:
					constant1->constant_value.unsigned_integer_constant -= constant2->constant_value.signed_long_constant;
					break;
				case INT_CONST_FORCE_U:
					constant1->constant_value.signed_integer_constant -= constant2->constant_value.unsigned_integer_constant;
					break;
				case INT_CONST:
					constant1->constant_value.signed_integer_constant -= constant2->constant_value.signed_integer_constant;
					break;
				case CHAR_CONST:
					constant1->constant_value.signed_integer_constant -= constant2->constant_value.char_constant;
					break;
				//This should never happen
				default:
					printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
					exit(1);
			}

			break;

		case LONG_CONST_FORCE_U:
			//Now go based on the second one's type
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
				case CHAR_CONST:
					constant1->constant_value.unsigned_long_constant -= constant2->constant_value.char_constant;
					break;
				//This should never happen
				default:
					printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
					exit(1);
			}
			
			break;

		case LONG_CONST:
			//Now go based on the second one's type
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
				case CHAR_CONST:
					constant1->constant_value.signed_long_constant -= constant2->constant_value.char_constant;
					break;
				//This should never happen
				default:
					printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
					exit(1);
			}

			break;

		case CHAR_CONST:
			//Now go based on the second one's type
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
				case CHAR_CONST:
					constant1->constant_value.char_constant -= constant2->constant_value.char_constant;
					break;
				//This should never happen
				default:
					printf("Fatal internal compiler error: Unsupported constant subtraction operation\n");
					exit(1);
			}

			break;

		//This should never happen
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
 * Select the appropriate set type given the circumstances, including the operand and the signedness
 */
instruction_type_t select_appropriate_set_stmt(ollie_token_t op, u_int8_t is_signed){
	if(is_signed == TRUE){
		switch(op){
			case G_THAN:
				return SETG;
			case L_THAN:
				return SETL;
			case G_THAN_OR_EQ:
				return SETGE;
			case L_THAN_OR_EQ:
				return SETLE;
			case NOT_EQUALS:
				return SETNE;
			case EQUALS:
				return SETE;
			default:
				return SETE;
		}
	} else {
		switch(op){
			case G_THAN:
				return SETA;
			case L_THAN:
				return SETB;
			case G_THAN_OR_EQ:
				return SETAE;
			case L_THAN_OR_EQ:
				return SETBE;
			case NOT_EQUALS:
				return SETNE;
			case EQUALS:
				return SETE;
			default:
				return SETE;
		}
	}
}

/**
 * Is the given register caller saved?
 */
u_int8_t is_register_caller_saved(general_purpose_register_t reg){
	switch(reg){
		case RAX:
		case RDI:
		case RSI:
		case RDX:
		case RCX:
		case R8:
		case R9:
		case R10:
		case R11:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Is the given register callee saved?
 */
u_int8_t is_register_callee_saved(general_purpose_register_t reg){
	//This is all determined based on the register type
	switch(reg){
		case RBX:
		case RBP:
		case R12:
		case R13:
		case R14:
		case R15:
			return TRUE;
		default:
			return FALSE;
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
