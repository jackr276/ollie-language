/**
 * Author: Jack Robbins
 *
 * This file contains the implementation for the APIs defined in the header file of the same name
 *
 * The instruction selector for Ollie is what's known as a peephole selector. We crawl the entirety
 * of the generated LLIR(OIR) that we're given. We then simplify various known patterns and finally we convert the resultant 
 * simplified OIR into assembly using a variety of patten matching
*/

#include "instruction_selector.h"
#include "../queue/heap_queue.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/types.h>

//For standardization across all modules
#define TRUE 1
#define FALSE 0

//We'll need this a lot, so we may as well have it here
static generic_type_t* u64;
static generic_type_t* u32;
static generic_type_t* i32;
static generic_type_t* u8;

//The window for our "sliding window" optimizer
typedef struct instruction_window_t instruction_window_t;


/**
 * Will we be printing these out as instructions or as three address code
 * statements?
 */
typedef enum {
	PRINT_THREE_ADDRESS_CODE,
	PRINT_INSTRUCTION
} instruction_printing_mode_t;


/**
 * The widow that we have here will store three instructions at once. This allows
 * us to look at three instruction patterns at any given time.
 */
struct instruction_window_t{
	//We store three instructions and a status
	instruction_t* instruction1;
	instruction_t* instruction2;
	instruction_t* instruction3;
};


/**
 * This is used to manage when we need to swap a variable out and handle all use case
 * modifications
 */
static void replace_variable(three_addr_var_t* old, three_addr_var_t* new){
	//Decrement the old one's use count
	old->use_count--;

	//And update the new one's use count
	new->use_count++;
}


/**
 * Is an operation valid for token folding? If it is, we'll return true
 * The invalid operations are &&, ||, / and %, and * *when* it is unsigned
 */
static u_int8_t is_operation_valid_for_constant_folding(instruction_t* instruction){
	switch(instruction->op){
		case DOUBLE_AND:
		case DOUBLE_OR:
		case F_SLASH:
		case MOD:
			return FALSE;
		case STAR:
			//If this is unsigned, we cannot do this
			if(is_type_signed(instruction->assignee->type) == FALSE){
				return FALSE;
			}
			//But if it is signed, we can
			return TRUE;
		default:
			return TRUE;
	}

}


/**
 * Can an assignment statement be optimized away? If the assignment statement
 * involves converting between types, or it involves memory indirection, then
 * we cannot simply remove it
 */
static u_int8_t can_assignment_instruction_be_removed(instruction_t* assignment_instruction){
	//If this is a constant assignment, then yes we can
	if(assignment_instruction->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT){
		return TRUE;
	}

	//Otherwise, we know that we have a regular assignment statement
	//This cannot be optimized away
	if(is_expanding_move_required(assignment_instruction->assignee->type, assignment_instruction->op1->type) == TRUE){
		return FALSE;
	}

	//Otherwise if we get here, then we can
	return TRUE;
}


/**
 * A helper function that selects and returns the appopriately sized move for 
 * a logical and, or or not statement
 */
static instruction_t* emit_appropriate_move_statement(three_addr_var_t* destination, three_addr_var_t* source){
	//We will first compare the sizes and see if a conversion is needed
	if(is_expanding_move_required(destination->type, source->type) == TRUE){
		//Go based on whether or not the type is signed
		if(is_type_signed(destination->type) == TRUE){
			return emit_movsx_instruction(destination, source);
		} else {
			return emit_movzx_instruction(destination, source);
		}
	
	//Otherwise return a regular move instruction
	} else {
		return emit_movX_instruction(destination, source);
	}
}


/**
 * Multiply two constants together
 * 
 * NOTE: The result is always stored in the first one
 */
static void multiply_constants(three_addr_const_t* constant1, three_addr_const_t* constant2){
	//Handle our multiplications
	if(constant1->const_type == INT_CONST){
		if(constant2->const_type == INT_CONST){
			constant1->constant_value.integer_constant *= constant2->constant_value.integer_constant;
		} else {
			constant1->constant_value.integer_constant *= constant2->constant_value.long_constant;
		}
	} else if(constant1->const_type == LONG_CONST){
		if(constant2->const_type == INT_CONST){
			constant1->constant_value.long_constant *= constant2->constant_value.integer_constant;
		} else {
			constant1->constant_value.long_constant *= constant2->constant_value.long_constant;
		}
	}
}


/**
 * Emit a converting move instruction directly, with no need to do instruction selection afterwards
 */
static instruction_t* emit_converting_move_instruction_direct(three_addr_var_t* destination, three_addr_var_t* source){
	//Allocate it
	instruction_t* converting_move = calloc(1, sizeof(instruction_t));

	//Select type based on signedness
	if(is_type_signed(destination->type) == TRUE){
		converting_move->instruction_type = MOVSX;
	} else {
		converting_move->instruction_type = MOVZX;
	}

	//Now load in the registers
	converting_move->destination_register = destination;
	converting_move->source_register = source;

	//And return it
	return converting_move;
}


/**
 * Handle a converting move operation and return the variable that results from it. This function will also handle the implicit conversion
 * between 32 bit and unsigned 64 bit integers
 */
static three_addr_var_t* handle_expanding_move_operation(instruction_t* after_instruction, three_addr_var_t* source, generic_type_t* desired_type){
	//A generic holder for our assignee
	three_addr_var_t* assignee;

	//Extract the source type
	generic_type_t* source_type = source->type;

	//Is the desired type a 64 bit integer *and* the source type a U32 or I32? If this is the case, then 
	//movzx functions are actually invalid because x86 processors operating in 64 bit mode automatically
	//zero pad when 32 bit moves happen
	if(is_type_unsigned_64_bit(desired_type) == TRUE && is_type_32_bit_int(source->type) == TRUE){
		//Emit a variable copy of the source
		assignee = emit_var_copy(source);

		//Reassign it's type to be the desired type
		assignee->type = desired_type;

		//Select the size appropriately after the type is reassigned
		assignee->variable_size = get_type_size(assignee->type);

	//Otherwise we have a normal case here
	} else {
		//Emit the assignment instruction
		instruction_t* instruction = emit_converting_move_instruction_direct(emit_temp_var(desired_type), source);

		//Now we'll insert it before the "after instruction"
		insert_instruction_before_given(instruction, after_instruction);

		//Reassign the destination register
		assignee = instruction->destination_register;
	}

	//Give back the assignee, in whatever form it may be
	return assignee;
}


/**
 * Simple utility for us to print out an instruction window in its three address code
 * (before instruction selection) format
 */
static void print_instruction_window_three_address_code(instruction_window_t* window){
	printf("----------- Instruction Window ------------\n");
	//We'll just print out all three instructions
	if(window->instruction1 != NULL){
		print_three_addr_code_stmt(stdout, window->instruction1);
	} else {
		printf("EMPTY\n");
	}

	if(window->instruction2 != NULL){
		print_three_addr_code_stmt(stdout, window->instruction2);
	} else {
		printf("EMPTY\n");
	}
	
	if(window->instruction3 != NULL){
		print_three_addr_code_stmt(stdout, window->instruction3);
	} else {
		printf("EMPTY\n");
	}

	printf("-------------------------------------------\n");
}


/**
 * Simple utility for us to print out an instruction window in the post
 * instruction selection format
 */
static void print_instruction_window(instruction_window_t* window){
	printf("----------- Instruction Window ------------\n");
	//We'll just print out all three instructions
	if(window->instruction1 != NULL){
		print_instruction(stdout, window->instruction1, PRINTING_VAR_IN_INSTRUCTION);
	} else {
		printf("EMPTY\n");
	}

	if(window->instruction2 != NULL){
		print_instruction(stdout, window->instruction2, PRINTING_VAR_IN_INSTRUCTION);
	} else {
		printf("EMPTY\n");
	}
	
	if(window->instruction3 != NULL){
		print_instruction(stdout, window->instruction3, PRINTING_VAR_IN_INSTRUCTION);
	} else {
		printf("EMPTY\n");
	}

	printf("-------------------------------------------\n");
}


/**
 * Emit a conversion instruction for division preparation
 *
 * We use this during the process of emitting division instructions
 *
 * NOTE: This is only needed for signed division
 */
static instruction_t* emit_conversion_instruction(three_addr_var_t* converted){
	//First we'll allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//We'll need the size to select the appropriate instruction
	variable_size_t size = get_type_size(converted->type);

	switch(size){
		case QUAD_WORD:
			instruction->instruction_type = CQTO;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = CLTD;
			break;
		case WORD:
			instruction->instruction_type = CWTL;
			break;
		case BYTE:
			instruction->instruction_type = CBTW;
			break;
		default:
			break;
	}

	//And now we'll give it back
	return instruction;
}


/**
 * Emit a sete instruction
 *
 * The sete instruction is used on a byte
 */
static instruction_t* emit_sete_instruction(three_addr_var_t* destination){
	//First we'll allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//And we'll set the class
	instruction->instruction_type = SETE;

	//Finally we set the destination
	instruction->destination_register = destination;

	//And now we'll give it back
	return instruction;
}


/**
 * Emit a setne instruction
 *
 * The setne instruction is used on a byte
 */
static instruction_t* emit_setne_instruction(three_addr_var_t* destination){
	//First we'll allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//And we'll set the class
	instruction->instruction_type = SETNE;

	//Finally we set the destination
	instruction->destination_register = destination;

	//And now we'll give it back
	return instruction;
}


/**
 * Emit an ANDx instruction
 */
static instruction_t* emit_and_instruction(three_addr_var_t* destination, three_addr_var_t* source){
	//First we'll allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//We'll need the size of the variable
	variable_size_t size = get_type_size(destination->type);

	switch(size){
		case QUAD_WORD:	
			instruction->instruction_type = ANDQ;
			break;
		case DOUBLE_WORD:	
			instruction->instruction_type = ANDL;
			break;
		case WORD:	
			instruction->instruction_type = ANDW;
			break;
		case BYTE:	
			instruction->instruction_type = ANDB;
			break;
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
 * Emit an ORx instruction
 */
static instruction_t* emit_or_instruction(three_addr_var_t* destination, three_addr_var_t* source){
	//First we'll allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//We'll need the size of the variable
	variable_size_t size = get_type_size(destination->type);

	switch(size){
		case QUAD_WORD:	
			instruction->instruction_type = ORQ;
			break;
		case DOUBLE_WORD:	
			instruction->instruction_type = ORL;
			break;
		case WORD:	
			instruction->instruction_type = ORW;
			break;
		case BYTE:	
			instruction->instruction_type = ORB;
			break;
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
 * Emit a divX or idivX instruction
 *
 * Division instructions have no destination that need be written out. They only have two sources - a direct
 * source and an implicit source
 */
static instruction_t* emit_div_instruction(three_addr_var_t* assignee, three_addr_var_t* direct_source, three_addr_var_t* implicit_source, u_int8_t is_signed){
	//First we'll allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//We set the size based on the destination 
	variable_size_t size = get_type_size(assignee->type);

	//Now we'll decide this based on size and signedness
	switch (size) {
		case BYTE:
			if(is_signed == TRUE){
				instruction->instruction_type = IDIVB;
			} else {
				instruction->instruction_type = DIVB;
			}
			break;

		case WORD:
			if(is_signed == TRUE){
				instruction->instruction_type = IDIVW;
			} else {
				instruction->instruction_type = DIVW;
			}
			break;

		case DOUBLE_WORD:
			if(is_signed == TRUE){
				instruction->instruction_type = IDIVL;
			} else {
				instruction->instruction_type = DIVL;
			}
			break;

		case QUAD_WORD:
			if(is_signed == TRUE){
				instruction->instruction_type = IDIVQ;
			} else {
				instruction->instruction_type = DIVQ;
			}
			break;

		//Should never reach this
		default:
			break;
	}

	//Finally we set the sources
	instruction->source_register = direct_source;
	//This implicit source is important for our uses in the register allocator
	instruction->source_register2 = implicit_source;

	//And now we'll give it back
	return instruction;
}


/**
 * Emit a divX or idivX instruction that is intended for modulus
 *
 * Division instructions have no destination that need be written out. They only have a direct source and an implicit source
 */
static instruction_t* emit_mod_instruction(three_addr_var_t* assignee, three_addr_var_t* direct_source, three_addr_var_t* implicit_source, u_int8_t is_signed){
	//First we'll allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//We set the size based on the destination 
	variable_size_t size = get_type_size(assignee->type);

	//Now we'll decide this based on size and signedness
	switch (size) {
		case BYTE:
			if(is_signed == TRUE){
				instruction->instruction_type = IDIVB_FOR_MOD;
			} else {
				instruction->instruction_type = DIVB_FOR_MOD;
			}
			break;
		case WORD:
			if(is_signed == TRUE){
				instruction->instruction_type = IDIVW_FOR_MOD;
			} else {
				instruction->instruction_type = DIVW_FOR_MOD;
			}
			break;
		case DOUBLE_WORD:
			if(is_signed == TRUE){
				instruction->instruction_type = IDIVL_FOR_MOD;
			} else {
				instruction->instruction_type = DIVL_FOR_MOD;
			}
			break;

		case QUAD_WORD:
			if(is_signed == TRUE){
				instruction->instruction_type = IDIVQ_FOR_MOD;
			} else {
				instruction->instruction_type = DIVQ_FOR_MOD;
			}
			break;

		//Should never reach this
		default:
			break;
	}

	//Finally we set the sources
	instruction->source_register = direct_source;
	instruction->source_register2 = implicit_source;

	//And now we'll give it back
	return instruction;
}


/**
 * Initialize the instruction window by taking in the first 3 values in the head block
 */
static instruction_window_t initialize_instruction_window(basic_block_t* head){
	//Grab the window
	instruction_window_t window;
	window.instruction1 = NULL;
	window.instruction2 = NULL;
	window.instruction3 = NULL;

	//The first instruction is the leader statement
	window.instruction1 = head->leader_statement;

	//If this is null(possible but rare), just give it back
	if(window.instruction1 == NULL){
		return window;
	}

	//Instruction 2 is next to the head
	window.instruction2 = window.instruction1->next_statement;

	//If this isn't null, 3 is this guy's next one
	if(window.instruction2 != NULL){
		window.instruction3 = window.instruction2->next_statement;
	}
	
	//And now we give back the window
	return window;
}


/**
 * Reconstruct the instruction window after some kind of deletion/reordering
 *
 * The "seed" is always the first instruction, it's what we use to set the rest up
 */
static void reconstruct_window(instruction_window_t* window, instruction_t* seed){
	//Instruction 1 is always the seed
	window->instruction1 = seed;

	//The second one always comes after here
	instruction_t* second = seed->next_statement;

	//This is always the next statement
	window->instruction2 = second;

	//If second is not null, third is just what is after second
	if(second != NULL){
		window->instruction3 = second->next_statement;
	} else {
		window->instruction3 = NULL;
	}
}


/**
 * Advance the window up by 1 instruction. This means that the lowest instruction slides
 * out of our window, and the one next to the highest instruction slides into it
 */
static instruction_window_t* slide_window(instruction_window_t* window){
	//Instruction1 becomes instruction 2
	window->instruction1 = window->instruction2;
	//Instruction2 becomes instruction3
	window->instruction2 = window->instruction3;

	//Instruction3 becomes what's next to instruction 2
	if(window->instruction2 != NULL){
		window->instruction3 = window->instruction2->next_statement;
	} else {
		window->instruction3 = NULL;
	}

	//Give it back
	return window;
}


/**
 * Jump instructions are basically already done for us. It's a very simple one-to-one
 * mapping that we need to do here
 */
static void select_jump_instruction(instruction_t* instruction){
	//We already know that we have a jump here, we'll just need to switch on
	//what the type is
	switch (instruction->jump_type) {
		case JUMP_TYPE_JMP:
			instruction->instruction_type = JMP;
			break;
		case JUMP_TYPE_JE:
			instruction->instruction_type = JE;
			break;
		case JUMP_TYPE_JNE:
			instruction->instruction_type = JNE;
			break;
		case JUMP_TYPE_JG:
			instruction->instruction_type = JG;
			break;
		case JUMP_TYPE_JGE:
			instruction->instruction_type = JGE;
			break;
		case JUMP_TYPE_JL:
			instruction->instruction_type = JL;
			break;
		case JUMP_TYPE_JLE:
			instruction->instruction_type = JLE;
			break;
		case JUMP_TYPE_JA:
			instruction->instruction_type = JA;
			break;
		case JUMP_TYPE_JAE:
			instruction->instruction_type = JAE;
			break;
		case JUMP_TYPE_JB:
			instruction->instruction_type = JB;
			break;
		case JUMP_TYPE_JBE:
			instruction->instruction_type = JBE;
			break;
		case JUMP_TYPE_JZ:
			instruction->instruction_type = JZ;
			break;
		case JUMP_TYPE_JNZ:
			instruction->instruction_type = JNZ;
			break;
		default:
			break;
	}
}


/**
 * A very simple helper function that selects the right move instruction based
 * solely on variable size. Done to avoid code duplication
 */
static instruction_type_t select_move_instruction(variable_size_t size){
	//Go based on size
	switch(size){
		case BYTE:
			return MOVB;
		case WORD:
			return MOVW;
		case DOUBLE_WORD:
			return MOVL;
		case QUAD_WORD:
			return MOVQ;
		default:
			return MOVQ;
	}
}


/**
 * Select a register movement instruction based on the source and destination sizes
 */
static instruction_type_t select_register_movement_instruction(variable_size_t destination_size, variable_size_t source_size, u_int8_t is_signed){
	//If these are the exact same, then we can just call the helper and be done
	if(destination_size == source_size){
		return select_move_instruction(destination_size);
	} 

	//However, if they're not the same, we'll need some kind of converting move here
	if(is_signed == TRUE){
		return MOVSX;
	} else {
		return MOVZX;
	}
}


/**
 * A very simple helper function that selects the right add instruction based
 * solely on variable size. Done to avoid code duplication
 */
static instruction_type_t select_add_instruction(variable_size_t size){
	//Go based on size
	switch(size){
		case BYTE:
			return ADDB;
		case WORD:
			return ADDW;
		case DOUBLE_WORD:
			return ADDL;
		case QUAD_WORD:
			return ADDQ;
		default:
			return ADDQ;
	}
}


/**
 * A very simple helper function that selects the right lea instruction based
 * solely on variable size. Done to avoid code duplication
 */
static instruction_type_t select_lea_instruction(variable_size_t size){
	//Go based on size
	switch(size){
		case BYTE:
		case WORD:
			return LEAW;
		case DOUBLE_WORD:
			return LEAL;
		case QUAD_WORD:
			return LEAQ;
		default:
			return LEAQ;
	}
}


/**
 * A very simple helper function that selects the right add instruction based
 * solely on variable size. Done to avoid code duplication
 */
static instruction_type_t select_sub_instruction(variable_size_t size){
	//Go based on size
	switch(size){
		case BYTE:
			return SUBB;
		case WORD:
			return SUBW;
		case DOUBLE_WORD:
			return SUBL;
		case QUAD_WORD:
			return SUBQ;
		default:
			return SUBQ;
	}
}


/**
 * A very simple helper function that selects the right add instruction based
 * solely on variable size. Done to avoid code duplication
 */
static instruction_type_t select_cmp_instruction(variable_size_t size){
	//Go based on size
	switch(size){
		case BYTE:
			return CMPB;
		case WORD:
			return CMPW;
		case DOUBLE_WORD:
			return CMPL;
		case QUAD_WORD:
			return CMPQ;
		default:
			return CMPQ;
	}
}


/**
 * Handle a register/immediate to memory move type instruction selection with an address calculation
 *
 * DOES NOT DO DELETION/WINDOW REORDERING
 */
static void handle_two_instruction_address_calc_to_memory_move(instruction_t* address_calculation, instruction_t* memory_access){
	//Select the variable size
	variable_size_t size;
	three_addr_var_t* address_calc_reg1;
	three_addr_var_t* address_calc_reg2;

	//Select the size based on what we're moving in
	if(memory_access->op1 != NULL){
		size = get_type_size(memory_access->op1->type);
	} else {
		size = get_type_size(memory_access->op1_const->type);
	}

	//Now based on the size, we can select what variety to register/immediate to memory move we have here
	switch (size) {
		case BYTE:
			memory_access->instruction_type = REG_TO_MEM_MOVB;
			break;
		case WORD:
			memory_access->instruction_type = REG_TO_MEM_MOVW;
			break;
		case DOUBLE_WORD:
			memory_access->instruction_type = REG_TO_MEM_MOVL;
			break;
		case QUAD_WORD:
			memory_access->instruction_type = REG_TO_MEM_MOVQ;
			break;
		//WE DO NOT DO FLOATS YET
		default:
			memory_access->instruction_type = REG_TO_MEM_MOVQ;
			break;
	}

	//If we have a bin op with const statement, we'll have a constant in our answer
	/**
	 * If we have this case, we expect to see something like this
	 * t26 <- t24 + 4
	 * (t26) <- 3
	 */
	if(address_calculation->statement_type == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT){
		//So we know that the destination will be t26, the destination will remain unchanged
		//We'll have a register source and an offset
		memory_access->offset = address_calculation->op1_const;

		//Grab this out so we can take a look
		address_calc_reg1 = address_calculation->op1;

		//If this is the case, we need a converting move
		if(is_type_address_calculation_compatible(address_calc_reg1->type) == FALSE){
			//Reassign what reg1 is
			address_calc_reg1 = handle_expanding_move_operation(memory_access, address_calc_reg1, u64);
		}

		memory_access->address_calc_reg1 = address_calc_reg1;

		//This is offset only mode
		memory_access->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;
		
	//Or if we have a statement like this(rare but may happen, covering our bases)
	} else if(address_calculation->statement_type == THREE_ADDR_CODE_BIN_OP_STMT){
		//Grab both registers out
		address_calc_reg1 = address_calculation->op1;
		address_calc_reg2 = address_calculation->op2;

		//Do we need any conversion for reg1?
		if(is_type_address_calculation_compatible(address_calc_reg1->type) == FALSE){
			//Reassign what reg1 is
			address_calc_reg1 = handle_expanding_move_operation(memory_access, address_calc_reg1, u64);
		}

		//Same exact treatment for reg2
		if(is_type_address_calculation_compatible(address_calc_reg2->type) == FALSE){
			address_calc_reg2 = handle_expanding_move_operation(memory_access, address_calc_reg2, u64);
		}

		//Finally we add these into the conversions
		memory_access->address_calc_reg1 = address_calc_reg1;
		memory_access->address_calc_reg2 = address_calc_reg2;

		//This is offset only mode
		memory_access->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_ONLY;
	}

	//It's either an assign const or regular assignment. Either way,
	//we'll need to set the appropriate source value
	if(memory_access->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT){
		memory_access->source_immediate = memory_access->op1_const;
	} else {
		memory_access->source_register = memory_access->op1;
	}
}


/**
 * Handle the case where we can condense three instructions into one big address calculation for a to-memory move
 * t7 <- arr_0 + 340
 * t8 <- t7 + arg_0 * 4
 * (t8) <- 3
 *
 * Should become
 * mov(w/l/q) $3, 340(arr_0, arg_0, 4)

 *
 * NOTE: Does not do deletion or reordering
 */
static void handle_three_instruction_address_calc_to_memory_move(instruction_t* offset_calc, instruction_t* lea_statement, instruction_t* memory_access){
	//Select the variable size
	variable_size_t size;

	//Grab out what block we're in
	basic_block_t* block = offset_calc->block_contained_in;

	//Select the size based on what we're moving in
	if(memory_access->op1 != NULL){
		size = get_type_size(memory_access->op1->type);
	} else {
		size = get_type_size(memory_access->op1_const->type);
	}

	//Now based on the size, we can select what variety to register/immediate to memory move we have here
	switch (size) {
		case BYTE:
			memory_access->instruction_type = REG_TO_MEM_MOVB;
			break;
		case WORD:
			memory_access->instruction_type = REG_TO_MEM_MOVW;
			break;
		case DOUBLE_WORD:
			memory_access->instruction_type = REG_TO_MEM_MOVL;
			break;
		case QUAD_WORD:
			memory_access->instruction_type = REG_TO_MEM_MOVQ;
			break;
		//WE DO NOT DO FLOATS YET
		default:
			memory_access->instruction_type = REG_TO_MEM_MOVQ;
			break;
	}

	//Set the address calculation mode
	memory_access->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_OFFSET_AND_SCALE;
	
	//The offset comes from the first instruction
	memory_access->offset = offset_calc->op1_const;

	//Pull these out for our uses
	three_addr_var_t* address_calc_reg1 = offset_calc->op1;
	three_addr_var_t* address_calc_reg2 = lea_statement->op2;

	//Do we need any conversion for reg1?
	if(is_type_address_calculation_compatible(address_calc_reg1->type) == FALSE){
		//Reassign what reg1 is
		address_calc_reg1 = handle_expanding_move_operation(memory_access, address_calc_reg1, u64);
	}

	//Same exact treatment for reg2
	if(is_type_address_calculation_compatible(address_calc_reg2->type) == FALSE){
		address_calc_reg2 = handle_expanding_move_operation(memory_access, address_calc_reg2, u64);
	}

	//The first operand also comes from the first instruction
	memory_access->address_calc_reg1 = address_calc_reg1;

	//The second instruction gives us the second register and lea offset
	memory_access->address_calc_reg2 = address_calc_reg2;
	
	//There is nothing typewise to worry about with this
	memory_access->lea_multiplicator = lea_statement->lea_multiplicator;

	//Now we'll set the sources that we have
	if(memory_access->op1 != NULL){
		memory_access->source_register = memory_access->op1;
	} else {
		memory_access->source_immediate = memory_access->op1_const;
	}

	//And that's all
}


/**
 * Handle a memory to register move type instruction selection with an address calculation
 */
static void handle_two_instruction_address_calc_from_memory_move(instruction_t* address_calculation, instruction_t* memory_access){
	//Temporary storage, declaring here
	three_addr_var_t* address_calc_reg1;
	three_addr_var_t* address_calc_reg2;

	//Select the variable size based on the assignee
	variable_size_t size = get_type_size(memory_access->assignee->type);

	//Now based on the size, we can select what variety to register/immediate to memory move we have here
	switch (size) {
		case BYTE:
			memory_access->instruction_type = MEM_TO_REG_MOVB;
			break;
		case WORD:
			memory_access->instruction_type = MEM_TO_REG_MOVW;
			break;
		case DOUBLE_WORD:
			memory_access->instruction_type = MEM_TO_REG_MOVL;
			break;
		case QUAD_WORD:
			memory_access->instruction_type = MEM_TO_REG_MOVQ;
			break;
		//WE DO NOT DO FLOATS YET
		default:
			memory_access->instruction_type = MEM_TO_REG_MOVQ;
			break;
	}

	/**
	 * ======== BIN OP WITH CONST =================
	 *
	 * Here is what this would look like
	 *
	 * t26 <- t24 + 4
	 * (t26) <- 3
	 * 
	 * mov(w/l/q) $3, 4(t24)
	 *     op1_const  op2_const assignee
	 */
	if(address_calculation->statement_type == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT){
		//So we know that the destination will be t26, the destination will remain unchanged
		//We'll have a register source and an offset
		memory_access->offset = address_calculation->op1_const;

		//Grab this for analysis
		address_calc_reg1 = address_calculation->op1;

		//Do we need any conversion for reg1?
		if(is_type_address_calculation_compatible(address_calc_reg1->type) == FALSE){
			//Reassign what reg1 is
			address_calc_reg1 = handle_expanding_move_operation(memory_access, address_calc_reg1, u64);
		}

		//Once we're done with any needed conversions, we'll add it in here
		memory_access->address_calc_reg1 = address_calc_reg1;

		//This is offset only mode
		memory_access->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;
		
	//Otherwise, we just have a regular bin op statement
	} else {
		//Extract these two for analysis
		address_calc_reg1 = address_calculation->op1;
		address_calc_reg2 = address_calculation->op2;

		//Do we need any conversion for reg1?
		if(is_type_address_calculation_compatible(address_calc_reg1->type) == FALSE){
			//Reassign what reg1 is
			address_calc_reg1 = handle_expanding_move_operation(memory_access, address_calc_reg1, u64);
		}

		//Same exact treatment for reg2
		if(is_type_address_calculation_compatible(address_calc_reg2->type) == FALSE){
			address_calc_reg2 = handle_expanding_move_operation(memory_access, address_calc_reg2, u64);
		}

 		//So we know that the destination will be t26, the destination will remain unchanged
		//We'll have a register source and an offset
		memory_access->address_calc_reg1 = address_calc_reg1;
		memory_access->address_calc_reg2 = address_calc_reg2;

		//This is offset only mode
		memory_access->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_ONLY;
	}

	//Set the destination as well
	memory_access->destination_register = memory_access->assignee;
}


/**
 * Handle the case where we can condense three instructions into one big address calculation for a to-memory move
 * t7 <- arr_0 + 340
 * t8 <- t7 + arg_0 * 4
 * t9 <- (t8)
 *
 * Should become
 * mov(w/l/q) 340(arr_0, arg_0, 4), t9
 *
 * NOTE: Does not do deletion or reordering
 */
static void handle_three_instruction_address_calc_from_memory_move(instruction_t* offset_calc, instruction_t* lea_statement, instruction_t* memory_access){
	//We'll first select the variable size based on the destination
	variable_size_t size = get_type_size(memory_access->assignee->type);

	//Now based on the size, we can select what variety to register/immediate to memory move we have here
	switch (size) {
		case BYTE:
			memory_access->instruction_type = MEM_TO_REG_MOVB;
			break;
		case WORD:
			memory_access->instruction_type = MEM_TO_REG_MOVW;
			break;
		case DOUBLE_WORD:
			memory_access->instruction_type = MEM_TO_REG_MOVL;
			break;
		case QUAD_WORD:
			memory_access->instruction_type = MEM_TO_REG_MOVQ;
			break;
		//WE DO NOT DO FLOATS YET
		default:
			memory_access->instruction_type = MEM_TO_REG_MOVQ;
			break;
	}

	//Set the calculation mode
	memory_access->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_OFFSET_AND_SCALE;

	//Pull these out for our uses. We'll want to first determine if we need any conversions here
	three_addr_var_t* address_calc_reg1 = offset_calc->op1;
	three_addr_var_t* address_calc_reg2 = lea_statement->op2;
	
	//Do we need any conversion for reg1?
	if(is_type_address_calculation_compatible(address_calc_reg1->type) == FALSE){
		//Reassign what reg1 is
		address_calc_reg1 = handle_expanding_move_operation(memory_access, address_calc_reg1, u64);
	}

	//Same exact treatment for reg2
	if(is_type_address_calculation_compatible(address_calc_reg2->type) == FALSE){
		address_calc_reg2 = handle_expanding_move_operation(memory_access, address_calc_reg2, u64);
	}

	//The offset and first register come from the offset calculation
	memory_access->offset = offset_calc->op1_const;
	memory_access->address_calc_reg1 = address_calc_reg1;
	memory_access->address_calc_reg2 = address_calc_reg2;
	memory_access->lea_multiplicator = lea_statement->lea_multiplicator;

	//Now we'll set the destination register. We don't need to worry about any
	//immediate values here, because we can't load into an immediate
	memory_access->destination_register = memory_access->assignee;

	//And we're set
}


/**
 * t26 <- arr_0 + t25
 * t28 <- t26 + 8
 * t29 <- (t28)
 *
 * Should become
 * mov(w/l/q) 8(arr_0, t25), t29
 */
static void handle_three_instruction_registers_and_offset_only_from_memory_move(instruction_t* additive_statement, instruction_t* offset_calc, instruction_t* memory_access){
	//Need these for analysis, declaring here
	three_addr_var_t* address_calc_reg1;
	three_addr_var_t* address_calc_reg2;

	//Let's first decide what the appropriate move instruction would be
	//We'll first select the variable size based on the destination
	variable_size_t size = get_type_size(memory_access->assignee->type);

	//Now based on the size, we can select what variety to register/immediate to memory move we have here
	switch (size) {
		case BYTE:
			memory_access->instruction_type = MEM_TO_REG_MOVB;
			break;
		case WORD:
			memory_access->instruction_type = MEM_TO_REG_MOVW;
			break;
		case DOUBLE_WORD:
			memory_access->instruction_type = MEM_TO_REG_MOVL;
			break;
		case QUAD_WORD:
			memory_access->instruction_type = MEM_TO_REG_MOVQ;
			break;
		//WE DO NOT DO FLOATS YET
		default:
			memory_access->instruction_type = MEM_TO_REG_MOVQ;
			break;
	}

	//Extract these two values out for analysis
	address_calc_reg1 = additive_statement->op1;
	address_calc_reg2 = additive_statement->op2;

	//Do we need any conversion for reg1?
	if(is_type_address_calculation_compatible(address_calc_reg1->type) == FALSE){
		//Reassign what reg1 is
		address_calc_reg1 = handle_expanding_move_operation(memory_access, address_calc_reg1, u64);
	}

	//Same exact treatment for reg2
	if(is_type_address_calculation_compatible(address_calc_reg2->type) == FALSE){
		address_calc_reg2 = handle_expanding_move_operation(memory_access, address_calc_reg2, u64);
	}

	//Now that we've done any needed conversions, we can reassign
	memory_access->address_calc_reg1 = address_calc_reg1;
	memory_access->address_calc_reg2 = address_calc_reg2;

	//We'll get the offset from offset_calc
	memory_access->offset = offset_calc->op1_const;

	//Now we can put in the address calculation type
	memory_access->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_AND_OFFSET;

	//And finally, we'll set the destination appropriately
	memory_access->destination_register = memory_access->assignee;
}


/**
 * This is the to-memory equivalent of above
 *
 * t26 <- arr_0 + t25
 * t28 <- t26 + 8
 * (t28) <- 3
 *
 * Should become
 * mov(w/l/q) $3, 8(arr_0, t25)
 */
static void handle_three_instruction_registers_and_offset_only_to_memory_move(instruction_t* additive_statement, instruction_t* offset_calc, instruction_t* memory_access){
	//Declare these two holders for analysis later on
	three_addr_var_t* address_calc_reg1;
	three_addr_var_t* address_calc_reg2;

	//Let's first decide the variable size based on what we'll be moving in
	variable_size_t size;

	//Use the op1 if it's there
	if(memory_access->op1 != NULL){
		size = get_type_size(memory_access->op1->type);
	//Otherwise we need to use the constant
	} else {
		size = get_type_size(memory_access->op1_const->type);
	}

	//Now based on the size, we can select what variety to register/immediate to memory move we have here
	switch (size) {
		case BYTE:
			memory_access->instruction_type = REG_TO_MEM_MOVB;
			break;
		case WORD:
			memory_access->instruction_type = REG_TO_MEM_MOVW;
			break;
		case DOUBLE_WORD:
			memory_access->instruction_type = REG_TO_MEM_MOVL;
			break;
		case QUAD_WORD:
			memory_access->instruction_type = REG_TO_MEM_MOVQ;
			break;
		//WE DO NOT DO FLOATS YET
		default:
			memory_access->instruction_type = REG_TO_MEM_MOVQ;
			break;
	}
	
	//Let's pull these two values out to see what we're dealing with
	address_calc_reg1 = additive_statement->op1;
	address_calc_reg2 = additive_statement->op2;

	//Do we need any conversion for reg1?
	if(is_type_address_calculation_compatible(address_calc_reg1->type) == FALSE){
		//Reassign what reg1 is
		address_calc_reg1 = handle_expanding_move_operation(memory_access, address_calc_reg1, u64);
	}

	//Same exact treatment for reg2
	if(is_type_address_calculation_compatible(address_calc_reg2->type) == FALSE){
		address_calc_reg2 = handle_expanding_move_operation(memory_access, address_calc_reg2, u64);
	}

	//We'll get the first and second register from the additive statement
	memory_access->address_calc_reg1 = address_calc_reg1;
	memory_access->address_calc_reg2 = address_calc_reg2;

	//Now we can put in the address calculation type
	memory_access->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_AND_OFFSET;

	//We'll get the offset from offset_calc
	memory_access->offset = offset_calc->op1_const;

	//Now we'll set the source/source immediate based on what we're given
	//Use the op1 if it's there
	if(memory_access->op1 != NULL){
		memory_access->source_register = memory_access->op1;

	//Otherwise we need to use the constant
	} else {
		memory_access->source_immediate = memory_access->op1_const;
	}
}


/**
 * Emit a byte copy of a given source variable. This will be used for the left and right shift instructions
 * where the shift amount needed is itself a byte
 */
static three_addr_var_t* emit_byte_copy_of_variable(three_addr_var_t* source){
	//We'll want a direct copy of this variable
	three_addr_var_t* copy = emit_var_copy(source);

	//We'll then want to modify the type and size
	copy->variable_size = BYTE;
	//This one's type will be a u8
	copy->type = u8;

	//Finally give back the copy
	return copy;
}


/**
 * Handle a left shift operation. In doing a left shift, we account
 * for the possibility that we have a signed value
 */
static void handle_left_shift_instruction(instruction_t* instruction){
	//Is this a signed or unsigned instruction?
	u_int8_t is_signed = is_type_signed(instruction->assignee->type);

	//We'll also need the size of the variable
	variable_size_t size = get_type_size(instruction->assignee->type);

	switch (size) {
		case BYTE:
			if(is_signed == TRUE){
				instruction->instruction_type = SALB;
			} else {
				instruction->instruction_type = SHLB;
			}
			break;
		case WORD:
			if(is_signed == TRUE){
				instruction->instruction_type = SALW;
			} else {
				instruction->instruction_type = SHLW;
			}
			break;
		case DOUBLE_WORD:
			if(is_signed == TRUE){
				instruction->instruction_type = SALL;
			} else {
				instruction->instruction_type = SHLL;
			}
			break;
		//Everything else falls here
		default:
			if(is_signed == TRUE){
				instruction->instruction_type = SALQ;
			} else {
				instruction->instruction_type = SHLQ;
			}
			break;		
	}

	//Now we'll move over the operands
	instruction->destination_register = instruction->assignee;
	
	//We can have an immediate value or we can have a register
	if(instruction->op2 != NULL){
		//We will always emit the byte copy version of the source register here
		instruction->source_register = emit_byte_copy_of_variable(instruction->op2);

	//Otherwise we have an immediate source
	} else {
		instruction->source_immediate = instruction->op1_const;
	}
}


/**
 * Handle a right shift operation. This helper will determine if we
 * need an arithmetic or a logical right shift dependant on the operation
 */
static void handle_right_shift_instruction(instruction_t* instruction){
	//Is this a signed or unsigned instruction?
	u_int8_t is_signed = is_type_signed(instruction->assignee->type);

	//We'll also need the size of the variable
	variable_size_t size = get_type_size(instruction->assignee->type);

	switch (size) {
		case BYTE:
			if(is_signed == TRUE){
				instruction->instruction_type = SARB;
			} else {
				instruction->instruction_type = SHRB;
			}
			break;
		case WORD:
			if(is_signed == TRUE){
				instruction->instruction_type = SARW;
			} else {
				instruction->instruction_type = SHRW;
			}
			break;
		case DOUBLE_WORD:
			if(is_signed == TRUE){
				instruction->instruction_type = SARL;
			} else {
				instruction->instruction_type = SHRL;
			}
			break;
		//Everything else falls here
		default:
			if(is_signed == TRUE){
				instruction->instruction_type = SARQ;
			} else {
				instruction->instruction_type = SHRQ;
			}
			break;		
	}

	//Now we'll move over the operands
	instruction->destination_register = instruction->assignee;

	//We can have an immediate value or we can have a register
	if(instruction->op2 != NULL){
		//We will always emit the byte copy version of the source register here
		instruction->source_register = emit_byte_copy_of_variable(instruction->op2);

	//Otherwise we have an immediate source
	} else {
		instruction->source_immediate = instruction->op1_const;
	}
}


/**
 * Handle a bitwise inclusive or operation
 */
static void handle_bitwise_inclusive_or_instruction(instruction_t* instruction){
	//We need to know what size we're dealing with
	variable_size_t size = get_type_size(instruction->assignee->type);

	switch(size){
		case QUAD_WORD:
			instruction->instruction_type = ORQ;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = ORL;
			break;
		case WORD:
			instruction->instruction_type = ORW;
			break;
		case BYTE:
			instruction->instruction_type = ORB;
			break;
		default:
			break;
	}

	//And we always have a destination register
	instruction->destination_register = instruction->assignee;

	//We can have an immediate value or we can have a register
	if(instruction->op2 != NULL){
		//If we need to convert, we'll do that here
		if(is_expanding_move_required(instruction->assignee->type, instruction->op2->type) == TRUE){
			instruction->source_register = handle_expanding_move_operation(instruction, instruction->op2, instruction->assignee->type);

		//Otherwise it's a direct translation
		} else {
			instruction->source_register = instruction->op2;
		}

	//Otherwise we have an immediate source
	} else {
		instruction->source_immediate = instruction->op1_const;
	}
}


/**
 * Handle a bitwise and operation
 */
static void handle_bitwise_and_instruction(instruction_t* instruction){
	//We need to know what size we're dealing with
	variable_size_t size = get_type_size(instruction->assignee->type);

	switch(size){
		case QUAD_WORD:
			instruction->instruction_type = ANDQ;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = ANDL;
			break;
		case WORD:
			instruction->instruction_type = ANDW;
			break;
		case BYTE:
			instruction->instruction_type = ANDB;
			break;
		default:
			break;
	}

	//And we always have a destination register
	instruction->destination_register = instruction->assignee;

	//We can have an immediate value or we can have a register
	if(instruction->op2 != NULL){
		//If we need to convert, we'll do that here
		if(is_expanding_move_required(instruction->assignee->type, instruction->op2->type) == TRUE){
			instruction->source_register = handle_expanding_move_operation(instruction, instruction->op2, instruction->assignee->type);

		//Otherwise it's a direct translation
		} else {
			instruction->source_register = instruction->op2;
		}

	//Otherwise we have an immediate source
	} else {
		instruction->source_immediate = instruction->op1_const;
	}
}


/**
 * Handle a bitwise exclusive or operation
 */
static void handle_bitwise_exclusive_or_instruction(instruction_t* instruction){
	//We need to know what size we're dealing with
	variable_size_t size = get_type_size(instruction->assignee->type);

	switch(size){
		case QUAD_WORD:
			instruction->instruction_type = XORQ;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = XORL;
			break;
		case WORD:
			instruction->instruction_type = XORW;
			break;
		case BYTE:
			instruction->instruction_type = XORB;
			break;
		default:
			break;
	}
	
	//And we always have a destination register
	instruction->destination_register = instruction->assignee;

	//We can have an immediate value or we can have a register
	if(instruction->op2 != NULL){
		//If we need to convert, we'll do that here
		if(is_expanding_move_required(instruction->assignee->type, instruction->op2->type) == TRUE){
			instruction->source_register = handle_expanding_move_operation(instruction, instruction->op2, instruction->assignee->type);

		//Otherwise it's a direct translation
		} else {
			instruction->source_register = instruction->op2;
		}

	//Otherwise we have an immediate source
	} else {
		instruction->source_immediate = instruction->op1_const;
	}
}


/**
 * Handle a cmp operation. This is used whenever we have
 * relational operation
 */
static void handle_cmp_instruction(instruction_t* instruction){
	//Determine what our size is off the bat
	variable_size_t size = get_type_size(instruction->assignee->type);

	//Select this instruction
	instruction->instruction_type = select_cmp_instruction(size);
	
	//Since we have a comparison instruction, we don't actually have a destination
	//register as the registers remain unmodified in this event
	if(is_expanding_move_required(instruction->assignee->type, instruction->op1->type) == TRUE){
		//Let the helper deal with it
		instruction->source_register = handle_expanding_move_operation(instruction, instruction->op1, instruction->assignee->type);
	} else {
		//Otherwise we assign directly
		instruction->source_register = instruction->op1;
	}

	//If we have op2, we'll use source_register2
	if(instruction->op2 != NULL){
		if(is_expanding_move_required(instruction->assignee->type, instruction->op2->type) == TRUE){
			//Let the helper deal with it
			instruction->source_register2 = handle_expanding_move_operation(instruction, instruction->op2, instruction->assignee->type);
		} else {
			//Otherwise we assign directly
			instruction->source_register2 = instruction->op2;
		}

	//Otherwise we have a constant source
	} else {
		//Otherwise we use an immediate value
		instruction->source_immediate = instruction->op1_const;
	}
}


/**
 * Handle a subtraction operation
 */
static void handle_subtraction_instruction(instruction_t* instruction){
	//Determine what our size is off the bat
	variable_size_t size = get_type_size(instruction->assignee->type);

	//Select the appropriate level of minus instruction
	instruction->instruction_type = select_sub_instruction(size);

	//Again we just need the source and dest registers
	instruction->destination_register = instruction->assignee;

	//If we have a register value, we add that
	if(instruction->op2 != NULL){
		//Do we need any kind of type conversion here? If so we'll do that now
		if(is_expanding_move_required(instruction->assignee->type, instruction->op2->type) == TRUE){
			//If this is needed, we'll let the helper do it
			instruction->source_register = handle_expanding_move_operation(instruction, instruction->op2, instruction->assignee->type);

		//Otherwise let this deal with it
		} else {
			instruction->source_register = instruction->op2;
		}

	} else {
		//Otherwise grab the immediate source
		instruction->source_immediate = instruction->op1_const;
	}
}


/**
 * Handle an addition operation
 *
 * There are 2 varieties of addition instructions, we can split based on if
 * op1 and assignee are the same
 *
 * CASE 1:
 * t23 <- t23 + 34
 * addl $34, t23
 */
static void handle_addition_instruction(instruction_t* instruction){
	//Determine what our size is off the bat
	variable_size_t size = get_type_size(instruction->assignee->type);

	//Grab the add instruction that we want
	instruction->instruction_type = select_add_instruction(size);

	//We'll just need to set the source immediate and destination register
	instruction->destination_register = instruction->assignee;

	//If we have a register value, we add that
	if(instruction->op2 != NULL){
		//Do we need a type conversion? If so, we'll do that here
		if(is_expanding_move_required(instruction->assignee->type, instruction->op2->type) == TRUE){
			//Let the helper function deal with this
			instruction->source_register = handle_expanding_move_operation(instruction, instruction->op2, instruction->assignee->type);

		//Otherwise we'll just do this
		} else {
			instruction->source_register = instruction->op2;
		}

	} else {
		//Otherwise grab the immediate source
		instruction->source_immediate = instruction->op1_const;
	}
}


/**
 * Handle the case where we have different assignee and op1 values
 * 
 * CASE 2:
 * t25 <- t15 + t17
 * leal (t15, t17), t25
 */
static void handle_addition_instruction_lea_modification(instruction_t* instruction){
	//Determines what instruction to use
	variable_size_t size = get_type_size(instruction->assignee->type);

	//Now we'll get the appropriate lea instruction
	instruction->instruction_type = select_lea_instruction(size);

	//We always know what the destination register will be
	instruction->destination_register = instruction->assignee;

	//We always have this
	instruction->address_calc_reg1 = instruction->op1;

	//Now, we'll need to set the appropriate address calculation mode based
	//on what we're given
	//If we have op2, we'll have 2 registers
	if(instruction->statement_type == THREE_ADDR_CODE_BIN_OP_STMT){
		//2 registers in this case
		instruction->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_ONLY;

		//Extract for analysis
		three_addr_var_t* addresss_calc_reg2 = instruction->op2;

		//Does this adhere to the same type as reg1? It must, so if it does not we will force it
		//to 
		if(is_expanding_move_required(instruction->address_calc_reg1->type, addresss_calc_reg2->type) == TRUE){
			//Let the helper deal with it
			addresss_calc_reg2 = handle_expanding_move_operation(instruction, addresss_calc_reg2, instruction->address_calc_reg1->type);
		}

		//Whether or not a type conversion happened, we can assign this here
		instruction->address_calc_reg2 = addresss_calc_reg2;

	//Otherise it's just an offset(bin_op_with_const)
	} else {
		//We'll just have an offset here
		instruction->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;
		//This is definitely not 0 if we're here
		instruction->offset = instruction->op1_const;
	}
}


/**
 * Handle an unsigned multiplication operation
 *
 * Because of the extra instructions that this will generate, this will count as
 * a multiple instruction selection pattern
 *
 * x <- a * 2;
 *
 * mov $3, %rax <- Source is always in RAX
 * mull %rcx -> result in rax
 *
 *
 * NOTE: this is always the first instruction in the instruction window
*/
static void handle_unsigned_multiplication_instruction(instruction_window_t* window){
	//Instruction 1 is the multiplication instruction
	instruction_t* multiplication_instruction = window->instruction1;

	//We'll need to know the variables size
	variable_size_t size = get_type_size(multiplication_instruction->assignee->type);

	//A temp holder for the final second source variable
	three_addr_var_t* source;
	three_addr_var_t* source2;

	//If we need to convert, we'll do that here
	if(is_expanding_move_required(multiplication_instruction->assignee->type, multiplication_instruction->op2->type) == TRUE){
		//Let the helper deal with it
		source2 = handle_expanding_move_operation(multiplication_instruction, multiplication_instruction->op2, multiplication_instruction->assignee->type);

	//Otherwise this can be moved directly
	} else {
		//We first need to move the first operand into RAX
		instruction_t* move_to_rax = emit_movX_instruction(emit_temp_var(multiplication_instruction->op2->type), multiplication_instruction->op2);

		//Insert the move to rax before the multiplication instruction
		insert_instruction_before_given(move_to_rax, multiplication_instruction);

		//This is just the destination register here
		source2 = move_to_rax->destination_register;
	}

	//Let's also check is any conversions are needed for the first source register
	if(is_expanding_move_required(multiplication_instruction->assignee->type, multiplication_instruction->op1->type) == TRUE){
		source = handle_expanding_move_operation(multiplication_instruction, multiplication_instruction->op1, multiplication_instruction->assignee->type);

	//Otherwise we'll just assign this to be op1
	} else {
		source = multiplication_instruction->op1;
	}

	
	//We determine the instruction that we need based on signedness and size
	switch (size) {
		case BYTE:
			multiplication_instruction->instruction_type = MULB;
			break;
		case WORD:
			multiplication_instruction->instruction_type = MULW;
			break;
		case DOUBLE_WORD:
			multiplication_instruction->instruction_type = MULL;
			break;
		//Everything else falls here
		default:
			multiplication_instruction->instruction_type = MULQ;
			break;
	}

	//This is the case where we have two source registers
	multiplication_instruction->source_register = source;
	//The other source register is in RAX
	multiplication_instruction->source_register2 = source2;

	//This is the assignee, we just don't see it
	multiplication_instruction->destination_register = emit_temp_var(multiplication_instruction->assignee->type);

	//Once we've done all that, we need one final movement operation
	instruction_t* result_movement = emit_movX_instruction(multiplication_instruction->assignee, multiplication_instruction->destination_register);

	//Insert the result movement instruction to be after the multiplication operation
	insert_instruction_after_given(result_movement, multiplication_instruction);

	//We now need to reset the window here
	reconstruct_window(window, result_movement);
}


/**
 * Handle a multiplication operation
 *
 * A multiplication operation can be different based on size and sign
 */
static void handle_signed_multiplication_instruction(instruction_t* instruction){
	//We'll need to know the variables size
	variable_size_t size = get_type_size(instruction->assignee->type);

	//We determine the instruction that we need based on signedness and size
	switch (size) {
		case BYTE:
			instruction->instruction_type = IMULB;
			break;
		case WORD:
			instruction->instruction_type = IMULW;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = IMULL;
			break;
		//Everything else falls here
		default:
			instruction->instruction_type = IMULQ;
			break;
	}

	//Following this, we'll set the assignee and source
	instruction->destination_register = instruction->assignee;

	//Are we using an immediate or register?
	if(instruction->op2 != NULL){
		//Do we need a type conversion here?
		if(is_expanding_move_required(instruction->assignee->type, instruction->op2->type) == TRUE){
			//Let the helper deal with it
			instruction->source_register = handle_expanding_move_operation(instruction, instruction->op2, instruction->assignee->type);

		//Otherwise assign directly
		} else {
			//This is the case where we have a source register
			instruction->source_register = instruction->op2;
		}
		
	} else {
		//In this case we'll have an immediate source
		instruction->source_immediate = instruction->op1_const;
	}
}


/**
 * Handle a division operation
 *
 * t4 <- t2 / t3 
 *
 * Will become:
 * movl t2, t5(rax)
 * cltd
 * idivl t3(divide by t3, we already guarantee this is a temp var(register))
 * movl t5, t4 (rax has quotient)
 * 
 * As such, this will generate additional instructions for us, making it not
 * a "single instruction" pattern
 *
 * NOTE: We guarantee that the instruction we're after is always the first
 * instruction in the window
 */
static void handle_division_instruction(instruction_window_t* window){
	//Firstly, the instruction that we're looking for is the very first one
	instruction_t* division_instruction = window->instruction1;

	//A temp holder for the final second source variable
	three_addr_var_t* source;
	three_addr_var_t* source2;

	//If we need to convert, we'll do that here
	if(is_expanding_move_required(division_instruction->assignee->type, division_instruction->op1->type) == TRUE){
		//Let the helper deal with it
		source = handle_expanding_move_operation(division_instruction, division_instruction->op1, division_instruction->assignee->type);

	//Otherwise this can be moved directly
	} else {
		//We first need to move the first operand into RAX
		instruction_t* move_to_rax = emit_movX_instruction(emit_temp_var(division_instruction->op1->type), division_instruction->op1);

		//Insert the move to rax before the multiplication instruction
		insert_instruction_before_given(move_to_rax, division_instruction);

		//This is just the destination register here
		source = move_to_rax->destination_register;
	}

	//Let's determine signedness
	u_int8_t is_signed = is_type_signed(division_instruction->assignee->type);

	//Now, we'll need the appropriate extension instruction *if* we're doing signed division
	if(is_signed == TRUE){
		//Emit the cl instruction
		instruction_t* cl_instruction = emit_conversion_instruction(source);

		//Insert this before the given
		insert_instruction_before_given(cl_instruction, division_instruction);
	}

	//Do we need to do a type conversion? If so, we'll do a converting move here
	if(is_expanding_move_required(division_instruction->assignee->type, division_instruction->op2->type) == TRUE){
		source2 = handle_expanding_move_operation(division_instruction, division_instruction->op2, division_instruction->assignee->type);

	//Otherwise source 2 is just the op2
	} else {
		source2 = division_instruction->op2;
	}

	//Now we should have what we need, so we can emit the division instruction
	instruction_t* division = emit_div_instruction(division_instruction->assignee, source2, source, is_signed);

	//Insert this before the division instruction
	insert_instruction_before_given(division, division_instruction);

	//This is the assignee, we just don't see it
	division->destination_register = emit_temp_var(division_instruction->assignee->type);

	//Once we've done all that, we need one final movement operation
	instruction_t* result_movement = emit_movX_instruction(division_instruction->assignee, division->destination_register);

	//Insert this before the original division instruction
	insert_instruction_before_given(result_movement, division_instruction);

	//Delete the division instruction
	delete_statement(division_instruction);

	//Reconstruct the window here
	reconstruct_window(window, result_movement);
}


/**
 * Handle a modulus(remainder) operation
 * Handle a division operation
 *
 * t3 <- t4 % t5
 *
 * Will become:
 * movl t4, t6 (rax)
 * cltd
 * idivl t5
 * t3 <- t7 (rdx has remainder)
 *
 * As such, this will generate additional instructions for us, making it not
 * a "single instruction" pattern
 *
 * NOTE: We guarantee that the instruction we're after is always the first
 * instruction in the window
 */
static void handle_modulus_instruction(instruction_window_t* window){
	//Firstly, the instruction that we're looking for is the very first one
	instruction_t* modulus_instruction = window->instruction1;

	//A temp holder for the final second source variable
	three_addr_var_t* source;
	three_addr_var_t* source2;

	//If we need to convert, we'll do that here
	if(is_expanding_move_required(modulus_instruction->assignee->type, modulus_instruction->op1->type) == TRUE){
		//Let the helper deal with it
		source = handle_expanding_move_operation(modulus_instruction, modulus_instruction->op1, modulus_instruction->assignee->type);

	//Otherwise this can be moved directly
	} else {
		//We first need to move the first operand into RAX
		instruction_t* move_to_rax = emit_movX_instruction(emit_temp_var(modulus_instruction->op1->type), modulus_instruction->op1);

		//Insert the move to rax before the multiplication instruction
		insert_instruction_before_given(move_to_rax, modulus_instruction);

		//This is just the destination register here
		source = move_to_rax->destination_register;
	}

	//Let's determine signedness
	u_int8_t is_signed = is_type_signed(modulus_instruction->assignee->type);

	//Now, we'll need the appropriate extension instruction *if* we're doing signed division
	if(is_signed == TRUE){
		//Emit the cl instruction
		instruction_t* cl_instruction = emit_conversion_instruction(source);

		//Insert this before the mod instruction
		insert_instruction_before_given(cl_instruction, modulus_instruction);
	}

	//Do we need to do a type conversion? If so, we'll do a converting move here
	if(is_expanding_move_required(modulus_instruction->assignee->type, modulus_instruction->op2->type) == TRUE){
		source2 = handle_expanding_move_operation(modulus_instruction, modulus_instruction->op2, modulus_instruction->assignee->type);

	//Otherwise source 2 is just the op2
	} else {
		source2 = modulus_instruction->op2;
	}

	//Now we should have what we need, so we can emit the division instruction
	instruction_t* division = emit_mod_instruction(modulus_instruction->assignee, source2, source, is_signed);
	//This is the assignee, we just don't see it
	division->destination_register = emit_temp_var(modulus_instruction->assignee->type);

	//Insert this before the original modulus
	insert_instruction_before_given(division, modulus_instruction);

	//Once we've done all that, we need one final movement operation
	instruction_t* result_movement = emit_movX_instruction(modulus_instruction->assignee, division->destination_register);

	//Insert this after the original modulus
	insert_instruction_after_given(result_movement, modulus_instruction);

	//Finally we'll delete the old modulus instruction, as we no longer need it
	delete_statement(modulus_instruction);

	//Reconstruct the window starting at the result movement
	reconstruct_window(window, result_movement);
}


/**
 * We can translate a bin op statement a few different ways based on the operand
 */
static void handle_binary_operation_instruction(instruction_t* instruction){
	//Go based on what we have as the operation
	switch(instruction->op){
		/**
		 * 2 options here
		 *
		 * CASE 1:
		 * t23 <- t23 + 34
		 * addl $34, t23
		 *
		 * OR
		 *
		 * CASE 2:
		 * t25 <- t15 + t17
		 * leal t25, (t15, t17)
		 */
		case PLUS:
			//This is our first case
			if(variables_equal(instruction->op1, instruction->assignee, FALSE) == TRUE){
				//Let the helper do it
				handle_addition_instruction(instruction);
			//Otherwise we need to handle case 2
			} else {
				//Let this different version do it
				handle_addition_instruction_lea_modification(instruction);
			}

			break;

		case MINUS:
			//Let the helper do it
			handle_subtraction_instruction(instruction);
			break;
		/**
		 * NOTE: By the time that we reach here, we know that any unsigned multiplication will
		 * have already been dealt with. As such, we know for a fact that this will be the signed
		 * version
		 */
		case STAR:
			//Let the helper do it
			handle_signed_multiplication_instruction(instruction);
			break;
		//Hanlde a left shift instruction
		case L_SHIFT:
			handle_left_shift_instruction(instruction);
			break;
		//Handle a right shift operation
		case R_SHIFT:
			handle_right_shift_instruction(instruction);
			break;
		//Handle the (|) operator
		case SINGLE_OR:
			handle_bitwise_inclusive_or_instruction(instruction);
			break;
		//Handle the (&) operator in a binary operation context
		case SINGLE_AND:
			handle_bitwise_and_instruction(instruction);
			break;
		case CARROT:
			handle_bitwise_exclusive_or_instruction(instruction);
			break;
		//All of these instructions require us to use the CMP or CMPQ command
		case DOUBLE_EQUALS:
		case NOT_EQUALS:
		case G_THAN:
		case G_THAN_OR_EQ:
		case L_THAN:
		case L_THAN_OR_EQ:
			//Let the helper do it
			handle_cmp_instruction(instruction);
			break;
		default:
			break;
	}
}


/**
 * Handle an increment statement
 */
static void handle_inc_instruction(instruction_t* instruction){
	//Determine the size of the variable we need
	variable_size_t size = get_type_size(instruction->assignee->type);

	//If it's a quad word, there's a different instruction to use. Otherwise
	//it's just a regular inc
	switch(size){
		case QUAD_WORD:
			instruction->instruction_type = INCQ;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = INCL;
			break;
		case WORD:
			instruction->instruction_type = INCW;
			break;
		case BYTE:
			instruction->instruction_type = INCB;
			break;
		//We shouldn't get here, but if we do, make it INCQ
		default:
			instruction->instruction_type = INCQ;
			break;
	}

	//Set the destination as the assignee
	instruction->destination_register = instruction->assignee;
}

/**
 * Handle a decrement statement
 */
static void handle_dec_instruction(instruction_t* instruction){
	//Determine the size of the variable we need
	variable_size_t size = get_type_size(instruction->assignee->type);

	//If it's a quad word, there's a different instruction to use. Otherwise
	//it's just a regular inc
	switch(size){
		case QUAD_WORD:
			instruction->instruction_type = DECQ;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = DECL;
			break;
		case WORD:
			instruction->instruction_type = DECW;
			break;
		case BYTE:
			instruction->instruction_type = DECB;
			break;
		//We shouldn't get here, but if we do, make it INCQ
		default:
			instruction->instruction_type = DECQ;
			break;
	}

	//Set the destination as the assignee
	instruction->destination_register = instruction->assignee;
}


/**
 * Handle the case where we have a constant to register move
 */
static void handle_constant_to_register_move_instruction(instruction_t* instruction){
	//Select the destination size first
	variable_size_t size;

	//If this has a 0 indirection, we'll use it's size
	if(instruction->assignee->indirection_level == 0){
		size = get_type_size(instruction->assignee->type);
	//Otherwise take the constant's size
	} else {
		size = get_type_size(instruction->op1_const->type);
	}

	//Select based on size
	instruction->instruction_type = select_move_instruction(size);
	
	//We've already set the sources, now we set the destination as the assignee
	instruction->destination_register = instruction->assignee;
	//Set the source immediate here
	instruction->source_immediate = instruction->op1_const;

	//Handle the indirection levels here if we have a deref only case
	if(instruction->destination_register->indirection_level > 0){
		instruction->indirection_level = instruction->destination_register->indirection_level;
		instruction->calculation_mode = ADDRESS_CALCULATION_MODE_DEREF_ONLY_DEST;
	} 
}


/**
 * Handle a register to register move condition
 */
static void handle_register_to_register_move_instruction(instruction_t* instruction){
	//We have both a destination and source size to look at here
	variable_size_t destination_size;
	variable_size_t source_size;

	//Are these being dereferenced?
	u_int8_t assignee_is_deref = TRUE;
	u_int8_t op1_is_deref = TRUE;

	//If it's 0(most common), we don't need to do anything fancy
	if(instruction->assignee->indirection_level == 0){
		//Use the standard function here
		destination_size = get_type_size(instruction->assignee->type);
		//Set the flag
		assignee_is_deref = FALSE;
	}

	//If it's 0(most common), we don't need to do anything fancy
	if(instruction->op1->indirection_level == 0){
		//Use the standard function here
		source_size = get_type_size(instruction->op1->type);
		//Set the flag
		op1_is_deref = FALSE;
	}

	//Set the sources and destinations
	instruction->destination_register = instruction->assignee;
	instruction->source_register = instruction->op1;

	//If they both are not derferenced(most common), we'll invoke the
	//moving helper
	if(assignee_is_deref == FALSE && op1_is_deref == FALSE){
		//Use the helper to get the right sized move instruction
		instruction->instruction_type = select_register_movement_instruction(destination_size, source_size, is_type_signed(instruction->assignee->type));

	//If the assignee is being dereferenced, we'll need to rely on the souce
	} else if(assignee_is_deref == TRUE && op1_is_deref == FALSE){
		instruction->instruction_type = select_move_instruction(source_size);

	//Final case - the source is being derferenced. We'll need to rely on the destination
	} else if(assignee_is_deref == FALSE && op1_is_deref == TRUE){
		instruction->instruction_type = select_move_instruction(destination_size);
	}

	//Handle the indirection levels here if we have a deref only case
	if(instruction->destination_register->indirection_level > 0){
		instruction->indirection_level = instruction->destination_register->indirection_level;
		instruction->calculation_mode = ADDRESS_CALCULATION_MODE_DEREF_ONLY_DEST;

	} else if(instruction->source_register->indirection_level > 0){
		instruction->indirection_level = instruction->source_register->indirection_level;
		instruction->calculation_mode = ADDRESS_CALCULATION_MODE_DEREF_ONLY_SOURCE;
	}
}


/**
 * Handle a memory address assignment instruction. This instruction will take
 * the form of a lea statement, where the stack pointer is the first operand
 */
static void handle_address_assignment_instruction(instruction_t* instruction, type_symtab_t* symtab, three_addr_var_t* stack_pointer){
	//Always a leaq, we are dealing with addresses
	instruction->instruction_type = LEAQ;

	//The destination is the assignee
	instruction->destination_register = instruction->assignee;

	//The first address calculation register is the stack pointer
	instruction->address_calc_reg1 = stack_pointer;

	//Copy the source register over to op1
	instruction->source_register = instruction->op1;

	//This is just a placeholder for now - it will be occupied later on
	three_addr_const_t* constant = emit_long_constant_direct(-1, symtab);
	instruction->offset = constant;

	//This will print out with the offset only
	instruction->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;
}


/**
 * Handle a lea statement(in the three address code statement form)
 *
 * Lea statements(by the time we get here..) have the following in them:
 * op1: usually a memory address source
 * op2: the offset we're adding
 * lea_multiplicator: a multiple of 2 that we're multiplying op2 by
 */
static void handle_lea_statement(instruction_t* instruction){
	//Store address calc reg's 1 and 2
	three_addr_var_t* address_calc_reg1 = instruction->op1;
	three_addr_var_t* address_calc_reg2 = instruction->op2;
	
	//Select the size of our variable
	variable_size_t size = get_type_size(instruction->assignee->type);

	switch(size){
		case QUAD_WORD:
			instruction->instruction_type = LEAQ;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = LEAL;
			break;
		case BYTE:
		case WORD:
			instruction->instruction_type = LEAW;
			break;
		default:
			break;
	}

	//The base(address calc reg1) and index(address calc reg 2) registers must be the same type.
	//We determine that the base address is the dominating force, and takes precedence, so the address calc reg2
	//must adhere to this one's type
	if(is_expanding_move_required(address_calc_reg1->type, address_calc_reg2->type)){
		address_calc_reg2 = handle_expanding_move_operation(instruction, address_calc_reg2, address_calc_reg1->type);
	}

	//We already know what mode we'll need to use here
	instruction->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_AND_SCALE;

	//Now we can set the values
	instruction->destination_register = instruction->assignee;

	//Add op1 and op2
	instruction->address_calc_reg1 = address_calc_reg1;
	instruction->address_calc_reg2 = address_calc_reg2;

	//And the lea multiplicator is already in place..
}


/**
 *	//=========================== Logical Notting =============================
 * Although it may not seem like it, logical not is actually a multiple instruction
 * pattern
 *
 * This:
 * t9 <- logical not t9
 *
 * will become
 * test t9, t9
 * sete %al
 * movzx %al, t9
 *
 * NOTE: We know that instruction1 is the one that is a logical not instruction if we
 * get here
 */
static void handle_logical_not_instruction(cfg_t* cfg, instruction_window_t* window){
	//Let's grab the value out for convenience
	instruction_t* logical_not = window->instruction1;

	//Ensure that this one's size has been selected
	logical_not->assignee->variable_size = get_type_size(logical_not->assignee->type);

	//Now we'll need to generate three new instructions
	//First comes the test command. We're testing this against itself
	instruction_t* test_inst = emit_direct_test_instruction(logical_not->assignee, logical_not->assignee); 
	//Ensure that we set all these flags too
	test_inst->block_contained_in = logical_not->block_contained_in;
	test_inst->is_branch_ending = logical_not->is_branch_ending;

	//We'll need this type for our setne's
	generic_type_t* unsigned_int8_type = lookup_type_name_only(cfg->type_symtab, "u8")->type;

	//Now we'll set the AL register to 1 if we're equal here
	instruction_t* sete_inst = emit_sete_instruction(emit_temp_var(unsigned_int8_type));
	//Ensure that we set all these flags too
	sete_inst->block_contained_in = logical_not->block_contained_in;
	sete_inst->is_branch_ending = logical_not->is_branch_ending;

	//Finally we'll move the contents into t9
	instruction_t* movzx_instruction = emit_appropriate_move_statement(logical_not->assignee, sete_inst->destination_register);
	//Ensure that we set all these flags too
	movzx_instruction->block_contained_in = logical_not->block_contained_in;
	movzx_instruction->is_branch_ending = logical_not->is_branch_ending;

	//Preserve this before we lose it
	instruction_t* after_logical_not = logical_not->next_statement;

	//Delete the logical not statement, we no longer need it
	delete_statement(logical_not);

	//First insert the test instruction
	insert_instruction_before_given(test_inst, after_logical_not);

	//Then insert the sete instruction
	insert_instruction_before_given(sete_inst, after_logical_not);

	//Finally we insert the movzx
	insert_instruction_before_given(movzx_instruction, after_logical_not);

	//This is the new window
	reconstruct_window(window, movzx_instruction);
}


/**
 * Handle a logical OR instruction
 * 
 * t32 <- t32 || t19
 *
 * Will become:
 *
 * orq t19, t32 <---------- bitwise or
 * setne t33 <------------ if it isn't 0, we eval to TRUE(1)
 * movzx t33, t32  <-------------- move this into the result
 *
 * NOTE: We guarantee that the first instruction in the window is the one that
 * we're after in this case
 */
static void handle_logical_or_instruction(cfg_t* cfg, instruction_window_t* window){
	//Grab it out for convenience
	instruction_t* logical_or = window->instruction1;

	//Save the after instruction
	instruction_t* after_logical_or = window->instruction2;

	//Let's first emit the or instruction
	instruction_t* or_instruction = emit_or_instruction(logical_or->op1, logical_or->op2);

	//We'll need this type for our setne's
	generic_type_t* unsigned_int8_type = lookup_type_name_only(cfg->type_symtab, "u8")->type;

	//Now we need the setne instruction
	instruction_t* setne_instruction = emit_setne_instruction(emit_temp_var(unsigned_int8_type));

	//Following that we'll need the final movzx instruction
	instruction_t* movzx_instruction = emit_appropriate_move_statement(logical_or->assignee, setne_instruction->destination_register);

	//Select this one's size 
	logical_or->assignee->variable_size = get_type_size(logical_or->assignee->type);

	//Now we can delete the old logical or instruction
	delete_statement(logical_or);

	//First insert the or instruction
	insert_instruction_before_given(or_instruction, after_logical_or);

	//Then we need the setne
	insert_instruction_before_given(setne_instruction, after_logical_or);

	//And finally we need the movzx
	insert_instruction_before_given(movzx_instruction, after_logical_or);

	//Reconstruct the window starting at the movzbl
	reconstruct_window(window, movzx_instruction);
}


/**
 * Handle a logical and instruction
 *
 * t32 <- t32 && t19
 *
 * This will translate to:
 * testq t32, t32 <----- is this 0?
 * setne t33 <----------- if it's not make this one
 * testq t19, 19 <------- Is this 0?
 * setne t34 <------------ If it's not, make it a one
 * andq t33, t34 <---------- let's see if they're both 0, both 1, etc
 * movzx t34, t32 <---------- store t34 with the result
 *
 * Since this will spawn multiple instructions, it will be invoked from the multiple instruction
 * pattern selector
 * 
 * NOTE: We guarantee that the first instruction in the window is the one that we're after
 * in this case
 */
static void handle_logical_and_instruction(cfg_t* cfg, instruction_window_t* window){
	//Grab it out for convenience
	instruction_t* logical_and = window->instruction1;

	//Preserve this for ourselves
	instruction_t* after_logical_and = logical_and->next_statement;

	//Let's first emit our test instruction
	instruction_t* first_test = emit_direct_test_instruction(logical_and->op1, logical_and->op1);

	//We'll need this type for our setne's
	generic_type_t* unsigned_int8_type = lookup_type_name_only(cfg->type_symtab, "u8")->type;

	//Now we'll need a setne instruction that will set a new temp
	instruction_t* first_set = emit_setne_instruction(emit_temp_var(unsigned_int8_type));
	
	//Now we'll need the second test
	instruction_t* second_test = emit_direct_test_instruction(logical_and->op2, logical_and->op2);

	//Now the second setne
	instruction_t* second_set = emit_setne_instruction(emit_temp_var(unsigned_int8_type));

	//Now we'll need to ANDx these two values together to see if they're both 1
	instruction_t* and_inst = emit_and_instruction(first_set->destination_register, second_set->destination_register);

	//The final thing that we need is a movzx
	instruction_t* movzx_instruction = emit_appropriate_move_statement(logical_and->assignee, and_inst->destination_register);

	//Select this one's size 
	logical_and->assignee->variable_size = get_type_size(logical_and->assignee->type);

	//We no longer need the logical and statement
	delete_statement(logical_and);

	//Now we'll insert everything in order
	insert_instruction_before_given(first_test, after_logical_and);
	insert_instruction_before_given(first_set, after_logical_and);
	insert_instruction_before_given(second_test, after_logical_and);
	insert_instruction_before_given(second_set, after_logical_and);
	insert_instruction_before_given(and_inst, after_logical_and);
	insert_instruction_before_given(movzx_instruction, after_logical_and);
	
	//Reconstruct the window starting at the final move
	reconstruct_window(window, movzx_instruction);
}


/**
 * Handle a negation instruction. Very simple - all we need to do is select the suffix and
 * add it over
 */
static void handle_neg_instruction(instruction_t* instruction){
	//Find out what size we have
	variable_size_t size = get_type_size(instruction->assignee->type);

	switch(size){
		case QUAD_WORD:
			instruction->instruction_type = NEGQ;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = NEGL;
			break;
		case WORD:
			instruction->instruction_type = NEGW;
			break;
		case BYTE:
			instruction->instruction_type = NEGB;
			break;
		default:
			break;
	}

	//Now we'll just translate the assignee to be the destination(and source in this case) register
	instruction->destination_register = instruction->assignee;
}


/**
 * Handle a bitwise not(one's complement) instruction. Very simple - all we need to do is select the suffix and
 * add it over
 */
static void handle_not_instruction(instruction_t* instruction){
	//Find out what size we have
	variable_size_t size = get_type_size(instruction->assignee->type);

	switch(size){
		case QUAD_WORD:
			instruction->instruction_type = NOTQ;
			break;
		case DOUBLE_WORD:
			instruction->instruction_type = NOTL;
			break;
		case WORD:
			instruction->instruction_type = NOTW;
			break;
		case BYTE:
			instruction->instruction_type = NOTB;
			break;
		default:
			break;
	}

	//Now we'll just translate the assignee to be the destination(and source in this case) register
	instruction->destination_register = instruction->assignee;
}


/**
 * Handle a test instruction. The test instruction's op1 is acutally duplicated
 * to be both of its inputs in this case
 */
static void handle_test_instruction(instruction_t* instruction){
	//Find out what size we have
	variable_size_t size = get_type_size(instruction->assignee->type);

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

	//This actually has no real destination register, the assignee was a dummy
	//It does have 2 source registers however
	instruction->source_register = instruction->op1;
	instruction->source_register2 = instruction->op2;
}


/**
 * Select instructions that follow a singular pattern. This one single pass will run after
 * the pattern selector ran and perform one-to-one mappings on whatever is left.
 */
static void select_instruction_patterns(cfg_t* cfg, instruction_window_t* window){
	//We could see logical and/logical or
	if(is_instruction_binary_operation(window->instruction1) == TRUE){
		switch(window->instruction1->op){
			//Handle the logical and case
			case DOUBLE_AND:
				handle_logical_and_instruction(cfg, window);
				return;

			//Handle logical or
			case DOUBLE_OR:
				handle_logical_or_instruction(cfg, window);
				return;

			//Handle division
			case F_SLASH:
				//This will generate more than one instruction
				handle_division_instruction(window);
				return;

			//Handle modulus
			case MOD:	
				//This will generate more than one instruction
				handle_modulus_instruction(window);
				return;

			//If we have a multiplication *and* it's unsigned, we go here
			case STAR:
				//Only do this if we're signed
				if(is_type_signed(window->instruction1->assignee->type) == FALSE){
					//Let the helper deal with it
					handle_unsigned_multiplication_instruction(window);
					return;
				}
				break;

			default:
				break;
		}
	}

	/**
	 * If we have an assignment that follows a relational operator, we need to do appropriate
	 * setting logic. We'll do this by inserting a setX statement inbetween the relational operator
	 * and the assignment
	 */
	if(is_instruction_binary_operation(window->instruction1) == TRUE
		&& is_operator_relational_operator(window->instruction1->op) == TRUE
		&& window->instruction2->statement_type == THREE_ADDR_CODE_ASSN_STMT
		&& variables_equal(window->instruction1->assignee, window->instruction2->op1, FALSE) == TRUE){

		//Set the comparison and assignment instructions
		instruction_t* comparison = window->instruction1;
		instruction_t* assignment = window->instruction2;

		//Handle the comparison operation here
		handle_cmp_instruction(comparison);

		//We'll now need to insert inbetween here
		instruction_t* set_instruction = emit_setX_instruction(comparison->op, emit_temp_var(lookup_type_name_only(cfg->type_symtab, "u8")->type), is_type_signed(assignment->assignee->type));

		//We now also need to modify the move instruction
		//It will always be movzx
		assignment->instruction_type = MOVZX;
		//Assignee and destination are the same
		assignment->destination_register = assignment->assignee;
		//The source is now this set instruction's destination
		assignment->source_register = set_instruction->destination_register;

		//Now once we have the set instruction, we need to insert it between 1 and 2
		insert_instruction_before_given(set_instruction, assignment);

		//Reconstruct the window here, starting at the end
		reconstruct_window(window, assignment);
		return;
	}


	//============================= Address Calculation Optimization  ==============================
	//These are patterns that span multiple instructions. Often we're able to
	//condense these multiple instructions into one singular x86 instruction
	//
	//We want to ensure that we get the best possible outcome for memory movement address calculations.
	//This is where *a lot* of instructions get generated, so it's worth it to spend compilation time
	//compressing these

	/**
	 * =========================== Memory Movement Instructions =======================
	 * The to-memory case
	 *
	 * Moving from memory to a register or vice versa often presents opportunities, because
	 * we're able to make use of memory addressing mode. This will arise whenever we do
	 * a register-to-memory "store" or a memory-to-register "load". Remember, in x86 assembly
	 * we can't go from memory-to-memory, so every memory access operation will fall within
	 * this category
	 *
	 * t7 <- arr_0 + 340
	 * t8 <- t7 + arg_0 * 4
	 * (t8) <- 3
	 *
	 * Should become
	 * mov(w/l/q) $3, 340(arr_0, arg_0, 4)
	 */
	if(window->instruction1->statement_type == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT
		&& window->instruction2 != NULL
		&& window->instruction2->statement_type == THREE_ADDR_CODE_LEA_STMT
		&& is_instruction_assignment_operation(window->instruction3) == TRUE
		&& window->instruction3->assignee->indirection_level == 1
		&& window->instruction1->assignee->use_count <= 1
		&& variables_equal(window->instruction1->assignee, window->instruction2->op1, FALSE) == TRUE
		&& variables_equal(window->instruction2->assignee, window->instruction3->assignee, TRUE) == TRUE){

		//Let the helper deal with everything related to this
		handle_three_instruction_address_calc_to_memory_move(window->instruction1, window->instruction2, window->instruction3);

		//Once we're done doing this, the first 2 instructions are now useless
		delete_statement(window->instruction1);
		delete_statement(window->instruction2);

		//Reconstruct the window with instruction3 as the seed
		reconstruct_window(window, window->instruction3);
		return;
	}

	/**
	 * The from-memory case
	 *
	 * t7 <- arr_0 + 340
	 * t8 <- t7 + arg_0 * 4
	 * t9 <- (t8)
	 *
	 * Should become
	 * mov(w/l/q) 340(arr_0, arg_0, 4), t9
	 */
	if(window->instruction1->statement_type == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT
		&& window->instruction2 != NULL && window->instruction3 != NULL
		&& window->instruction2->statement_type == THREE_ADDR_CODE_LEA_STMT
		&& window->instruction3->statement_type == THREE_ADDR_CODE_ASSN_STMT
		&& window->instruction1->assignee->use_count <= 1
		&& window->instruction3->op1->indirection_level <= 1
		&& variables_equal(window->instruction1->assignee, window->instruction2->op1, FALSE) == TRUE
		&& variables_equal(window->instruction2->assignee, window->instruction3->op1, TRUE) == TRUE){

		//Let the helper deal with it
		handle_three_instruction_address_calc_from_memory_move(window->instruction1, window->instruction2, window->instruction3);

		//Once we're done doing this, the first 2 instructions are now useless
		delete_statement(window->instruction1);
		delete_statement(window->instruction2);
		
		//Reconstruct the window so that instruction3 is the start
		reconstruct_window(window, window->instruction3);
		return;
	}


	/**
	 * t26 <- arr_0 + t25
	 * t28 <- t26 + 8
	 * t29 <- (t28)
	 *
	 * Should become
	 * mov(w/l/q) 8(arr_0, t25), t29
	 */
	if(window->instruction2 != NULL && window->instruction3 != NULL
		&& window->instruction1->statement_type == THREE_ADDR_CODE_BIN_OP_STMT
		&& window->instruction2->statement_type == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT
		&& window->instruction3->statement_type == THREE_ADDR_CODE_ASSN_STMT
		&& window->instruction1->assignee->use_count <= 1
		&& window->instruction3->op1->indirection_level <= 1 //Only works for memory movement
		&& variables_equal(window->instruction1->assignee, window->instruction2->op1, FALSE) == TRUE
		&& variables_equal(window->instruction2->assignee, window->instruction3->op1, TRUE) == TRUE){

		//Let the helper deal with it
		handle_three_instruction_registers_and_offset_only_from_memory_move(window->instruction1, window->instruction2, window->instruction3);
		
		//Once the helper is done, we need to delete instructions 1 and 2
		delete_statement(window->instruction1);
		delete_statement(window->instruction2);

		//Reconstruct the window with instruction3 as the start
		reconstruct_window(window, window->instruction3);
		return;
	}

	
	/**
	 * t26 <- arr_0 + t25
	 * t28 <- t26 + 8
	 * (t28) <- t29
	 *
	 * Should become
	 * mov(w/l/q) t29, 8(arr_0, t25)
	 */
	if(window->instruction2 != NULL
		&& window->instruction1->statement_type == THREE_ADDR_CODE_BIN_OP_STMT
		&& window->instruction2->statement_type == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT
		&& is_instruction_assignment_operation(window->instruction3) == TRUE
		&& window->instruction3->assignee->indirection_level == 1 //Only works for memory movement
		&& window->instruction1->assignee->use_count <= 1
		&& variables_equal(window->instruction1->assignee, window->instruction2->op1, FALSE) == TRUE
		&& variables_equal(window->instruction2->assignee, window->instruction3->assignee, TRUE) == TRUE){

		//Let the helper deal with it
		handle_three_instruction_registers_and_offset_only_to_memory_move(window->instruction1, window->instruction2, window->instruction3);
		
		//Once the helper is done, we need to delete instructions 1 and 2
		delete_statement(window->instruction1);
		delete_statement(window->instruction2);

		//Reconstruct the window with instruction3 as the start
		reconstruct_window(window, window->instruction3);
		return;
	}

	 /**
	  * Handle to-memory movement with 2 operands
	  *
	  * Example:
	  * t25 <- t24 + 4
	  * (t25) <- 3
	  *
	  * Should become
	  * mov(w/l/q) 4(t24), t25
	  */
	//If we have some kind of offset calculation followed by a dereferencing assingment, we have either a 
	//register to memory or immediate to memory move. Either way, we can rewrite this using address computation mode
	if(is_instruction_binary_operation(window->instruction1) == TRUE
		&& window->instruction1->op == PLUS 
		&& is_instruction_assignment_operation(window->instruction2) == TRUE
		&& variables_equal(window->instruction1->assignee, window->instruction2->assignee, TRUE) == TRUE
		&& window->instruction1->assignee->use_count <= 1
		&& window->instruction2->assignee->indirection_level == 1){

		//Use the helper to keep things somewhat clean in here
		handle_two_instruction_address_calc_to_memory_move(window->instruction1, window->instruction2);

		//We can now delete instruction 1
		delete_statement(window->instruction1);

		//Reconstruct the window with instruction2 as the start
		reconstruct_window(window, window->instruction2);
		return;
	}


	/**
	 * ====================================== FROM MEMORY MOVEMENT ==================================
	 *
	 * Example:
	 * t43 <- oneDi32_0 + 8
	 * t44 <- (t43)
	 *
	 * should become
	 * mov(w/l/q) 8(oneDi32_0), t44
	 *
	 * Unlike the prior case, we won't need to worry about immediate source operands here
	 */
	if(window->instruction2 != NULL
		&& is_instruction_binary_operation(window->instruction1)
		&& window->instruction1->op == PLUS
		&& window->instruction2->statement_type == THREE_ADDR_CODE_ASSN_STMT
		&& variables_equal(window->instruction1->assignee, window->instruction2->op1, TRUE) == TRUE
		&& window->instruction1->assignee->use_count <= 1
		&& window->instruction2->op1->indirection_level == 1){

		//Use the helper to avoid an explosion of code here
		handle_two_instruction_address_calc_from_memory_move(window->instruction1, window->instruction2);

		//We can scrap instruction 1 now, it's useless
		delete_statement(window->instruction1);

		//Reconstruct the window with instruction2 as the start
		reconstruct_window(window, window->instruction2);
		return;
	}


	//Do we have a case where we have an indirect jump statement? If so we can handle that by condensing it into one
	if(window->instruction1->statement_type == THREE_ADDR_CODE_INDIR_JUMP_ADDR_CALC_STMT
		&& window->instruction2->statement_type == THREE_ADDR_CODE_INDIRECT_JUMP_STMT){
		//This will be flagged as an indirect jump
		window->instruction2->instruction_type = INDIRECT_JMP;

		//By default the true source is this, but we may need to emit a converting move
		three_addr_var_t* true_source = window->instruction1->op2;

		//What is the size of this source variable? It needs
		//to be 32 bits or more to avoid needing a conversion
		switch(true_source->variable_size){
			//These two mean that we're fine
			case QUAD_WORD:
			case DOUBLE_WORD:
				break;

			//Otherwise, a conversion is required
			default:
				//If it is signed, we'll want to preserve the signedness
				if(is_type_signed(true_source->type) == TRUE){
					true_source = handle_expanding_move_operation(window->instruction1, window->instruction1->op2, i32);

				//Otherwise, we'll use the unsigned version
				} else {
					true_source = handle_expanding_move_operation(window->instruction1, window->instruction1->op2, u32);
				}

				break;
		}

		//The source register is op1
		window->instruction2->source_register = true_source;

		//Store the jumping to block where the jump table is
		window->instruction2->jumping_to_block = window->instruction1->jumping_to_block;

		//We also have an "S" multiplicator factor that will always be a power of 2 stored in the lea_multiplicator
		window->instruction2->lea_multiplicator = window->instruction1->lea_multiplicator;

		//We're now able to delete instruction 1
		delete_statement(window->instruction1);

		//Reconstruct the window with instruction2 as the start
		reconstruct_window(window, window->instruction2);
		return;
	}

	//The instruction that we have here is the window's instruction 1
	instruction_t* instruction = window->instruction1;

	//Switch on whatever we have currently
	switch (instruction->statement_type) {
		//These have a helper
		case THREE_ADDR_CODE_ASSN_STMT:
			handle_register_to_register_move_instruction(instruction);
			break;
		case THREE_ADDR_CODE_LOGICAL_NOT_STMT:
			handle_logical_not_instruction(cfg, window);
			break;
		case THREE_ADDR_CODE_ASSN_CONST_STMT:
			handle_constant_to_register_move_instruction(instruction);
			break;
		case THREE_ADDR_CODE_MEM_ADDR_ASSIGNMENT:
			handle_address_assignment_instruction(instruction, cfg->type_symtab, cfg->stack_pointer);
			break;
		case THREE_ADDR_CODE_LEA_STMT:
			handle_lea_statement(instruction);
			break;
		//One-to-one mapping to nop
		case THREE_ADDR_CODE_IDLE_STMT:
			instruction->instruction_type = NOP;
			break;
		//One to one mapping here as well
		case THREE_ADDR_CODE_RET_STMT:
			instruction->instruction_type = RET;
			//We'll still store this, just in a hidden way
			instruction->source_register = instruction->op1;
			break;
		case THREE_ADDR_CODE_JUMP_STMT:
			//Let the helper do this and then leave
			select_jump_instruction(instruction);
			break;
		//Special case here - we don't change anything
		case THREE_ADDR_CODE_ASM_INLINE_STMT:
			instruction->instruction_type = ASM_INLINE;
			break;
		//The translation here takes the form of a call instruction
		case THREE_ADDR_CODE_FUNC_CALL:
			instruction->instruction_type = CALL;
			//The destination register is itself the assignee
			instruction->destination_register = instruction->assignee;
			break;
		//Similarly, an indirect function call also has it's own kind of
		//instruction
		case THREE_ADDR_CODE_INDIRECT_FUNC_CALL:
			instruction->instruction_type = INDIRECT_CALL;
			//In this case, the source register is the function name
			instruction->source_register = instruction->op1;
			//The destination register is itself the assignee
			instruction->destination_register = instruction->assignee;
			break;
			
		//Let the helper deal with this
		case THREE_ADDR_CODE_INC_STMT:
			handle_inc_instruction(instruction);
			break;
		//Again use the helper
		case THREE_ADDR_CODE_DEC_STMT:
			handle_dec_instruction(instruction);
			break;
		//Let the helper handle this one
		case THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT:
		case THREE_ADDR_CODE_BIN_OP_STMT:
			handle_binary_operation_instruction(instruction);
			break;
		//For a phi function, we perform an exact 1:1 mapping
		case THREE_ADDR_CODE_PHI_FUNC:
			//This is all we'll need
			instruction->instruction_type = PHI_FUNCTION;
			break;
		//Handle a neg statement
		case THREE_ADDR_CODE_NEG_STATEMENT:
			//Let the helper do it
			handle_neg_instruction(instruction);
			break;
		//Handle a neg statement
		case THREE_ADDR_CODE_BITWISE_NOT_STMT:
			//Let the helper do it
			handle_not_instruction(instruction);
			break;

		//Handle the testing statement
		case THREE_ADDR_CODE_TEST_STMT:
			//Let the helper do it
			handle_test_instruction(instruction);
			break;
			
		default:
			break;
	}
}


/**
 * Run through every block and convert each instruction or sequence of instructions
 * from three address code to assembly statements
 */
static void select_instructions(cfg_t* cfg, basic_block_t* head_block){
	//Save the current block here
	basic_block_t* current = head_block;

	while(current != NULL){
		//Initialize the sliding window(very basic, more to come)
		instruction_window_t window = initialize_instruction_window(current);

		//Run through the window so long as we are not at the end
		do{
			//Select the instructions
			select_instruction_patterns(cfg, &window);

			//Slide the window
			slide_window(&window);

		//Keep going if we aren't at the end
		} while(window.instruction1 != NULL);

		//Advance the current up
		current = current->direct_successor;
	}
}


/**
 * Does the block that we're passing in end in a direct(jmp) jump to
 * the very next block. If so, we'll return what block the jump goes to.
 * If not, we'll return null.
 */
static basic_block_t* does_block_end_in_jump(basic_block_t* block){
	//Initially we have a NULL here
	basic_block_t* jumps_to = NULL;

	//If we have an exit statement that is a direct jump, then we've hit our match
	if(block->exit_statement != NULL && block->exit_statement->statement_type == THREE_ADDR_CODE_JUMP_STMT
	 && block->exit_statement->jump_type == JUMP_TYPE_JMP){
		jumps_to = block->exit_statement->jumping_to_block;
	}

	//Give back whatever we found
	return jumps_to;
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
static u_int8_t is_power_of_2(int64_t value){
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
 * Take the binary logarithm of something that we already know
 * is a power of 2. 
 *
 * IMPORTANT: This function will *only* work with values that are already
 * known to be powers of 2. If you pass in something that isn't a power of 2,
 * the answer *will* be wrong
 *
 * Here's how this works:
 * Take 8: 1000, which is 2^3
 *
 * 1000 >> 1 = 0100 != 1, current power: 1
 * 0100 >> 1 = 0010 != 1, current power: 2 
 * 0010 >> 1 = 0001 != 1, current power: 3 
 *
 */
static u_int32_t log2_of_known_power_of_2(u_int64_t value){
	//Store a counter, initialize to 0
	u_int32_t counter = 0;

	//So long as we can shift to the left
	while(value != 1){
		//One more power here
		counter++;
		//Go back by 1
		value = value >> 1;
	}

	return counter;
}


/**
 * Take in a constant and update it with its binary log value
 */
static void update_constant_with_log2_value(three_addr_const_t* constant){
	//Switch based on the type
	switch(constant->const_type){
		case INT_CONST:
		case INT_CONST_FORCE_U:
			constant->constant_value.integer_constant = log2_of_known_power_of_2(constant->constant_value.integer_constant);
			break;

		case LONG_CONST:
		case LONG_CONST_FORCE_U:
			constant->constant_value.long_constant = log2_of_known_power_of_2(constant->constant_value.long_constant);
			break;

		case CHAR_CONST:
			constant->constant_value.char_constant = log2_of_known_power_of_2(constant->constant_value.char_constant);
			break;

		//We should never get here
		default:
			break;
	}
}


/**
 * Remediate the stack address issues that may have been caused by the previous optimization
 * step
 */
static void remediate_stack_address(cfg_t* cfg, instruction_t* instruction){
	//Grab this value out. We'll need it's stack offset
	three_addr_var_t* assignee = instruction->assignee;

	//This means that there is a stack offset
	if(assignee->stack_offset != 0){
		//We'll need to ensure that this is an addition statement
		instruction->statement_type = THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT;
		instruction->op = PLUS;

		//Now we'll need to either make a three address constant or update
		//the existing one
		if(instruction->op1_const != NULL){
			instruction->op1_const->constant_value.integer_constant = assignee->stack_offset;
		} else {
			instruction->op1_const = emit_int_constant_direct(assignee->stack_offset, cfg->type_symtab);
		}

	//Otherwise it's just the RSP value
	} else {
		//This is just an assignment statement then
		instruction->statement_type = THREE_ADDR_CODE_ASSN_STMT;
	}
}


/**
 * The pattern optimizer takes in a window and performs hyperlocal optimzations
 * on passing instructions. If we do end up deleting instructions, we'll need
 * to take care with how that affects the window that we take in
 */
static u_int8_t simplify_window(cfg_t* cfg, instruction_window_t* window){
	//By default, we didn't change anything
	u_int8_t changed = FALSE;

	//Let's perform some quick checks. If we see a window where the first instruction
	//is NULL or the second one is NULL, there's nothing we can do. We'll just leave in this
	//case
	if(window->instruction1 == NULL || window->instruction2 == NULL){
		return changed;
	}

	//Now we'll match based off of a series of patterns. Depending on the pattern that we
	//see, we perform one small optimization
	
	/**
	 * ================== CONSTANT ASSINGNMENT FOLDING ==========================
	 *
	 * If we see something like this
	 * t2 <- 0x8
	 * x0 <- t2
	 *
	 * We can "fold" to result in:
	 * x0 <- 0x8
	 * 
	 * This will also result in the deletion of the first statement
	 */

	//If we see a constant assingment first and then we see a an assignment
	if(window->instruction1->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT 
	 	&& window->instruction2->statement_type == THREE_ADDR_CODE_ASSN_STMT){
		
		//If the first assignee is what we're assigning to the next one, we can fold. We only do this when
		//we deal with temp variables. At this point in the program, all non-temp variables have been
		//deemed important, so we wouldn't want to remove their assignments
		if(window->instruction1->assignee->is_temporary == TRUE &&
			//Verify that this is not used more than once
			window->instruction1->assignee->use_count <= 1 &&
			variables_equal(window->instruction1->assignee, window->instruction2->op1, FALSE) == TRUE){
			//Grab this out for convenience
			instruction_t* binary_operation = window->instruction2;

			//Now we'll modify this to be an assignment const statement
			binary_operation->op1_const = window->instruction1->op1_const;

			//Modify the type of the assignment
			binary_operation->statement_type = THREE_ADDR_CODE_ASSN_CONST_STMT;

			//The use count here now goes down by one
			binary_operation->op1->use_count--;

			//Make sure that we now null out op1
			binary_operation->op1 = NULL;

			//Once we've done this, the first statement is entirely useless
			delete_statement(window->instruction1);

			//Once we've deleted the statement, we'll need to completely rewire the block
			//The binary operation is now the start
			reconstruct_window(window, binary_operation);
		
			//Whatever happened here, we did change something
			changed = TRUE;
		}
	}

	//This is the same case as above, we'll just now check instructions 2 and 3
	if(window->instruction2 != NULL && window->instruction2->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT 
	 	&& window->instruction3 != NULL && window->instruction3->statement_type == THREE_ADDR_CODE_ASSN_STMT){

		//If the first assignee is what we're assigning to the next one, we can fold. We only do this when
		//we deal with temp variables. At this point in the program, all non-temp variables have been
		//deemed important, so we wouldn't want to remove their assignments
		if(window->instruction2->assignee->is_temporary == TRUE &&
			//Verify that this is not used more than once
			window->instruction2->assignee->use_count <= 1 &&
			variables_equal(window->instruction2->assignee, window->instruction3->op1, FALSE) == TRUE){
			//Grab this out for convenience
			instruction_t* binary_operation = window->instruction3;

			//Now we'll modify this to be an assignment const statement
			binary_operation->op1_const = window->instruction2->op1_const;

			//The use count for op1 now goes down by 1
			binary_operation->op1->use_count--;

			//Make sure that we now NULL out the first non-const operand for the future
			binary_operation->op1 = NULL;
			
			//Modify the type of the assignment
			binary_operation->statement_type = THREE_ADDR_CODE_ASSN_CONST_STMT;

			//Once we've done this, the first statement is entirely useless
			delete_statement(window->instruction2);

			//We'll need to reconstruct the window. Instruction 1 is still the start
			reconstruct_window(window, window->instruction3);

			//Whatever happened here, we did change something
			changed = TRUE;
		}
	}


	/**
	 * ================= Handling redundant multiplications ========================
	 * t27 <- 5
	 * t27 <- t27 * 68
	 *
	 * Can become: t27 <- 340
	 */
	if(window->instruction1->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT 
		&& window->instruction2 != NULL
		&& window->instruction2->statement_type == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT
		&& window->instruction2->op == STAR
		&& window->instruction1->assignee->is_temporary == TRUE
		&& variables_equal(window->instruction2->op1, window->instruction1->assignee, FALSE) == TRUE){

		//We can multiply the constants now. The result will be stored in op1 const
		multiply_constants(window->instruction2->op1_const, window->instruction1->op1_const);

		//Instruction 2 is now simply an assign const statement
		window->instruction2->statement_type = THREE_ADDR_CODE_ASSN_CONST_STMT;

		//Op1 is now used one less time
		window->instruction2->op1->use_count--;

		//Null out where the old value was
		window->instruction2->op1 = NULL;

		//Instruction 1 is now completely useless *if* that was the only time that
		//his assignee was used. Otherwise, we need to keep it in
		if(window->instruction1->assignee->use_count == 0){
			delete_statement(window->instruction1);
		}

		//Reconstruct the window with instruction 2 as the start
		reconstruct_window(window, window->instruction2);

		//This counts as a change
		changed = TRUE;
	}


	/**
	 * --------------------- Redundnant copying elimination ------------------------------------
	 *  Let's now fold redundant copies. Here is an example of a redundant copy
	 * 	t10 <- x_2
	 * 	t11 <- t10
	 *
	 * This can be folded into simply:
	 * 	t11 <- x_2
	 *
	 * HOWEVER: There's a special case where we can't do this
	 * t30 <- (t29)
	 * (t25) <- t30
	 * 
	 * (t25) <- (t29) <--------- WRONG! memory-to-memory moves are impossible!
	 * So we'll need to ensure that we aren't doing this optimization if op1 of instruction1 is an indirect value
	 *
	 * t16 <- arr_0
	 * t17 <- (t16)
	 *
	 * t17 <- (arr_0)
	 *
	 *  | First->op1 | second->assignee | can_combine
	 *  | 1			 | 1				| 0
	 *  | 1   		 | 0 				| 1
	 *  | 0 		 | 1			    | 1
	 *  | 0 		 | 0 				| 1
	 *
	 * So we'll use XOR for this one
	 * 
	 *  | First->assignee | second->op1 | can_combine
	 *  | 1			 	  | 1				| 0
	 *  | 1   		 	  | 0 				| 1
	 *  | 0 		  	  | 1			    | 1
	 *  | 0 		  	  | 0 				| 1
	 *
	 * So we'll use XOR for this one as well
	 * 
	 */
	//If we have two consecutive assignment statements
	if(window->instruction2 != NULL 
		&& window->instruction2->statement_type == THREE_ADDR_CODE_ASSN_STMT 
		&& window->instruction1->statement_type == THREE_ADDR_CODE_ASSN_STMT
		&& can_assignment_instruction_be_removed(window->instruction1) == TRUE
		&& can_assignment_instruction_be_removed(window->instruction2) == TRUE){
		//Grab these out for convenience
		instruction_t* first = window->instruction1;
		instruction_t* second = window->instruction2;
		
		//If the variables are temp and the first one's assignee is the same as the second's op1, we can fold
		if(first->assignee->is_temporary == TRUE && variables_equal(first->assignee, second->op1, TRUE) == TRUE
			//And the assignee of the first statement is only ever used once
			&& first->assignee->use_count <= 1){
			//Now let's check for any indirection level violations that we need to account for
			//These both can't have higher indirection levels than 0
			if(!(first->op1->indirection_level > 0 && second->assignee->indirection_level > 0)
				//Same with these
				&& !(second->op1->indirection_level > 0 && first->assignee->indirection_level > 0)
				//We also can't combine these
				&& !(second->op1->indirection_level > 0 && first->op1->indirection_level > 0)){

				//Copy this over so we don't lose it
				first->op1->indirection_level += second->op1->indirection_level;

				//Manage our use state here
				replace_variable(second->op1, first->op1);

				//Reorder the op1's
				second->op1 = first->op1;

				//We can now delete the first statement
				delete_statement(first);

				//Reconstruct the window with second as the start
				reconstruct_window(window, second);
				
				//Regardless of what happened, we did see a change here
				changed = TRUE;
			}
		}
	}

	/**
	 * --------------------- Folding constant assignments in arithmetic expressions ----------------
	 *  In cases where we have a binary operation that is not a BIN_OP_WITH_CONST, but after simplification
	 *  could be, we want to eliminate unnecessary register pressure by having consts directly in the arithmetic expression 
	 *
	 * NOTE: This does not work for division or modulus instructions
	 */
	//Check first with 1 and 2
	if(window->instruction2 != NULL && window->instruction2->statement_type == THREE_ADDR_CODE_BIN_OP_STMT
		&& window->instruction1->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT){
		//Is the variable in instruction 1 temporary *and* the same one that we're using in instrution2? Let's check.
		if(window->instruction1->assignee->is_temporary == TRUE
			//Validate that the use count is less than 1
			&& window->instruction1->assignee->use_count <= 1
			&& is_operation_valid_for_constant_folding(window->instruction2) == TRUE //And it's valid for constant folding
			&& variables_equal(window->instruction1->assignee, window->instruction2->op2, FALSE) == TRUE){
			//If we make it in here, we know that we may have an opportunity to optimize. We simply 
			//Grab this out for convenience
			instruction_t* const_assignment = window->instruction1;

			//Let's mark that this is now a binary op with const statement
			window->instruction2->statement_type = THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT;

			//op2 is now used one less time
			window->instruction2->op2->use_count--;

			//We'll want to NULL out the secondary variable in the operation
			window->instruction2->op2 = NULL;
			
			//We'll replace it with the op1 const that we've gotten from the prior instruction
			window->instruction2->op1_const = const_assignment->op1_const;

			//We can now delete the very first statement
			delete_statement(window->instruction1);

			//Reconstruct the window with instruction2 as the start
			reconstruct_window(window, window->instruction2);

			//This does count as a change
			changed = TRUE;
		}
	}

	//Now check with 1 and 3. The prior compression may have made this more worthwhile
	if(window->instruction3 != NULL && window->instruction3->statement_type == THREE_ADDR_CODE_BIN_OP_STMT
		&& window->instruction1->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT){
		//Is the variable in instruction 1 temporary *and* the same one that we're using in instrution2? Let's check.
		if(window->instruction1->assignee->is_temporary == TRUE
			//Validate that this is not being used more than once
			&& window->instruction1->assignee->use_count <= 1
			&& is_operation_valid_for_constant_folding(window->instruction3) == TRUE //And it's valid for constant folding
			&& variables_equal(window->instruction2->assignee, window->instruction3->op2, FALSE) == FALSE
			&& variables_equal(window->instruction1->assignee, window->instruction3->op2, FALSE) == TRUE){
			//If we make it in here, we know that we may have an opportunity to optimize. We simply 
			//Grab this out for convenience
			instruction_t* const_assignment = window->instruction1;

			//Let's mark that this is now a binary op with const statement
			window->instruction3->statement_type = THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT;

			//Op2 for instruction3 is now used one less time
			window->instruction3->op2->use_count--;

			//We'll want to NULL out the secondary variable in the operation
			window->instruction3->op2 = NULL;
			
			//We'll replace it with the op1 const that we've gotten from the prior instruction
			window->instruction3->op1_const = const_assignment->op1_const;

			//We can now delete the very first statement
			delete_statement(window->instruction1);

			//Reconstruct the window with instruction2 as the seed
			reconstruct_window(window, window->instruction2);

			//This does count as a change
			changed = TRUE;
		}
	}


	/**
	 * ==================== Comparison expressions with unnecessary preceeding temp assignment ======================
	 *	If we have something like this:
	 *	t33 <- x_2;
	 *	t34 <- t33 < 2
	 *
	 * 	Because cmp instructions do not alter any register values, we're fine to ditch the preceeding assignment
	 * 	and rewrite like this:
	 * 	t34 <- x_2 < 2
	 *
	 */
	//Check first with 1 and 2. We need a binary operation that has a comparison operator in it
	if(is_instruction_binary_operation(window->instruction2) == TRUE
		&& window->instruction1->statement_type == THREE_ADDR_CODE_ASSN_STMT
		&& is_operator_relational_operator(window->instruction2->op) == TRUE){

		//Is the variable in instruction 1 temporary *and* the same one that we're using in instruction1? Let's check.
		if(window->instruction1->assignee->is_temporary == TRUE 
			//Make sure that this is the only use
			&& window->instruction1->assignee->use_count <= 1
			&& window->instruction1->op1->is_temporary == FALSE
			&& variables_equal(window->instruction1->assignee, window->instruction2->op1, FALSE) == TRUE){

			//Set these two to be equal
			window->instruction2->op1 = window->instruction1->op1;

			//We can now delete the very first statement
			delete_statement(window->instruction1);

			//Reconstruct the window with instruction2 as the seed
			reconstruct_window(window, window->instruction2);

			//This does count as a change
			changed = TRUE;
		}
	}


	/**
	 * -------------------- Arithmetic expressions with assignee the same as op1 ---------------------
	 *  There will be times where we generate arithmetic expressions like this:
	 *
	 * 	t19 <- a_3
	 * 	t20 <- t19 + y_0
	 * 	a_4 <- t20
	 *
	 *  Since a_4 and a_3 are the same variable(register), this is an ideal candidate to be compressed
	 *  like
	 *
	 *  a_4 <- a_3 + y_0
	 *
	 * This 3-instruction large optimizaion will look for this
	 */
	//If the first statement is an assignmehnt statement, and the second statement is a binary operation,
	//and the third statement is an assignment statement, we have our chance to optimize
	if(window->instruction1->statement_type == THREE_ADDR_CODE_ASSN_STMT
		&& is_instruction_binary_operation(window->instruction2) == TRUE
		&& window->instruction3 != NULL
		&& window->instruction3->statement_type == THREE_ADDR_CODE_ASSN_STMT){

		//Grab these out for convenience
		instruction_t* first = window->instruction1;
		instruction_t* second = window->instruction2;
		instruction_t* third = window->instruction3;

		//We still need further checks to see if this is indeed the pattern above. If
		//we survive all of these checks, we know that we're set to optimize
		if(first->assignee->is_temporary == TRUE && third->assignee->is_temporary == FALSE &&
			first->assignee->use_count <= 2 &&
			variables_equal_no_ssa(first->op1, third->assignee, FALSE) == TRUE &&
	 		variables_equal(first->assignee, second->op1, FALSE) == TRUE &&
	 		variables_equal(second->assignee, third->op1, FALSE) == TRUE){

			//Manage our use state here
			replace_variable(second->op1, first->op1);

			//The second op1 will now become the first op1
			second->op1 = first->op1;

			//And the second's assignee will now be the third's assignee
			second->assignee = third->assignee;

			//Following this, all we need to do is delete and rearrange
			delete_statement(first);
			delete_statement(third);

			//Reconstruct the window with second as the new instruction1
			reconstruct_window(window, second);

			//Regardless of what happened, we did change the window, so we'll
			//update this
			changed = TRUE;
		}
	}
	

	/**
	 * --------------------- Folding constant assingments in LEA statements ----------------------
	 *  In cases where we have a lea statement that uses a constant which is assigned to a temporary
	 *  variable right before it, we should eliminate that unnecessary assingment by folding that constant
	 *  into the lea statement.
	 *
	 *  We can also go one further by performing the said multiplication to get out the value that we want
	 *
	 * NOTE: This will actually produce invalid binary operation instructions in the short run. However, 
	 * when the instruction selector gets to them, we will turn them into memory move operations
	 */
	if(window->instruction2 != NULL && window->instruction2->statement_type == THREE_ADDR_CODE_LEA_STMT
		&& window->instruction1->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT){
		//If the first instruction's assignee is temporary and it matches the lea statement, then we have a match
		if(window->instruction1->assignee->is_temporary == TRUE &&
	 		variables_equal(window->instruction1->assignee, window->instruction2->op2, FALSE) == TRUE){

			//What we can do is rewrite the LEA statement all together as a simple addition statement. We'll
			//evaluate the multiplication of the constant and lea multiplicator at comptime
			u_int64_t address_offset = window->instruction2->lea_multiplicator;

			//Let's now grab what the constant is
			three_addr_const_t* constant = window->instruction1->op1_const;
			
			//What kind of constant do we have?
			if(constant->const_type == INT_CONST || constant->const_type == INT_CONST_FORCE_U){
				//If this is a the case, we'll multiply the address const by the int value
				address_offset *= constant->constant_value.integer_constant;
			//Otherwise, this has to be a long const
			} else {
				address_offset *= constant->constant_value.long_constant;
			}

			//Once we've done this, the address offset is now properly multiplied. We'll reuse
			//the constant from operation one, and convert the lea statement into a BIN_OP_WITH_CONST
			//statement. This saves a lot of loading and arithmetic operations
		
			//This is now a long const
			constant->const_type = LONG_CONST;

			//Set this to be the address offset
			constant->constant_value.long_constant = address_offset;

			//Add it into instruction 2
			window->instruction2->op1_const = constant;

			//We'll now transfrom instruction 2 into a bin op with const
			window->instruction2->op2 = NULL;
			window->instruction2->op = PLUS;
			window->instruction2->statement_type = THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT;

			//We can now scrap the first instruction entirely
			delete_statement(window->instruction1);

			//Reconstruct the window. Instruction 2 is the new start
			reconstruct_window(window, window->instruction2);

			//This counts as a change 
			changed = TRUE;
		}
	}
	
	/**
	 * ============================== Redundant copy folding =======================
	 * If we have some random temp assignment that's in the middle of everything, we can
	 * fold it to get rid of it
	 *
	 * t12 <- arr_0 + 476 
	 * t14 <- t12 <----------- assignment that's leftover from other simplifications
	 * (t14) <- 2
	 */
	if(window->instruction1 != NULL && window->instruction2 != NULL && window->instruction3 != NULL
	 	&& window->instruction1->assignee != NULL && window->instruction3->assignee != NULL
		&& window->instruction2->op1 != NULL
		&& window->instruction2->statement_type == THREE_ADDR_CODE_ASSN_STMT
		&& window->instruction2->cannot_be_combined == FALSE
		&& window->instruction2->assignee->is_temporary == TRUE
		&& window->instruction2->op1->is_temporary == TRUE
		&& variables_equal(window->instruction1->assignee, window->instruction2->op1, FALSE) == TRUE
		&& variables_equal(window->instruction2->assignee, window->instruction3->assignee, TRUE) == TRUE){

		//The third instruction's assignee is now going to be the first instruction's assignee
		three_addr_var_t* old_assignee = window->instruction3->assignee;

		//Emit a copy of this one
		window->instruction3->assignee = emit_var_copy(window->instruction1->assignee);

		//Now we'll be sure to update the indirection level
		window->instruction3->assignee->indirection_level = old_assignee->indirection_level;

		//We can remove the second instruction
		delete_statement(window->instruction2);

		//Reconstruct this window. Instruction 1 is still the seed
		reconstruct_window(window, window->instruction1);
		
		//This counts as a change
		changed = TRUE;

		//If we make it here we know that we have a case where this is doable
		printf("FOUND OPPORTUNITY\n");
	}



	/**
	 * =================== Adjacent assignment statement folding ====================
	 * If we have a binary operation or a bin op with const statement followed by an
	 * assignment of that result to a non-temporary variable, we have an opportunity
	 * to simplify. This would lead nicely into the following optimizations below
	 *
	 * Example: 
	 * t12 <- a_2 + 0x1
	 * a_3 <- t12
	 * could become
	 * a_3 <- a_2 + 0x1
	 */

	//If the first instruction is a binary operation and the immediately following instruction is an assignment
	//operation, this is a potential match
	if(is_instruction_binary_operation(window->instruction1) == TRUE
		&& window->instruction2 != NULL
		&& window->instruction2->statement_type == THREE_ADDR_CODE_ASSN_STMT){
		//For convenience/memory ease
		instruction_t* first = window->instruction1;
		instruction_t* second = window->instruction2;
		instruction_t* third = window->instruction3;

		//If we have a temporary start variable, a non temp end variable, and the variables
		//match in the corresponding spots, we have our opportunity
		if(first->assignee->is_temporary == TRUE && second->assignee->is_temporary == FALSE
			&& variables_equal(first->assignee, second->op1, FALSE) == TRUE
			//Special no-ssa comparison, we expect that the ssa would be different due to assignment levels
			&& variables_equal_no_ssa(second->assignee, first->op1, FALSE) == TRUE){

			//We will now take the second variables assignee to be the first statements assignee
			first->assignee = second->assignee;

			//That's really all we need to do for the first one. Now, we need to delete the second
			//statement entirely
			delete_statement(second);

			//We'll reconstruct the window here, the first is still the first
			reconstruct_window(window, first);

			//Regardless of what happened here, we did change the window so we'll set the flag
			changed = TRUE;

		//We could also have a scenario like this that will apply only to logical combination(&& and ||) operators.
		/**
		 * t33 <- t34 && t35
		 * x_0 <- t33
		 *
		 * Because of the way that we handle logical and, we can actuall eliminate the second assignment
		 * with no issue
		 * x_0 <- t34 && t35
		 *
		 * NOTE: This does not work for logical or, due to the way we handle logical OR
		 */
		} else if(first->op == DOUBLE_AND
				&& first->assignee->is_temporary == TRUE 
				&& variables_equal(first->assignee, second->op1, FALSE) == TRUE){

			//Set these to be equal
			first->assignee = second->assignee;

			//We can now scrap the second statement
			delete_statement(second);

			//We'll reconstruct the window here, the first is still the first
			reconstruct_window(window, first);

			//This counts as a change
			changed = TRUE;
		}
	}

	/**
	 * ================== Arithmetic Operation Simplifying ==========================
	 * After we do all of this folding, we can stand to ask the question of if we 
	 * have any simple arithmetic operations that can be folded together. Our first
	 * example of this is arithmetic operations with 0
	 *
	 * There are many cases here which allow simplification. Here are the first
	 * few with zero:
	 *
	 * t2 <- t4 + 0 can just become t2 <- t4
	 * t2 <- t4 - 0 can just become t2 <- t4
	 * t2 <- t4 * 0 can just become t2 <- 0 
	 * t2 <- t4 / 0 will stay the same, but we will produce an error
	 * 
	 *
	 *
	 *
	 * These may seem trivial, but this is not so uncommon when we're doing address calculation
	 */

	//Shove these all into an array for selecting
	instruction_t* instructions[3] = {window->instruction1, window->instruction2, window->instruction3};

	//The current instruction pointer
	instruction_t* current_instruction;

	//If we have a bin op with const statement, we have an opportunity
	for(u_int16_t i = 0; i < 3; i++){
		//Grab the current instruction out
		current_instruction = instructions[i];

		if(current_instruction != NULL && current_instruction->statement_type == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT){
			//Grab this out for convenience
			three_addr_const_t* constant = current_instruction->op1_const;

			//By default, we assume it's not 0
			u_int8_t const_is_0 = FALSE;
			//Is the const a 1?
			u_int8_t const_is_1 = FALSE;
			//Is the const a power of 2?
			u_int8_t const_is_power_of_2 = FALSE;

			//Switch based on the constant type
			switch(constant->const_type){
				case INT_CONST:
				case INT_CONST_FORCE_U:
					//Set the flag if we find anything
					if(constant->constant_value.integer_constant == 0){
						const_is_0 = TRUE;
					} else if (constant->constant_value.integer_constant == 1){
						const_is_1 = TRUE;
					} else {
						const_is_power_of_2 = is_power_of_2(constant->constant_value.integer_constant);
					}
					
					break;


				case LONG_CONST:
				case LONG_CONST_FORCE_U:
					//Set the flag if we find zero
					if(constant->constant_value.long_constant == 0){
						const_is_0 = TRUE;
					} else if(constant->constant_value.long_constant == 1){
						const_is_1 = TRUE;
					} else {
						const_is_power_of_2 = is_power_of_2(constant->constant_value.long_constant);
					}
					
					break;

				case CHAR_CONST:
					//Set the flag if we find zero
					if(constant->constant_value.char_constant == 0){
						const_is_0 = TRUE;
					} else if(constant->constant_value.char_constant == 1){
						const_is_1 = TRUE;
					} else {
						const_is_power_of_2 = is_power_of_2(constant->constant_value.long_constant);
					}

					break;
					

				//Just do nothing in the default case
				default:
					break;
			}

			//If this is 0, then we can optimize
			if(const_is_0 == TRUE){
				//Switch based on current instruction's op
				switch(current_instruction->op){
					//If we made it out of this conditional with the flag being set, we can simplify.
					//If this is the case, then this just becomes a regular assignment expression
					case PLUS:
					case MINUS:
						//We're just assigning here
						current_instruction->statement_type = THREE_ADDR_CODE_ASSN_STMT;
						//Wipe the values out
						current_instruction->op1_const = NULL;

						//We changed something
						changed = TRUE;

						break;

					case STAR:
						//Now we're assigning a const
						current_instruction->statement_type = THREE_ADDR_CODE_ASSN_CONST_STMT;

						//The constant is still the same thing(0), let's just wipe out the ops
						if(current_instruction->op1 != NULL){
							current_instruction->op1->use_count--;
							current_instruction->op1 = NULL;
						}
						//We changed something
						changed = TRUE;

						break;

					//Just do nothing here
					default:
						break;
				}


			//Notice how we do NOT mark any change as true here. This is because, even though yes we
			//did change the instructions, the sliding window itself did not change at all. This is
			//an important note as if we did mark a change, there are cases where this could
			//cause an infinite loop
			
			//What if this is a 1? Well if it is, we can transform this statement into an inc or dec statement
			//if it's addition or subtraction, or we can turn it into a simple assignment statement if it's multiplication
			//or division
			} else if(const_is_1 == TRUE){
				//Switch based on the op in the current instruction
				switch(current_instruction->op){
					//If it's an addition statement, turn it into an inc statement
					/**
				 	* NOTE: for addition and subtraction, since we'll be turning this into inc/dec statements, we'll
				 	* want to first make sure that the assignees are not temporary variables. If they are temporary variables,
				 	* then doing this would mess the whole operation up
				 	*/
					case PLUS:
						//If it's temporary, we jump out
						if(current_instruction->assignee->is_temporary == TRUE){
							break;
						}

						//Now turn it into an inc statement
						current_instruction->statement_type = THREE_ADDR_CODE_INC_STMT;
						//Wipe the values out
						current_instruction->op1_const = NULL;
						current_instruction->op = BLANK;
						//We changed something
						changed = TRUE;

						break;

					case MINUS:
						//If it's temporary, we jump out
						if(current_instruction->assignee->is_temporary == TRUE){
							break;
						}

						//Change what the class is
						current_instruction->statement_type = THREE_ADDR_CODE_DEC_STMT;
						//Wipe the values out
						current_instruction->op1_const = NULL;
						current_instruction->op = BLANK;
						//We changed something
						changed = TRUE;

						break;

					//These are both the same - handle a 1 multiply
					case STAR:
					case F_SLASH:
						//Change it to a regular assignment statement
						current_instruction->statement_type = THREE_ADDR_CODE_ASSN_STMT;
						//Wipe the operator out
						current_instruction->op1_const = NULL;
						current_instruction->op = BLANK;
						//We changed something
						changed = TRUE;

						break;

					//Just bail out
					default:
						break;
				}

			//What if we have a power of 2 here? For any kind of multiplication or division, this can
			//be optimized into a left or right shift if we have a compatible type(not a float)
			} else if(const_is_power_of_2 && current_instruction->assignee->type->type_class == TYPE_CLASS_BASIC 
				&& current_instruction->assignee->type->basic_type_token != F32 
				&& current_instruction->assignee->type->basic_type_token != F64){

				//If we have a star that's a left shift
				if(current_instruction->op == STAR){
					//Multiplication is a left shift
					current_instruction->op = L_SHIFT;
					//Update the constant with its log2 value
					update_constant_with_log2_value(current_instruction->op1_const);
					//We changed something
					changed = TRUE;

				} else if(current_instruction->op == F_SLASH){
					//Division is a right shift
					current_instruction->op = R_SHIFT;
					//Update the constant with its log2 value
					update_constant_with_log2_value(current_instruction->op1_const);
					//We changed something
					changed = TRUE;
				}
				//Otherwise, we don't need this
			}
		}
	}

	/**
	 * ================== Simplifying Consecutive Binary Operation with Constant statements ==============
	 * Here is an example:
	 * t2 <- arr_0 + 24
	 * t4 <- t2 + 4
	 * 
	 * We could turn this into
	 * t4 <- arr_0 + 28
	 *
	 * This is incredibly common with array address calculations, which is why we do it. We focus on the special case
	 * of two consecutive additions here for this reason. Any other two consecutive operations are usually quite uncommon
	 */
	//If instructions 1 and 2 are both BIN_OP_WITH_CONST
	if(window->instruction2 != NULL && window->instruction2->statement_type == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT
		&& window->instruction2->op == PLUS && window->instruction1->statement_type == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT 
		&& window->instruction1->op == PLUS){

		//Let's do this for convenience
		instruction_t* first = window->instruction1;
		instruction_t* second = window->instruction2;

		//Calculate this for now in case we need it
		generic_type_t* final_type = types_assignable(second->op1_const->type, first->op1_const->type);

		//If these are the same variable and the types are compatible, then we're good to go
		if(variables_equal(first->assignee, second->op1, FALSE) == TRUE && final_type != NULL){
			//What we'll do first is add the two constants. The resultant constant will be stored
			//in the second instruction's constant
			second->op1_const = add_constants(second->op1_const, first->op1_const);

			//Manage our use state here
			replace_variable(second->op1, first->op1);

			//Now that we've done that, we'll modify the second equation's op1 to be the first equation's op1
			second->op1 = first->op1;

			//Now that this is done, we can remove the first equation
			delete_statement(first);

			//We'll reconstruct the window with the second instruction being the
			//first instruction now
			reconstruct_window(window, second);

			//This counts as a change because we deleted
			changed = TRUE;
		}
	}

	/**
	 * There is a chance that we could be left with statements that assign to themselves
	 * like this:
	 *  t11 <- t11
	 *
	 *  These are guaranteed to be useless, so we can eliminate them
	 */
	if(window->instruction1 != NULL && window->instruction1->statement_type == THREE_ADDR_CODE_ASSN_STMT
		//If we get here, we have a temp assignment who is completely useless, so we delete
		&& window->instruction1->assignee->is_temporary == TRUE
		&& variables_equal(window->instruction1->assignee, window->instruction1->op1, FALSE) == TRUE){

		//Delete it
		delete_statement(window->instruction1);

		//Rebuild now based on instruction2
		reconstruct_window(window, window->instruction2);

		//Counts as a change
		changed = TRUE;
	}

	/**
	 * There is a chance that we could be left with statements that assign to themselves
	 * like this:
	 *  t11 <- 2
	 *
	 *  Where t11 has no real usage at all. Since this is the case, we can eliminate the whole
	 *  operation
	 *
	 *  These are guaranteed to be useless, so we can eliminate them
	 */
	if(window->instruction1 != NULL && window->instruction1->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT
		//If we get here, we have a temp assignment who is completely useless, so we delete
		&& window->instruction1->assignee->is_temporary == TRUE
		//Ensure that it's not being used at all
		&& window->instruction1->assignee->use_count == 0
		//We can't mess with memory movement instructions
		&& window->instruction1->assignee->indirection_level == 0){

		//Delete it
		delete_statement(window->instruction1);

		//Rebuild now based on instruction2
		reconstruct_window(window, window->instruction2);

		//Counts as a change
		changed = TRUE;
	}

	/**
	 * Final check - in the previous optimization module, there is a chance that we've deleted
	 * items in the stack that have caused our old stack addresses to be out of sync. We'll hitch
	 * a ride on this instruction crawl to remediate anything stack addresses
	 */
	if(window->instruction1->op1 != NULL && window->instruction1->op1->is_stack_pointer == TRUE){
		//Remediate the stack address
		remediate_stack_address(cfg, window->instruction1);
	}

	//Return whether or not we changed the block return changed;
	return changed;
}


/**
 * Make one pass through the sliding window for simplification. This could include folding,
 * etc. Simplification happens first over the entirety of the OIR using the sliding window
 * technique. Following this, the instruction selector runs over the same area
 */
static u_int8_t simplifier_pass(cfg_t* cfg, basic_block_t* head){
	//First we'll grab the head
	basic_block_t* current = head;

	u_int8_t window_changed = FALSE;
	u_int8_t changed;

	//So long as this isn't NULL
	while(current != NULL){
		changed = FALSE;
		
		//Initialize the sliding window(very basic, more to come)
		instruction_window_t window = initialize_instruction_window(current);

		//Run through and simplify everything we can
		do{
			//Simplify the window
			changed = simplify_window(cfg, &window);

			//Set this flag if it was changed
			if(changed == TRUE){
				window_changed = TRUE;
			}

			//And slide it
			slide_window(&window);

		//So long as we aren't at the end
		} while(window.instruction1 != NULL);


		//Advance to the direct successor
		current = current->direct_successor;
	}

	//Give back whether or not it's been changed
	return window_changed;
}


/**
 * We'll make use of a while change algorithm here. We make passes
 * until we see the first pass where we experience no chnage at all.
 */
static void simplify(cfg_t* cfg, basic_block_t* head){
	while(simplifier_pass(cfg, head) == TRUE);
}


/**
 * The first step in our instruction selector is to get the instructions stored in
 * a straight line in the exact way that we want them to be. This is done with a breadth-first
 * search traversal of the simplified CFG that has been optimized. 
 *
 * One special consideration we'll take is ordering nodes with a given jump next to eachother.
 * For example, if block .L15 ends in a direct jump to .L16, we'll endeavor to have .L16 right
 * after .L15 so that in a later stage, we can eliminate that jump.
 */
static basic_block_t* order_blocks(cfg_t* cfg){
	//We'll first wipe the visited status on this CFG
	reset_visited_status(cfg, TRUE);
	
	//We will perform a breadth first search and use the "direct successor" area
	//of the blocks to store them all in one chain
	
	//The current block
	basic_block_t* previous;
	//The starting point that all traversals will use
	basic_block_t* head_block;

	//Initialize these to null first
	previous = head_block = NULL;
	
	//We'll need to use a queue every time, we may as well just have one big one
	heap_queue_t* queue = heap_queue_alloc();

	//For each function
	for(u_int16_t _ = 0; _ < cfg->function_entry_blocks->current_index; _++){
		//Grab the function block out
		basic_block_t* func_block = dynamic_array_get_at(cfg->function_entry_blocks, _);

		//This function start block is the begging of our BFS	
		enqueue(queue, func_block);
		
		//So long as the queue is not empty
		while(queue_is_empty(queue) == HEAP_QUEUE_NOT_EMPTY){
			//Grab this block off of the queue
			basic_block_t* current = dequeue(queue);

			//If previous is NULL, this is the first block
			if(previous == NULL){
				previous = current;
				//This is also the head block then
				head_block = previous;
			//We need to handle the rare case where we reach two of the same blocks(maybe the block points
			//to itself) but neither have been visited. We make sure that, in this event, we do not set the
			//block to be it's own direct successor
			} else if(previous != current && current->visited == FALSE){
				//We'll add this in as a direct successor
				previous->direct_successor = current;

				//Do we end in a jump?
				basic_block_t* end_jumps_to = does_block_end_in_jump(previous);

				//If we do AND what we're jumping to is the direct successor, then we'll
				//delete the jump statement as it is now unnecessary
				if(end_jumps_to == previous->direct_successor){
					//Get rid of this jump as it's no longer needed
					delete_statement(previous->exit_statement);
				}

				//Add this in as well
				previous = current;
			}

			//Make sure that we flag this as visited
			current->visited = TRUE;

			//Let's first check for our special case - us jumping to a given block as the very last statement. If
			//this turns back something that isn't null, it'll be the first thing we add in
			basic_block_t* direct_end_jump = does_block_end_in_jump(current);

			//If this is the case, we'll add it in first
			if(direct_end_jump != NULL && direct_end_jump->visited == FALSE){
				//Add it into the queue
				enqueue(queue, direct_end_jump);
			}

			//Now we'll go through each of the successors in this node
			for(u_int16_t idx = 0; current->successors != NULL && idx < current->successors->current_index; idx++){
				//Now as we go through here, if the direct end jump wasn't NULL, we'll have already added it in. We don't
				//want to have that happen again, so we'll make sure that if it's not NULL we don't double add it

				//Grab the successor
				basic_block_t* successor = dynamic_array_get_at(current->successors, idx);

				//If we had that jumping to block case happen, make sure we skip over it to avoid double adding
				if(successor == direct_end_jump){
					continue;
				}

				//If the block is completely empty(function end block), we'll also skip
				if(successor->leader_statement == NULL){
					successor->visited = TRUE;
					continue;
				}

				//Otherwise it's not, so we'll add it in
				if(successor->visited == FALSE){
					enqueue(queue, successor);
				}
			}
		}
	}

	//Destroy the queue when done
	heap_queue_dealloc(queue);

	//Set this for later on
	cfg->head_block = head_block;

	//Give back the head block
	return head_block;
}


/**
 * Print a block our for reading
*/
static void print_ordered_block(basic_block_t* block, instruction_printing_mode_t mode){
	//If this is some kind of switch block, we first print the jump table
	if(block->jump_table != NULL){
		print_jump_table(stdout, block->jump_table);
	}

	//Switch here based on the type of block that we have
	switch(block->block_type){
		//Function entry blocks need extra printing
		case BLOCK_TYPE_FUNC_ENTRY:
			printf("%s:\n", block->function_defined_in->func_name.string);
			print_stack_data_area(&(block->function_defined_in->data_area));
			break;

		//By default just print the name
		default:
			printf(".L%d:\n", block->block_id);
			break;
	}

	//Now grab a cursor and print out every statement that we 
	//have
	instruction_t* cursor = block->leader_statement;

	//So long as it isn't null
	while(cursor != NULL){
		//Based on what mode we're given here, we call the appropriate
		//print statement
		if(mode == PRINT_THREE_ADDRESS_CODE){
			//Hand off to printing method
			print_three_addr_code_stmt(stdout, cursor);
		} else {
			print_instruction(stdout, cursor, PRINTING_VAR_IN_INSTRUCTION);
		}


		//Move along to the next one
		cursor = cursor->next_statement;
	}

	//For spacing
	printf("\n");
}


/**
 * Run through using the direct successor strategy and print all ordered blocks.
 * We print much less here than the debug printer in the CFG, because all dominance
 * relations are now useless
 */
static void print_ordered_blocks(basic_block_t* head_block, instruction_printing_mode_t mode){
	//Run through the direct successors so long as the block is not null
	basic_block_t* current = head_block;

	//So long as this one isn't NULL
	while(current != NULL){
		//Print it
		print_ordered_block(current, mode);
		//Advance to the direct successor
		current = current->direct_successor;
	}
}


/**
 * A function that selects all instructions, via the peephole method. This kind of 
 * operation completely translates the CFG out of a CFG. When done, we have a straight line
 * of code that we print out
 */
void select_all_instructions(compiler_options_t* options, cfg_t* cfg){
	//Grab these two general use types first
	u64 = lookup_type_name_only(cfg->type_symtab, "u64")->type;
	i32 = lookup_type_name_only(cfg->type_symtab, "i32")->type;
	u32 = lookup_type_name_only(cfg->type_symtab, "u32")->type;
	u8 = lookup_type_name_only(cfg->type_symtab, "u8")->type;

	//Our very first step in the instruction selector is to order all of the blocks in one 
	//straight line. This step is also able to recognize and exploit some early optimizations,
	//such as when a block ends in a jump to the block right below it
	basic_block_t* head_block = order_blocks(cfg);

	//Do we need to print intermediate representations?
	u_int8_t print_irs = options->print_irs;

	//We'll first print before we simplify
	if(print_irs == TRUE){
		printf("============================== BEFORE SIMPLIFY ========================================\n");
		print_ordered_blocks(head_block, PRINT_THREE_ADDRESS_CODE);
		printf("============================== AFTER SIMPLIFY ========================================\n");
	}

	//Once we've printed, we now need to simplify the operations. OIR already comes in an expanded
	//format that is used in the optimization phase. Now, we need to take that expanded IR and
	//recognize any redundant operations, dead values, unnecessary loads, etc.
	simplify(cfg, head_block);

	//If we need to print IRS, we can do so here
	if(print_irs == TRUE){
		print_ordered_blocks(head_block, PRINT_THREE_ADDRESS_CODE);
		printf("============================== AFTER INSTRUCTION SELECTION ========================================\n");
	}

	//Once we're done simplifying, we'll use the same sliding window technique to select instructions.
	select_instructions(cfg, head_block);

	//Final IR printing if requested by user
	if(print_irs == TRUE){
		print_ordered_blocks(head_block,PRINT_INSTRUCTION);
	}
}
