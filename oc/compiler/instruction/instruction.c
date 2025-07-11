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

//For standardization and convenience
#define TRUE 1
#define FALSE 0

//The atomically increasing temp name id
static int32_t current_temp_id = 0;
//The current function
static symtab_function_record_t* current_function = NULL;

//All created vars
three_addr_var_t* emitted_vars = NULL;
//All created constants
three_addr_const_t* emitted_consts = NULL;

/**
 * A helper function for our atomically increasing temp id
 */
static int32_t increment_and_get_temp_id(){
	current_temp_id++;
	return current_temp_id;
}


/**
 * Insert an instruction in a block before the given instruction
 */
void insert_instruction_before_given(instruction_t* insertee, instruction_t* given){
	//Let's first grab out which block we've got
	basic_block_t* block = given->block_contained_in;
	//Mark this while we're here
	insertee->block_contained_in = block;

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
 * Determine the signedness of a jump type
 */
u_int8_t is_jump_type_signed(jump_type_t type){
	switch(type){
		case JUMP_TYPE_JG:
		case JUMP_TYPE_JGE:
		case JUMP_TYPE_JLE:
		case JUMP_TYPE_JL:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * A simple helper function to determine if an operator is a comparison operator
 */
u_int8_t is_operator_relational_operator(Token op){
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
 * Helper function to determine if an instruction is a binary operation
 */
u_int8_t is_instruction_binary_operation(instruction_t* instruction){
	//Speedup with NULL processing
	if(instruction == NULL){
		return FALSE;
	}

	//Switch based on class
	switch(instruction->CLASS){
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
	switch(instruction->CLASS){
		case THREE_ADDR_CODE_ASSN_STMT:
		case THREE_ADDR_CODE_ASSN_CONST_STMT:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Select the size of a constant based on its type
 */
variable_size_t select_constant_size(three_addr_const_t* constant){
	//Switch on the const type
	switch(constant->const_type){
		case INT_CONST:
		case INT_CONST_FORCE_U:
			return DOUBLE_WORD;

		case FLOAT_CONST:
			return DOUBLE_PRECISION;

		case LONG_CONST:
		case LONG_CONST_FORCE_U:
			return QUAD_WORD;

		case CHAR:
			return BYTE;

		//Everything is dword by default
		default:
			return DOUBLE_WORD;
	}
}


/**
 * Select the size based only on a type
 */
variable_size_t select_type_size(generic_type_t* type){
	//What the size will be
	variable_size_t size;

	//Probably the most common option
	if(type->type_class == TYPE_CLASS_BASIC){
		//Extract for convenience
		Token basic_type = type->basic_type->basic_type;

		//Switch based on this
		switch (basic_type) {
			case U_INT8:
			case S_INT8:
			case CHAR:
				size = BYTE;
				break;

			case U_INT16:
			case S_INT16:
				size = WORD;
				break;

			//These are 32 bit(double word)
			case S_INT32:
			case U_INT32:
				size = DOUBLE_WORD;
				break;

			//This is SP
			case FLOAT32:
				size = SINGLE_PRECISION;
				break;

			//This is double precision
			case FLOAT64:
				size = DOUBLE_PRECISION;
				break;

			//These are all quad word(64 bit)
			case U_INT64:
			case S_INT64:
				size = QUAD_WORD;
				break;
		
			//We shouldn't get here
			default:
				break;
		}

	//These will always be 64 bits
	} else if(type->type_class == TYPE_CLASS_POINTER || type->type_class == TYPE_CLASS_ARRAY
				|| type->type_class == TYPE_CLASS_CONSTRUCT){
		size = QUAD_WORD;

	//This should never happen, but a sane default doesn't hurt
	} else if(type->type_class == TYPE_CLASS_ALIAS){
		size = QUAD_WORD;
	//Catch all down here
	} else {
		size = DOUBLE_WORD;
	}

	//Give it back
	return size;
}


/**
 * Select the size of a given variable based on its type
 */
variable_size_t select_variable_size(three_addr_var_t* variable){
	//What the size will be
	variable_size_t size;
	
	//Grab this type out of here
	generic_type_t* type = variable->type;
	
	//Probably the most common option
	if(type->type_class == TYPE_CLASS_BASIC){
		//Extract for convenience
		Token basic_type = type->basic_type->basic_type;

		//Switch based on this
		switch (basic_type) {
			case U_INT8:
			case S_INT8:
			case CHAR:
				size = BYTE;
				break;

			case U_INT16:
			case S_INT16:
				size = WORD;
				break;

			//These are 32 bit(double word)
			case S_INT32:
			case U_INT32:
				size = DOUBLE_WORD;
				break;

			//This is SP
			case FLOAT32:
				size = SINGLE_PRECISION;
				break;

			//This is double precision
			case FLOAT64:
				size = DOUBLE_PRECISION;
				break;

			//These are all quad word(64 bit)
			case U_INT64:
			case S_INT64:
				size = QUAD_WORD;
				break;
		
			//We shouldn't get here
			default:
				break;
		}

	//These will always be 64 bits
	} else if(type->type_class == TYPE_CLASS_POINTER || type->type_class == TYPE_CLASS_ARRAY
				|| type->type_class == TYPE_CLASS_CONSTRUCT){
		size = QUAD_WORD;

	//This should never happen, but a sane default doesn't hurt
	} else if(type->type_class == TYPE_CLASS_ALIAS){
		size = QUAD_WORD;
	//Catch all down here
	} else {
		size = DOUBLE_WORD;
	}

	//It wouldn't hurt to store this
	variable->variable_size = size;

	//Give it back
	return size;
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
 * Is this a division instruction?
 */
u_int8_t is_division_instruction(instruction_t* instruction){
	//Just in case
	if(instruction == NULL){
		return FALSE;
	}

	switch(instruction->instruction_type){
		case DIVQ:
		case DIVL:
		case IDIVQ:
		case IDIVL:
			return TRUE;
		default:
			return FALSE;
	}
}

/**
 * Is this a division instruction that's intended for modulus??
 */
u_int8_t is_modulus_instruction(instruction_t* instruction){
	//Just in case
	if(instruction == NULL){
		return FALSE;
	}

	switch(instruction->instruction_type){
		case DIVL_FOR_MOD:
		case DIVQ_FOR_MOD:
		case IDIVL_FOR_MOD:
		case IDIVQ_FOR_MOD:
			return TRUE;
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
		//These are our three candidates
		case MOVL:
		case MOVW:
		case MOVQ:
			//If we have an immediate value OR we 
			//have some indirection, we say false
			if(instruction->source_register == NULL
				|| instruction->indirection_level > 0){
				return FALSE;
			}

			//Otherwise it is a pure copy
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

	//Attach this for memory management
	var->next_created = emitted_vars;
	emitted_vars = var;

	//Mark this as temporary
	var->is_temporary = TRUE;
	//Store the type info
	var->type = type;
	//Store the temp var number
	var->temp_var_number = increment_and_get_temp_id();

	//Select the size of this variable
	var->variable_size = select_variable_size(var);

	//Finally we'll bail out
	return var;
}


/**
 * Dynamically allocate and create a non-temp var. We emit a separate, distinct variable for 
 * each SSA generation. For instance, if we emit x1 and x2, they are distinct. The only thing 
 * that they share is the overall variable that they're linked back to, which stores their type information,
 * etc.
*/
three_addr_var_t* emit_var(symtab_variable_record_t* var, u_int8_t is_label){
	//Let's first create the non-temp variable
	three_addr_var_t* emitted_var = calloc(1, sizeof(three_addr_var_t));

	//Attach it for memory management
	emitted_var->next_created = emitted_vars;
	emitted_vars = emitted_var;

	//This is not temporary
	emitted_var->is_temporary = FALSE;
	//We always store the type as the type with which this variable was defined in the CFG
	emitted_var->type = var->type_defined_as;
	//And store the symtab record
	emitted_var->linked_var = var;

	//Select the size of this variable
	emitted_var->variable_size = select_variable_size(emitted_var);

	//And we're all done
	return emitted_var;
}


/**
 * Create and return a temporary variable from a live range
*/
three_addr_var_t* emit_temp_var_from_live_range(live_range_t* range){
	//Let's first create the non-temp variable
	three_addr_var_t* emitted_var = calloc(1, sizeof(three_addr_var_t));

	//Attach it for memory management
	emitted_var->next_created = emitted_vars;
	emitted_vars = emitted_var;

	//This is temporary
	emitted_var->is_temporary = TRUE;

	//Link this in with our live range
	emitted_var->associated_live_range = range;
	dynamic_array_add(range->variables, emitted_var);

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

	//Copy the memory
	memcpy(emitted_var, var, sizeof(three_addr_var_t));
	
	//Attach it for memory management
	emitted_var->next_created = emitted_vars;
	emitted_vars = emitted_var;

	//Transfer this status over
	emitted_var->is_temporary = var->is_temporary;

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
 * Emit a movzx(zero extend) instruction
 */
instruction_t* emit_movzx_instruction(three_addr_var_t* source, three_addr_var_t* destination){
	//First we allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//Set the instruction type
	instruction->instruction_type = MOVZX;

	//Set the source and destination
	instruction->source_register = source;
	instruction->destination_register = destination;

	//And following that, we're all set
	return instruction;
}


/**
 * Emit a movsx(sign extend) instruction
 */
instruction_t* emit_movsx_instruction(three_addr_var_t* source, three_addr_var_t* destination){
	//First we allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//Set the instruction type
	instruction->instruction_type = MOVSX;

	//Set the source and destination
	instruction->source_register = source;
	instruction->destination_register = destination;

	//And following that, we're all set
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
 * Emit a movX instruction
 *
 * This is used for when we need extra moves(after a division/modulus)
 */
instruction_t* emit_movX_instruction(three_addr_var_t* destination, three_addr_var_t* source){
	//First we'll allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//We set the size based on the destination 
	variable_size_t size = select_variable_size(destination);

	switch (size) {
		case BYTE:
			instruction->instruction_type = MOVB;
			break;
		case WORD:
			instruction->instruction_type = MOVW;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = MOVL;
			break;
		case QUAD_WORD:
			instruction->instruction_type = MOVQ;
			break;
		//Should never reach this
		default:
			break;
	}

	//Finally we set the destination
	instruction->destination_register = destination;
	instruction->source_register = source;

	//And now we'll give it back
	return instruction;
}


/**
 * Emit a statement that is in LEA form
 */
instruction_t* emit_lea_instruction(three_addr_var_t* assignee, three_addr_var_t* op1, three_addr_var_t* op2, u_int64_t type_size){
	//First we allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Now we'll make our populations
	stmt->CLASS = THREE_ADDR_CODE_LEA_STMT;
	stmt->assignee = assignee;
	stmt->op1 = op1;
	stmt->op2 = op2;
	stmt->lea_multiplicator = type_size;
	//What function are we in
	stmt->function = current_function;

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
	stmt->CLASS = THREE_ADDR_CODE_INDIR_JUMP_ADDR_CALC_STMT;
	stmt->assignee = assignee;
	//We store the jumping to block as our operand. It's really a jump table
	stmt->jumping_to_block = op1;
	stmt->op2 = op2;
	stmt->lea_multiplicator = type_size;

	//And now we'll give it back
	return stmt;
}


/**
 * Emit a copy of this statement
 */
instruction_t* emit_label_instruction(three_addr_var_t* label){
	//Let's first allocate the statement
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//All we do now is give this the label 
	stmt->assignee = label;
	//Note the class too
	stmt->CLASS = THREE_ADDR_CODE_LABEL_STMT;
	//What function are we in
	stmt->function = current_function;
	//And give it back
	return stmt;
}


/**
 * Emit a direct jump statement. This is used only with jump statements the user has made
 */
instruction_t* emit_direct_jmp_instruction(three_addr_var_t* jumping_to){
	//First allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Now all we need to do is give it the label
	stmt->assignee = jumping_to;
	//Note the class too
	stmt->CLASS = THREE_ADDR_CODE_DIR_JUMP_STMT;
	//What function are we in
	stmt->function = current_function;
	//and give it back
	return stmt;
}


/**
 * Directly emit an idle statement
 */
instruction_t* emit_idle_instruction(){
	//First we allocate
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Store the class
	stmt->CLASS = THREE_ADDR_CODE_IDLE_STMT;
	//What function are we in
	stmt->function = current_function;
	//And we're done
	return stmt;
}


/**
 * Emit a setX instruction
 */
instruction_t* emit_setX_instruction(Token op, three_addr_var_t* destination_register, u_int8_t is_signed){
	//First allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//We'll need to give it the assignee
	stmt->destination_register = destination_register;

	//We'll determine the actual instruction type using the helper
	stmt->instruction_type = select_appropriate_set_stmt(op, is_signed);

	//Once that's done, we'll return
	return stmt;
}


/**
 * Print an 8-bit register out. The names used for these are still
 * 64 bits because 8, 16, 32 and 64 bit uses can't occupy the same register at the 
 * same time
 */
static void print_8_bit_register_name(FILE* fl, register_holder_t reg){
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
static void print_16_bit_register_name(FILE* fl, register_holder_t reg){
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
static void print_32_bit_register_name(FILE* fl, register_holder_t reg){
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
static void print_64_bit_register_name(FILE* fl, register_holder_t reg){
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
	//If we have a block header, we will NOT print out any indirection info
	//We will first print out any and all indirection("(") opening parens
	for(u_int16_t i = 0; mode == PRINTING_VAR_INLINE && i < variable->indirection_level; i++){
		fprintf(fl, "(");
	}
	
	//If we're printing live ranges, we'll use the LR number
	if(mode == PRINTING_LIVE_RANGES){
		fprintf(fl, "LR%d", variable->associated_live_range->live_range_id);
	} else if(mode == PRINTING_REGISTERS){
		if(variable->associated_live_range->reg == NO_REG){
			fprintf(fl, "LR%d", variable->associated_live_range->live_range_id);
		} else

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

	//Otherwise if it's a temp
	} else if(variable->is_temporary == TRUE){
		//Print out it's temp var number
		fprintf(fl, "t%d", variable->temp_var_number);
	} else {
		//Otherwise, print out the SSA generation along with the variable
		fprintf(fl, "%s_%d", variable->linked_var->var_name, variable->ssa_generation);
	}

	//Lastly we print out the remaining indirection characters
	for(u_int16_t i = 0; mode == PRINTING_VAR_INLINE && i < variable->indirection_level; i++){
		fprintf(fl, ")");
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
	//We'll now interpret what we have here
	if(constant->const_type == INT_CONST){
		fprintf(fl, "%d", constant->int_const);
	} else if(constant->const_type == LONG_CONST){
		fprintf(fl, "%ld", constant->long_const);
	} else if(constant->const_type == FLOAT_CONST){
		fprintf(fl, "%f", constant->float_const);
	} else if(constant->const_type == CHAR_CONST){
		fprintf(fl, "'%c'", constant->char_const);
	} else {
		fprintf(fl, "\"%s\"", constant->str_const);
	}
}


/**
 * Pretty print a three address code statement
 *
*/
void print_three_addr_code_stmt(FILE* fl, instruction_t* stmt){
	//If it's a binary operator statement(most common), we'll
	//print the whole thing
	if(stmt->CLASS == THREE_ADDR_CODE_BIN_OP_STMT){
		//What is our op?
		char* op = "";

		//Whatever we have here
		switch (stmt->op) {
			case PLUS:
				op = "+";
				break;
			case MINUS:
				op = "-";
				break;
			case STAR:
				op = "*";
				break;
			case F_SLASH:
				op = "/";
				break;
			case MOD:
				op = "%";
				break;
			case G_THAN:
				op = ">";
				break;
			case L_THAN:
				op = "<";
				break;
			case L_SHIFT:
				op = "<<";
				break;
			case R_SHIFT:
				op = ">>";
				break;
			case SINGLE_AND:
				op = "&";
				break;
			case SINGLE_OR:
				op = "|";
				break;
			case CARROT:
				op = "^";
				break;
			case DOUBLE_OR:
				op = "||";
				break;
			case DOUBLE_AND:
				op = "&&";
				break;
			case DOUBLE_EQUALS:
				op = "==";
				break;
			case NOT_EQUALS:
				op = "!=";
				break;
			case G_THAN_OR_EQ:
				op = ">=";
				break;
			case L_THAN_OR_EQ:
				op = "<=";
				break;
			default:
				fprintf(fl, "BAD OP");
				exit(1);
		}

		//This one comes first
		print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);

		//Then the arrow
		fprintf(fl, " <- ");

		//Now we'll do op1, token, op2
		print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
		fprintf(fl, " %s ", op);
		print_variable(fl, stmt->op2, PRINTING_VAR_INLINE);

		//And end it out here
		fprintf(fl, "\n");

	//If we have a bin op with const
	} else if(stmt->CLASS == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT){
		//What is our op?
		char* op = "";

		//Whatever we have here
		switch (stmt->op) {
			case PLUS:
				op = "+";
				break;
			case MINUS:
				op = "-";
				break;
			case STAR:
				op = "*";
				break;
			case F_SLASH:
				op = "/";
				break;
			case MOD:
				op = "%";
				break;
			case G_THAN:
				op = ">";
				break;
			case L_THAN:
				op = "<";
				break;
			case L_SHIFT:
				op = "<<";
				break;
			case R_SHIFT:
				op = ">>";
				break;
			case SINGLE_AND:
				op = "&";
				break;
			case SINGLE_OR:
				op = "|";
				break;
			case CARROT:
				op = "^";
				break;
			case DOUBLE_OR:
				op = "||";
				break;
			case DOUBLE_AND:
				op = "&&";
				break;
			case DOUBLE_EQUALS:
				op = "==";
				break;
			case NOT_EQUALS:
				op = "!=";
				break;
			case G_THAN_OR_EQ:
				op = ">=";
				break;
			case L_THAN_OR_EQ:
				op = "<=";
				break;
			default:
				fprintf(fl, "BAD OP");
				exit(1);
		}

		//This one comes first
		print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);

		//Then the arrow
		fprintf(fl, " <- ");

		//Now we'll do op1, token, op2
		print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
		fprintf(fl, " %s ", op);

		//Print the constant out
		print_three_addr_constant(fl, stmt->op1_const);

		//We need a newline here
		fprintf(fl, "\n");
	
	//If we have a regular const assignment
	} else if(stmt->CLASS == THREE_ADDR_CODE_ASSN_STMT){
		//We'll print out the left and right ones here
		print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);
		fprintf(fl, " <- ");
		print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
		fprintf(fl, "\n");
	//Assigning a memory address to a variable
	} else if (stmt->CLASS == THREE_ADDR_CODE_MEM_ADDR_ASSIGNMENT){
		//We'll print out the left and right ones here
		print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);
		fprintf(fl, " <- Memory Address of ");
		print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
		fprintf(fl, "\n");
	} else if(stmt->CLASS == THREE_ADDR_CODE_ASSN_CONST_STMT){
		//First print out the assignee
		print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);
		fprintf(fl, " <- ");

		//Print the constant out
		print_three_addr_constant(fl, stmt->op1_const);
		//Newline needed
		fprintf(fl, "\n");

	//Print out a return statement
	} else if(stmt->CLASS == THREE_ADDR_CODE_RET_STMT){
		//Use asm keyword here, getting close to machine code
		fprintf(fl, "ret ");

		//If it has a returned variable
		if(stmt->op1 != NULL){
			print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
		}
		
		//No matter what, print a newline
		fprintf(fl, "\n");

	//Print out a jump statement
	} else if(stmt->CLASS == THREE_ADDR_CODE_JUMP_STMT){
		//Use asm keyword here, getting close to machine code
		switch(stmt->jump_type){
			case JUMP_TYPE_JE:
				fprintf(fl, "je");
				break;
			case JUMP_TYPE_JNE:
				fprintf(fl, "jne");
				break;
			case JUMP_TYPE_JG:
				fprintf(fl, "jg");
				break;
			case JUMP_TYPE_JL:
				fprintf(fl, "jl");
				break;
			case JUMP_TYPE_JNZ:
				fprintf(fl, "jnz");
				break;
			case JUMP_TYPE_JZ:
				fprintf(fl, "jz");
				break;
			case JUMP_TYPE_JMP:
				fprintf(fl, "jmp");
				break;
			case JUMP_TYPE_JGE:
				fprintf(fl, "jge");
				break;
			case JUMP_TYPE_JLE:
				fprintf(fl, "jle");
				break;
			case JUMP_TYPE_JAE:
				fprintf(fl, "jae");
				break;
			case JUMP_TYPE_JBE:
				fprintf(fl, "jbe");
				break;
			case JUMP_TYPE_JA:
				fprintf(fl, "ja");
				break;
			case JUMP_TYPE_JB:
				fprintf(fl, "jb");
				break;
			default:
				fprintf(fl, "jmp");
				break;
		}

		//Then print out the block label
		fprintf(fl, " .L%d\n", ((basic_block_t*)(stmt->jumping_to_block))->block_id);

	//If we have a function call go here
	} else if(stmt->CLASS == THREE_ADDR_CODE_FUNC_CALL){
		//First we'll print out the assignment, if one exists
		if(stmt->assignee != NULL){
			//Print the variable and assop out
			print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);
			fprintf(fl, " <- ");
		}

		//No matter what, we'll need to see the "call" keyword, followed
		//by the function name
		fprintf(fl, "call %s(", stmt->called_function->func_name);

		//Grab this out
		dynamic_array_t* func_params = stmt->function_parameters;

		//Now we can go through and print out all of our parameters here
		for(u_int16_t i = 0; func_params != NULL && i < func_params->current_index; i++){
			//Grab it out
			three_addr_var_t* func_param = dynamic_array_get_at(func_params, i);
			
			//Print this out here
			print_variable(fl, func_param, PRINTING_VAR_INLINE);

			//If we need to, print out a comma
			if(i != func_params->current_index - 1){
				fprintf(fl, ", ");
			}
		}

		//Now at the very end, close the whole thing out
		fprintf(fl, ")\n");

	//If we have a binary operator with a constant
	} else if (stmt->CLASS == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT){
		//TODO MAY OR MAY NOT NEED
	} else if (stmt->CLASS == THREE_ADDR_CODE_INC_STMT){
		fprintf(fl, "inc ");
		print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);
		fprintf(fl, "\n");
	} else if (stmt->CLASS == THREE_ADDR_CODE_DEC_STMT){
		fprintf(fl, "dec ");
		print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);
		fprintf(fl, "\n");
	} else if (stmt->CLASS == THREE_ADDR_CODE_BITWISE_NOT_STMT){
		print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);
		fprintf(fl, " <- not ");
		print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
		fprintf(fl, "\n");
	} else if(stmt->CLASS == THREE_ADDR_CODE_NEG_STATEMENT){
		print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);
		fprintf(fl, " <- neg ");
		print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
		fprintf(fl, "\n");
	} else if (stmt->CLASS == THREE_ADDR_CODE_LOGICAL_NOT_STMT){
		print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);
		//We will use a sequence of commands to do this
		fprintf(fl, " <- logical_not ");
		print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
		fprintf(fl, "\n");
	//For a label statement, we need to trim off the $ that it has
	} else if(stmt->CLASS == THREE_ADDR_CODE_LABEL_STMT){
		//Let's print it out. This is an instance where we will not use the print var
		fprintf(fl, "%s:\n", stmt->assignee->linked_var->var_name + 1);
	} else if(stmt->CLASS == THREE_ADDR_CODE_DIR_JUMP_STMT){
		//This is an instance where we will not use the print var
		fprintf(fl, "jmp %s\n", stmt->assignee->linked_var->var_name + 1);
	//Display an assembly inline statement
	} else if(stmt->CLASS == THREE_ADDR_CODE_ASM_INLINE_STMT){
		//Should already have a trailing newline
		fprintf(fl, "%s", stmt->inlined_assembly);
	} else if(stmt->CLASS == THREE_ADDR_CODE_IDLE_STMT){
		//Just print a nop
		fprintf(fl, "nop\n");
	//If we have a lea statement, we will print it out in plain algebraic form here
	} else if(stmt->CLASS == THREE_ADDR_CODE_LEA_STMT){
		//Var name comes first
		print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);

		//Print the assignment operator
		fprintf(fl, " <- ");

		//Now print out the rest in order
		print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
		//Then we have a plus
		fprintf(fl, " + ");

		//If we have a constant, we'll print that. Otherwise, print op2
		if(stmt->op1_const != NULL){
			//Print the constant out
			print_three_addr_constant(fl, stmt->op1_const);
			fprintf(fl, "\n");
		} else {
			//Then we have the third one, times some multiplier
			print_variable(fl, stmt->op2, PRINTING_VAR_INLINE);
			//And the finishing sequence
			fprintf(fl, " * %ld\n", stmt->lea_multiplicator);
		}
	//Print out a phi function 
	} else if(stmt->CLASS == THREE_ADDR_CODE_PHI_FUNC){
		//Print it in block header mode
		print_variable(fl, stmt->assignee, PRINTING_VAR_BLOCK_HEADER);
		fprintf(fl, " <- PHI(");

		//For convenience
		dynamic_array_t* phi_func_params = stmt->phi_function_parameters;

		//Now run through all of the parameters
		for(u_int16_t _ = 0; phi_func_params != NULL && _ < phi_func_params->current_index; _++){
			//Print out the variable
			print_variable(fl, dynamic_array_get_at(phi_func_params, _), PRINTING_VAR_BLOCK_HEADER);

			//If it isn't the very last one, add a comma space
			if(_ != phi_func_params->current_index - 1){
				fprintf(fl, ", ");
			}
		}

		fprintf(fl, ")\n");
	//Print out an indirect jump statement
	} else if(stmt->CLASS == THREE_ADDR_CODE_INDIR_JUMP_ADDR_CALC_STMT){
		print_variable(fl, stmt->assignee, PRINTING_VAR_INLINE);

		//Print out the jump block ID
		fprintf(fl, " <- .JT%d + ", ((jump_table_t*)(stmt->jumping_to_block))->jump_table_id);
		
		//Now print out the variable
		print_variable(fl, stmt->op2, PRINTING_VAR_INLINE);

		//Finally the multiplicator
		fprintf(fl, " * %ld\n", stmt->lea_multiplicator);
	//Print out an indirect jump statement
	} else if(stmt->CLASS == THREE_ADDR_CODE_INDIRECT_JUMP_STMT){
		switch(stmt->jump_type){
			case JUMP_TYPE_JE:
				fprintf(fl, "je");
				break;
			case JUMP_TYPE_JNE:
				fprintf(fl, "jne");
				break;
			case JUMP_TYPE_JG:
				fprintf(fl, "jg");
				break;
			case JUMP_TYPE_JL:
				fprintf(fl, "jl");
				break;
			case JUMP_TYPE_JNZ:
				fprintf(fl, "jnz");
				break;
			case JUMP_TYPE_JZ:
				fprintf(fl, "jz");
				break;
			case JUMP_TYPE_JMP:
				fprintf(fl, "jmp");
				break;
			case JUMP_TYPE_JGE:
				fprintf(fl, "jge");
				break;
			case JUMP_TYPE_JLE:
				fprintf(fl, "jle");
				break;
			case JUMP_TYPE_JAE:
				fprintf(fl, "jae");
				break;
			case JUMP_TYPE_JBE:
				fprintf(fl, "jbe");
				break;
			case JUMP_TYPE_JA:
				fprintf(fl, "ja");
				break;
			case JUMP_TYPE_JB:
				fprintf(fl, "jb");
				break;
			default:
				fprintf(fl, "jmp");
				break;
		}

		//Indirection
		fprintf(fl, " *");

		//Now the variable
		print_variable(fl, stmt->op1, PRINTING_VAR_INLINE);
		fprintf(fl, "\n");
	} 
}


/**
 * Print a constant as an immediate($ prefixed) value
 */
static void print_immediate_value(FILE* fl, three_addr_const_t* constant){
	//We'll now interpret what we have here
	if(constant->const_type == INT_CONST){
		fprintf(fl, "$%d", constant->int_const);
	} else if(constant->const_type == LONG_CONST){
		fprintf(fl, "$%ld", constant->long_const);
	} else if(constant->const_type == FLOAT_CONST){
		fprintf(fl, "$%f", constant->float_const);
	} else if(constant->const_type == CHAR_CONST){
		fprintf(fl, "$%d", constant->char_const);
	} 
}


/**
 * Print a constant as an immediate(not $ prefixed) value
 */
static void print_immediate_value_no_prefix(FILE* fl, three_addr_const_t* constant){
	//We'll now interpret what we have here
	if(constant->const_type == INT_CONST){
		fprintf(fl, "%d", constant->int_const);
	} else if(constant->const_type == LONG_CONST){
		fprintf(fl, "%ld", constant->long_const);
	} else if(constant->const_type == FLOAT_CONST){
		fprintf(fl, "%f", constant->float_const);
	} else if(constant->const_type == CHAR_CONST){
		fprintf(fl, "%d", constant->char_const);
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
			for(u_int8_t i = 0; i < instruction->indirection_level; i++){
				fprintf(fl, "(");
			}

			if(instruction->calculation_mode == ADDRESS_CALCULATION_MODE_DEREF_ONLY_SOURCE){
				print_variable(fl, instruction->source_register, mode);
			} else {
				print_variable(fl, instruction->destination_register, mode);
			}

			for(u_int8_t i = 0; i < instruction->indirection_level; i++){
				fprintf(fl, ")");
			}

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
 * Print a movzx or movsx(converting move) instruction
 */
static void print_converting_move(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	//First we'll determine what to print
	if(instruction->instruction_type == MOVZX){
		fprintf(fl, "movzx ");
	} else {
		fprintf(fl, "movsx ");
	}

	//Now we'll print the source and destination
	print_variable(fl, instruction->source_register, mode);
	fprintf(fl, ", ");
	print_variable(fl, instruction->destination_register, mode);

	fprintf(fl, "\n");
}


/**
 * Handle a simple register to register or immediate to register move
 */
static void print_register_to_register_move(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
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
		default:
			break;
	}

	//Print the appropriate variable here
	if(instruction->source_register != NULL){
		//If we have a source-only dereference print it
		if(instruction->calculation_mode == ADDRESS_CALCULATION_MODE_DEREF_ONLY_SOURCE){
			print_addressing_mode_expression(fl, instruction, mode);
		} else {
			print_variable(fl, instruction->source_register, mode);
		}
	} else {
		print_immediate_value(fl, instruction->source_immediate);
	}

	//Needed comma
	fprintf(fl, ",");

	//Now print our destination
	if(instruction->calculation_mode == ADDRESS_CALCULATION_MODE_DEREF_ONLY_DEST){
		print_addressing_mode_expression(fl, instruction, mode);
	} else {
		print_variable(fl, instruction->destination_register, mode);
	} 

	//A final newline is needed for all instructions
	fprintf(fl, "\n");
}


/**
 * Handle a complex register(or immediate) to memory move with a complex
 * address offset calculation
 */
static void print_register_to_memory_move(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	//First let's print out the appropriate instruction
	switch(instruction->instruction_type){
		case REG_TO_MEM_MOVB:
			fprintf(fl, "movb ");
			break;
		case REG_TO_MEM_MOVW:
			fprintf(fl, "movw ");
			break;
		case REG_TO_MEM_MOVL:
			fprintf(fl, "movl ");
			break;
		case REG_TO_MEM_MOVQ:
			fprintf(fl, "movq ");
			break;
		//Should never hit this
		default:
			break;
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
	//First thing we'll do is print the appropriate move statement
	switch(instruction->instruction_type){
		case MEM_TO_REG_MOVB:
			fprintf(fl, "movb ");
			break;
		case MEM_TO_REG_MOVW:
			fprintf(fl, "movw ");
			break;
		case MEM_TO_REG_MOVL:
			fprintf(fl, "movl ");
			break;
		case MEM_TO_REG_MOVQ:
			fprintf(fl, "movq ");
			break;
		//Should never hit this
		default:
			break;
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
	fprintf(fl, " /* --> ");
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
		case DIVB_FOR_MOD:
			fprintf(fl, "divb ");
			break;
		case DIVW:
		case DIVW_FOR_MOD:
			fprintf(fl, "divw ");
			break;
		case DIVL:
		case DIVL_FOR_MOD:
			fprintf(fl, "divl ");
			break;
		case DIVQ:
		case DIVQ_FOR_MOD:
			fprintf(fl, "divq ");
			break;
		case IDIVB:
		case IDIVB_FOR_MOD:
			fprintf(fl, "idivb ");
			break;
		case IDIVW:
		case IDIVW_FOR_MOD:
			fprintf(fl, "idivw ");
			break;
		case IDIVL:
		case IDIVL_FOR_MOD:
			fprintf(fl, "idivl ");
			break;
		case IDIVQ:
		case IDIVQ_FOR_MOD:
			fprintf(fl, "idivq ");
			break;
		//We'll never get here, just to stop the compiler from complaining
		default:
			break;
	}

	//We'll only have a source register here
	print_variable(fl, instruction->source_register, mode);

	fprintf(fl, " /* --> ");
	print_variable(fl, instruction->destination_register, mode);
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
			fprintf(fl, "subw ");
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
 * Print out a movzbl instruction
 */
static void print_movzbl_instruction(FILE* fl, instruction_t* instruction, variable_printing_mode_t mode){
	//First we'll just print out the opcode
	fprintf(fl, "movzbl ");

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
	basic_block_t* jumping_to_block = instruction->jumping_to_block;

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
			fprintf(fl, "cqto\n");
			break;
		case CLTD:
			fprintf(fl, "cltd\n");
			break;
		case CWTL:
			fprintf(fl, "cltw\n");
			break;
		case CBTW:
			fprintf(fl, "cbtw\n");
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
			fprintf(fl, "%s", instruction->inlined_assembly);
			break;
		case CALL:
			fprintf(fl, "call %s", instruction->called_function->func_name);
			if(instruction->destination_register != NULL){
				fprintf(fl, " /* --> ");
				print_variable(fl, instruction->destination_register, mode);
			}
			fprintf(fl, " */\n");
			break;
		case PUSH:
			fprintf(fl, "push ");
			print_variable(fl, instruction->source_register, mode);
			fprintf(fl, "\n");
			break;
		case POP:
			fprintf(fl, "pop ");
			print_variable(fl, instruction->source_register, mode);
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
		case DIVB_FOR_MOD:
		case DIVW_FOR_MOD:
		case DIVL_FOR_MOD:
		case DIVQ_FOR_MOD:
		case IDIVB_FOR_MOD:
		case IDIVW_FOR_MOD:
		case IDIVQ_FOR_MOD:
		case IDIVL_FOR_MOD:
			print_division_instruction(fl, instruction, mode);
			break;

		//Handle the special addressing modes that we could have here
		case REG_TO_MEM_MOVB:
		case REG_TO_MEM_MOVL:
		case REG_TO_MEM_MOVW:
		case REG_TO_MEM_MOVQ:
			print_register_to_memory_move(fl, instruction, mode);
			break;

		case MEM_TO_REG_MOVB:
		case MEM_TO_REG_MOVL:
		case MEM_TO_REG_MOVW:
		case MEM_TO_REG_MOVQ:
			print_memory_to_register_move(fl, instruction, mode);
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

		//Handle basic move instructions(no complex addressing)
		case MOVB:
		case MOVW:
		case MOVL:
		case MOVQ:
			//Invoke the helper
			print_register_to_register_move(fl, instruction, mode);
			break;

		//Handle a converting move
		case MOVSX:
		case MOVZX:
			print_converting_move(fl, instruction, mode);
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
			jump_table_t* jumping_to_block = instruction->jumping_to_block;

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
			dynamic_array_t* phi_func_params = instruction->phi_function_parameters;

			//Now run through all of the parameters
			for(u_int16_t _ = 0; phi_func_params != NULL && _ < phi_func_params->current_index; _++){
				//Print out the variable
				print_variable(fl, dynamic_array_get_at(phi_func_params, _), PRINTING_VAR_BLOCK_HEADER);

				//If it isn't the very last one, add a comma space
				if(_ != phi_func_params->current_index - 1){
					fprintf(fl, ", ");
				}
			}

			fprintf(fl, ")\n");

		//Show a default error message
		default:
			//fprintf(fl, "Not yet selected\n");
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
	dec_stmt->CLASS = THREE_ADDR_CODE_DEC_STMT;
	dec_stmt->assignee = emit_var_copy(decrementee);
	dec_stmt->op1 = decrementee;
	//What function are we in
	dec_stmt->function = current_function;
	//And give it back
	return dec_stmt;
}


/**
 * Emit a decrement instruction
 */
instruction_t* emit_inc_instruction(three_addr_var_t* incrementee){
	//First allocate it
	instruction_t* inc_stmt = calloc(1, sizeof(instruction_t));

	//Now we populate
	inc_stmt->CLASS = THREE_ADDR_CODE_INC_STMT;
	inc_stmt->assignee = emit_var_copy(incrementee);
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

	//Attach it for memory management
	constant->next_created = emitted_consts;
	emitted_consts = constant;

	//Grab a reference to the const node for convenience
	constant_ast_node_t* const_node_raw = (constant_ast_node_t*)(const_node->node);

	//Now we'll assign the appropriate values
	constant->const_type = const_node_raw->constant_type; 
	constant->type = const_node->inferred_type;

	//Now based on what type we have we'll make assignments
	switch(constant->const_type){
		case CHAR_CONST:
			constant->char_const = const_node_raw->char_val;
			//Set the 0 flag if true
			if(const_node_raw->char_val == 0){
				constant->is_value_0 = TRUE;
			}
			break;
		case INT_CONST:
			constant->int_const = const_node_raw->int_val;
			//Set the 0 flag if true
			if(const_node_raw->int_val == 0){
				constant->is_value_0 = TRUE;
			}
			break;
		case FLOAT_CONST:
			constant->float_const = const_node_raw->float_val;
			break;
		case STR_CONST:
			strcpy(constant->str_const, const_node_raw->string_val);
			break;
		case LONG_CONST:
			constant->long_const = const_node_raw->long_val;
			//Set the 0 flag if 
			if(const_node_raw->long_val == 0){
				constant->is_value_0 = TRUE;
			}
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
 * Emit a return statement. The returnee variable may or may not be null
 */
instruction_t* emit_ret_instruction(three_addr_var_t* returnee){
	//First allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Let's now populate it appropriately
	stmt->CLASS = THREE_ADDR_CODE_RET_STMT;
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
instruction_t* emit_binary_operation_instruction(three_addr_var_t* assignee, three_addr_var_t* op1, Token op, three_addr_var_t* op2){
	//First allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Let's now populate it with the appropriate values
	stmt->CLASS = THREE_ADDR_CODE_BIN_OP_STMT;
	stmt->assignee = assignee;
	stmt->op1 = op1;
	stmt->op = op;
	stmt->op2 = op2;
	//What function are we in
	stmt->function = current_function;

	//If the operator is a || or && operator, then this statement is eligible for short circuiting
	if(op == DOUBLE_AND || op == DOUBLE_OR){
		stmt->is_short_circuit_eligible = TRUE;
	}

	//Give back the newly allocated statement
	return stmt;
}


/**
 * Emit a binary operation with a constant three address code statement
 */
instruction_t* emit_binary_operation_with_const_instruction(three_addr_var_t* assignee, three_addr_var_t* op1, Token op, three_addr_const_t* op2){
	//First allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Let's now populate it with the appropriate values
	stmt->CLASS = THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT;
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
	stmt->CLASS = THREE_ADDR_CODE_ASSN_STMT;
	//Let's now populate it with values
	stmt->assignee = assignee;
	stmt->op1 = op1;
	//What function are we in
	stmt->function = current_function;
	//And that's it, we'll just leave our now
	return stmt;
}


/**
 * Emit a conditional assignment instruction
 */
instruction_t* emit_conditional_assignment_instruction(three_addr_var_t* assignee, three_addr_var_t* op1, Token prior_operator, u_int8_t is_signed, u_int8_t inverse_assignment){
	//First we'll allocate the instruction
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Now define the class
	stmt->CLASS = THREE_ADDR_CODE_CONDITIONAL_MOVEMENT_STMT;

	//Now we'll populate with values
	stmt->assignee = assignee;
	stmt->op1 = op1;

	//What function are we in
	stmt->function = current_function;

	//Now let's see what kind of conditional move we have
	if(inverse_assignment == FALSE){
		switch(prior_operator){
			case G_THAN:
				if(is_signed == TRUE){
					stmt->move_type = CONDITIONAL_MOVE_G;
				} else {
					stmt->move_type = CONDITIONAL_MOVE_A;
				}

				break;

			case L_THAN:
				if(is_signed == TRUE){
					stmt->move_type = CONDITIONAL_MOVE_L;
				} else {
					stmt->move_type = CONDITIONAL_MOVE_B;
				}

				break;
			case G_THAN_OR_EQ:
				if(is_signed == TRUE){
					stmt->move_type = CONDITIONAL_MOVE_GE;
				} else {
					stmt->move_type = CONDITIONAL_MOVE_AE;
				}

				break;

			case L_THAN_OR_EQ:
				if(is_signed == TRUE){
					stmt->move_type = CONDITIONAL_MOVE_LE;
				} else {
					stmt->move_type = CONDITIONAL_MOVE_BE;
				}

				break;

			case NOT_EQUALS:
				//Move if not zero
				stmt->move_type = CONDITIONAL_MOVE_NE;
				break;		

			case DOUBLE_EQUALS:
				//Move if equal
				stmt->move_type = CONDITIONAL_MOVE_E;
				break;		

			//By default it's just a not zero move
			default:
				stmt->move_type = CONDITIONAL_MOVE_NZ;
				break;
		}
	
	//Otherwise we're in so called "inverse" mode, where we do everything in reverse
	} else {
		switch(prior_operator){
			case G_THAN:
				if(is_signed == TRUE){
					stmt->move_type = CONDITIONAL_MOVE_LE;
				} else {
					stmt->move_type = CONDITIONAL_MOVE_BE;
				}

				break;

			case L_THAN:
				if(is_signed == TRUE){
					stmt->move_type = CONDITIONAL_MOVE_GE;
				} else {
					stmt->move_type = CONDITIONAL_MOVE_AE;
				}

				break;
			case G_THAN_OR_EQ:
				if(is_signed == TRUE){
					stmt->move_type = CONDITIONAL_MOVE_L;
				} else {
					stmt->move_type = CONDITIONAL_MOVE_B;
				}

				break;

			case L_THAN_OR_EQ:
				if(is_signed == TRUE){
					stmt->move_type = CONDITIONAL_MOVE_G;
				} else {
					stmt->move_type = CONDITIONAL_MOVE_A;
				}

				break;

			case NOT_EQUALS:
				//Move if not zero
				stmt->move_type = CONDITIONAL_MOVE_E;
				break;		

			case DOUBLE_EQUALS:
				//Move if equal
				stmt->move_type = CONDITIONAL_MOVE_NE;
				break;		

			//By default it's just a not zero move
			default:
				stmt->move_type = CONDITIONAL_MOVE_Z;
				break;
		}
	}
	
	//Give back the statement
	return stmt;
}


/**
 * Emit a memory address assignment statement
 */
instruction_t* emit_memory_address_assignment(three_addr_var_t* assignee, three_addr_var_t* op1){
	//First allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Let's now populate it with values
	stmt->CLASS = THREE_ADDR_CODE_MEM_ADDR_ASSIGNMENT;
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
instruction_t* emit_memory_access_instruction(three_addr_var_t* assignee, three_addr_var_t* op1, memory_access_type_t access_type){
	//First we allocate
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Let's now populate it with values
	stmt->CLASS = THREE_ADDR_CODE_MEM_ACCESS_STMT;
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
	variable_size_t size = select_variable_size(assignee);

	//Select the appropriate register
	switch(size){
		case BYTE:
			stmt->instruction_type = MEM_TO_REG_MOVB;
			break;
		case WORD:
			stmt->instruction_type = MEM_TO_REG_MOVW;
			break;
		case DOUBLE_WORD:
			stmt->instruction_type = MEM_TO_REG_MOVL;
			break;
		case QUAD_WORD:
			stmt->instruction_type = MEM_TO_REG_MOVQ;
			break;
		default:
			break;
	}

	stmt->destination_register = assignee;
	//Stack pointer is source 1
	stmt->address_calc_reg1 = stack_pointer;
	stmt->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;

	//Emit an integer constant for this offset
	stmt->offset = emit_long_constant_direct(offset, symtab);

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
	variable_size_t size = select_variable_size(source);

	//Select the appropriate register
	switch(size){
		case BYTE:
			stmt->instruction_type = REG_TO_MEM_MOVB;
			break;
		case WORD:
			stmt->instruction_type = REG_TO_MEM_MOVW;
			break;
		case DOUBLE_WORD:
			stmt->instruction_type = REG_TO_MEM_MOVL;
			break;
		case QUAD_WORD:
			stmt->instruction_type = REG_TO_MEM_MOVQ;
			break;
		default:
			break;
	}

	//We'll have the stored variable as our source
	stmt->source_register = source;
	
	//Stack pointer our base address
	stmt->address_calc_reg1 = stack_pointer;
	stmt->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;

	//Emit an integer constant for this offset
	stmt->offset = emit_long_constant_direct(offset, symtab);

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
	stmt->CLASS = THREE_ADDR_CODE_ASSN_CONST_STMT;
	stmt->assignee = assignee;
	stmt->op1_const = constant;
	//What function are we in
	stmt->function = current_function;
	//And that's it, we'll now just give it back
	return stmt;
}


/**
 * Emit a jump statement where we jump to the block with the ID provided
 */
instruction_t* emit_jmp_instruction(void* jumping_to_block, jump_type_t jump_type){
	//First allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Let's now populate it with values
	stmt->CLASS = THREE_ADDR_CODE_JUMP_STMT;
	stmt->jumping_to_block = jumping_to_block;
	stmt->jump_type = jump_type;
	//What function are we in
	stmt->function = current_function;
	//Give the statement back
	return stmt;
}


/**
 * Emit an indirect jump statement. The jump statement can take on several different types of jump
 */
instruction_t* emit_indirect_jmp_instruction(three_addr_var_t* address, jump_type_t jump_type){
	//First we allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Let's now populate it with values
	stmt->CLASS = THREE_ADDR_CODE_INDIRECT_JUMP_STMT;
	//The address we're jumping to is in op1
	stmt->op1 = address;
	stmt->jump_type = jump_type;
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
	stmt->CLASS = THREE_ADDR_CODE_FUNC_CALL;
	stmt->called_function = func_record;
	stmt->assignee = assigned_to;
	//What function are we in
	stmt->function = current_function;
	//We do NOT add parameters here, instead we had them in the CFG function
	//Just give back the result
	return stmt;
}


/**
 * Emit an int constant direct 
 */
three_addr_const_t* emit_int_constant_direct(int int_const, type_symtab_t* symtab){
	three_addr_const_t* constant = calloc(1, sizeof(three_addr_const_t));

	//Attach it for memory management
	constant->next_created = emitted_consts;
	emitted_consts = constant;

	//Store the class
	constant->const_type = INT_CONST;
	//Store the int value
	constant->int_const = int_const;

	//Lookup what we have in here(i32)
	constant->type = lookup_type_name_only(symtab, "i32")->type;

	//Set this flag if we need to
	if(int_const == 0){
		constant->is_value_0 = TRUE;
	}

	//Return out
	return constant;
}


/**
 * Emit an unsigned in constant directly. Used for address calculations
 */
three_addr_const_t* emit_unsigned_int_constant_direct(int int_const, type_symtab_t* symtab){

	three_addr_const_t* constant = calloc(1, sizeof(three_addr_const_t));

	//Attach it for memory management
	constant->next_created = emitted_consts;
	emitted_consts = constant;

	//Store the class
	constant->const_type = INT_CONST;
	//Store the int value
	constant->int_const = int_const;

	//Lookup what we have in here(u32)
	constant->type = lookup_type_name_only(symtab, "u32")->type;

	//Set this flag if we need to
	if(int_const == 0){
		constant->is_value_0 = TRUE;
	}

	//Return out
	return constant;
}


/**
 * Emit a long constant direct 
 */
three_addr_const_t* emit_long_constant_direct(long long_const, type_symtab_t* symtab){
	three_addr_const_t* constant = calloc(1, sizeof(three_addr_const_t));

	//Attach it for memory management
	constant->next_created = emitted_consts;
	emitted_consts = constant;

	//Store the class
	constant->const_type = LONG_CONST;
	//Store the int value
	constant->long_const = long_const;

	//Lookup what we have in here(i32)
	constant->type = lookup_type_name_only(symtab, "i64")->type;

	//Set this flag if we need to
	if(long_const == 0){
		constant->is_value_0 = TRUE;
	}

	//Return out
	return constant;
}


/**
 * Emit a negation statement
 */
instruction_t* emit_neg_instruction(three_addr_var_t* assignee, three_addr_var_t* negatee){
	//First we'll create the negation
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Now we'll assign whatever we need
	stmt->CLASS = THREE_ADDR_CODE_NEG_STATEMENT;
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
	stmt->CLASS = THREE_ADDR_CODE_BITWISE_NOT_STMT;
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
	stmt->CLASS = THREE_ADDR_CODE_LOGICAL_NOT_STMT;
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
instruction_t* emit_asm_inline_instruction(asm_inline_stmt_ast_node_t* asm_inline_node){
	//First we allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//Store the class
	stmt->CLASS = THREE_ADDR_CODE_ASM_INLINE_STMT;

	//Then we'll allocate the needed space for the string holding the assembly
	stmt->inlined_assembly = calloc(asm_inline_node->max_length, sizeof(char));

	//Copy the assembly over
	strncpy(stmt->inlined_assembly, asm_inline_node->asm_line_statements, asm_inline_node->length);
	//What function are we in
	stmt->function = current_function;

	//And we're done, now we'll bail out
	return stmt;
}


/**
 * Emit a phi function for a given variable. Once emitted, these statements are compiler exclusive,
 * but they are needed for our optimization
 */
instruction_t* emit_phi_function(symtab_variable_record_t* variable, generic_type_t* type){
	//First we allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//We'll just store the assignee here, no need for anything else
	stmt->assignee = emit_var(variable, FALSE);

	//Note what kind of node this is
	stmt->CLASS = THREE_ADDR_CODE_PHI_FUNC;
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
	stmt->source_immediate = emit_int_constant_direct(offset, type_symtab);

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
	stmt->source_immediate = emit_int_constant_direct(offset, type_symtab);

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
	copy->phi_function_parameters = NULL;
	copy->inlined_assembly = NULL;
	copy->next_statement = NULL;
	copy->previous_statement = NULL;
	
	//If we have function call parameters, emit a copy of them
	if(copied->function_parameters != NULL){
		copy->function_parameters = clone_dynamic_array(copied->function_parameters);
	}

	//Give back the copied one
	return copied;
}


/**
 * Emit the sum of two given constants. The result will overwrite the second constant given
 *
 * The result will be: constant2 = constant1 + constant2
 */
three_addr_const_t* add_constants(three_addr_const_t* constant1, three_addr_const_t* constant2){
	// Switch based on the type
	switch(constant2->const_type){
		//If this is a type as such, we'll add the int constant and char constant
		//from constant 1. We can do this because whatever is unused is set to 0
		//by the calloc
		case INT_CONST:
		case INT_CONST_FORCE_U:
			//If it's any of these we'll add the int value
			if(constant1->const_type == INT_CONST || constant1->const_type == INT_CONST_FORCE_U){
				constant2->int_const += constant1->int_const;
			//Otherwise add the long value
			} else if(constant1->const_type == LONG_CONST || constant1->const_type == LONG_CONST_FORCE_U){
				constant2->int_const += constant1->long_const;
			//Only other option is char
			} else {
				constant2->int_const += constant1->char_const;
			}
			break;
		case LONG_CONST:
		case LONG_CONST_FORCE_U:
			//If it's any of these we'll add the int value
			if(constant1->const_type == INT_CONST || constant1->const_type == INT_CONST_FORCE_U){
				constant2->long_const += constant1->int_const;
			//Otherwise add the long value
			} else if(constant1->const_type == LONG_CONST || constant1->const_type == LONG_CONST_FORCE_U){
				constant2->long_const += constant1->long_const;
			//Only other option is char
			} else {
				constant2->long_const += constant1->char_const;
			}

			break;
		//Can't really see this ever happening, but it won't hurt
		case CHAR_CONST:
			//Add the other one's char const
			constant2->char_const += constant1->char_const;
			break;
		//Mainly for us as the programmer
		default:
			print_parse_message(PARSE_ERROR, "Attempt to add incompatible constants", 0);
			break;
	}

	//We always give back constant 2
	return constant2;
}


/**
 * Select the appropriate jump type to use. We can either use
 * inverse jumps or direct jumps
 */
jump_type_t select_appropriate_jump_stmt(Token op, jump_category_t jump_type, u_int8_t is_signed){
	//Let's see what we have here
	switch(op){
		case G_THAN:
			if(jump_type == JUMP_CATEGORY_INVERSE){
				if(is_signed == TRUE){
					//Signed version
					return JUMP_TYPE_JLE;
				} else {
					//Unsigned version
					return JUMP_TYPE_JBE;
				}
			} else {
				if(is_signed == TRUE){
					//Signed version
					return JUMP_TYPE_JG;
				} else {
					//Unsigned version
					return JUMP_TYPE_JA;
				}
			}
		case L_THAN:
			if(jump_type == JUMP_CATEGORY_INVERSE){
				if(is_signed == TRUE){
					//Signed version
					return JUMP_TYPE_JGE;
				} else {
					//Unsigned version
					return JUMP_TYPE_JAE;
				}
			} else {
				if(is_signed == TRUE){
					//Signed version
					return JUMP_TYPE_JL;
				} else {
					//Unsigned version
					return JUMP_TYPE_JB;
				}
			}
		case L_THAN_OR_EQ:
			if(jump_type == JUMP_CATEGORY_INVERSE){
				if(is_signed == TRUE){
					//Signed version
					return JUMP_TYPE_JG;
				} else {
					//Unsigned version
					return JUMP_TYPE_JA;
				}
			} else {
				if(is_signed == TRUE){
					//Signed version
					return JUMP_TYPE_JLE;
				} else {
					//Unsigned version
					return JUMP_TYPE_JBE;
				}
			}
		case G_THAN_OR_EQ:
			if(jump_type == JUMP_CATEGORY_INVERSE){
				if(is_signed == TRUE){
					//Signed version
					return JUMP_TYPE_JL;
				} else {
					//Unsigned version
					return JUMP_TYPE_JB;
				}
			} else {
				if(is_signed == TRUE){
					//Signed version
					return JUMP_TYPE_JGE;
				} else {
					//Unsigned version
					return JUMP_TYPE_JAE;
				}
			}
		case DOUBLE_EQUALS:
			if(jump_type == JUMP_CATEGORY_INVERSE){
				return JUMP_TYPE_JNE;
			} else {
				return JUMP_TYPE_JE;
			}
		case NOT_EQUALS:
			if(jump_type == JUMP_CATEGORY_INVERSE){
				return JUMP_TYPE_JE;
			} else {
				return JUMP_TYPE_JNE;
			}
		//If we get here, it was some kind of
		//non relational operator. In this case,
		//we default to 0 = false non zero = true
		default:
			if(jump_type == JUMP_CATEGORY_INVERSE){
				return JUMP_TYPE_JZ;
			} else {
				return JUMP_TYPE_JNZ;
			}
	}
}


/**
 * Select the appropriate set type given the circumstances, including the operand and the signedness
 */
instruction_type_t select_appropriate_set_stmt(Token op, u_int8_t is_signed){
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
u_int8_t is_register_caller_saved(register_holder_t reg){
	switch(reg){
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
u_int8_t is_register_callee_saved(register_holder_t reg){
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
 * Are two variables equal? A helper method for searching
 */
u_int8_t variables_equal(three_addr_var_t* a, three_addr_var_t* b, u_int8_t ignore_indirect_level){
	//Easy way to tell here
	if(a == NULL || b == NULL){
		return FALSE;
	}

	//Another easy way to tell
	if(a->is_temporary != b->is_temporary){
		return FALSE;
	}

	//Another way to tell
	if(a->indirection_level != b->indirection_level && ignore_indirect_level == FALSE){
		return FALSE;
	}

	//For temporary variables, the comparison is very easy
	if(a->is_temporary){
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
u_int8_t variables_equal_no_ssa(three_addr_var_t* a, three_addr_var_t* b, u_int8_t ignore_indirect_level){
	//Easy way to tell here
	if(a == NULL || b == NULL){
		return FALSE;
	}

	//Another easy way to tell
	if(a->is_temporary != b->is_temporary){
		return FALSE;
	}

	//Another way to tell
	if(a->indirection_level != b->indirection_level && ignore_indirect_level == FALSE){
		return FALSE;
	}

	//For temporary variables, the comparison is very easy
	if(a->is_temporary){
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

	//If we have an asm inline statement
	if(stmt->CLASS == THREE_ADDR_CODE_ASM_INLINE_STMT){
		//We must also free the pointer in here
		free(stmt->inlined_assembly);
	}

	//If we have a phi function, deallocate the dynamic array
	if(stmt->phi_function_parameters != NULL){
		dynamic_array_dealloc(stmt->phi_function_parameters);
	}

	//If we have function parameters get rid of them
	if(stmt->function_parameters != NULL){
		dynamic_array_dealloc(stmt->function_parameters);
	}
	
	//Free the overall stmt -- variables handled elsewhere
	free(stmt);
}


/**
 * Deallocate all variables using our global list strategy
*/
void deallocate_all_vars(){
	//For holding 
	three_addr_var_t* temp;

	//Run through the whole list
	while(emitted_vars != NULL){
		//Hold onto it here
		temp = emitted_vars;
		//Advance
		emitted_vars = emitted_vars->next_created;
		//Free the one we just had
		free(temp);
	}
}


/**
 * Deallocate all constants using our global list strategy
*/
void deallocate_all_consts(){
	//For holding
	three_addr_const_t* temp;

	//Run through the whole list
	while(emitted_consts != NULL){
		//Hold onto it here
		temp = emitted_consts;
		//Advance
		emitted_consts = emitted_consts->next_created;
		//Deallocate temp
		free(temp);
	}
}
