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

//The window for our "sliding window" optimizer
typedef struct instruction_window_t instruction_window_t;

/**
 * What is the status of our sliding window? Are we at the beginning,
 * middle or end of the sequence?
 */
typedef enum {
	WINDOW_AT_START,
	WINDOW_AT_MIDDLE,
	WINDOW_AT_END,
} window_status_t;


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
	//This will tell us, at a quick glance, whether we're at the beginning,
	//middle or end of a sequence
	window_status_t status;
};


/**
 * Set the window status to see if we're actually at the end. We do not
 * count as being "at the end" unless the window's last 2 statements are NULL
 */
static void set_window_status(instruction_window_t* window){
	//This is our case to check for
	if(window->instruction2 == NULL && window->instruction3 == NULL){
		window->status = WINDOW_AT_END;
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
			constant1->int_const *= constant2->int_const;
		} else {
			constant1->int_const *= constant2->long_const;
		}
	} else if(constant1->const_type == LONG_CONST){
		if(constant2->const_type == INT_CONST){
			constant1->long_const *= constant2->int_const;
		} else {
			constant1->long_const *= constant2->long_const;
		}
	}
}


/**
 * Simple utility for us to print out an instruction window in its three address code
 * (before instruction selection) format
 */
static void print_instruction_window_three_address_code(instruction_window_t* window){
	printf("----------- Instruction Window ------------\n");
	//We'll just print out all three instructions
	if(window->instruction1 != NULL){
		print_three_addr_code_stmt(window->instruction1);
	} else {
		printf("EMPTY\n");
	}

	if(window->instruction2 != NULL){
		print_three_addr_code_stmt(window->instruction2);
	} else {
		printf("EMPTY\n");
	}
	
	if(window->instruction3 != NULL){
		print_three_addr_code_stmt(window->instruction3);
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
		print_instruction(window->instruction1, PRINTING_VAR_IN_INSTRUCTION);
	} else {
		printf("EMPTY\n");
	}

	if(window->instruction2 != NULL){
		print_instruction(window->instruction2, PRINTING_VAR_IN_INSTRUCTION);
	} else {
		printf("EMPTY\n");
	}
	
	if(window->instruction3 != NULL){
		print_instruction(window->instruction3, PRINTING_VAR_IN_INSTRUCTION);
	} else {
		printf("EMPTY\n");
	}

	printf("-------------------------------------------\n");
}


/**
 * Emit a test instruction
 *
 * Test instructions inherently have no assignee as they don't modify registers
 *
 * NOTE: This may only be used DURING the process of register selection
 */
static instruction_t* emit_test_instruction(three_addr_var_t* op1, three_addr_var_t* op2){
	//First we'll allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//We'll need the size to select the appropriate instruction
	variable_size_t size = select_variable_size(op1);

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
	variable_size_t size = select_variable_size(converted);

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
	variable_size_t size = select_variable_size(destination);

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
	variable_size_t size = select_variable_size(destination);

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
 * Emit a movzbl instruction
 */
static instruction_t* emit_movzbl_instruction(three_addr_var_t* destination, three_addr_var_t* source){
	//First we'll allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//And we'll set the class
	instruction->instruction_type = MOVZBL;

	//Finally we set the destination
	instruction->destination_register = destination;
	instruction->source_register = source;

	//And now we'll give it back
	return instruction;
}


/**
 * Emit a divX or idivX instruction
 *
 * Division instructions have no destination that need be written out. They only have a source
 */
static instruction_t* emit_div_instruction(three_addr_var_t* source, u_int8_t is_signed){
	//First we'll allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//We set the size based on the destination 
	variable_size_t size = select_variable_size(source);

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

	//Finally we set the source
	instruction->source_register = source;

	//And now we'll give it back
	return instruction;
}


/**
 * Emit a divX or idivX instruction that is intended for modulus
 *
 * Division instructions have no destination that need be written out. They only have a source
 */
static instruction_t* emit_mod_instruction(three_addr_var_t* source, u_int8_t is_signed){
	//First we'll allocate it
	instruction_t* instruction = calloc(1, sizeof(instruction_t));

	//We set the size based on the destination 
	variable_size_t size = select_variable_size(source);

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

	//Finally we set the source
	instruction->source_register = source;

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

	//If the next one is NULL, we have 2 NULL instructions
	//following this. This is very rare, but it could happen
	if(window.instruction1->next_statement == NULL){
		window.instruction2 = NULL;
		window.instruction3 = NULL;
	} else {
		//Otherwise we know we have a second instruction
		window.instruction2 = window.instruction1->next_statement;
		//No such checks are needed for instruction 3, we have no possibility of a null pointer
		//error here
		window.instruction3 = window.instruction2->next_statement;
	}

	//We're at the beginning here by default
	if(window.instruction2 == NULL || window.instruction3 == NULL){
		window.status = WINDOW_AT_END;
	} else {
		window.status = WINDOW_AT_START;
	}
	
	//And now we give back the window
	return window;
}


/**
 * Advance the window up by 1 instruction. This means that the lowest instruction slides
 * out of our window, and the one next to the highest instruction slides into it
 */
static instruction_window_t* slide_window(instruction_window_t* window){
	//It should be fairly easy to slide -- except in the case
	//where we're at the end of a block. In that case, we need to slide to that
	//new block or we'll need an entirely new window if we end in a discrete jump
	//to said block

	
	//If the third operation is not the end, then we're good to just bump everything up
	//This is the simplest case, and allows us to just bump everything up and get out
	if(window->instruction3 != NULL){
		window->instruction1 = window->instruction1->next_statement;
		window->instruction2 = window->instruction2->next_statement;
		window->instruction3 = window->instruction3->next_statement;
		//We're in the thick of it here
		window->status = WINDOW_AT_MIDDLE;
	//Handle this case
	} else if(window->instruction2 == NULL){
		window->instruction1 = NULL;
		window->instruction2 = NULL;
		window->instruction3 = NULL;
		//Definitely at the end
		window->status = WINDOW_AT_END;

	//This means that we don't have a full block, but we also don't have a null
	//instruction 2
	} else {
		window->instruction1 = window->instruction1->next_statement;
		window->instruction2 = window->instruction2->next_statement;
		window->instruction3 = NULL;
		//We know we're at the end
		window->status = WINDOW_AT_END;
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

	//Select the size based on what we're moving in
	if(memory_access->op1 != NULL){
		size = select_variable_size(memory_access->op1);
	} else {
		size = select_constant_size(memory_access->op1_const);
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
	if(address_calculation->CLASS == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT){
		//So we know that the destination will be t26, the destination will remain unchanged
		//We'll have a register source and an offset
		memory_access->offset = address_calculation->op1_const;
		memory_access->address_calc_reg1 = address_calculation->op1;
		//This is offset only mode
		memory_access->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;
		
	//Or if we have a statement like this(rare but may happen, covering our bases)
	} else if(address_calculation->CLASS == THREE_ADDR_CODE_BIN_OP_STMT){
		//So we know that the destination will be t26, the destination will remain unchanged
		//We'll have a register source and an offset
		memory_access->address_calc_reg1 = address_calculation->op1;
		memory_access->address_calc_reg2 = address_calculation->op2;

		//This is offset only mode
		memory_access->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_ONLY;
 
	//Another very common case - a lea statement
	}

	//It's either an assign const or regular assignment. Either way,
	//we'll need to set the appropriate source value
	if(memory_access->CLASS == THREE_ADDR_CODE_ASSN_CONST_STMT){
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

	//Select the size based on what we're moving in
	if(memory_access->op1 != NULL){
		size = select_variable_size(memory_access->op1);
	} else {
		size = select_constant_size(memory_access->op1_const);
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

	//The first operand also comes from the first instruction
	memory_access->address_calc_reg1 = offset_calc->op1;

	//The second instruction gives us the second register and lea offset
	memory_access->address_calc_reg2 = lea_statement->op2;
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
 *
 * DOES NOT DO DELETION/WINDOW REORDERING
 */
static void handle_two_instruction_address_calc_from_memory_move(instruction_t* address_calculation, instruction_t* memory_access){
	//Select the variable size based on the assignee
	variable_size_t size = select_variable_size(memory_access->assignee);

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
	if(address_calculation->CLASS == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT){
		//So we know that the destination will be t26, the destination will remain unchanged
		//We'll have a register source and an offset
		memory_access->offset = address_calculation->op1_const;
		memory_access->address_calc_reg1 = address_calculation->op1;
		//This is offset only mode
		memory_access->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;
		
	//Otherwise, we just have a regular bin op statement
	} else {
 		//So we know that the destination will be t26, the destination will remain unchanged
		//We'll have a register source and an offset
		memory_access->address_calc_reg1 = address_calculation->op1;
		memory_access->address_calc_reg2 = address_calculation->op2;

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
	variable_size_t size = select_variable_size(memory_access->assignee);

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

	//The offset and first register come from the offset calculation
	memory_access->offset = offset_calc->op1_const;
	memory_access->address_calc_reg1 = offset_calc->op1;

	//And the second register and scale come from the lea statement
	memory_access->address_calc_reg2 = lea_statement->op2;
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
	//Let's first decide what the appropriate move instruction would be
	//We'll first select the variable size based on the destination
	variable_size_t size = select_variable_size(memory_access->assignee);

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

	//Now we can put in the address calculation type
	memory_access->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_AND_OFFSET;

	//We'll get the first and second register from the additive statement
	memory_access->address_calc_reg1 = additive_statement->op1;
	memory_access->address_calc_reg2 = additive_statement->op2;

	//We'll get the offset from offset_calc
	memory_access->offset = offset_calc->op1_const;

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
	//Let's first decide the variable size based on what we'll be moving in
	variable_size_t size;

	//Use the op1 if it's there
	if(memory_access->op1 != NULL){
		size = select_variable_size(memory_access->op1);
	//Otherwise we need to use the constant
	} else {
		size = select_constant_size(memory_access->op1_const);
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

	//Now we can put in the address calculation type
	memory_access->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_AND_OFFSET;

	//We'll get the first and second register from the additive statement
	memory_access->address_calc_reg1 = additive_statement->op1;
	memory_access->address_calc_reg2 = additive_statement->op2;

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
 * Handle a left shift operation. In doing a left shift, we account
 * for the possibility that we have a signed value
 */
static void handle_left_shift_instruction(instruction_t* instruction){
	//Is this a signed or unsigned instruction?
	u_int8_t is_signed = is_type_signed(instruction->assignee->type);

	//We'll also need the size of the variable
	variable_size_t size = select_variable_size(instruction->assignee);

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
	if(instruction->op1_const != NULL){
		instruction->source_immediate = instruction->op1_const;
	} else {
		instruction->source_register = instruction->op2;
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
	variable_size_t size = select_variable_size(instruction->assignee);

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
	if(instruction->op1_const != NULL){
		instruction->source_immediate = instruction->op1_const;
	} else {
		instruction->source_register = instruction->op2;
	}
}


/**
 * Handle a bitwise inclusive or operation
 */
static void handle_bitwise_inclusive_or_instruction(instruction_t* instruction){
	//We need to know what size we're dealing with
	variable_size_t size = select_variable_size(instruction->assignee);

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

	//Now that we've done that, we'll move over the operands
	if(instruction->op1_const != NULL){
		instruction->source_immediate = instruction->op1_const;
	} else {
		//Otherwise we have a register source here
		instruction->source_register = instruction->op2;
	}

	//And we always have a destination register
	instruction->destination_register = instruction->assignee;
}


/**
 * Handle a bitwise and operation
 */
static void handle_bitwise_and_instruction(instruction_t* instruction){
	//We need to know what size we're dealing with
	variable_size_t size = select_variable_size(instruction->assignee);

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

	//Now that we've done that, we'll move over the operands
	if(instruction->op1_const != NULL){
		instruction->source_immediate = instruction->op1_const;
	} else {
		//Otherwise we have a register source here
		instruction->source_register = instruction->op2;
	}

	//And we always have a destination register
	instruction->destination_register = instruction->assignee;
}


/**
 * Handle a bitwise exclusive or operation
 */
static void handle_bitwise_exclusive_or_instruction(instruction_t* instruction){
	//We need to know what size we're dealing with
	variable_size_t size = select_variable_size(instruction->assignee);

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
	
	//Now that we've done that, we'll move over the operands
	if(instruction->op1_const != NULL){
		instruction->source_immediate = instruction->op1_const;
	} else {
		//Otherwise we have a register source here
		instruction->source_register = instruction->op2;
	}

	//And we always have a destination register
	instruction->destination_register = instruction->assignee;
}


/**
 * Handle a cmp operation. This is used whenever we have
 * relational operation
 */
static void handle_cmp_instruction(instruction_t* instruction){
	//Determine what our size is off the bat
	variable_size_t size = select_variable_size(instruction->assignee);

	//Select this instruction
	instruction->instruction_type = select_cmp_instruction(size);
	
	//Since we have a comparison instruction, we don't actually have a destination
	//register as the registers remain unmodified in this event
	instruction->source_register = instruction->op1;

	//If we have op2, we'll use source_register2
	if(instruction->op2 != NULL){
		instruction->source_register2 = instruction->op2;
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
	variable_size_t size = select_variable_size(instruction->assignee);

	//Select the appropriate level of minus instruction
	instruction->instruction_type = select_sub_instruction(size);

	//Again we just need the source and dest registers
	instruction->destination_register = instruction->assignee;

	//If we have a register value, we add that
	if(instruction->op2 != NULL){
		instruction->source_register = instruction->op2;
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
	variable_size_t size = select_variable_size(instruction->assignee);

	//Grab the add instruction that we want
	instruction->instruction_type = select_add_instruction(size);

	//We'll just need to set the source immediate and destination register
	instruction->destination_register = instruction->assignee;

	//If we have a register value, we add that
	if(instruction->op2 != NULL){
		instruction->source_register = instruction->op2;
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
 * leal t25, (t15, t17)
 */
static void handle_addition_instruction_lea_modification(instruction_t* instruction){
	//Determines what instruction to use
	variable_size_t size = select_variable_size(instruction->assignee);

	//Now we'll get the appropriate lea instruction
	instruction->instruction_type = select_lea_instruction(size);

	//We always know what the destination register will be
	instruction->destination_register = instruction->assignee;

	//We always have this
	instruction->address_calc_reg1 = instruction->op1;

	//Now, we'll need to set the appropriate address calculation mode based
	//on what we're given
	//If we have op2, we'll have 2 registers
	if(instruction->CLASS == THREE_ADDR_CODE_BIN_OP_STMT){
		//2 registers in this case
		instruction->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_ONLY;
		instruction->address_calc_reg2 = instruction->op2;

	//Otherise it's just an offset(bin_op_with_const)
	} else {
		//We'll just have an offset here
		instruction->calculation_mode = ADDRESS_CALCULATION_MODE_OFFSET_ONLY;
		//This is definitely not 0 if we're here
		instruction->offset = instruction->op1_const;
	}
}


/**
 * Handle a multiplication operation
 *
 * A multiplication operation can be different based on size and sign
 */
static void handle_multiplication_instruction(instruction_t* instruction){
	//We'll need to know the variables size
	variable_size_t size = select_variable_size(instruction->assignee);

	//We also need to determine if it's signed or not due to there being separate instructions
	u_int8_t is_variable_signed = is_type_signed(instruction->assignee->type);

	//We determine the instruction that we need based on signedness and size
	switch (size) {
		case WORD:
		case DOUBLE_WORD:
			if(is_variable_signed == TRUE){
				instruction->instruction_type = IMULL;
			} else {
				instruction->instruction_type = MULL;
			}
			break;
		//Everything else falls here
		default:
			if(is_variable_signed == TRUE){
				instruction->instruction_type = IMULQ;
			} else {
				instruction->instruction_type = MULQ;
			}
			break;
	}

	//Following this, we'll set the assignee and source
	instruction->destination_register = instruction->assignee;

	//Are we using an immediate or register?
	if(instruction->op2 != NULL){
		//This is the case where we have a source register
		instruction->source_register = instruction->op2;
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
	//Also have this as a reference
	instruction_t* after_division = window->instruction2;
	//And we'll want this for reference too
	basic_block_t* block = division_instruction->block_contained_in;

	//We first need to perform a move of the dividence into rax
	instruction_t* move_to_rax = emit_movX_instruction(emit_temp_var(division_instruction->op1->type), division_instruction->op1);

	//Let's now attach this where division was
	if(division_instruction->previous_statement != block->leader_statement){
		//This effectively deletes the old division instruction
		division_instruction->previous_statement->next_statement = move_to_rax;
		move_to_rax->previous_statement = division_instruction->previous_statement;
	} else {
		//Otherwise, this is the leader statement of its block
		block->leader_statement = move_to_rax;
	}

	//This may become the cl instruction
	instruction_t* current_end = move_to_rax;

	//Let's determine signedness
	u_int8_t is_signed = is_type_signed(division_instruction->assignee->type);

	//Now, we'll need the appropriate extension instruction *if* we're doing signed division
	if(is_signed == TRUE){
		//Emit the cl instruction
		instruction_t* cl_instruction = emit_conversion_instruction(move_to_rax->destination_register);

		//Link it with our move statement
		move_to_rax->next_statement = cl_instruction;
		cl_instruction->previous_statement = move_to_rax;

		//Keep this reference so we don't get tangled up
		current_end = cl_instruction;
	}

	//Now we should have what we need, so we can emit the division instruction
	instruction_t* division = emit_div_instruction(window->instruction1->op2, is_signed);
	//This is the assignee, we just don't see it
	division->destination_register = emit_temp_var(division_instruction->assignee->type);

	//Once it's been emitted, we link it in like all the rest
	current_end->next_statement = division;
	division->previous_statement = current_end;

	//Once we've done all that, we need one final movement operation
	instruction_t* result_movement = emit_movX_instruction(division_instruction->assignee, division->destination_register);

	//Tie it in here
	division->next_statement = result_movement;
	result_movement->previous_statement = division;

	//And now we can tie it in to our overall statement
	result_movement->next_statement = after_division;
	
	//Avoid any null pointer dereference here
	if(after_division != NULL){
		after_division->previous_statement = result_movement;
	} else {
		//This is the new exit statement
		block->exit_statement = result_movement;
	}

	//Now we need to repopulate the window
	window->instruction1 = move_to_rax;
	window->instruction2 = window->instruction1->next_statement;
	window->instruction3 = window->instruction2->next_statement;

	//Now we'll cycle the window to get to the end
	slide_window(window);
	slide_window(window);

	//Set the status
	set_window_status(window);
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
	//Also have this as a reference
	instruction_t* after_mod = window->instruction2;
	//And we'll want this for reference too
	basic_block_t* block = modulus_instruction->block_contained_in;

	//We first need to perform a move of the dividence into rax
	instruction_t* move_to_rax = emit_movX_instruction(emit_temp_var(modulus_instruction->op1->type), modulus_instruction->op1);

	//Let's now attach this where division was
	if(modulus_instruction->previous_statement != block->leader_statement){
		//This effectively deletes the old division instruction
		modulus_instruction->previous_statement->next_statement = move_to_rax;
		//Add to the end
		move_to_rax->previous_statement = modulus_instruction->previous_statement;
	} else {
		//Otherwise, this is the leader statement of its block
		block->leader_statement = move_to_rax;
	}

	//This may become the cl instruction
	instruction_t* current_end = move_to_rax;

	//Let's determine signedness
	u_int8_t is_signed = is_type_signed(modulus_instruction->assignee->type);

	//Now, we'll need the appropriate extension instruction *if* we're doing signed division
	if(is_signed == TRUE){
		//Emit the cl instruction
		instruction_t* cl_instruction = emit_conversion_instruction(move_to_rax->destination_register);

		//Link it with our move statement
		move_to_rax->next_statement = cl_instruction;
		cl_instruction->previous_statement = move_to_rax;

		//Keep this reference so we don't get tangled up
		current_end = cl_instruction;
	}

	//Now we should have what we need, so we can emit the division instruction
	instruction_t* division = emit_mod_instruction(window->instruction1->op2, is_signed);
	//This is the assignee, we just don't see it
	division->destination_register = emit_temp_var(modulus_instruction->assignee->type);

	//Once it's been emitted, we link it in like all the rest
	current_end->next_statement = division;
	division->previous_statement = current_end;

	//Once we've done all that, we need one final movement operation
	instruction_t* result_movement = emit_movX_instruction(modulus_instruction->assignee, division->destination_register);

	//Tie it in here
	division->next_statement = result_movement;
	result_movement->previous_statement = division;

	//And now we can tie it in to our overall statement
	result_movement->next_statement = after_mod;
	
	//Avoid any null pointer dereference here
	if(after_mod != NULL){
		after_mod->previous_statement = result_movement;
	} else {
		//This is the new exit statement
		block->exit_statement = result_movement;
	}

	//Now we need to repopulate the window
	window->instruction1 = move_to_rax;
	window->instruction2 = window->instruction1->next_statement;
	window->instruction3 = window->instruction2->next_statement;

	//Now we'll cycle the window to get to the end
	slide_window(window);
	slide_window(window);

	//Set the status
	set_window_status(window);
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
		case STAR:
			//Let the helper do it
			handle_multiplication_instruction(instruction);
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
	variable_size_t size = select_variable_size(instruction->assignee);

	//If it's a quad word, there's a different instruction to use. Otherwise
	//it's just a regular inc
	if(size == QUAD_WORD){
		instruction->instruction_type = INCQ;
	} else {
		instruction->instruction_type = INCL;
	}

	//Set the destination as the assignee
	instruction->destination_register = instruction->assignee;
}

/**
 * Handle a decrement statement
 */
static void handle_dec_instruction(instruction_t* instruction){
	//Determine the size of the variable we need
	variable_size_t size = select_variable_size(instruction->assignee);

	//If it's a quad word, there's a different instruction to use. Otherwise
	//it's just a regular inc
	if(size == QUAD_WORD){
		instruction->instruction_type = DECQ;
	} else {
		instruction->instruction_type = DECL;
	}

	//Set the destination as the assignee
	instruction->destination_register = instruction->assignee;
}


/**
 * Handle a regular move condition
 *
 * We also account for cases where we have variables with indirection levels
 */
static void handle_to_register_move_instruction(instruction_t* instruction){
	variable_size_t size;

	//Select the variable size based on the assignee, unless it's an address
	if(instruction->assignee->indirection_level == 0){
		size = select_variable_size(instruction->assignee);
	} else {
		if(instruction->op1 != NULL){
			size = select_variable_size(instruction->op1);
		} else {
			//Use the const
			size = select_constant_size(instruction->op1_const);
		}
	}

	//Set the source appropriately for later
	if(instruction->op1 != NULL){
		//We'll have a register source
		instruction->source_register = instruction->op1;
	//Otherwise it must be a constant
	} else {
		//Set this as well if we can
		instruction->source_immediate = instruction->op1_const;
	}

	//Use the helper to get the right sized move instruction
	instruction->instruction_type = select_move_instruction(size);
	
	//We've already set the sources, now we set the destination as the assignee
	instruction->destination_register = instruction->assignee;

	//Handle the indirection levels here if we have a deref only case
	if(instruction->destination_register->indirection_level > 0){
		instruction->indirection_level = instruction->destination_register->indirection_level;
		instruction->calculation_mode = ADDRESS_CALCULATION_MODE_DEREF_ONLY_DEST;

	} else if(instruction->source_register != NULL && instruction->source_register->indirection_level > 0){
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

	//This is just a placeholder for now - it will be occupied later on
	three_addr_const_t* constant = emit_long_constant_direct(0, symtab);
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
	//Select the size of our variable
	variable_size_t size = select_variable_size(instruction->assignee);

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

	//We already know what mode we'll need to use here
	instruction->calculation_mode = ADDRESS_CALCULATION_MODE_REGISTERS_AND_SCALE;

	//Now we can set the values
	instruction->destination_register = instruction->assignee;

	//Add op1 and op2
	instruction->address_calc_reg1 = instruction->op1;
	instruction->address_calc_reg2 = instruction->op2;

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
 * movzbl %al, t9
 *
 * NOTE: We know that instruction1 is the one that is a logical not instruction if we
 * get here
 */
static void handle_logical_not_instruction(cfg_t* cfg, instruction_window_t* window){
	//Let's grab the value out for convenience
	instruction_t* logical_not = window->instruction1;

	//Ensure that this one's size has been selected
	logical_not->assignee->variable_size = select_variable_size(logical_not->assignee);

	//Now we'll need to generate three new instructions
	//First comes the test command. We're testing this against itself
	instruction_t* test_inst = emit_test_instruction(logical_not->assignee, logical_not->assignee); 
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
	instruction_t* movzbl_inst = emit_movzbl_instruction(logical_not->assignee, sete_inst->destination_register);
	//Ensure that we set all these flags too
	movzbl_inst->block_contained_in = logical_not->block_contained_in;
	movzbl_inst->is_branch_ending = logical_not->is_branch_ending;

	//Grab the block out for convenience
	basic_block_t* block = logical_not->block_contained_in;
	//Preserve this before we lose it
	instruction_t* after_logical_not = logical_not->next_statement;

	//If this is the case, we do a normal addition
	if(logical_not->previous_statement != NULL){
		//We'll sever the connection and delete 
		logical_not->previous_statement->next_statement = test_inst;
		test_inst->previous_statement = logical_not->previous_statement;
	//Otherwise it's the head
	} else {
		block->leader_statement = test_inst;
	}

	//Now we'll add all of the individual linkages
	test_inst->next_statement = sete_inst;
	sete_inst->previous_statement = test_inst;

	sete_inst->next_statement = movzbl_inst;
	movzbl_inst->previous_statement = sete_inst;

	//Now connect it back to the end
	movzbl_inst->next_statement = after_logical_not;

	//If this isn't null we can point back
	if(after_logical_not != NULL){
		after_logical_not->previous_statement = movzbl_inst;
	} else {
		//We know it's the exit block then
		block->exit_statement = movzbl_inst;
	}

	//We now need to rework the window. We know that these are 
	//the values of the three in-window items now
	window->instruction1 = test_inst;
	window->instruction2 = sete_inst;
	window->instruction3 = movzbl_inst;
	
	//Now we'll cycle the window twice
	slide_window(window);
	slide_window(window);
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
 * movzbl t33, t32  <-------------- move this into the result
 *
 * NOTE: We guarantee that the first instruction in the window is the one that
 * we're after in this case
 */
static void handle_logical_or_instruction(cfg_t* cfg, instruction_window_t* window){
	//Grab it out for convenience
	instruction_t* logical_or = window->instruction1;

	//Save the after instruction
	instruction_t* after_logical_or = window->instruction2;

	//And grab the block out
	basic_block_t* block = logical_or->block_contained_in;

	//Let's first emit the or instructio
	instruction_t* or_instruction = emit_or_instruction(logical_or->op1, logical_or->op2);

	//We'll need this type for our setne's
	generic_type_t* unsigned_int8_type = lookup_type_name_only(cfg->type_symtab, "u8")->type;

	//Now we need the setne instruction
	instruction_t* setne_instruction = emit_setne_instruction(emit_temp_var(unsigned_int8_type));

	//Let's link the two together
	or_instruction->next_statement = setne_instruction;
	setne_instruction->previous_statement = or_instruction;

	//Following that we'll need the final movzbl instruction
	instruction_t* movzbl_instruction = emit_movzbl_instruction(logical_or->assignee, setne_instruction->destination_register);

	//Select this one's size 
	logical_or->assignee->variable_size = select_variable_size(logical_or->assignee);

	//Now we'll link this one too
	setne_instruction->next_statement = movzbl_instruction;
	movzbl_instruction->previous_statement = setne_instruction;

	//Now everything is complete, we're able to do the final linkage
	//If this is the case, we do a normal addition
	if(logical_or->previous_statement != NULL){
		//We'll sever the connection and delete 
		logical_or->previous_statement->next_statement = or_instruction;
		or_instruction->previous_statement = logical_or->previous_statement;
	//Otherwise it's the head
	} else {
		block->leader_statement = or_instruction;
	}

	//No matter what this will point to whatever came next
	movzbl_instruction->next_statement = after_logical_or;

	//Now we'll need to sever the end
	if(after_logical_or != NULL){
		after_logical_or->previous_statement = movzbl_instruction;
	} else {
		//We know it's the exit block then
		block->exit_statement = movzbl_instruction;
	}

	//Now that we're all linked, we need to redo the window
	window->instruction1 = or_instruction;
	window->instruction2 = setne_instruction;
	window->instruction3 = movzbl_instruction;

	//Slide the window twice so we don't waste a cycle
	slide_window(window);
	slide_window(window);
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
 * movzbl t34, t32 <---------- store t34 with the result
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

	//Grab the block out too
	basic_block_t* block = logical_and->block_contained_in;

	//Let's first emit our test instruction
	instruction_t* first_test = emit_test_instruction(logical_and->op1, logical_and->op1);

	//We'll need this type for our setne's
	generic_type_t* unsigned_int8_type = lookup_type_name_only(cfg->type_symtab, "u8")->type;

	//Now we'll need a setne instruction that will set a new temp
	instruction_t* first_set = emit_setne_instruction(emit_temp_var(unsigned_int8_type));
	
	//Link these two
	first_test->next_statement = first_set;
	first_set->previous_statement = first_test;

	//Now we'll need the second test
	instruction_t* second_test = emit_test_instruction(logical_and->op2, logical_and->op2);

	//Link it to the prior
	first_set->next_statement = second_test;
	second_test->previous_statement = first_set;

	//Now the second setne
	instruction_t* second_set = emit_setne_instruction(emit_temp_var(unsigned_int8_type));

	//Link to the prior
	second_test->next_statement = second_set;
	second_set->previous_statement = second_test;

	//Now we'll need to ANDx these two values together to see if they're both 1
	instruction_t* and_inst = emit_and_instruction(first_set->destination_register, second_set->destination_register);

	//Again link it
	second_set->next_statement = and_inst;
	and_inst->previous_statement = second_set;

	//The final thing that we need is a movzbl
	instruction_t* final_move = emit_movzbl_instruction(logical_and->assignee, and_inst->destination_register);

	//Select this one's size 
	logical_and->assignee->variable_size = select_variable_size(logical_and->assignee);
	
	//Do this one's linkage
	and_inst->next_statement = final_move;
	final_move->previous_statement = and_inst;
	
	//Now connect it back to the end
	final_move->next_statement = after_logical_and;

	//Let's now delete the statement
	//If this is the case, we do a normal addition
	if(logical_and->previous_statement != NULL){
		//We'll sever the connection and delete 
		logical_and->previous_statement->next_statement = first_test;
		first_test->previous_statement = logical_and->previous_statement;
	//Otherwise it's the head
	} else {
		block->leader_statement = first_test;
	}

	//If this isn't null we can point back
	if(after_logical_and != NULL){
		after_logical_and->previous_statement = final_move;
	} else {
		//We know it's the exit block then
		block->exit_statement = final_move;
	}

	//We now need to rework the window. We know that these are 
	//the values of the three in-window items now
	window->instruction1 = first_test;
	window->instruction2 = first_set;
	window->instruction3 = second_test;
	
	//Now we'll cycle the window five times
	slide_window(window);
	slide_window(window);
	slide_window(window);
	slide_window(window);
	slide_window(window);
}



/**
 * The first part of the instruction selector to run is the pattern selector. This 
 * first set of passes will determine if there are any large patterns that we can optimize
 * with our instructions. This will likely leave a lot of instructions not selected, 
 * which is part of the plan
 */
static u_int8_t select_multiple_instruction_patterns(cfg_t* cfg, instruction_window_t* window){
	//Have we changed the window at all? Very similar to the simplify function
	u_int8_t changed = FALSE;

	//Handle a logical not instruction selection. This does generate multiple new instructions,
	//so it has to go here
	if(window->instruction1->CLASS == THREE_ADDR_CODE_LOGICAL_NOT_STMT){
		//Let this handle it
		handle_logical_not_instruction(cfg, window);

		//This does count as a change
		changed = TRUE;
	}

	//We could see logical and/logical or
	if(window->instruction1->CLASS == THREE_ADDR_CODE_BIN_OP_STMT
	  	|| window->instruction1->CLASS == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT){
		//Handle the logical and case
		if(window->instruction1->op == DOUBLE_AND){
			handle_logical_and_instruction(cfg, window);
			changed = TRUE;
			
		} else if(window->instruction1->op == DOUBLE_OR){
			handle_logical_or_instruction(cfg, window);
			changed = TRUE;

		//Division is a bit unique
		} else if(window->instruction1->op == F_SLASH){
			//This will generate more than one instruction
			handle_division_instruction(window);
			changed = TRUE;

		//Mod is very similar to division but there are some differences
		//that warrant a separate function
		} else if(window->instruction1->op == MOD){
			//This will generate more than one instruction
			handle_modulus_instruction(window);
			changed = TRUE;
		}
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
	if(window->instruction1->CLASS == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT
		&& window->instruction2 != NULL && window->instruction3 != NULL
		&& window->instruction2->CLASS == THREE_ADDR_CODE_LEA_STMT
		&& (window->instruction3->CLASS == THREE_ADDR_CODE_ASSN_CONST_STMT || window->instruction3->CLASS == THREE_ADDR_CODE_ASSN_STMT)
		&& window->instruction3->assignee->indirection_level == 1
		&& variables_equal(window->instruction1->assignee, window->instruction2->op1, FALSE) == TRUE
		&& variables_equal(window->instruction2->assignee, window->instruction3->assignee, TRUE) == TRUE){

		//Let the helper deal with everything related to this
		handle_three_instruction_address_calc_to_memory_move(window->instruction1, window->instruction2, window->instruction3);

		//Once we're done doing this, the first 2 instructions are now useless
		delete_statement(cfg, window->instruction1->block_contained_in, window->instruction1);
		delete_statement(cfg, window->instruction2->block_contained_in, window->instruction2);

		//Now we'll rearrange the window to reflect this
		window->instruction1 = window->instruction3;
		window->instruction2 = window->instruction1->next_statement;

		//Avoid any null pointer dereference
		if(window->instruction2 == NULL){
			window->instruction3 = NULL;
		} else {
			window->instruction3 = window->instruction2->next_statement;
		}

		//Let the helper handle the status
		set_window_status(window);

		//This counts as a change
		changed = TRUE;
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
	if(window->instruction1->CLASS == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT
		&& window->instruction2 != NULL && window->instruction3 != NULL
		&& window->instruction2->CLASS == THREE_ADDR_CODE_LEA_STMT
		&& window->instruction3->CLASS == THREE_ADDR_CODE_ASSN_STMT
		&& window->instruction3->op1->indirection_level == 1
		&& variables_equal(window->instruction1->assignee, window->instruction2->op1, FALSE) == TRUE
		&& variables_equal(window->instruction2->assignee, window->instruction3->op1, TRUE) == TRUE){

		//Let the helper deal with it
		handle_three_instruction_address_calc_from_memory_move(window->instruction1, window->instruction2, window->instruction3);

		//Once we're done doing this, the first 2 instructions are now useless
		delete_statement(cfg, window->instruction1->block_contained_in, window->instruction1);
		delete_statement(cfg, window->instruction2->block_contained_in, window->instruction2);

		//Now we'll rearrange the window to reflect this
		window->instruction1 = window->instruction3;
		window->instruction2 = window->instruction1->next_statement;

		//Avoid any null pointer dereference
		if(window->instruction2 == NULL){
			window->instruction3 = NULL;
		} else {
			window->instruction3 = window->instruction2->next_statement;
		}

		//Let the helper handle the status
		set_window_status(window);

		//This counts as a change
		changed = TRUE;
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
		&& window->instruction1->CLASS == THREE_ADDR_CODE_BIN_OP_STMT
		&& window->instruction2->CLASS == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT
		&& window->instruction3->CLASS == THREE_ADDR_CODE_ASSN_STMT
		&& window->instruction3->op1->indirection_level == 1 //Only works for memory movement
		&& variables_equal(window->instruction1->assignee, window->instruction2->op1, FALSE) == TRUE
		&& variables_equal(window->instruction2->assignee, window->instruction3->op1, TRUE) == TRUE){

		//Let the helper deal with it
		handle_three_instruction_registers_and_offset_only_from_memory_move(window->instruction1, window->instruction2, window->instruction3);
		
		//Once the helper is done, we need to delete instructions 1 and 2
		delete_statement(cfg, window->instruction1->block_contained_in, window->instruction1);
		delete_statement(cfg, window->instruction2->block_contained_in, window->instruction2);

		//Now we'll shift the window appropriately
		window->instruction1 = window->instruction3;
		window->instruction2 = window->instruction1->next_statement;

		//Avoid a null pointer dereference
		if(window->instruction2 == NULL){
			window->instruction3 = NULL;
		} else {
			window->instruction3 = window->instruction2->next_statement;
		}

		//Update the window after this
		set_window_status(window);

		//This counts as a change
		changed = TRUE;
	}

	
	/**
	 * t26 <- arr_0 + t25
	 * t28 <- t26 + 8
	 * (t28) <- t29
	 *
	 * Should become
	 * mov(w/l/q) t29, 8(arr_0, t25)
	 */
	if(window->instruction2 != NULL && window->instruction3 != NULL
		&& window->instruction1->CLASS == THREE_ADDR_CODE_BIN_OP_STMT
		&& window->instruction2->CLASS == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT
		&& (window->instruction3->CLASS == THREE_ADDR_CODE_ASSN_STMT || window->instruction3->CLASS == THREE_ADDR_CODE_ASSN_CONST_STMT)
		&& window->instruction3->assignee->indirection_level == 1 //Only works for memory movement
		&& variables_equal(window->instruction1->assignee, window->instruction2->op1, FALSE) == TRUE
		&& variables_equal(window->instruction2->assignee, window->instruction3->assignee, TRUE) == TRUE){

		//Let the helper deal with it
		handle_three_instruction_registers_and_offset_only_to_memory_move(window->instruction1, window->instruction2, window->instruction3);
		
		//Once the helper is done, we need to delete instructions 1 and 2
		delete_statement(cfg, window->instruction1->block_contained_in, window->instruction1);
		delete_statement(cfg, window->instruction2->block_contained_in, window->instruction2);

		//Now we'll shift the window appropriately
		window->instruction1 = window->instruction3;
		window->instruction2 = window->instruction1->next_statement;

		//Avoid a null pointer dereference
		if(window->instruction2 == NULL){
			window->instruction3 = NULL;
		} else {
			window->instruction3 = window->instruction2->next_statement;
		}

		//Update the window after this
		set_window_status(window);

		//This counts as a change
		changed = TRUE;

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
	if(window->instruction2 != NULL
		&& (window->instruction1->CLASS == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT || window->instruction1->CLASS == THREE_ADDR_CODE_BIN_OP_STMT)
		&& window->instruction1->op == PLUS 
		&& (window->instruction2->CLASS == THREE_ADDR_CODE_ASSN_STMT || window->instruction2->CLASS == THREE_ADDR_CODE_ASSN_CONST_STMT)
		&& variables_equal(window->instruction1->assignee, window->instruction2->assignee, TRUE) == TRUE
		&& window->instruction2->assignee->indirection_level == 1){

		//Use the helper to keep things somewhat clean in here
		handle_two_instruction_address_calc_to_memory_move(window->instruction1, window->instruction2);

		//We can now delete instruction 1
		delete_statement(cfg, window->instruction1->block_contained_in, window->instruction1);

		//Once that's done, everything needs to be shifted back by 1
		window->instruction1 = window->instruction2;
		window->instruction2 = window->instruction1->next_statement;

		if(window->instruction2 == NULL){
			window->instruction3 = NULL;
		} else {
			window->instruction3 = window->instruction2->next_statement;
		}

		//We'll need to update the status accordingly
		set_window_status(window);

		//This counts as a change
		changed = TRUE;
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
		&& (window->instruction1->CLASS == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT || window->instruction1->CLASS == THREE_ADDR_CODE_BIN_OP_STMT)
		&& window->instruction1->op == PLUS
		&& window->instruction2->CLASS == THREE_ADDR_CODE_ASSN_STMT
		&& variables_equal(window->instruction1->assignee, window->instruction2->op1, TRUE) == TRUE
		&& window->instruction2->op1->indirection_level == 1){

		//Use the helper to avoid an explosion of code here
		handle_two_instruction_address_calc_from_memory_move(window->instruction1, window->instruction2);

		//We can scrap instruction 1 now, it's useless
		delete_statement(cfg, window->instruction1->block_contained_in, window->instruction1);

		//Push everyting back by 1
		window->instruction1 = window->instruction2;
		window->instruction2 = window->instruction3;

		//Avoid any null pointer dereference
		if(window->instruction2 == NULL){
			window->instruction3 = NULL;
		} else {
			window->instruction3 = window->instruction2->next_statement;
		}

		//Let the helper update the status
		set_window_status(window);

		//This counts as a change
		changed = TRUE;
	}


	//Do we have a case where we have an indirect jump statement? If so we can handle that by condensing it into one
	if(window->instruction1->CLASS == THREE_ADDR_CODE_INDIR_JUMP_ADDR_CALC_STMT && window->instruction2->CLASS == THREE_ADDR_CODE_INDIRECT_JUMP_STMT){
		//This will be flagged as an indirect jump
		window->instruction2->instruction_type = INDIRECT_JMP;

		//The source register is op1
		window->instruction2->source_register = window->instruction1->op2;

		//Store the jumping to block where the jump table is
		window->instruction2->jumping_to_block = window->instruction1->jumping_to_block;

		//We also have an "S" multiplicator factor that will always be a power of 2 stored in the lea_multiplicator
		window->instruction2->lea_multiplicator = window->instruction1->lea_multiplicator;

		//We're now able to delete instruction 1
		delete_statement(cfg, window->instruction1->block_contained_in, window->instruction1);

		//Now we shift everything backwards
		window->instruction1 = window->instruction2;
		window->instruction2 = window->instruction2->next_statement;

		//Avoid any kind of null pointer dereference
		if(window->instruction2 == NULL){
			window->instruction3 = NULL;
		} else {
			window->instruction3 = window->instruction2->next_statement;
		}

		//Reevaluate the status now
		set_window_status(window);

		//This counts as a change
		changed = TRUE;
	}

	return changed;
}


/**
 * Handle a negation instruction. Very simple - all we need to do is select the suffix and
 * add it over
 */
static void handle_neg_instruction(instruction_t* instruction){
	//Find out what size we have
	variable_size_t size = select_variable_size(instruction->assignee);

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
	variable_size_t size = select_variable_size(instruction->assignee);

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
 * Select instructions that follow a singular pattern. This one single pass will run after
 * the pattern selector ran and perform one-to-one mappings on whatever is left.
 */
static void select_single_instruction_patterns(cfg_t* cfg, instruction_window_t* window){
	//Make an array for us
	instruction_t* instructions[3] = {window->instruction1, window->instruction2, window->instruction3};

	//The current instruction
	instruction_t* current;

	//Run through each of the tree instructions
	for(u_int8_t _ = 0; _ < 3; _++){
		//Grab whichever current is
		current = instructions[_];
	
		//If this is the case, it's already been selected so bail
		if(current == NULL || current->instruction_type != NONE){
			continue;
		}

		//Switch on whatever we have currently
		switch (current->CLASS) {
			//These have a helper
			case THREE_ADDR_CODE_ASSN_STMT:
			case THREE_ADDR_CODE_ASSN_CONST_STMT:
				handle_to_register_move_instruction(current);
				break;
			case THREE_ADDR_CODE_MEM_ADDR_ASSIGNMENT:
				handle_address_assignment_instruction(current, cfg->type_symtab, cfg->stack_pointer);
				break;
			case THREE_ADDR_CODE_LEA_STMT:
				handle_lea_statement(current);
				break;
			//One-to-one mapping to nop
			case THREE_ADDR_CODE_IDLE_STMT:
				current->instruction_type = NOP;
				break;
			//One to one mapping here as well
			case THREE_ADDR_CODE_RET_STMT:
				current->instruction_type = RET;
				//We'll still store this, just in a hidden way
				current->source_register = current->op1;
				break;
			case THREE_ADDR_CODE_JUMP_STMT:
			case THREE_ADDR_CODE_DIR_JUMP_STMT:
				//Let the helper do this and then leave
				select_jump_instruction(current);
				break;
			//Special case here - we don't change anything
			case THREE_ADDR_CODE_ASM_INLINE_STMT:
				current->instruction_type = ASM_INLINE;
				break;
			//The translation here takes the form of a call instruction
			case THREE_ADDR_CODE_FUNC_CALL:
				current->instruction_type = CALL;
				//The destination register is itself the assignee
				current->destination_register = current->assignee;
				break;
			//Let the helper deal with this
			case THREE_ADDR_CODE_INC_STMT:
				handle_inc_instruction(current);
				break;
			//Again use the helper
			case THREE_ADDR_CODE_DEC_STMT:
				handle_dec_instruction(current);
				break;
			//Let the helper handle this one
			case THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT:
			case THREE_ADDR_CODE_BIN_OP_STMT:
				handle_binary_operation_instruction(current);
				break;
			//For a phi function, we perform an exact 1:1 mapping
			case THREE_ADDR_CODE_PHI_FUNC:
				//This is all we'll need
				current->instruction_type = PHI_FUNCTION;
				break;
			//Handle a neg statement
			case THREE_ADDR_CODE_NEG_STATEMENT:
				//Let the helper do it
				handle_neg_instruction(current);
				break;
			//Handle a neg statement
			case THREE_ADDR_CODE_BITWISE_NOT_STMT:
				//Let the helper do it
				handle_not_instruction(current);
				break;
			default:
				break;
		}
	}
}


/**
 * Perform one pass of the multi pattern instruction selector. We will keep performing passes
 * until we no longer see the changed flag
 */
static u_int8_t multi_instruction_pattern_selector_pass(cfg_t* cfg, basic_block_t* head_block){
	u_int8_t changed;
	u_int8_t window_changed = FALSE;

	//Keep track of the current block
	basic_block_t* current = head_block;

	//So long as this isn't NULL
	while(current != NULL){
		//By default there's no change
		changed = FALSE;
		
		//Initialize the sliding window(very basic, more to come)
		instruction_window_t window = initialize_instruction_window(current);

		//Run through and simplify everything we can
		do {
			//Select the patterns
			changed = select_multiple_instruction_patterns(cfg, &window);

			//Set this flag if it was changed
			if(changed == TRUE){
				window_changed = TRUE;
			}

			//And slide it
			slide_window(&window);

		//So long as we aren't at the end
		} while(window.status != WINDOW_AT_END);


		//Advance to the direct successor
		current = current->direct_successor;
	}

	return window_changed;
}


/**
 * Run through every block and convert each instruction or sequence of instructions
 * from three address code to assembly statements
 */
static void select_instructions(cfg_t* cfg, basic_block_t* head_block){
	//First we'll grab the head
	basic_block_t* current = head_block;

	/**
	 * We first go through and perform the multiple pattern instruction
	 * selection. This allows us to catch any large patterns and select them
	 * first, before they'd be obfuscated by the single pattern selector
	 */
	while(multi_instruction_pattern_selector_pass(cfg, head_block) == TRUE);

	//Reset current here
	current = head_block;

	//So long as this isn't NULL
	while(current != NULL){
		//Initialize the sliding window(very basic, more to come)
		instruction_window_t window = initialize_instruction_window(current);

		//Keep going so long as the window isn't at the end
		do{
			//Select the instructions
			//TODO only single pattern for now
			select_single_instruction_patterns(cfg, &window);

			//Slide the window
			slide_window(&window);

		//Keep going if we aren't at the end
		} while(window.status != WINDOW_AT_END);

		//Advance to the direct successor
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
	if(block->exit_statement != NULL && block->exit_statement->CLASS == THREE_ADDR_CODE_JUMP_STMT
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
 * A simple helper function to determine if an operator is a comparison operator
 */
static u_int8_t is_comparison_operator(Token op){
	if(op == G_THAN || op == L_THAN || op == G_THAN_OR_EQ || op == L_THAN_OR_EQ
	   || op == DOUBLE_EQUALS || op == NOT_EQUALS){
		return TRUE;
	}

	return FALSE;
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
	//These types use the 32 bit field
	if(constant->const_type == INT_CONST || constant->const_type == INT_CONST_FORCE_U || constant->const_type == HEX_CONST){
		constant->int_const = log2_of_known_power_of_2(constant->int_const);
	//Use the 64 bit field
	} else if(constant->const_type == LONG_CONST || constant->const_type == LONG_CONST_FORCE_U){
		constant->long_const = log2_of_known_power_of_2(constant->long_const);
	//Use the 8 bit field
	} else if(constant->const_type == CHAR_CONST){
		constant->char_const = log2_of_known_power_of_2(constant->char_const);
	}
	//Anything else we ignore
}


/**
 * Remediate the stack address issues that may have been caused by the previous optimization
 * step
 */
static void remediate_stack_address(cfg_t* cfg, instruction_t* instruction){
	//What block is this instruction in
	basic_block_t* block = instruction->block_contained_in;

	//Grab this value out. We'll need it's stack offset
	three_addr_var_t* assignee = instruction->assignee;

	//This means that there is a stack offset
	if(assignee->stack_offset != 0){
		//We'll need to ensure that this is an addition statement
		instruction->CLASS = THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT;
		instruction->op = PLUS;

		//Now we'll need to either make a three address constant or update
		//the existing one
		if(instruction->op1_const != NULL){
			instruction->op1_const->int_const = assignee->stack_offset;
		} else {
			instruction->op1_const = emit_int_constant_direct(assignee->stack_offset, cfg->type_symtab);
		}

	//Otherwise it's just the RSP value
	} else {
		//This is just an assignment statement then
		instruction->CLASS = THREE_ADDR_CODE_ASSN_STMT;
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
	if(window->instruction1->CLASS == THREE_ADDR_CODE_ASSN_CONST_STMT 
	 	&& window->instruction2->CLASS == THREE_ADDR_CODE_ASSN_STMT){
		
		//If the first assignee is what we're assigning to the next one, we can fold. We only do this when
		//we deal with temp variables. At this point in the program, all non-temp variables have been
		//deemed important, so we wouldn't want to remove their assignments
		if(window->instruction1->assignee->is_temporary == TRUE &&
			variables_equal(window->instruction1->assignee, window->instruction2->op1, FALSE) == TRUE){
			//Grab this out for convenience
			instruction_t* binary_operation = window->instruction2;

			//Now we'll modify this to be an assignment const statement
			binary_operation->op1_const = window->instruction1->op1_const;

			//Modify the type of the assignment
			binary_operation->CLASS = THREE_ADDR_CODE_ASSN_CONST_STMT;

			//Make sure that we now null out op1
			binary_operation->op1 = NULL;

			//Once we've done this, the first statement is entirely useless
			delete_statement(cfg, window->instruction1->block_contained_in, window->instruction1);

			//Once we've deleted the statement, we'll need to completely rewire the block
			
			//The second instruction is now the first one
			window->instruction1 = binary_operation;
			//Now instruction 2 becomes instruction 3
			window->instruction2 = window->instruction3;

			//We could have a case where instruction 3 is NULL here, let's account for that
			if(window->instruction2 == NULL){
				//Set instruction 3 to be NULL
				window->instruction3 = NULL;
			} else {
				//Otherwise we're safe to crawl
				window->instruction3 = window->instruction2->next_statement;
			}

			//Allow the helper to set this status
			set_window_status(window);

			//Whatever happened here, we did change something
			changed = TRUE;
		}
	}

	//This is the same case as above, we'll just now check instructions 2 and 3
	if(window->instruction2 != NULL && window->instruction2->CLASS == THREE_ADDR_CODE_ASSN_CONST_STMT 
	 	&& window->instruction3 != NULL && window->instruction3->CLASS == THREE_ADDR_CODE_ASSN_STMT){

		//If the first assignee is what we're assigning to the next one, we can fold. We only do this when
		//we deal with temp variables. At this point in the program, all non-temp variables have been
		//deemed important, so we wouldn't want to remove their assignments
		if(window->instruction2->assignee->is_temporary == TRUE &&
			variables_equal(window->instruction2->assignee, window->instruction3->op1, FALSE) == TRUE){
			//Grab this out for convenience
			instruction_t* binary_operation = window->instruction3;

			//Now we'll modify this to be an assignment const statement
			binary_operation->op1_const = window->instruction2->op1_const;

			//Make sure that we now NULL out the first non-const operand for the future
			binary_operation->op1 = NULL;
			
			//Modify the type of the assignment
			binary_operation->CLASS = THREE_ADDR_CODE_ASSN_CONST_STMT;

			//Once we've done this, the first statement is entirely useless
			delete_statement(cfg, window->instruction2->block_contained_in, window->instruction2);

			//Once we've deleted the statement, we'll need to completely rewire the block
			
			//The second instruction is now the first one
			window->instruction2 = binary_operation;
			//Now instruction 2 becomes instruction 3
			window->instruction3 = window->instruction3->next_statement;

			//Set the status accordingly
			set_window_status(window);

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
	if(window->instruction1->CLASS == THREE_ADDR_CODE_ASSN_CONST_STMT 
		&& window->instruction2 != NULL
		&& window->instruction2->CLASS == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT
		&& window->instruction2->op == STAR
		&& window->instruction1->assignee->is_temporary == TRUE
		&& variables_equal(window->instruction2->op1, window->instruction1->assignee, FALSE) == TRUE){

		//We can multiply the constants now. The result will be stored in op1 const
		multiply_constants(window->instruction2->op1_const, window->instruction1->op1_const);

		//Instruction 2 is now simply an assign const statement
		window->instruction2->CLASS = THREE_ADDR_CODE_ASSN_CONST_STMT;
		//Null out where the old value was
		window->instruction2->op1 = NULL;

		//Instruction 1 is now completely useless
		delete_statement(cfg, window->instruction1->block_contained_in, window->instruction1);

		//We now need to rearrange this window
		window->instruction1 = window->instruction2;
		window->instruction2 = window->instruction2->next_statement;

		//Avoid any null pointer derefence
		if(window->instruction2 == NULL){
			window->instruction3 = NULL;
		} else {
			window->instruction3 = window->instruction2->next_statement;
		}

		//Update the window status
		set_window_status(window);

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
	 */
	//If we have two consecutive assignment statements
	if(window->instruction2 != NULL && window->instruction2->CLASS == THREE_ADDR_CODE_ASSN_STMT &&
		window->instruction1->CLASS == THREE_ADDR_CODE_ASSN_STMT){
		//Grab these out for convenience
		instruction_t* first = window->instruction1;
		instruction_t* second = window->instruction2;
		
		//If the variables are temp and the first one's assignee is the same as the second's op1, we can fold
		if(first->assignee->is_temporary == TRUE && variables_equal(first->assignee, second->op1, TRUE) == TRUE
			//If we bitwise AND their two indirection levels and get a value that isn't 0, we'd have our wrong case
			&& (first->assignee->indirection_level & second->op1->indirection_level) == 0
			&& (first->op1->indirection_level & second->assignee->indirection_level) == 0){

			//This is a special case we're we'll need to transfer the indirection over
			if(second->op1->indirection_level > 0 && first->op1->indirection_level == 0){
				first->op1->indirection_level = second->op1->indirection_level;
			}

			//Reorder the op1's
			second->op1 = first->op1;

			//We can now delete the first statement
			delete_statement(cfg, first->block_contained_in, first);

			//Once the first one has been deleted, we'll need to fold down
			window->instruction1 = second;
			window->instruction2 = second->next_statement;

			//We need to account for this case
			if(window->instruction2 == NULL){
				window->instruction3 = NULL;
			} else {
				window->instruction3 = window->instruction2->next_statement;
			}

			//Let the helper set the status
			set_window_status(window);

			//Regardless of what happened, we did see a change here
			changed = TRUE;
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
	if(window->instruction2 != NULL && window->instruction2->CLASS == THREE_ADDR_CODE_BIN_OP_STMT
		&& window->instruction1->CLASS == THREE_ADDR_CODE_ASSN_CONST_STMT){
		//Is the variable in instruction 1 temporary *and* the same one that we're using in instrution2? Let's check.
		if(window->instruction1->assignee->is_temporary == TRUE
			&& window->instruction2->op != DOUBLE_AND  //Due to the way we use these, we can't optimize in this way
			&& window->instruction2->op != DOUBLE_OR
			&& window->instruction2->op != F_SLASH //These are also excluded due to the way x86 division works
			&& window->instruction2->op != MOD
			&& variables_equal(window->instruction1->assignee, window->instruction2->op2, FALSE) == TRUE){
			//If we make it in here, we know that we may have an opportunity to optimize. We simply 
			//Grab this out for convenience
			instruction_t* const_assignment = window->instruction1;

			//Let's mark that this is now a binary op with const statement
			window->instruction2->CLASS = THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT;

			//We'll want to NULL out the secondary variable in the operation
			window->instruction2->op2 = NULL;
			
			//We'll replace it with the op1 const that we've gotten from the prior instruction
			window->instruction2->op1_const = const_assignment->op1_const;

			//We can now delete the very first statement
			delete_statement(cfg, window->instruction1->block_contained_in, window->instruction1);

			//Following this, we'll shift everything appropriately now that instruction1 is gone
			window->instruction1 = window->instruction2;
			window->instruction2 = window->instruction3;

			//If this is NULL, mark that we're at the end
			if(window->instruction2 == NULL){
				window->instruction3 = NULL;
			} else {
				//Otherwise we'll shift this forward
				window->instruction3 = window->instruction2->next_statement;
			}

			//Let the helper set the status
			set_window_status(window);

			//This does count as a change
			changed = TRUE;
		}
	}

	//Now check with 1 and 3. The prior compression may have made this more worthwhile
	if(window->instruction3 != NULL && window->instruction3->CLASS == THREE_ADDR_CODE_BIN_OP_STMT
		&& window->instruction1->CLASS == THREE_ADDR_CODE_ASSN_CONST_STMT){
		//Is the variable in instruction 1 temporary *and* the same one that we're using in instrution2? Let's check.
		if(window->instruction1->assignee->is_temporary == TRUE
			&& window->instruction3->op != DOUBLE_AND  //Due to the way we use these, we can't optimize in this way
			&& window->instruction3->op != DOUBLE_OR
			&& window->instruction3->op != F_SLASH //These are also excluded due to the way x86 division works
			&& window->instruction3->op != MOD
			&& variables_equal(window->instruction2->assignee, window->instruction3->op2, FALSE) == FALSE
			&& variables_equal(window->instruction1->assignee, window->instruction3->op2, FALSE) == TRUE){
			//If we make it in here, we know that we may have an opportunity to optimize. We simply 
			//Grab this out for convenience
			instruction_t* const_assignment = window->instruction1;
			printf("HERE\n");

			//Let's mark that this is now a binary op with const statement
			window->instruction3->CLASS = THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT;

			//We'll want to NULL out the secondary variable in the operation
			window->instruction3->op2 = NULL;
			
			//We'll replace it with the op1 const that we've gotten from the prior instruction
			window->instruction3->op1_const = const_assignment->op1_const;

			//We can now delete the very first statement
			delete_statement(cfg, window->instruction1->block_contained_in, window->instruction1);

			//Following this, we'll shift everything appropriately now that instruction1 is gone
			window->instruction1 = window->instruction2;
			window->instruction2 = window->instruction3;

			//Otherwise we'll shift this forward
			window->instruction3 = window->instruction2->next_statement;

			//Let the helper set the window status
			set_window_status(window);

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
	if(window->instruction2 != NULL 
		&& (window->instruction2->CLASS == THREE_ADDR_CODE_BIN_OP_STMT || window->instruction2->CLASS == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT)
		&& window->instruction1->CLASS == THREE_ADDR_CODE_ASSN_STMT && is_comparison_operator(window->instruction2->op) == TRUE){

		//Is the variable in instruction 1 temporary *and* the same one that we're using in instruction1? Let's check.
		if(window->instruction1->assignee->is_temporary == TRUE 
			&& window->instruction1->op1->is_temporary == FALSE
			&& variables_equal(window->instruction1->assignee, window->instruction2->op1, FALSE) == TRUE){

			//Set these two to be equal
			window->instruction2->op1 = window->instruction1->op1;

			//We can now delete the very first statement
			delete_statement(cfg, window->instruction1->block_contained_in, window->instruction1);

			//Following this, we'll shift everything appropriately now that instruction1 is gone
			window->instruction1 = window->instruction2;
			window->instruction2 = window->instruction3;

			//If this is NULL, mark that we're at the end
			if(window->instruction2 == NULL){
				window->instruction3 = NULL;
			} else {
				//Otherwise we'll shift this forward
				window->instruction3 = window->instruction2->next_statement;
			}

			//Let the helper set the status
			set_window_status(window);

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
	if(window->instruction1->CLASS == THREE_ADDR_CODE_ASSN_STMT
		&& window->instruction2 != NULL && (window->instruction2->CLASS == THREE_ADDR_CODE_BIN_OP_STMT ||
		window->instruction2->CLASS == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT) && window->instruction3 != NULL
		&& window->instruction3->CLASS == THREE_ADDR_CODE_ASSN_STMT){

		//Grab these out for convenience
		instruction_t* first = window->instruction1;
		instruction_t* second = window->instruction2;
		instruction_t* third = window->instruction3;

		//We still need further checks to see if this is indeed the pattern above. If
		//we survive all of these checks, we know that we're set to optimize
		if(first->assignee->is_temporary == TRUE && third->assignee->is_temporary == FALSE &&
			variables_equal_no_ssa(first->op1, third->assignee, FALSE) == TRUE &&
	 		variables_equal(first->assignee, second->op1, FALSE) == TRUE &&
	 		variables_equal(second->assignee, third->op1, FALSE) == TRUE){

			//The second op1 will now become the first op1
			second->op1 = first->op1;

			//And the second's assignee will now be the third's assignee
			second->assignee = third->assignee;

			//Following this, all we need to do is delete and rearrange
			delete_statement(cfg, first->block_contained_in, first);
			delete_statement(cfg, third->block_contained_in, third);

			//Now that the second is the only one left, we'll shift the window down
			//Second becomes first
			window->instruction1 = second;
			window->instruction2 = second->next_statement;

			//We need to account for cases where this happens
			if(window->instruction2 == NULL){
				window->instruction3 = NULL;
			} else {
				//Advance it
				window->instruction3 = window->instruction2->next_statement;
			}

			//Allow the helper to set this status
			set_window_status(window);

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
	if(window->instruction2 != NULL && window->instruction2->CLASS == THREE_ADDR_CODE_LEA_STMT
		&& window->instruction1->CLASS == THREE_ADDR_CODE_ASSN_CONST_STMT){
		//If the first instruction's assignee is temporary and it matches the lea statement, then we have a match
		if(window->instruction1->assignee->is_temporary == TRUE &&
	 		variables_equal(window->instruction1->assignee, window->instruction2->op2, FALSE) == TRUE){

			//What we can do is rewrite the LEA statement all together as a simple addition statement. We'll
			//evaluate the multiplication of the constant and lea multiplicator at comptime
			u_int64_t address_offset = window->instruction2->lea_multiplicator;

			//Let's now grab what the constant is
			three_addr_const_t* constant = window->instruction1->op1_const;
			
			//What kind of constant do we have?
			if(constant->const_type == INT_CONST || constant->const_type == HEX_CONST
			   || constant->const_type == INT_CONST_FORCE_U){
				//If this is a the case, we'll multiply the address const by the int value
				address_offset *= constant->int_const;
			//Otherwise, this has to be a long const
			} else {
				address_offset *= constant->long_const;
			}

			//Once we've done this, the address offset is now properly multiplied. We'll reuse
			//the constant from operation one, and convert the lea statement into a BIN_OP_WITH_CONST
			//statement. This saves a lot of loading and arithmetic operations
		
			//This is now a long const
			constant->const_type = LONG_CONST;

			//Set this to be the address offset
			constant->long_const = address_offset;

			//Add it into instruction 2
			window->instruction2->op1_const = constant;

			//We'll now transfrom instruction 2 into a bin op with const
			window->instruction2->op2 = NULL;
			window->instruction2->op = PLUS;
			window->instruction2->CLASS = THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT;

			//We can now scrap the first instruction entirely
			delete_statement(cfg, window->instruction1->block_contained_in, window->instruction1);

			//And now, we'll shift everything to the left
			window->instruction1 = window->instruction2;
			window->instruction2 = window->instruction3;
			
			//If this is NULL, avoid a null pointer reference
			if(window->instruction2 == NULL){
				window->instruction3 = NULL;
			} else {
				//Otherwise we'll set it to be the next one
				window->instruction3 = window->instruction2->next_statement;
			}

			//Allow the helper to set the status
			set_window_status(window);

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
		&& window->instruction2->CLASS == THREE_ADDR_CODE_ASSN_STMT
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
		delete_statement(cfg, window->instruction2->block_contained_in, window->instruction2);

		//We'll need to update the window now
		window->instruction2 = window->instruction3;
		window->instruction3 = window->instruction2->next_statement;

		//Update the window status
		set_window_status(window);
		
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
	if((window->instruction1->CLASS == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT
		|| window->instruction1->CLASS == THREE_ADDR_CODE_BIN_OP_STMT)
		&& window->instruction2 != NULL
		&& window->instruction2->CLASS == THREE_ADDR_CODE_ASSN_STMT){
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
			delete_statement(cfg, second->block_contained_in, second);

			//Once it's been deleted, we'll need to take the appropriate steps to update the window
			window->instruction2 = third;

			//If this one is already NULL, we know we're at the end
			if(third == NULL){
				window->instruction3 = NULL;
			} else {
				window->instruction3 = third->next_statement;
			}

			//Allow the helper to set the status
			set_window_status(window);

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
			delete_statement(cfg, second->block_contained_in, second);

			//Now we'll modify the window to be as we need
			window->instruction2 = third;

			//If this one is already NULL, we know we're at the end
			if(third == NULL){
				window->instruction3 = NULL;
			} else {
				window->instruction3 = third->next_statement;
			}

			//Allow the helper to set the status
			set_window_status(window);

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

		if(current_instruction != NULL && current_instruction->CLASS == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT){
			//Grab this out for convenience
			three_addr_const_t* constant = current_instruction->op1_const;

			//By default, we assume it's not 0
			u_int8_t const_is_0 = FALSE;
			//Is the const a 1?
			u_int8_t const_is_1 = FALSE;
			//Is the const a power of 2?
			u_int8_t const_is_power_of_2 = FALSE;

			//What kind of constant do we have?
			if(constant->const_type == INT_CONST || constant->const_type == HEX_CONST
			   || constant->const_type == INT_CONST_FORCE_U){
				//Set the flag if we find anything
				if(constant->int_const == 0){
					const_is_0 = TRUE;
				} else if (constant->int_const == 1){
					const_is_1 = TRUE;
				} else {
					const_is_power_of_2 = is_power_of_2(constant->int_const);
				}

			//Otherwise, this has to be a long const
			} else if(constant->const_type == LONG_CONST || constant->const_type == LONG_CONST_FORCE_U){
				//Set the flag if we find zero
				if(constant->long_const == 0){
					const_is_0 = TRUE;
				} else if(constant->long_const == 1){
					const_is_1 = TRUE;
				} else {
					const_is_power_of_2 = is_power_of_2(constant->long_const);
				}

			//If we have a character constant, this is also a candidate
			} else if(constant->const_type == CHAR_CONST){
				//Set the flag if we find zero
				if(constant->char_const == 0){
					const_is_0 = TRUE;
				} else if(constant->char_const == 1){
					const_is_1 = TRUE;
				} else {
					const_is_power_of_2 = is_power_of_2(constant->long_const);
				}
			}
		
			//If this is 0, then we can optimize
			if(const_is_0 == TRUE){
				//If we made it out of this conditional with the flag being set, we can simplify.
				//If this is the case, then this just becomes a regular assignment expression
				if(current_instruction->op == PLUS || current_instruction->op == MINUS){
					//We're just assigning here
					current_instruction->CLASS = THREE_ADDR_CODE_ASSN_STMT;
					//Wipe the values out
					current_instruction->op1_const = NULL;
					current_instruction->op2 = NULL;
					//We changed something
					changed = TRUE;
				//If this is a multiplication, we'll turn this into a 0 assignment
				} else if(current_instruction->op == STAR){
					//Now we're assigning a const
					current_instruction->CLASS = THREE_ADDR_CODE_ASSN_CONST_STMT;
					//The constant is still the same thing(0), let's just wipe out the ops
					current_instruction->op1 = NULL;
					current_instruction->op2 = NULL;
					//We changed something
					changed = TRUE;
				//We'll need to throw a warning here about 0 division
				// TODO ADD MORE
				} else if (current_instruction->op == F_SLASH || current_instruction->op == MOD){
					//Throw a warning, not much else to do here
					print_parse_message(PARSE_ERROR, "Division by 0 will always error", 0);
					//Throw a failure in this case
					exit(0);
				}

				//Notice how we do NOT mark any change as true here. This is because, even though yes we
				//did change the instructions, the sliding window itself did not change at all. This is
				//an important note as if we did mark a change, there are cases where this could
				//cause an infinite loop
			
			//What if this is a 1? Well if it is, we can transform this statement into an inc or dec statement
			//if it's addition or subtraction, or we can turn it into a simple assignment statement if it's multiplication
			//or division
			} else if(const_is_1 == TRUE){
				//If it's an addition statement, turn it into an inc statement
				/**
				 * NOTE: for addition and subtraction, since we'll be turning this into inc/dec statements, we'll
				 * want to first make sure that the assignees are not temporary variables. If they are temporary variables,
				 * then doing this would mess the whole operation up
				 */
				if(current_instruction->op == PLUS && current_instruction->assignee->is_temporary == FALSE){
					//Now turn it into an inc statement
					current_instruction->CLASS = THREE_ADDR_CODE_INC_STMT;
					//Wipe the values out
					current_instruction->op1_const = NULL;
					current_instruction->op = BLANK;
					//We changed something
					changed = TRUE;
				//Otherwise if we have a minus, we can turn this into a dec statement
				} else if(current_instruction->op == MINUS && current_instruction->assignee->is_temporary == FALSE){
					//Change what the class is
					current_instruction->CLASS = THREE_ADDR_CODE_DEC_STMT;
					//Wipe the values out
					current_instruction->op1_const = NULL;
					current_instruction->op = BLANK;
					//We changed something
					changed = TRUE;
				//What if we have multiplication or division? If so, multiplying/dividing by 1
				//is idempotent, so we can transform these into assignment statements
				} else if(current_instruction->op == STAR || current_instruction->op == F_SLASH){
					//Change it to a regular assignment statement
					current_instruction->CLASS = THREE_ADDR_CODE_ASSN_STMT;
					//Wipe the operator out
					current_instruction->op1_const = NULL;
					current_instruction->op = BLANK;
					//We changed something
					changed = TRUE;
				}
			//What if we have a power of 2 here? For any kind of multiplication or division, this can
			//be optimized into a left or right shift if we have a compatible type(not a float)
			} else if(const_is_power_of_2 && current_instruction->assignee->type->type_class == TYPE_CLASS_BASIC 
				&& current_instruction->assignee->type->basic_type->basic_type != FLOAT32 
				&& current_instruction->assignee->type->basic_type->basic_type != FLOAT64){

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
	if(window->instruction2 != NULL && window->instruction2->CLASS == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT
		&& window->instruction2->op == PLUS && window->instruction1->CLASS == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT 
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

			//Now that we've done that, we'll modify the second equation's op1 to be the first equation's op1
			second->op1 = first->op1;

			//Now that this is done, we can remove the first equation
			delete_statement(cfg, first->block_contained_in, first);

			//And now we shift everything back by 1
			window->instruction1 = second;
			window->instruction2 = second->next_statement;

			//Handle the potentiality for null values here
			if(window->instruction2 == NULL){
				window->instruction3 = NULL;
			} else {
				window->instruction3 = window->instruction2->next_statement;
			}

			//And now we can update the status
			set_window_status(window);

			//This counts as a change because we deleted
			changed = TRUE;
		}
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
		} while(window.status != WINDOW_AT_END);


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

	//If the global variables are not null, then these
	//are the global variables
	if(cfg->global_variables != NULL){
		head_block = cfg->global_variables;
		previous = head_block;
	} else {
		previous = head_block = NULL;
	}
	
	//We'll need to use a queue every time, we may as well just have one big one
	heap_queue_t* queue = heap_queue_alloc();

	//For each function
	for(u_int16_t _ = 0; _ < cfg->function_blocks->current_index; _++){
		//Grab the function block out
		basic_block_t* func_block = dynamic_array_get_at(cfg->function_blocks, _);

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
					delete_statement(cfg, previous, previous->exit_statement);
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
	if(block->block_type == BLOCK_TYPE_SWITCH || block->jump_table.nodes != NULL){
		print_jump_table(&(block->jump_table));
	}

	//If it's a function entry block, we need to print this out
	if(block->block_type == BLOCK_TYPE_FUNC_ENTRY){
		printf("%s:\n", block->function_defined_in->func_name);
		print_stack_data_area(&(block->function_defined_in->data_area));
	} else {
		printf(".L%d:\n", block->block_id);
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
			print_three_addr_code_stmt(cursor);
		} else {
			print_instruction(cursor, PRINTING_VAR_IN_INSTRUCTION);
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
