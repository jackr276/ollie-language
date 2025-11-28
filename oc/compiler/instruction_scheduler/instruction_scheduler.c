/**
 * Author: Jack Robbins
 *
 * This file stores the implementations for the APIs defined in the header file of the same name
*/

#include "instruction_scheduler.h"
#include "../data_dependency_graph/data_dependency_graph.h"
#include <sys/types.h>


/**
 * Does the given instruction have a *Data Dependence* on the candidate. We will know
 * if the instruction does depend on it if the candidate *assigns* the one of the source
 * values in the instruction
 *
 * Once we find a dependence, we add it an leave. There's no point in searching on once
 * one has been established. For this reason, we will search the most used dependency's first
 *
 * Candidate: movb $1, t5
 * ...
 * ...
 * Given: addb t4, t5
 *
 * Then we need to establish a dependence between given and the candidate
 */
static void update_dependence(instruction_t* given, instruction_t* candidate){
	printf("Checking:");
	print_instruction(stdout, candidate, PRINTING_VAR_IN_INSTRUCTION);

	//The candidate never even assigns anything, so why bother checking
	if(is_destination_assigned(candidate) == FALSE){
		return;
	}

	//We are looking to see if the candidate's destination register
	//is used by us
	three_addr_var_t* destination_register = candidate->destination_register;

	//No point in going on here if there is no destination register to speak of
	//in the candidate
	if(destination_register == NULL){
		return;
	}

	//Go for the source first
	if(given->source_register != NULL){
		//These variables are equal, we have a dependence
		if(variables_equal(given->source_register, destination_register, FALSE) == TRUE){
			//Given depends on candidate
			add_dependence(candidate, given);
			return;
		}
	}

	/**
	 * Recall the unique procedures for the destination register
	 *	
	 *	1.) It can be a source & a destination(is_destination_also_source)
	 *	2.) It can exclusively be a source(we ruled this out at the top)
	 *	3.) It's just a destination, in which case we don't care(we'd just skip here)
	 */
	if(is_destination_also_operand(given) == TRUE){
		//These variables are equal, we have a dependence
		if(variables_equal(given->destination_register, destination_register, FALSE) == TRUE){
			//Given depends on candidate
			add_dependence(candidate, given);
			return;
		}
	}

	//Second source
	if(given->source_register2 != NULL){
		//These variables are equal, we have a dependence
		if(variables_equal(given->source_register2, destination_register, FALSE) == TRUE){
			//Given depends on candidate
			add_dependence(candidate, given);
			return;
		}
	}

	//Address calc registers
	if(given->address_calc_reg1 != NULL){
		//These variables are equal, we have a dependence
		if(variables_equal(given->address_calc_reg1, destination_register, FALSE) == TRUE){
			//Given depends on candidate
			add_dependence(candidate, given);
			return;
		}
	}

	//Address calc registers
	if(given->address_calc_reg2 != NULL){
		//These variables are equal, we have a dependence
		if(variables_equal(given->address_calc_reg2, destination_register, FALSE) == TRUE){
			//Given depends on candidate
			add_dependence(candidate, given);
			return;
		}
	}

	//Now run through all of the parameters *if* they exist
	if(given->parameters != NULL){
		//Run through them all
		dynamic_array_t* params = given->parameters;
		for(u_int16_t i = 0; i < params->current_index; i++){
			//Extract it
			three_addr_var_t* param = dynamic_array_get_at(params, i);

			//Add it and leave if so
			if(variables_equal(param, destination_register, FALSE) == TRUE){
				add_dependence(candidate, given);
				return;
			}
		}
	}


	/**
	 * Recall that there exist some instructions that have 2 destination registers.
	 * If this is the case, we will need to perform all of this checking again
	 */
	three_addr_var_t* destination_register2 = given->destination_register2;
	if(destination_register2 == NULL){
		return;
	}

	//Go for the source first
	if(given->source_register != NULL){
		//These variables are equal, we have a dependence
		if(variables_equal(given->source_register, destination_register2, FALSE) == TRUE){
			//Given depends on candidate
			add_dependence(candidate, given);
			return;
		}
	}

	/**
	 * Recall the unique procedures for the destination register
	 *	
	 *	1.) It can be a source & a destination(is_destination_also_source)
	 *	2.) It can exclusively be a source(we ruled this out at the top)
	 *	3.) It's just a destination, in which case we don't care(we'd just skip here)
	 */
	if(is_destination_also_operand(given) == TRUE){
		//These variables are equal, we have a dependence
		if(variables_equal(given->destination_register, destination_register2, FALSE) == TRUE){
			//Given depends on candidate
			add_dependence(candidate, given);
			return;
		}
	}

	//Second source
	if(given->source_register2 != NULL){
		//These variables are equal, we have a dependence
		if(variables_equal(given->source_register2, destination_register2, FALSE) == TRUE){
			//Given depends on candidate
			add_dependence(candidate, given);
			return;
		}
	}

	//Address calc registers
	if(given->address_calc_reg1 != NULL){
		//These variables are equal, we have a dependence
		if(variables_equal(given->address_calc_reg1, destination_register2, FALSE) == TRUE){
			//Given depends on candidate
			add_dependence(candidate, given);
			return;
		}
	}

	//Address calc registers
	if(given->address_calc_reg2 != NULL){
		//These variables are equal, we have a dependence
		if(variables_equal(given->address_calc_reg2, destination_register2, FALSE) == TRUE){
			//Given depends on candidate
			add_dependence(candidate, given);
			return;
		}
	}

	//Now run through all of the parameters *if* they exist
	if(given->parameters != NULL){
		//Run through them all
		dynamic_array_t* params = given->parameters;
		for(u_int16_t i = 0; i < params->current_index; i++){
			//Extract it
			three_addr_var_t* param = dynamic_array_get_at(params, i);

			//Add it and leave if so
			if(variables_equal(param, destination_register2, FALSE) == TRUE){
				add_dependence(candidate, given);
				return;
			}
		}
	}
}


/**
 * Build the dependency graph inside of a block. We're also given our instruction list here for reference
 */
static void build_dependency_graph_for_block(basic_block_t* block, instruction_t** instructions){
	//Run through the instruction list backwards. Logically speaking, we're going to
	//find the instruction with the maximum number of dependencies later on down in the block
	for(int32_t i = block->number_of_instructions - 1; i >= 0; i--){
		//Extract it
		instruction_t* current = instructions[i];

		printf("On instruction:\n");
		print_instruction(stdout, current, PRINTING_VAR_IN_INSTRUCTION);
		printf("\n");

		//For this instruction, we need to backtrace through the list and figure out:
		//	1.) Do the dependencies get assigned in this block? It is fully possible
		//	that they do not
		//	2.) If they do get assigned in this block, what are those instructions that
		//	are doing the assignment
		
		//Let's now backtrace from this current instruction up the chain. We can
		//start at this given instruction's immediate predecessor
		for(int32_t j = i - 1; j >= 0; j--){
			//Extract it
			instruction_t* candidate = instructions[j];


			//Update the dependence
			update_dependence(current, candidate);
		}
	}
}


/**
 * Run through a block and perform the reordering/scheduling in it step by step.
 * Once this function returns, we can consider that block 100% done from a scheduling perspective
 */
static void schedule_instructions_in_block(basic_block_t* block, u_int8_t debug_printing, u_int8_t print_irs){
	/**
	 * Step 0: load all of the instructions into a static array. This is going
	 * to be an efficiency boost because we need to traverse up and down
	 * the block to find assignments for our data relationships
	 */
	//A list of all instructions in the block. We actually have a set
	//number of instructions in the block, which allows us to do this
	instruction_t* instructions[block->number_of_instructions];

	//Grab a cursor
	instruction_t* instruction_cursor = block->leader_statement;
	
	//Current index in the list
	u_int32_t list_index = 0;

	//Run through and add them all in
	while(instruction_cursor != NULL){
		//Add it in
		instructions[list_index] = instruction_cursor;

		//Increment
		list_index++;

		//Now we advance
		instruction_cursor = instruction_cursor->next_statement;
	}

	//By the time we're here, we now have a list that we can traverse
	//quicker than if we had to use the linked list approaach

	/**
	 * Step 1: build the data dependency graph inside of the block. This is done by
	 * the helper function. Nothing else can be done until this is done
	 */
	build_dependency_graph_for_block(block, instructions);

}


/**
 * Root level function that is exposed via the API
 */
cfg_t* schedule_all_instructions(cfg_t* cfg, compiler_options_t* options){
	//Grab these flags for later
	u_int8_t debug_printing = options->enable_debug_printing;
	u_int8_t print_irs = options->print_irs;

	/**
	 * Really all that we'll do here is invoke the block
	 * schedule for each basic block in the graph. Blocks
	 * are scheduled independent of other blocks, so we
	 * don't need to worry about our current function or
	 * anything like that here
	*/
	//For every single function
	for(u_int16_t i = 0; i < cfg->function_entry_blocks->current_index; i++){
		//Grab the function entry
		basic_block_t* cursor = dynamic_array_get_at(cfg->function_entry_blocks, i);

		//Run through everything in here
		while(cursor != NULL){
			//Invoke the block scheduler itself
			schedule_instructions_in_block(cursor, debug_printing, print_irs);

			//Advance it up using the direct successor
			cursor = cursor->direct_successor;
		}
	}

	//Give back the final CFG. This is more symbolic
	//than anything, the CFG itself has been modified by
	//the whole procedure
	return cfg;
}
