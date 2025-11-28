/**
 * Author: Jack Robbins
 *
 * This file stores the implementations for the APIs defined in the header file of the same name
*/

#include "instruction_scheduler.h"
#include <sys/types.h>


/**
 * Build the dependency graph inside of a block
 */
static void build_dependency_graph_for_block(basic_block_t* block){

}


/**
 * Run through a block and perform the reordering/scheduling in it
 */
static void schedule_instructions_in_block(basic_block_t* block, u_int8_t debug_printing, u_int8_t print_irs){

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
