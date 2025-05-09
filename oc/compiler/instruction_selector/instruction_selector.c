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
#include <stdio.h>
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
		print_instruction(window->instruction1);
	} else {
		printf("EMPTY\n");
	}

	if(window->instruction2 != NULL){
		print_instruction(window->instruction2);
	} else {
		printf("EMPTY\n");
	}
	
	if(window->instruction3 != NULL){
		print_instruction(window->instruction3);
	} else {
		printf("EMPTY\n");
	}

	printf("-------------------------------------------\n");
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
 * Select the size of a given variable based on its type
 */
static variable_size_t select_variable_size(three_addr_var_t* variable){
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
			//We round up to 16 bit here
			case U_INT8:
			case S_INT8:
			case U_INT16:
			case S_INT16:
			case CHAR:
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
				size = QUAD_WORD;
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
 * A very simple helper function that selects the right move instruction based
 * solely on variable size. Done to avoid code duplication
 */
static instruction_type_t select_move_instruction(variable_size_t size){
	//Go based on size
	switch(size){
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
 * A very simple helper function that selects the right lea instruction based
 * solely on variable size. Done to avoid code duplication
 */
static instruction_type_t select_lea_instruction(variable_size_t size){
	//Go based on size
	switch(size){
		case WORD:
		case DOUBLE_WORD:
			return LEA;
		case QUAD_WORD:
			return LEAQ;
		default:
			return LEAQ;
	}
}


/**
 * Select the size of a constant based on its type
 */
variable_size_t select_constant_size(three_addr_const_t* constant){
	variable_size_t size;

	//This are all 32 bit
	if(constant->const_type == INT_CONST || constant->const_type == INT_CONST_FORCE_U
		|| constant->const_type == HEX_CONST){
		size = DOUBLE_WORD;

	//Default for a float is double precision
	} else if(constant->const_type == FLOAT_CONST){
		size = DOUBLE_PRECISION;

	//These are all 64 bit
	} else if(constant->const_type == LONG_CONST || constant->const_type == LONG_CONST_FORCE_U){
		size = QUAD_WORD;

	//Sane default
	} else {
		size = QUAD_WORD;
	}

	return size;
}


/**
 * Handle a register/immediate to memory move type instruction selection with an address calculation
 *
 * DOES NOT DO DELETION/WINDOW REORDERING
 */
static void handle_address_calc_to_memory_move(instruction_t* address_calculation, instruction_t* memory_access){
	//Select the variable size
	variable_size_t size;

	//Select the appropriate size for later
	if(memory_access->op1 != NULL){
		size = select_variable_size(memory_access->op1);
	//Otherwise it must be a constant
	} else {
		size = select_constant_size(memory_access->op1_const);
	}

	//Now based on the size, we can select what variety to register/immediate to memory move we have here
	switch (size) {
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
	if(address_calculation->CLASS == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT){

		
	//Or if we have a statement like this(rare but may happen, covering our bases)
	} else if(address_calculation->CLASS == THREE_ADDR_CODE_BIN_OP_STMT){
 
	//Another very common case - a lea statement
	} else if(address_calculation->CLASS == THREE_ADDR_CODE_LEA_STMT){

	}

}


/**
 * Handle a memory to register move type instruction selection with an address calculation
 *
 * DOES NOT DO DELETION/WINDOW REORDERING
 */
static void handle_address_calc_from_memory_move(instruction_t* address_calculation, instruction_t* memory_access){
	//Select the variable size
	variable_size_t size;

	//Select the appropriate size for later
	if(memory_access->op1 != NULL){
		size = select_variable_size(memory_access->op1);
	//Otherwise it must be a constant
	} else {
		size = select_constant_size(memory_access->op1_const);
	}

	//Use the helper to get the right sized move instruction
	memory_access->instruction_type = select_move_instruction(size);
	
	//Now based on the size, we can select what variety to register/immediate to memory move we have here
	switch (size) {
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
		//Assign memory access's op1_const to be this value
		//memory_access->op2_const = address_calculation->op1_const;

		//Now we'll need to set the assignee to be the base address. This makes the most sense
		//memory_access->assignee = address_calculation->assignee;

		//That should really be all. After we do all of this, the actual address calculation instruction is useless.
		//The helper will delete it

	//Or if we have a statement like this(rare but may happen, covering our bases)
	} else if(address_calculation->CLASS == THREE_ADDR_CODE_BIN_OP_STMT){
 
	//Another very common case - a lea statement
	} else if(address_calculation->CLASS == THREE_ADDR_CODE_LEA_STMT){

	}
}




/**
 * Handle a bin-op-with-const statement
 *
 * We can translate a bin op with const operation a few different ways based on the 
 * operand
 */
static void handle_binary_operation_with_const_instruction(instruction_t* instruction){
	//Determine what our size is off the bat
	variable_size_t size = select_variable_size(instruction->assignee);

	//Go based on what we have as the operation
	switch(instruction->op){
		/**
		 * There are 2 routes we could take with a plus. 
		 * 	1.) t4 <- t2 + 3: since the operand and assignee are different, we can
		 * 		use an address calculation instruction to do this. 
		 * 		We'll make this lea(q) 3(t2), t4
		 *
		 * 	2.) x3 <- x2 + 4: since the operand and assignee are the same, we're fine overwriting.
		 * 		This can be a regular addition instruction.
		 * 		We'll make this: add(w/l/q) $4, x3
		 */
		case PLUS:
			//Let's see if we have case 1
			if(variables_equal(instruction->assignee, instruction->op1, FALSE) == FALSE){
				//Grab the lea that we need and set it to be the instruction type
				instruction->instruction_type = select_lea_instruction(size);

				//We'll need to perform an address calculation move. This will fall under
				//the category of ADDRESS_CALCULATION_MODE_CONST_ONLY
				//Set this flag for later
				instruction->calculation_mode = ADDRESS_CALCULATION_MODE_CONST_ONLY;

				//Now assign the constants appropriately
				instruction->destination_register = instruction->assignee;

				//The source register is op1
				instruction->source_register = instruction->op1;
				//And the constant additive is op1 const
				instruction->constant_additive = instruction->op1_const;
			//Else we've got case 2
			} else {

			}

			break;

		case MINUS:
			break;
		case STAR:
			break;
		case F_SLASH:
			break;
		default:
			break;
	}

}


/**
 * Handle a bin-op-with-const statement
 *
 * We can translate a bin op statement a few differnet ways based on the operand
 */
static void handle_binary_operation_intruction(instruction_t* instruction){
	switch(instruction->op){
		case PLUS:

		case MINUS:

		case STAR:

		case F_SLASH:
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
		instruction->instruction_type = INC;
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
		instruction->instruction_type = DEC;
	}

	//Set the destination as the assignee
	instruction->destination_register = instruction->assignee;
}


/**
 * Handle a regular move condition
 */
static void handle_to_register_move_instruction(instruction_t* instruction){
	//Select the variable size
	variable_size_t size;

	//Select the appropriate size for later
	if(instruction->op1 != NULL){
		size = select_variable_size(instruction->op1);
		//May as well set this here
		instruction->source_register = instruction->op1;
	//Otherwise it must be a constant
	} else {
		size = select_constant_size(instruction->op1_const);
		//Set this as well if we can
		instruction->source_immediate = instruction->op1_const;
	}

	//Use the helper to get the right sized move instruction
	instruction->instruction_type = select_move_instruction(size);
	
	//We've already set the sources, now we set the destination as the assignee
	instruction->destination_register = instruction->assignee;
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

	//============================= The "Grand Patterns" ==============================
	//These are patterns that span multiple instructions. Often we're able to
	//condense these multiple instructions into one singular x86 instruction

	/**
	 * =========================== Memory Movement Instructions =======================
	 * Moving from memory to a register or vice versa often presents opportunities, because
	 * we're able to make use of memory addressing mode. This will arise whenever we do
	 * a register-to-memory "store" or a memory-to-register "load". Remember, in x86 assembly
	 * we can't go from memory-to-memory, so every memory access operation will fall within
	 * this category
	 *
	 * First handle to memory movement
	 * Example:
	 * t26 <- t24 + 4
	 * (t26) <- 3
	 *
	 * should become:
	 * mov(w/l/q) $3, 4(t24)
	 */
	//If we have some kind of offset calculation followed by a dereferencing assingment, we have either a 
	//register to memory or immediate to memory move. Either way, we can rewrite this using address computation mode
	if(window->instruction1->instruction_type == NONE 
		&& window->instruction1->CLASS == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT 
		&& window->instruction1->op == PLUS 
		&& window->instruction2 != NULL
		&& window->instruction2->instruction_type == NONE 
		&& (window->instruction2->CLASS == THREE_ADDR_CODE_ASSN_STMT || window->instruction2->CLASS == THREE_ADDR_CODE_ASSN_CONST_STMT)
		&& variables_equal(window->instruction1->assignee, window->instruction2->assignee, TRUE) == TRUE
		&& window->instruction2->assignee->indirection_level == 1){

		/*
		//Use the helper to keep things somewhat clean in here
		handle_address_calc_to_memory_move(window->instruction1, window->instruction2);

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

		set_window_status(window);

		//This counts as a change
		//changed = TRUE;
		*/
	}

	//We also need to perform the exact same kind of optimization for instructions 2 and 3
	if(window->instruction2 != NULL && window->instruction2->CLASS == THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT && window->instruction2->op == PLUS 
		&& window->instruction3 != NULL &&
		(window->instruction3->CLASS == THREE_ADDR_CODE_ASSN_STMT || window->instruction3->CLASS == THREE_ADDR_CODE_ASSN_CONST_STMT)
		&& variables_equal(window->instruction2->assignee, window->instruction3->assignee, TRUE) == TRUE
		&& window->instruction3->assignee->indirection_level == 1){

		/*
		//Use the helper to keep things somewhat clean in here
		handle_address_calc_to_memory_move(window->instruction2, window->instruction3);

		//This counts as a change
		//changed = TRUE;
		*/
	}

	return changed;
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
			//One-to-one mapping to nop
			case THREE_ADDR_CODE_IDLE_STMT:
				current->instruction_type = NOP;
				break;
			//One to one mapping here as well
			case THREE_ADDR_CODE_RET_STMT:
				current->instruction_type = RET;
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
			case THREE_ADDR_CODE_BIN_OP_STMT:
				handle_binary_operation_intruction(current);
				break;
			//Same here
			case THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT:
				handle_binary_operation_with_const_instruction(current);
				break;
			default:
				break;
		}
	}
}


/**
 * Run through every block and convert each instruction or sequence of instructions
 * from three address code to assembly statements
 */
static void select_instructions(cfg_t* cfg, basic_block_t* head_block){
	//First we'll grab the head
	basic_block_t* current = head_block;

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
 * Take in two constant and a binary operand and combine them in whichever way we can
 */
three_addr_const_t* combine_constants(three_addr_const_t* const1, three_addr_const_t* const2, Token operator){

	//We always give back the result in const1
	return const1;
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
	 * --------------------- Redundnant copying elimination ------------------------------------
	 *  Let's now fold redundant copies. Here is an example of a redundant copy
	 * 	t10 <- x_2
	 * 	t11 <- t10
	 *
	 * This can be folded into simply:
	 * 	t11 <- x_2
	 */
	//If we have two consecutive assignment statements
	if(window->instruction2 != NULL && window->instruction2->CLASS == THREE_ADDR_CODE_ASSN_STMT &&
		window->instruction1->CLASS == THREE_ADDR_CODE_ASSN_STMT){
		//Grab these out for convenience
		instruction_t* first = window->instruction1;
		instruction_t* second = window->instruction2;
		
		//If the variables are temp and the first one's assignee is the same as the second's op1, we can fold
		if(first->assignee->is_temporary == TRUE && variables_equal(first->assignee, second->op1, FALSE) == TRUE){
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
	 */
	//Check first with 1 and 2
	if(window->instruction2 != NULL && window->instruction2->CLASS == THREE_ADDR_CODE_BIN_OP_STMT
		&& window->instruction1->CLASS == THREE_ADDR_CODE_ASSN_CONST_STMT){
		//Is the variable in instruction 1 temporary *and* the same one that we're using in instrution2? Let's check.
		if(window->instruction1->assignee->is_temporary == TRUE &&
			variables_equal(window->instruction1->assignee, window->instruction2->op2, FALSE) == TRUE){
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
		if(window->instruction1->assignee->is_temporary == TRUE &&
			variables_equal(window->instruction1->assignee, window->instruction3->op2, FALSE) == TRUE){
			//If we make it in here, we know that we may have an opportunity to optimize. We simply 
			//Grab this out for convenience
			instruction_t* const_assignment = window->instruction1;

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
				} else {
					//Throw a warning, not much else to do here
					print_parse_message(WARNING, "Division by 0 will always error", 0);
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
			//be optimized into a  left or right shift if we have a compatible type(not a float)
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
		generic_type_t* final_type = types_compatible(second->op1_const->type, first->op1_const->type);

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
	 * ------------------------ Optimizing adjacent statements into LEA statements ----------------
	 *  In cases where we have a multiplication statement next to an addition statement, or vice versa,
	 *  odds are we can write it as a lea statement
	 *
	 */
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
		printf("%s:\n", block->func_record->func_name);
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
			print_instruction(cursor);
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
basic_block_t* select_all_instructions(cfg_t* cfg){
	//Our very first step in the instruction selector is to order all of the blocks in one 
	//straight line. This step is also able to recognize and exploit some early optimizations,
	//such as when a block ends in a jump to the block right below it
	basic_block_t* head_block = order_blocks(cfg);

	//DEBUG
	//We'll first print before we simplify
	printf("============================== BEFORE SIMPLIFY ========================================\n");
	print_ordered_blocks(head_block, PRINT_THREE_ADDRESS_CODE);

	printf("============================== AFTER SIMPLIFY ========================================\n");
	//Once we've printed, we now need to simplify the operations. OIR already comes in an expanded
	//format that is used in the optimization phase. Now, we need to take that expanded IR and
	//recognize any redundant operations, dead values, unnecessary loads, etc.
	simplify(cfg, head_block);
	print_ordered_blocks(head_block, PRINT_THREE_ADDRESS_CODE);

	printf("============================== AFTER INSTRUCTION SELECTION ========================================\n");
	//Once we're done simplifying, we'll use the same sliding window technique to select instructions.
	select_instructions(cfg, head_block);
	//Print them out
	print_ordered_blocks(head_block,PRINT_INSTRUCTION);

	//FOR NOW
	return NULL;
}

