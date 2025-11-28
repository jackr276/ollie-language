/**
 * Author: Jack Robbins
 *
 * This file stores the implementations for the APIs defined in the header file of the same name
*/

#include "instruction_scheduler.h"
#include <sys/types.h>


/**
 * Build the dependency graph inside of a block. We're also given our instruction list here for reference
 */
static void build_dependency_graph_for_block(basic_block_t* block, instruction_t** instructions){
	//Run through the instruction list backwards. Logically speaking, we're going to
	//find the instruction with the maximum number of dependencies later on down in the block
	for(int32_t i = block->number_of_instructions - 1; i >= 0; i--){
		//Extract it
		instruction_t* current = instructions[i];

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
