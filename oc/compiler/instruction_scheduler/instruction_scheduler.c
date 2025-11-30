/**
 * Author: Jack Robbins
 *
 * This file stores the implementations for the APIs defined in the header file of the same name
*/

#include "instruction_scheduler.h"
#include "../data_dependency_graph/data_dependency_graph.h"
#include <stdint.h>
#include <stdio.h>
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
 * We are looking for only a specific variable here(say t4). Once we find that variable's
 * definition, we're done for that given variable
 */
static void update_dependence_for_variable(data_dependency_graph_t* graph, instruction_t* given, instruction_t** instructions, three_addr_var_t* variable, u_int32_t start){
	//Predeclare due to the switch
	three_addr_var_t* destination;
	three_addr_var_t* destination2;

	//Run through all of the instructions starting at what we were given
	for(int32_t i = start; i >= 0; i--){
		//Extract it
		instruction_t* current = instructions[i];
		
		//If we don't even deal with the destination why bother
		if(is_destination_assigned(current) == FALSE){
			continue;
		}

		//Some instructions(namely compares), have
		//special rules where we hang onto the assignee
		//for just this reason
		switch(current->instruction_type){
			case CMPQ:
			case CMPW:
			case CMPL:
			case CMPB:
			case TESTB:
			case TESTL:
			case TESTW:
			case TESTQ:
				//The cmp instructions store their symbolic assignees in the assignee slot
				if(variables_equal(current->assignee, variable, FALSE) == TRUE){
					//Add it in
					add_dependence(graph, current, given);
					return;
				}

				break;
			
			//All others we just leave
			default:
				//Grab these out
				destination = current->destination_register;
				destination2 = current->destination_register2;

				//If they're equal then we're good
				if(variables_equal(destination, variable, FALSE) == TRUE){
					//Given depends on current
					add_dependence(graph, current, given);

					//We're done
					return;
				}

				//We're also done here
				if(variables_equal(destination2, variable, FALSE) == TRUE){
					//Given depends on current
					add_dependence(graph, current, given);

					//We're done
					return;
				}

				break;
		}

		//Otherwise we just keep going
	}
}


/**
 * Build the dependency graph inside of a block. We're also given our instruction list here for reference
 */
static void build_dependency_graph_for_block(data_dependency_graph_t* graph, basic_block_t* block, instruction_t** instructions){
	//Predeclare for any/all function parameter lists due to
	//the nature of the switch
	dynamic_array_t* function_parameters;


	//Run through the instruction list backwards. Logically speaking, we're going to
	//find the instruction with the maximum number of dependencies later on down in the block
	//We only go down to one here because for the first instruction, there is nothing of
	//value to check as it's the very first one
	for(int32_t i = block->number_of_instructions - 1; i >= 1; i--){
		//Extract it
		instruction_t* current = instructions[i];

		//Go by the instruction type to handle special cases 
		//more efficiently
		switch(current->instruction_type){
			/**
			 * Jump and set instructions store the op1's that they depend on, though
			 * this is intentionally looked over by the selector, we need to account for it
			 * here
			 */
			case SETNE:
			case SETA:
			case SETAE:
			case SETE:
			case SETB:
			case SETBE:
			case SETG:
			case SETGE:
			case SETL:
			case SETLE:
			case JNE:
			case JE:
			case JNZ:
			case JZ:
			case JA:
			case JAE:
			case JB:
			case JBE:
			case JL:
			case JLE:
			case JG:
			case JGE:
				update_dependence_for_variable(graph, current, instructions, current->op1, i - 1);
				break;

			//We can actually skip phi functions, reason being that they
			//will always come at the front of a block and are always going
			//to have their dependencies coming from outside of the block
			case PHI_FUNCTION:
				break;

			/**
			 * For an indirect call, we need to consider:
			 * 	1.) The source register
			 * 	2.) The parameters
			 */
			case INDIRECT_CALL:
				//Update the dependence for the source var
				update_dependence_for_variable(graph, current, instructions, current->source_register, i - 1);

				//Really just acts as a cleaner cast
				function_parameters = current->parameters;

				//This can happen, in which case we just leave
				if(function_parameters == NULL){
					break;
				}

				//Otherwise, we update all of the parameters
				for(u_int16_t j = 0; j < function_parameters->current_index; j++){
					//Invoke the helper for each given parameter
					update_dependence_for_variable(graph, current, instructions, dynamic_array_get_at(function_parameters, j), i - 1);
				}

				break;

			/**
			 * For a direct call, all that we
			 * need to consider are the parameters
			 */
			case CALL:
				//Really just acts as a cleaner cast
				function_parameters = current->parameters;

				//This can happen, in which case we just leave
				if(function_parameters == NULL){
					break;
				}

				//Otherwise, we update all of the parameters
				for(u_int16_t j = 0; j < function_parameters->current_index; j++){
					//Invoke the helper for each given parameter
					update_dependence_for_variable(graph, current, instructions, dynamic_array_get_at(function_parameters, j), i - 1);
				}

				break;
				
			default:
				//For this instruction, we need to backtrace through the list and figure out:
				//	1.) Do the dependencies get assigned in this block? It is fully possible
				//	that they do not
				//	2.) If they do get assigned in this block, what are those instructions that
				//	are doing the assignment
				
				//For each variable in the instruction, we need to perform the search
				if(is_destination_also_operand(current) == TRUE){
					//Start searching here, beginngin at the last instruction
					update_dependence_for_variable(graph, current, instructions, current->destination_register, i - 1);
				}

				//Same for the source
				if(current->source_register != NULL){
					//Start searching here, beginngin at the last instruction
					update_dependence_for_variable(graph, current, instructions, current->source_register, i - 1);
				}

				//Same for the second source
				if(current->source_register2 != NULL){
					//Start searching here, beginngin at the last instruction
					update_dependence_for_variable(graph, current, instructions, current->source_register2, i - 1);
				}

				//And the address calc registers
				if(current->address_calc_reg1 != NULL){
					//Start searching here, beginngin at the last instruction
					update_dependence_for_variable(graph, current, instructions, current->address_calc_reg1, i - 1);
				}

				//And the address calc registers
				if(current->address_calc_reg2 != NULL){
					//Start searching here, beginngin at the last instruction
					update_dependence_for_variable(graph, current, instructions, current->address_calc_reg2, i - 1);
				}

				break;
		}
	}

	/**
	 * Once we've done all of that, we can now go back and compute all of the
	 * roots in the graph. The roots are simply instructions that have no successors
	 */
	for(int32_t i = block->number_of_instructions - 1; i >= 1; i--){
		//Extract it
		instruction_t* current = instructions[i];
	}
}


/**
 * Perform list scheduling on the entire block. Once this function executes, our block schedule
 * is considered final and we are done
 *
 * Cycle <- 1
 * Readylist <- leaves in priority order(higher is higher priority)
 * Activelist <- {}
 *
 * while (Readylist U Activelist != {}):
 * 	for each instruction in Activelist:
 * 		if cycles(instruction) + start(instruction) < Cycle: //We are done, it's finished
 * 			remove instruction from Activelist
 * 			for each successor s of instruction:
 * 				if s is ready
 * 					add s to Readylist
 * 	if Readylist != {}
 * 		remove an instruction from ReadyList
 * 		Start(instruction) <- Cycle
 * 		add instruction to active
 *
 * 	Cycle <- Cycle + 1
 */
//static void list_schedule_block(basic_block_t* block, instruction_t** instructions, dynamic_array_t* leaves){}


/**
 * Run through a block and perform the reordering/scheduling in it step by step.
 * Once this function returns, we can consider that block 100% done from a scheduling perspective
 *
 * Steps in the scheduling:
 * 	1.) Get the estimated cycle count(cost) for each instruction. This is also where we break
 * 	 	all of the bons holding the instructions in the block(leader, exit, etc)
 * 	2.) Build a data dependency graph for the entire block
 * 	3.) With the data dependency graph in hand, compute the priorities for each instruction
 * 	4.) Use the list scheduling algorithm to schedule instructions
 */
static void schedule_instructions_in_block(basic_block_t* block, u_int8_t debug_printing ){
	/**
	 * Step 0: load all of the instructions into a static array. This is going
	 * to be an efficiency boost because we need to traverse up and down
	 * the block to find assignments for our data relationships
	 */
	//A list of all instructions in the block. We actually have a set
	//number of instructions in the block, which allows us to do this
	instruction_t* instructions[block->number_of_instructions];

	//Let's also allocate the block's dependency graph
	data_dependency_graph_t dependency_graph = dependency_graph_alloc(block->number_of_instructions);

	//Grab a cursor
	instruction_t* instruction_cursor = block->leader_statement;

	//Current index in the list
	u_int32_t list_index = 0;

	/**
	 * Step 1: get the estimated cycle count for each instruction.
	 * We will also break all of the links here in the block
	 */
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
	 * Step 2: build the data dependency graph inside of the block. This is done by
	 * the helper function. Nothing else can be done until this is done
	 */
	build_dependency_graph_for_block(&dependency_graph, block, instructions);

	//Only if we want debug printing we can show this
	if(debug_printing == TRUE){
		printf("============================ Block .L%d ============================\n", block->block_id);
		//Print out the dependence graph for the block
		print_data_dependence_graph(stdout, &dependency_graph);
		//Now let's display the roots and leaves
		printf("============================ Block .L%d ============================\n", block->block_id);
	}

	/**
	 * Step 3: for each instruction, compute it's priority using the 
	 * length of longest weighted path for an instruction to a
	 * root in the dependency graph
	 */

	/**
	 * Step 4: use the list scheduler to reorder the entire block.
	 * The algorithm is detailed in the function
	 */

	//We're done with it, we can deallocate now
	dependency_graph_dealloc(&dependency_graph);
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
			schedule_instructions_in_block(cursor, debug_printing);

			//Advance it up using the direct successor
			cursor = cursor->direct_successor;
		}
	}

	//If we want to print our IR's we will display what we look like post-scheduling
	if(print_irs == TRUE){
		printf("============================= After Scheduling ===========================\n");
		printf("============================= After Scheduling ===========================\n");
	}

	//Give back the final CFG. This is more symbolic
	//than anything, the CFG itself has been modified by
	//the whole procedure
	return cfg;
}
