/**
 * Author: Jack Robbins
 * This file contains implementations for functions defined in the header file basic_block.h
 */

#include "basic_block.h"
#include <stdlib.h>
#include <string.h>

/**
 * Most normal blocks will rarely exceed 8 predecessors/successors, so this is our default size
 */
#define DEFAULT_EDGE_SET_SIZE 8


//================================== CFG Edge Set Utility ========================================
/**
 * Allocate a CFG edge set using the default size
 */
edge_set_t edge_set_alloc(){
	/**
	 * Stack allocate an empty edge set
	 */
	edge_set_t set = EMPTY_EDGE_SET;

	/**
	 * Allocate the underlying array and set the other values
	 */
	set.edges = calloc(DEFAULT_EDGE_SET_SIZE, sizeof(cfg_edge_t));
	set.max_index = DEFAULT_EDGE_SET_SIZE;
	set.current_index = 0;
	return set;
}


/**
 * Clone the given edge set into an entirely separate memory region
 */
edge_set_t clone_edge_set(edge_set_t* source){
	edge_set_t set = EMPTY_EDGE_SET;

	/**
	 * First allocate/initialize what is needed
	 */
	set.edges = calloc(source->max_index, sizeof(cfg_edge_t));
	set.max_index = source->max_index;
	set.current_index = 0;

	/**
	 * Then copy everything over and return out
	 */
	memcpy(set.edges, source->edges, source->max_index * sizeof(cfg_edge_t));
	return set;
}


/**
 * Dynamically resize an edge set to accomodate the given index if need be
 */
void edge_set_dynamic_resize_if_needed(edge_set_t* edge_set, int32_t index){
	/**
	 * No need to resize here if we're smaller than the maximum index
	 */
	if(index < edge_set->max_index){
		return;
	}

	/**
	 * Otherwise we need a resize. We will resize to be double the size of
	 * the requested index
	 */
	edge_set->max_index = index * 2;
	edge_set->edges = realloc(edge_set->edges, sizeof(cfg_edge_t) * edge_set->max_index);
}


/**
 * Delete the edge value at the given index inside of the edge set
 */
void edge_set_delete_at_index(edge_set_t* edge_set, int32_t index){
	/**
	 * If we're at or above the current index then this is an impossible
	 * deletion. We will exit silently
	 */
	if(index >= edge_set->current_index){
		return;
	}
	
	/**
	 * Shift every value to the left by one, starting at the given index.
	 * This will overwrite the original value at said index and perform the
	 * deletion
	 */
	for(int32_t i = index; i < edge_set->current_index - 1; i++){
		edge_set->edges[i] = edge_set->edges[i + 1];
	}

	/**
	 * One final thing - the current index is now 1 too high. We'll wipe
	 * out whatever was in the highest spot and decrement that current index
	 */
	edge_set->edges[edge_set->current_index - 1] = EMPTY_CFG_EDGE;
	(edge_set->current_index)--;
}


/**
 * Deallocate a CFG edge set
 */
void edge_set_dealloc(edge_set_t* edge_set){
	/**
	 * Free the underlying edge set array
	 */
	if(edge_set->edges != NULL){
		free(edge_set->edges);
	}

	/**
	 * Zero these out for safety
	 */
	edge_set->edges = NULL;
	edge_set->current_index = 0;
	edge_set->max_index = 0;
}

//================================== CFG Edge Set Utility ========================================

//================================== Basic Block Utilities =======================================
/**
 * Merge two basic blocks. We always return a pointer to a, b will be deallocated
 *
 * IMPORTANT NOTE: ONCE BLOCKS ARE MERGED, BLOCK B IS GONE
 */
basic_block_t* merge_blocks(basic_block_t* a, basic_block_t* b, variable_symtab_t* variable_symtab, dynamic_array_t* function_blocks){
	//If b is null, we just return a
	if(b->leader_statement == NULL){
		return a;
	}

	//What function are we in - will be important for updates
	symtab_function_record_t* function_contained_in = a->function_defined_in;

	/**
	 * Merge entry and exit statements int
	 */
	if(a->exit_statement == NULL){
		a->leader_statement = b->leader_statement;
		a->exit_statement = b->exit_statement;
	} else {
		//Chain the old exit to the start of b
		a->exit_statement->next_statement = b->leader_statement;
		//B's leader is now chained to the xit
		b->leader_statement->previous_statement = a->exit_statement;
		a->exit_statement = b->exit_statement;
	}

	/**
	 * If we're gonna merge two blocks, then they'll share all the same successors and predecessors
	 * Let's merge predecessors first if we have any
	 *
	 * NOTE: at this point we only have real edges, so hardcode the edge type to real
	 */
	for(int32_t i = 0; i < b->predecessors.current_index; i++){
		add_predecessor_only(a, b->predecessors.edges[i].block, EDGE_TYPE_REAL);
	}

	/**
	 * Now merge successors if we have any
	 *
	 * NOTE: at this point we only have real edges, so hardcode the edge type to real
	 */
	for(int32_t i = 0; i < b->successors.current_index; i++){
		add_successor_only(a, b->successors.edges[i].block, EDGE_TYPE_REAL);
	}

	/**
	 * FOR EACH Successor of B, it will have a reference to B as a predecessor.
	 * This is now wrong though. So, for each successor of B, it will need
	 * to have A as predecessor
	 */
	for(int32_t i = 0; i < b->successors.current_index; i++){
		/**
		 * Grab the successor block first
		 */
		basic_block_t* successor_block = b->successors.edges[i].block;

		/**
		 * Now for each of the predecessors that equals b, it needs to now point to A
		 */
		for(int32_t j = 0; j < successor_block->predecessors.current_index; j++){
			/**
			 * If it's pointing to b, it needs to be updated
			 */
			if(successor_block->predecessors.edges[j].block == b){
				successor_block->predecessors.edges[j].block = a;
			}
		}
	}

	//Copy over the block type and terminal type
	if(a->block_type != BLOCK_TYPE_FUNC_ENTRY){
		a->block_type = b->block_type;
	}

	//If b has a jump table, we'll need to add this in as well
	a->jump_table = b->jump_table;
	b->jump_table = NULL;

	/**
	 * For each statement in B, we need to flag that it's
	 * now defined inside of a
	 */
	instruction_t* b_stmt = b->leader_statement;
	while(b_stmt != NULL){
		b_stmt->block_contained_in = a;
		b_stmt = b_stmt->next_statement;
	}

	/**
	 * All variables maintain a "block declared in" tag that tells us
	 * where the variable was literally declared by the user. This is
	 * very important to us for mutability checking. Since block B is
	 * now block A, any variables that were declared in B now becom
	 * declared in A. We will crawl all sheafs related to this function
	 * and update any "declared in" tags from B to a
	 */
	for(int32_t i = 0; i < variable_symtab->sheafs.current_index; i++){
		symtab_variable_sheaf_t* sheaf = variable_symtab->sheafs.internal_array[i];

		//Not in the function skip it
		if(sheaf->function_contained_in != function_contained_in){
			continue;
		}

		//Run through the kespace
		for(int32_t j = 0; j < VARIABLE_KEYSPACE; j++){
			//Remember records may be chained
			symtab_variable_record_t* cursor = sheaf->records[j];
			while(cursor != NULL){
				/**
				 * If the cursor was declared in block b, update
				 * it to now point to block a
				 */
				if(cursor->block_declared_in == b){
					cursor->block_declared_in = a;
				}

				//Bump up to the next one
				cursor = cursor->next;
			}

		}
	}
	
	//IMPORTANT--wipe b's statements out
	b->leader_statement = NULL;
	b->exit_statement = NULL;

	//Update the instruction counts
	a->number_of_instructions += b->number_of_instructions;

	/**
	 * Remove this block from the set of all function blocks in the
	 * current function
	 */
	dynamic_array_delete(function_blocks, b);

	//And finally we'll deallocate b
	basic_block_dealloc(b);

	//Give back the pointer to a
	return a;
}


/**
 * Print a block with OIR inside of it
 */
void print_OIR_block(FILE* fl, basic_block_t* block, emit_dominance_frontier_selection_t print_df){
	/**
	 * If this is some kind of switch block, we first print the jump table
	 */
	if(block->jump_table != NULL){
		print_jump_table(fl, block->jump_table);
	}

	/**
	 * If it's not a function block print the number. Otherwise we need
	 * the function name
	 */
	if(block->block_type != BLOCK_TYPE_FUNC_ENTRY){
		fprintf(fl, ".L%d", block->block_id);
	} else {
		fprintf(fl, "%s", block->function_defined_in->func_name.string);
	}

	/**
	 * Print out all variables that are "used before definition"
	 */
	if(block->used_before_definition.current_index != 0){
		printf("(");
		for(int32_t i = 0; i < block->used_before_definition.current_index; i++){
			print_variable(stdout, block->used_before_definition.internal_array[i], PRINTING_VAR_BLOCK_HEADER);

			if(i != block->used_before_definition.current_index - 1){
				printf(", ");
			}
		}
		printf(")");
	}

	printf(":\n");

	/**
	 * Now the estimated execution frequency
	 */
	printf("Estimated Execution Frequency: %d\n", block->estimated_execution_frequency);

	/**
	 * Print out all predecessors
	 */
	printf("Predecessors: {");
	for(int32_t i = 0; i < block->predecessors.current_index; i++){
		cfg_edge_t* edge = get_edge_at_index(&(block->predecessors), i);
		print_cfg_edge(fl, edge);

		if(i != block->predecessors.current_index - 1){
			printf(", ");
		}
	}
	printf("}\n");

	/**
	 * Now print out all successor edges
	 */
	printf("Successors: {");
	for(int32_t i = 0; i < block->successors.current_index; i++){
		cfg_edge_t* edge = get_edge_at_index(&(block->successors), i);
		print_cfg_edge(fl, edge);

		if(i != block->successors.current_index - 1){
			printf(", ");
		}
	}
	printf("}\n");

	/**
	 * Print out all assigned variables if any exist
	 */
	if(block->assigned_variables.current_index != 0){
		printf("Assigned: (");
		for(int32_t i = 0; i < block->assigned_variables.current_index; i++){
			print_variable(stdout, block->assigned_variables.internal_array[i], PRINTING_VAR_BLOCK_HEADER);

			//If it isn't the very last one, we need a comma
			if(i != block->assigned_variables.current_index - 1){
				printf(", ");
			}
		}
		printf(")\n");
	}

	/**
	 * Print LIVE_IN variables if we have any
	 */
	if(block->live_in.internal_array != NULL){
		printf("LIVE_IN: (");
		for(int32_t i = 0; i < block->live_in.current_index; i++){
			print_variable(stdout, block->live_in.internal_array[i], PRINTING_VAR_BLOCK_HEADER);

			//If it isn't the very last one, print out a comma
			if(i != block->live_in.current_index - 1){
				printf(", ");
			}
		}
		printf(")\n");
	}

	/**
	 * Print LIVE_OUT variables if we have any
	 */
	if(block->live_out.internal_array != NULL){
		printf("LIVE_OUT: (");
		for(int32_t i = 0; i < block->live_out.current_index; i++){
			print_variable(stdout, block->live_out.internal_array[i], PRINTING_VAR_BLOCK_HEADER);

			//If it isn't the very last one, print out a comma
			if(i != block->live_out.current_index - 1){
				printf(", ");
			}
		}
		printf(")\n");
	}

	/**
	 * Print out the dominance frontier if we're in DEBUG mode
	 */
	if(print_df == EMIT_DOMINANCE_FRONTIER && block->dominance_frontier.internal_array != NULL){
		printf("Dominance frontier: {");
		for(int32_t i = 0; i < block->dominance_frontier.current_index; i++){
			basic_block_t* printing_block = block->dominance_frontier.internal_array[i];

			//Print the block's ID or the function name
			if(printing_block->block_type == BLOCK_TYPE_FUNC_ENTRY){
				printf("%s", printing_block->function_defined_in->func_name.string);
			} else {
				printf(".L%d", printing_block->block_id);
			}

			//If it isn't the very last one, we need a comma
			if(i != block->dominance_frontier.current_index - 1){
				printf(", ");
			}
		}
		printf("}\n");
	}
	//
	/**
	 * Print out the reverse dominance frontier if we're in DEBUG mode
	 */
	if(print_df == EMIT_DOMINANCE_FRONTIER && block->reverse_dominance_frontier.internal_array != NULL){
		printf("Reverse Dominance frontier: {");
		for(int32_t i = 0; i < block->reverse_dominance_frontier.current_index; i++){
			basic_block_t* printing_block = block->reverse_dominance_frontier.internal_array[i];

			//Print the block's ID or the function name
			if(printing_block->block_type == BLOCK_TYPE_FUNC_ENTRY){
				printf("%s", printing_block->function_defined_in->func_name.string);
			} else {
				printf(".L%d", printing_block->block_id);
			}

			//If it isn't the very last one, we need a comma
			if(i != block->reverse_dominance_frontier.current_index - 1){
				printf(", ");
			}
		}
		printf("}\n");
	}

	/**
	 * Now run through and print out every statement in order
	 */
	instruction_t* cursor = block->leader_statement;
	while(cursor != NULL){
		print_three_addr_code_stmt(stdout, cursor);
		cursor = cursor->next_statement;
	}

	/**
	 * Newline for extra spacing
	 */
	printf("\n");
}


/**
 * Utility for deallocating a basic block
 */
void basic_block_dealloc(basic_block_t* block){
	//Just in case
	if(block == NULL){
		printf("ERROR: Attempt to deallocate a null block");
		exit(1);
	}

	//Deallocate the live variable array
	if(block->used_before_definition.internal_array != NULL){
		dynamic_array_dealloc(&(block->used_before_definition));
	}

	//Deallocate the assigned variable array
	if(block->assigned_variables.internal_array != NULL){
		dynamic_array_dealloc(&(block->assigned_variables));
	}

	//Deallocate the domninance frontier
	if(block->dominance_frontier.internal_array != NULL){
		dynamic_array_dealloc(&(block->dominance_frontier));
	}

	//Deallocate the reverse dominance frontier
	if(block->reverse_dominance_frontier.internal_array != NULL){
		dynamic_array_dealloc(&(block->reverse_dominance_frontier));
	}

	//Deallocate the dominator children frontier
	if(block->dominator_children.internal_array != NULL){
		dynamic_array_dealloc(&(block->dominator_children));
	}

	//Deallocate the liveness sets
	if(block->live_out.internal_array != NULL){
		dynamic_array_dealloc(&(block->live_out));
	}

	if(block->live_in.internal_array != NULL){
		dynamic_array_dealloc(&(block->live_in));
	}

	/**
	 * Deallocate both of the edge sets
	 */
	edge_set_dealloc(&(block->predecessors));
	edge_set_dealloc(&(block->successors));

	//If this is a switch statement entry block, then it will have a jump table
	if(block->jump_table != NULL){
		jump_table_dealloc(block->jump_table);
	}

	//Otherwise its fine so
	free(block);
}

//================================== Basic Block Utilities =======================================
