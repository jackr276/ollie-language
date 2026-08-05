/**
 * Author: Jack Robbins
 * This file implements the APIs for the static analyzer that were defined in the
 * header file of the same name
 */

#include "static_analyzer.h"
#include "../graph_analyzer/graph_analyzer.h"
#include <assert.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ucontext.h>

//Store these globally for easy access
static three_addr_var_t* instruction_pointer_var;
static three_addr_var_t* stack_pointer_var;

//Hold references to the error/warning counts
static u_int32_t* error_count;
static u_int32_t* warning_count;

//The current dependency node(changed by function)
static dependency_graph_node_t* current_dependency_node;

//Holder for all error info that we want to print
static char error_info[ERROR_SIZE * 5];

//========================================= General Utilities =============================================

/**
 * Take a file that may look like: ./oc/test_files/sample.ol and return sample.ol
 */
static inline char* extract_file_name_from_fully_qualified_name(char* fully_qualified_name){
	int32_t length = strlen(fully_qualified_name);

	//Roll this back until we have the index of the first /
	int32_t i = length - 1;
	for(; i >= 0; i--){
		if(fully_qualified_name[i] == '/'){
			break;
		}
	}

	//Offset into this to get it(+ 1 to get past the /)
	return fully_qualified_name + i + 1;
}


/**
 * Print a static analyzer error message in a nice formatted way
 */
static void print_static_analyzer_message(error_message_type_t message_type, char* info, u_int32_t line_number){
	//Mapped by index to the enum values
	static const char* type[] = {"WARNING", "ERROR", "INFO", "DEBUG"};

	//Get just the important part of the file name
	char* file_name = extract_file_name_from_fully_qualified_name(current_dependency_node->file_name);

	/**
	 * If it's the main node print out the file only, otherwise we'll
	 * also need the module name
	 */
	if(current_dependency_node->type != DEPENDENCY_GRAPH_NODE_TYPE_MAIN){
		fprintf(stdout, "\n[MODULE %s | FILE: %s] --> [LINE %d | COMPILER %s]: %s\n", current_dependency_node->module_name.string, file_name, line_number, type[message_type], info);
	} else {
		fprintf(stdout, "\n[FILE: %s] --> [LINE %d | COMPILER %s]: %s\n", file_name, line_number, type[message_type], info);
	}
}


/**
 * Run through an entire array of function blocks and reset the status for
 * every single one. We assume that the caller knows what they are doing, and
 * that the blocks inside of the array are really the correct blocks
 */
static inline void reset_visited_status_for_function(dynamic_array_t* function_blocks){
	//Run through all of the blocks
	for(int32_t i = 0; i < function_blocks->current_index; i++){
		//Extract the current block
		basic_block_t* current = dynamic_array_get_at(function_blocks, i);

		//Flag it as false
		current->visited = FALSE;
	}
}


/**
 * Run through an entire array of function blocks and reset the status and 
 * "already_has_phi_func" fields for every single one. We assume that 
 * the caller knows what they are doing, and that the blocks inside of 
 * the array are really the correct blocks
 */
static inline void reset_status_for_phi_function_insertion(dynamic_array_t* function_blocks){
	//Run through all of the blocks
	for(int32_t i = 0; i < function_blocks->current_index; i++){
		//Extract the current block
		basic_block_t* current = dynamic_array_get_at(function_blocks, i);

		//Flag it as false
		current->visited = FALSE;

		//Remove the phi function flag
		current->already_has_phi_func = FALSE;
	}
}


/**
 * A special helper function that we use for dynamic arrays of variables. Since variables
 * can be duplicated, we need to compare the symtab variable record, not the three address
 * variable itself. This does a simple linear scan to search
 */
static inline u_int8_t does_variable_dynamic_array_contain_symtab_variable(dynamic_array_t* variable_array, symtab_variable_record_t* variable){
	for(int32_t i = 0; i < variable_array->current_index; i++){
		//Avoid a function call by grabbing directly
		three_addr_var_t* candidate = variable_array->internal_array[i];

		//Only a hit if the linked var matches
		if(candidate->linked_var == variable){
			return TRUE;
		}
	}

	return FALSE;
}


/**
 * Does the block assign this variable? We'll do a simple linear scan to find out
 */
static inline u_int8_t does_block_assign_variable(basic_block_t* block, symtab_variable_record_t* variable){
	/**
	 * If the linked variable to this var is ours, we do assign
	 */
	for(int32_t i = 0; i < block->assigned_variables.current_index; i++){
		three_addr_var_t* var = dynamic_array_get_at(&(block->assigned_variables), i);
		
		//Now we'll compare the linked variable to the record
		if(var->linked_var == variable){
			return TRUE;
		}
	}

	return FALSE;
}


/**
 * Add a variable into the DEF set. Unlike the use set, the only thing that we need to check and make sure of here
 * is that the variable isn't already in there
 */
static inline void add_variable_to_def_set(three_addr_var_t* variable, basic_block_t* block){
	//Is the variable NULL? If so then return
	if(variable == NULL){
		return;
	}

	//We do not need to bother tracking these variables - they are a sure thing
	if(variable == instruction_pointer_var || variable == stack_pointer_var){
		return;
	}

	/**
	 * If we have variables that are temporary or "memory addresses", then
	 * they are not going to change so we do not need to track them
	 */
	switch(variable->variable_type){
		case VARIABLE_TYPE_TEMP:
		case VARIABLE_TYPE_MEMORY_ADDRESS:
		case VARIABLE_TYPE_FUNCTION_ADDRESS:
		case VARIABLE_TYPE_STACK_PARAM_MEMORY_ADDRESS:
		case VARIABLE_TYPE_LOCAL_CONSTANT:
			return;
		default:
			break;
	}

	//Extract the set that we'll be working with
	dynamic_array_t* def_set = &(block->assigned_variables);

	//Otherwise, let's make sure it's not also in DEF
	for(int32_t i = 0; i < def_set->current_index; i++){
		//Grab it out
		three_addr_var_t* defined = dynamic_array_get_at(def_set, i);

		//It's been defined in this block, so we don't care
		if(variables_equal_no_ssa(defined, variable) == TRUE){
			return;
		}
	}

	//If we make it all of the way down here, then we can add it
	dynamic_array_add(def_set, variable);
}


/**
 * Add a phi statement into the basic block. Phi statements are always added, without exception,
 * to the very front of the block
 *
 * This statement also takes care of the linking that we need to do. When we have a phi-function, we'll
 * need to link it back to whichever variables it refers to
 */
static inline void add_phi_statement(basic_block_t* target, instruction_t* phi_statement){
	//Counts as an instruction
	target->number_of_instructions++;

	//Mark the block that we're in
	phi_statement->block_contained_in = target;

	/**
	 * Special case -- we're adding the head so this 
	 * is now the head and the tail
	 */
	if(target->leader_statement == NULL){
		target->leader_statement = phi_statement;
		target->exit_statement = phi_statement;
		return;

	/**
	 * Otherwise just do a regular insertion
	 */
	} else {
		phi_statement->next_statement = target->leader_statement;
		target->leader_statement->previous_statement = phi_statement;
		target->leader_statement = phi_statement;
	}
}


/**
 * Is a given symtab variable SSA eligible?
 * 
 * Ineligible:
 * 	Global variables
 * 	Static variables
 * 	Enum variables
 * 	Struct variables
 *
 * These are all ineligible because they are fundamentally differnt than what an actual
 * SSA variable is. For instance static and global variables are basically equivalent
 * to variables stored in memory and as such do not count for SSA
 */
static inline u_int8_t is_symtab_variable_ssa_eligible(symtab_variable_record_t* variable){
	switch(variable->membership){
		case ENUM_MEMBER:
		case STRUCT_MEMBER:
		case STATIC_VARIABLE:
		case GLOBAL_VARIABLE:
			return FALSE;
		default:
			return TRUE;
	}
}


/**
 * Is a given variable SSA eligible? We do this by looking at the type of the
 * variable and whether or not the linked var is NULL. If the linked var is NULL
 * we would get segfaults
 */
static inline u_int8_t is_variable_ssa_eligible(three_addr_var_t* variable){
	//Sanity check
	if(variable == NULL){
		return FALSE;
	}

	switch(variable->variable_type){
		/**
		 * If we have a linked variable, give back yes/no based on whether or
		 * not the linked variable itself is eligible for SSA(criteria above)
		 */
		case VARIABLE_TYPE_MEMORY_ADDRESS:
		case VARIABLE_TYPE_NON_TEMP:
			if(variable->linked_var != NULL){
				return is_symtab_variable_ssa_eligible(variable->linked_var);
			} else {
				return FALSE;
			}
		
		/**
		 * Return by copy addresses are *never* SSA eligible. This
		 * would actually case the SSA system to crash because there
		 * is no real assignment for this kind of variable
		 */
		case VARIABLE_TYPE_RETURN_BY_COPY_ADDRESS:
			return FALSE;

		default:
			return FALSE;
	}
}


/**
 * Generate a new name for the given three address variable
 *
 * For a left hand side(assignment) new name:
 * 	- Grab the next generation level from the counter
 * 	- Use the lightstack to get the generation level that we're overwriting
 * 	  if it exists
 * 	- Incrememnt the counter for the next go around
 * 	- Push the current generation level to the lightstack because it's now 
 * 	  the previous generation for the next go around
 * 	- Store the SSA generation to the three address variable
 *  - Store the overwrite mapping such that map[new_generation] = previous(overwritten) generation
 */
static inline void lhs_new_name(three_addr_var_t* var){
	//Grab the linked variable out
	symtab_variable_record_t* linked_var = var->linked_var;

	//Grab the name out of the counter
	int32_t current_generation_level = linked_var->ssa_counter;

	/**
	 * Get the generation level that is overwritten. We do this
	 * by peeking on the lighstack. If there is nothing on the
	 * lighstack, we'll use a sentinel value of -1 to show
	 * that it overwrites nothing
	 */
	int32_t overwritten_generation_level;
	if(linked_var->counter_stack.top_index != 0){
		overwritten_generation_level = lightstack_peek(&(linked_var->counter_stack));
	} else {
		overwritten_generation_level = OVERWRITES_NOTHING;
	}

	//Now we increment the counter for the next go around
	(linked_var->ssa_counter)++;

	//Put the current generation level on the stack
	lightstack_push(&(linked_var->counter_stack), current_generation_level);

	//Update the three address variable itself
	var->ssa_generation = current_generation_level;

	/**
	 * Variable overwrite mapping - use the current generation
	 * level as an index and the overwritten generation as a value
	 *
	 * First we'll resize if it's needed
	 */
	dynamic_integer_array_resize_to_fit_index_if_needed(&(linked_var->ssa_overwritten_generation_map), current_generation_level);
	linked_var->ssa_overwritten_generation_map.internal_array[current_generation_level] = overwritten_generation_level;
}


/**
 * For a left hand size(assignment) for a phi function specifically:
 * 	- Grab the next generation level from the counter
 * 	- Incrememnt the counter for the next go around
 * 	- Push the current generation level to the lightstack because it's now 
 * 	  the previous generation for the next go around
 *  - Store the overwrite mapping such that map[new_generation] = -1. This is because
 *    phi functions do not explicitly overwrite one value only. They instead overwrite
 *    multiple values potentially
 */
static inline void phi_function_lhs_new_name(three_addr_var_t* phi_assignee){
	//Grab the linked variable out
	symtab_variable_record_t* linked_var = phi_assignee->linked_var;

	//Grab the name out of the counter
	int32_t current_generation_level = linked_var->ssa_counter;

	//Now we increment the counter for the next go around
	(linked_var->ssa_counter)++;

	//Put the current generation level on the stack
	lightstack_push(&(linked_var->counter_stack), current_generation_level);

	//Update the three address variable itself
	phi_assignee->ssa_generation = current_generation_level;

	/**
	 * Variable overwrite mapping - use the current generation
	 * level as an index and the overwritten generation as a value
	 *
	 * First we'll resize if it's needed
	 */
	dynamic_integer_array_resize_to_fit_index_if_needed(&(linked_var->ssa_overwritten_generation_map), current_generation_level);
	linked_var->ssa_overwritten_generation_map.internal_array[current_generation_level] = OVERWRITES_NOTHING;
}


/**
 * For a left hand side(assignment) new name without an explicit three_addr_var:
 * 	- Grab the next generation level from the counter
 * 	- Use the lightstack to get the generation level that we're overwriting
 * 	  if it exists
 * 	- Incrememnt the counter for the next go around
 * 	- Push the current generation level to the lightstack because it's now 
 * 	  the previous generation for the next go around
 *  - Store the overwrite mapping such that map[new_generation] = previous(overwritten) generation
 */
static inline void lhs_new_name_direct(symtab_variable_record_t* variable){
	//Grab the name out of the counter
	int32_t current_generation_level = variable->ssa_counter;

	/**
	 * Get the generation level that is overwritten. We do this
	 * by peeking on the lighstack. If there is nothing on the
	 * lighstack, we'll use a sentinel value of -1 to show
	 * that it overwrites nothing
	 */
	int32_t overwritten_generation_level;
	if(variable->counter_stack.top_index != 0){
		overwritten_generation_level = lightstack_peek(&(variable->counter_stack));
	} else {
		overwritten_generation_level = OVERWRITES_NOTHING;
	}

	//Now we increment the counter for the next go around
	(variable->ssa_counter)++;

	//Put the current generation level on the stack
	lightstack_push(&(variable->counter_stack), current_generation_level);

	/**
	 * Variable overwrite mapping - use the current generation
	 * level as an index and the overwritten generation as a value
	 *
	 * First we'll resize if it's needed
	 */
	dynamic_integer_array_resize_to_fit_index_if_needed(&(variable->ssa_overwritten_generation_map), current_generation_level);
	variable->ssa_overwritten_generation_map.internal_array[current_generation_level] = overwritten_generation_level;
}


/**
 * For an RHS(use) new name:
 * 	Get the generation number by peeking the stack and assigning
 */
static inline void rhs_new_name(three_addr_var_t* var){
	//Grab the linked var out
	symtab_variable_record_t* linked_var = var->linked_var;

	//Grab the value off of the stack
	u_int16_t generation_level = lightstack_peek(&(linked_var->counter_stack));

	//Store the generation level in here
	var->ssa_generation = generation_level;
}

//========================================= General Utilities =============================================


/**
 * Since static variables also count for us as global variables, we need to
 * be able to handle a case where say, for instance, that two separate
 * functions have a static variable called "x". If we just left it as is,
 * we would have an ambiguous reference and the assembler woudl fail. To fix
 * this, we will mangle those names such that we now get "x.0" and "x.1" instead
 * of two "x"'s
 */
static void mangle_static_variable_names(dynamic_array_t* global_variables){
	//We'll keep a running id to mangle things
	u_int32_t static_var_mangler = 0;
	char mangler[100];

	/**
	 * Run through all of our global variables here
	 */
	for(int32_t i = 0; i < global_variables->current_index; i++){
		//Extract our current candidate
		global_variable_t* candidate = dynamic_array_get_at(global_variables, i);
		
		/**
		 * Global variable name collision is already enforced by the symtab in the
		 * parser so we can skip this for efficiency's sake
		 */
		if(candidate->variable->membership == GLOBAL_VARIABLE){
			continue;
		}

		//Print this into the buffer
		snprintf(mangler, 100, ".%d", static_var_mangler);
		
		//Now concatenate it to our variable name
		dynamic_string_concatenate(&(candidate->variable->var_name), mangler);

		//Bump it up for the next go around
		static_var_mangler++;
	}
}


/**
 * In order to perform definite assignment analysis, we will need to insert
 * initial "undef" assignments for every SSA eligible variable that exists
 * inside of each given function. This will become our "_0" value and we will
 * know that any use of _0 will be a use-before-intialization, and will
 * therefore be an error
 *
 * We will not do this with actual statements. Instead we will use the lhs_new_name_direct
 * helper to emit the intial value for each given value
 *
 * This so-called "poison" initialization will let us know that, if we get to a point
 * where an _0 variable is being used, that variable is being used in an unitialized
 * way
 */
static inline void emit_synthetic_initializations(variable_symtab_t* symtab){
	/**
	 * Now for the symtab, we will run through every variable
	 * in here and pick out the ones that are assigned in the current
	 * function that we are looking at here
	 */
	for(int32_t i = 0; i < symtab->sheafs.current_index; i++){
		symtab_variable_sheaf_t* sheaf = dynamic_array_get_at(&(symtab->sheafs), i);

		//Run through the variable keyspace in the sheaf
		for(int32_t i = 0; i < VARIABLE_KEYSPACE; i++){
			//Extract our value(remember about how these get chained)
			symtab_variable_record_t* cursor = sheaf->records[i];

			//Iterate through each layer of this record index
			while(cursor != NULL){
				/**
				 * IMPORTANT - some variables are completely ineligible. If 
				 * we don't bar for this here we will get null pointer exceptions
				 */
				if(is_symtab_variable_ssa_eligible(cursor) == FALSE){
					cursor = cursor->next;
					continue;
				}

				/**
				 * We will need to maintain a map of what generations we've overwritten
				 * whenever we have a new LHS assignment. We can allocate it now and it
				 * will be populated by the renamer
				 */
				cursor->ssa_overwritten_generation_map = dynamic_integer_array_alloc();

				//Emit the LHS new name directly here
				lhs_new_name_direct(cursor);

				//Emit the three address representation
				three_addr_var_t* starting_variable = emit_var(cursor);

				/**
				 * This counts as a definition for this variable inside of our
				 * given function's entry block
				 *
				 *
				 * TODO DO WE NEED THIS
				 *
				 *
				 */
				add_variable_to_def_set(starting_variable, cursor->function_declared_in->function_entry_block);

				//Bump up to the next record(remember they can be chained)
				cursor = cursor->next;
			}
		}
	}
}


/**
 * Emit a phi function for a given variable. Once emitted, these statements are compiler exclusive,
 * but they are needed for our optimization
 */
static instruction_t* emit_phi_function(symtab_variable_record_t* variable){
	//First we allocate it
	instruction_t* stmt = calloc(1, sizeof(instruction_t));

	//We'll just store the assignee here, no need for anything else
	stmt->operands.oir.assignee = emit_var(variable);

	//Create our parameter array
	stmt->parameters = dynamic_array_alloc();

	//Note what kind of node this is
	stmt->statement_type = THREE_ADDR_CODE_PHI_FUNC;

	//And give the statement back
	return stmt;
}


/**
 * if(x0 == 0){
 * 	x1 = 2;
 * } else {
 * 	x2 = 3;
 * }
 * 
 * x3 <- phi(x1, x2)
 *
 * This means that x3 is x1 if it comes from the first branch and x2 if it comes
 * from the second branch
 *
 * To insert phi functions, we take the following approach:
 * 	worklist <- {}
 *
 * 	For each SSA eligible variable V:
 * 		For each block B in the function assigns V:
 * 			add it onto the worklist
 * 			Flag B as having been on the worklist
 *
 * 		While worklist is not empty:
 * 			Remove block B from the worklist
 *
 * 			if B was ever on the worklist: 	<------ avoid revisiting blocks
 * 				continue
 *
 * 			for each dominance frontier block D of block B:
 * 				if D already has a phi function for V: <-------- avoid double insertions
 * 					continue
 * 	TODO UPDATE
 *
 * 				if a variable is not LIVE_OUT AND it's not USED at D:
 * 					continue
 *
 * 				Add the phi function
 * 				Add D to the worklist
 * 				Flag D as having been on the worklist
 *
 *
 * We will use the "visited" tag to keep track of whether or not we've already
 * evaluated this block or not. We will need to reset this for every variable
 *
 * The phi function inserter runs over the entire CFG(so all functions, files, everything).
 * We may change this in the future, but doing this over the entire CFG allows us to keep
 * all of our work down to very few allocations(one initial worklist allocation + some
 * resizes) which is a big win if we have 100s or 1000s of functions to do
 */
static inline void insert_phi_functions(variable_symtab_t* var_symtab){
	/**
	 * We need to maintain a worklist for our algorithm. Instead of constantly
	 * reallocating and deallocating, we can just maintain one that we clear
	 * whenever we're done using
	 */
	dynamic_array_t worklist = dynamic_array_alloc();

	/**
	 * Step 1: For every single sheaf(lexical level/scope) that we have in the symbol table,
	 * and within every sheaf run through every single defined variable
	 */
	for(int32_t i = 0; i < var_symtab->sheafs.current_index; i++){
		//Grab the current sheaf
		symtab_variable_sheaf_t* sheaf_cursor = dynamic_array_get_at(&(var_symtab->sheafs), i);

		for(int32_t j = 0; j < VARIABLE_KEYSPACE; j++){
			symtab_variable_record_t* record = sheaf_cursor->records[j];

			/**
			 * Remember that symtab records can be chained in case
			 * of hash collisions, so we need to run through every
			 * variable like this
			 */
			while(record != NULL){
				/**
				 * Certain variable types are completely ineligible, so checking
				 * them would be a waste. As such we will skip all of these ineligible
				 * variables here
				 */
				if(is_symtab_variable_ssa_eligible(record) == FALSE){
					record = record->next;
					continue;
				}

				/**
				 * To improve efficiency, we will grab the list of all blocks for the given
				 * function that this variable was contained within and only scan those. Remember
				 * that things like global variables are ineligible for SSA to begin with
				 * due to how they are stored, so this is fine for us
				 */
				symtab_function_record_t* variable_function = record->function_declared_in;
				dynamic_array_t* function_blocks = &(variable_function->function_blocks);

				/**
				 * Reset the "has_phi_function" tag on all of our blocks
				 * for the next go around
				 */
				reset_status_for_phi_function_insertion(function_blocks);

				/**
				 * Queue up every block that we have on record as assigning this
				 * given variable
				 */
				for(int32_t k = 0; k < function_blocks->current_index; k++){
					basic_block_t* block = dynamic_array_get_at(function_blocks, k);

					/**
					 * Enqueue to our worklist if the block assigns this variable. Also flag
					 * the visited tag on the block so that we don't end up reprocessing this
					 */
					if(does_block_assign_variable(block, record) == TRUE){
						dynamic_array_add(&worklist, block);
						block->visited = TRUE;
					}
				}

				//So long as the worklist is not empty
				while(dynamic_array_is_empty(&worklist) == FALSE){
					//O(1) removal delete from back
					basic_block_t* node = dynamic_array_delete_from_back(&worklist);

					/**
					 * For each block that assigns our variable, run through
					 * every block in that block's dominance frontier(just barely
					 * not dominated by that block). If the block in the dominance
					 * frontier either uses the variable, *or* the variable is
					 * live_out at that block, we'll need to insert a phi function
					 * join node
					 */
					for(int32_t l = 0; l < node->dominance_frontier.current_index; l++){
						basic_block_t* df_node = dynamic_array_get_at(&(node->dominance_frontier), l);

						/**
						 * If this already has a phi function for this run we skip it
						 */
						if(df_node->already_has_phi_func == TRUE){
							continue;
						}

						/**
						 * Function exit block - these blocks exist purely
						 * to make graph analyis nice. We don't ever need
						 * a phi function in them though
						 */
						if(df_node->block_type == BLOCK_TYPE_FUNC_EXIT){
							continue;
						}

						/**
						 * ----------------------------------------
						 *  CRITERION:
						 *
						 *  1.) For mutable variables - we generate a pruned
						 *  	SSA form, meaning that if a variable is NOT
						 *  	Live-out at the join node, that means that
						 *  	it is not LIVE-IN at any of the successors of
						 *  	that block. If a variable is not active(used) at
						 *  	the join node either, that means that the phi
						 *  	function is useless. So, we will skip inserting a phi function
						 *  	if the variable is not used and not LIVE_OUT at N
						 *
						 *  2.) For immutable variables - we are going to need
						 *  	all phi functions to verify that we are not mutating.
						 *  	Therefore, we will always insert phi functions for 
						 *  	mutability analysis
						 * ----------------------------------------
						 */
						if(record->type_defined_as->mutability == MUTABLE){
							if(does_variable_dynamic_array_contain_symtab_variable(&(df_node->used_before_definition), record) == FALSE 
								&& does_variable_dynamic_array_contain_symtab_variable(&(df_node->live_out), record) == FALSE){
								continue;
							}
						}

						/**
						 * If we make it here that means that we don't already have one, so we'll add it
						 *
						 * This only emits the skeleton of a phi function - variables will be added
						 * later
						 */
						instruction_t* phi_stmt = emit_phi_function(record);

						//Add the phi statement into the block	
						add_phi_statement(df_node, phi_stmt);

						/**
						 * Flag that this already has a phi function and add it
						 * to the worklist
						 */
						df_node->already_has_phi_func = TRUE;
						dynamic_array_add(&worklist, df_node);
					}
				}

				//Wipe the worklist now
				clear_dynamic_array(&worklist);
			
				//Advance to the next record in the chain
				record = record->next;
			}
		}
	}

	//Scrap this once done
	dynamic_array_dealloc(&worklist);
}


/**
 * Rename all variables to be in SSA form. This is the final step in our conversion
 *
 * Algorithm:
 *
 * rename(){
 * 	if b previously visited:
 * 		return
 * 		
 *	for each phi-function p in b
 * 		v = LHS(p)
 * 		vn = GenName(v) and replace v with vn
 * 	for each statement s in b
 * 		for each variable v in the RHS of s
 * 			replace V with Top(Stacks[V]);
 * 		for each variable V in the LHS
 * 			vn = GenName(V) and replace v with vn
 * 		for each CFG successor s of b
 * 			j <- position in s's phi-functon belonging to b
 * 			for each phi function p in s
 * 				replace the jth operand of RHS(p) with Top(Stacks[V])
 * 		for each s in the dominator children of b
 * 			Rename(s)
 * 		for each phi-function or statement t in b
 * 			for each vi in the LHS(T)
 * 				pop(Stacks[V])
 * }
 */
static void rename_block(basic_block_t* entry){
	//If we've previously visited this block, then return
	if(entry->visited == TRUE){
		return;
	}

	//Flag that we've visited
	entry->visited = TRUE;

	/**
	 * If this is a function entry block, then all of it's
	 * parameters have technically already been "assigned" by the
	 * time we end up in here. As such we'll give them all a direct
	 * left hand new name
	 */
	if(entry->block_type == BLOCK_TYPE_FUNC_ENTRY){
		symtab_function_record_t* function_defined_in = entry->function_defined_in;
		
		/**
		 * We store function parameters as symtab variables so we'll need to perform a direct
		 * rename here
		 */
		for(int32_t i = 0; i < function_defined_in->function_parameters.current_index; i++){
			lhs_new_name_direct(dynamic_array_get_at(&(function_defined_in->function_parameters), i));
		}
	}

	instruction_t* cursor = entry->leader_statement;
	
	/**
	 * We'll now crawl the block renaming every single instruction. Some instructions require
	 * special consideration/handling as seen below
	 */
	while(cursor != NULL){
		switch(cursor->statement_type){
			case THREE_ADDR_CODE_PHI_FUNC:
				/**
				 * Phi functions are a special case because they overwrite
				 * multiple definitions, not just one. We'll use a special
				 * rule to account for this
				 */
				phi_function_lhs_new_name(cursor->operands.oir.assignee);
				break;
				
			/**
			 * Function calls are a special case because they have a parameter
			 * array that we'll need to conisder
			 */
			case THREE_ADDR_CODE_FUNC_CALL:
			case THREE_ADDR_CODE_INDIRECT_FUNC_CALL:
				if(is_variable_ssa_eligible(cursor->operands.oir.operand1) == TRUE){
					rhs_new_name(cursor->operands.oir.operand1);
				}
				
				//Function calls contain parameters that count as RHS vars
				dynamic_array_t* func_params = &(cursor->parameters);

				for(int32_t k = 0; k < func_params->current_index; k++){
					three_addr_var_t* current_param = dynamic_array_get_at(func_params, k);

					if(is_variable_ssa_eligible(current_param) == TRUE){
						rhs_new_name(current_param);
					}
				}

				if(is_variable_ssa_eligible(cursor->operands.oir.assignee) == TRUE){
					lhs_new_name(cursor->operands.oir.assignee);
				}

				break;

			/**
			 * All other cases we just rename as we see appropriate
			 */
			default:
				if(is_variable_ssa_eligible(cursor->operands.oir.operand1) == TRUE){
					rhs_new_name(cursor->operands.oir.operand1);
				}

				if(is_variable_ssa_eligible(cursor->operands.oir.operand2) == TRUE){
					rhs_new_name(cursor->operands.oir.operand2);
				}

				if(is_variable_ssa_eligible(cursor->operands.oir.address_operand1) == TRUE){
					rhs_new_name(cursor->operands.oir.address_operand1);
				}

				if(is_variable_ssa_eligible(cursor->operands.oir.address_operand2) == TRUE){
					rhs_new_name(cursor->operands.oir.address_operand2);
				}

				/**
				 * After we rename the RHS, we need to rename the left hand variable if
				 * it itself is eligible
				 */
				if(is_variable_ssa_eligible(cursor->operands.oir.assignee) == TRUE){
					lhs_new_name(cursor->operands.oir.assignee);
				}

				break;
		}

		//Advance up to the next statement
		cursor = cursor->next_statement;
	}

	/**
	 * For each successor of i, we'll need to update the phi functions with the new names
	 * that we've given for variables in this block
	 */
	for(int32_t i = 0; i < entry->successors.current_index; i++){
		basic_block_t* successor = dynamic_array_get_at(&(entry->successors), i);
		instruction_t* succ_cursor = successor->leader_statement;

		/**
		 * Crawl through every phi function in the successor(they're all at the top) and
		 * for each one generate a new variable and add it in
		 */
		while(succ_cursor != NULL && succ_cursor->statement_type == THREE_ADDR_CODE_PHI_FUNC){
			//We have a phi function, so what are we assigning to it?
			symtab_variable_record_t* phi_func_assignee = succ_cursor->operands.oir.assignee->linked_var;

			//Emit a new variable for this one
			three_addr_var_t* phi_func_param = emit_var(phi_func_assignee);

			//Emit the name for this variable
			rhs_new_name(phi_func_param);

			//Add this as a parameter
			dynamic_array_add(&(succ_cursor->parameters), phi_func_param);

			succ_cursor = succ_cursor->next_statement;
		}
	}

	/**
	 * Now that we're done with the renaming, we'll go through each dominator child in this node
	 * and perform the same operation
	 */
	for(int32_t i = 0; i < entry->dominator_children.current_index; i++){
		rename_block(dynamic_array_get_at(&(entry->dominator_children), i));
	}

	/**
	 * Again if this is a function entry block, then we need to unwind the stack
	 * so that we avoid excessive variable numbers here as well
	 */
	if(entry->block_type == BLOCK_TYPE_FUNC_ENTRY){
		symtab_function_record_t* function_defined_in = entry->function_defined_in;
		
		//We need to pop these all only once so that we have parity with what we did up top
		for(int32_t i = 0; i < function_defined_in->function_parameters.current_index; i++){
			//Get the function parameter out
			symtab_variable_record_t* function_param = dynamic_array_get_at(&(function_defined_in->function_parameters), i);

			//Pop it off here
			lightstack_pop(&(function_param->counter_stack));
		}
	}

	/**
	 * Once we're done, we'll need to unwind our stack here. Anything that involves an assignee, we'll
	 * need to pop it's stack so we don't have excessive variable numbers. We'll now iterate over again
	 * and perform pops whereever we see a variable being assigned
	 */
	cursor = entry->leader_statement;
	while(cursor != NULL){
		//If we see a statement that has an assignee that is not temporary, we'll unwind(pop) his stack
		if(is_variable_ssa_eligible(cursor->operands.oir.assignee) == TRUE){
			lightstack_pop(&(cursor->operands.oir.assignee->linked_var->counter_stack));
		}

		//Advance to the next one
		cursor = cursor->next_statement;
	}
}


/**
 * Rename all of the variables in the CFG
 */
static inline void rename_all_variables(cfg_t* cfg){
	//Before we do this - let's reset the entire CFG(all created blocks)
	reset_visited_status_for_function(&(cfg->created_blocks));

	/**
	 * We will call the rename block function on the first block
	 * for each of our functions. The rename block function is 
	 * recursive, so that should in theory take care of everything for us
	 */
	for(int32_t i = 0; i < cfg->function_entry_blocks.current_index; i++){
		rename_block(dynamic_array_get_at(&(cfg->function_entry_blocks), i));
	}
}


/**
 * Once we have all SSA generation handled, we know the maximum amount of SSA generations
 * for each variable. As such, we can now go through and initialize all of the initialization
 * state maps by dynamically allocating arrays of the required size(number of ssa generations)
 * for each one
 */
static inline void create_all_initialization_state_maps(variable_symtab_t* variables){
	//For every single lexical scope
	for(int32_t i = 0; i < variables->sheafs.current_index; i++){
		symtab_variable_sheaf_t* sheaf = dynamic_array_get_at(&(variables->sheafs), i);

		//Traverse the entire record keyspace
		for(int32_t j = 0; j < VARIABLE_KEYSPACE; j++){
			symtab_variable_record_t* cursor = sheaf->records[j]; 

			//Remember that records can be chained from hash collisions
			while(cursor != NULL){
				//If it's not SSA eligible then skip
				if(is_symtab_variable_ssa_eligible(cursor) == FALSE || cursor->ssa_counter == 0){
					cursor = cursor->next;
					continue;
				}

				//Initialize the map to be all 0s(uninitialized) at first
				cursor->initialization_state_map = calloc(cursor->ssa_counter, sizeof(variable_initialization_state_t));
				cursor = cursor->next;
			}
		}
	}
}


/**
 * This pass will do everything needed to convert the CFG into SSA(static single assignment) form.
 * As a reminder, static single assignment form is an IR form where every variable is assigned
 * only once
 */
static void convert_cfg_to_ssa_form(cfg_t* cfg, variable_symtab_t* variables){
	/**
	 * Step 1: We will do a synthetic initialization with a poison value so that
	 * <var_name>_0(0th generation) marks an uninitialized variable. This will be useful
	 * for us down the road when we do uninitialized variable usage checking and mutation
	 * checking
	 */
	emit_synthetic_initializations(variables);

	/**
	 * Step 2: Insert join nodes(phi functions) at blocks where different definitions
	 * of variables meet. These join nodes form the basis of the SSA renaming and also
	 * will be used for our uninitialized variable detection
	 */
	insert_phi_functions(variables);

	/**
	 * Step 3: Rename all variables into SSA using the standard algorithm. SSA form is heavily
	 * relied on by the uninitialized variable/mutation checker and by the optimizer down the
	 * road
	 */
	rename_all_variables(cfg);

	/**
	 * Step 4: now that we've performed all variable renaming, we will initialize
	 * the SSA generation to variable state maps inside of each symtab record
	 * in preparation for definite assignment analysis
	 */
	create_all_initialization_state_maps(variables);
}


/**
 * Get a variable's initialization state using the SSA gen to state mapping inside
 * of the linked symtab variable
 */
static inline variable_initialization_state_t get_variable_initialization_state(three_addr_var_t* variable){
	symtab_variable_record_t* linked_var = variable->linked_var;
	return linked_var->initialization_state_map[variable->ssa_generation];
}


/**
 * Set a variable's initialization state using the SSA gen to state mapping inside
 * of the linked symtab variable
 */
static inline void set_variable_initialization_state(three_addr_var_t* variable, variable_initialization_state_t new_state){
	symtab_variable_record_t* linked_var = variable->linked_var;
	linked_var->initialization_state_map[variable->ssa_generation] = new_state;
}


/**
 * Before we can perform the actual dataflow analysis, we need to go through and populate the initialization
 * states for all SSA generations. We do this by flagging every single SSA generation that gets assigned
 * to as "definitely initialized". This works because our algorithm is an "optimistic" algorithm, meaning
 * that we assume that everything is initialized properly and need to be proven wrong by the dataflow
 * analysis
 */
static inline void populate_all_initialization_states(symtab_function_record_t* function, dynamic_array_t* postorder_traversal){
	/**
	 * Function parameters are a unique case because we know that by the time we hit the function
	 * entry they are initialized. This preparatory step will acknowledge that fact by populating
	 * all function parameters of generation one with a state of "definitely initialized"
	 */
	for(int32_t i = 0; i < function->function_parameters.current_index; i++){
		symtab_variable_record_t* parameter = dynamic_array_get_at(&(function->function_parameters), i);

		/**
		 * We know for a fact that the first generation of function parameters will *always*
		 * be initialized so we need to populate that now
		 */
		parameter->initialization_state_map[1] = VARIABLE_STATE_DEFINITELY_INITIALIZED;
	}

	/**
	 * Now we will run through every block and set *everything* with an SSA eligible
	 * assignee to be initialized. We do this becuase our forward analysis is an
	 * optimistic algorithm. In other words, we will go through and assume that 
	 * all variables are properly initialized at first and then downgrade them
	 * in the checker as needed
	 */
	for(int32_t i = 0; i < postorder_traversal->current_index; i++){
		basic_block_t* block = dynamic_array_get_at(postorder_traversal, i);
		instruction_t* cursor = block->leader_statement;

		/**
		 * For each instruction, if we have an SSA eligible assignee, then
		 * we are going to assume that it's initialized. This is true also
		 * for phi functions at this stage, though that may change when
		 * we perform the dataflow analysis
		 */
		while(cursor != NULL){
			three_addr_var_t* assignee = cursor->operands.oir.assignee;
			if(is_variable_ssa_eligible(assignee) == TRUE){
				set_variable_initialization_state(assignee, VARIABLE_STATE_DEFINITELY_INITIALIZED);
			}

			//Onto the next one
			cursor = cursor->next_statement;
		}
	}
}


/**
 * Update initialization states in the block by checking every phi function's
 * parameters and performing our merge operation on them. If we find at least
 * one parameter is uninitialized/maybe initialized, then the phi function assignee
 * turns to maybe initialized. The only thing that can do this is a phi function, so
 * we don't need to do any propogation for non-phi functions which is a nice optimization
 * for us
 */
static inline u_int8_t update_initialization_states_in_block(basic_block_t* block){
	/**
	 * Has there been a change in *at least* one initialization status
	 * in an assignee in the block? By default assume no
	 */
	u_int8_t changed = FALSE;

	/**
	 * Crawl over every phi function in the block. These
	 * are the only instructions that can be updated at this
	 * point. Remember that phi functions always occur at the
	 * start of each block, so the instant we see an instruction
	 * that is not one we can stop processing and leave
	 */
	instruction_t* cursor = block->leader_statement;
	while(cursor != NULL && cursor->statement_type == THREE_ADDR_CODE_PHI_FUNC){
		//Extract and save for later
		three_addr_var_t* assignee = cursor->operands.oir.assignee;
		variable_initialization_state_t current_init_state = get_variable_initialization_state(assignee);

		//Assume that we're going to be definitely initialized for sure
		variable_initialization_state_t new_init_state = VARIABLE_STATE_DEFINITELY_INITIALIZED;

		/**
		 * This will act as a sort of "merge" for us where we'll scan the initialization states
		 * of all the phi function parameters. If we see any one that is uninitialized or maybe
		 * uninitialized, then the phi function's assignee is maybe uninitialized
		 */
		for(int32_t i = 0; i < cursor->parameters.current_index; i++){
			three_addr_var_t* parameter = dynamic_array_get_at(&(cursor->parameters), i);

			/**
			 * If we see at least one that is not definitely initialized, then this whole
			 * thing goes to a state of maybe initialized
			 */
			if(get_variable_initialization_state(parameter) != VARIABLE_STATE_DEFINITELY_INITIALIZED){
				new_init_state = VARIABLE_STATE_MAYBE_INITIALIZED;
				break;
			}
		}

		/**
		 * Save the new variable initialization state and record if there
		 * was a change in state
		 */
		if(new_init_state != current_init_state){
			set_variable_initialization_state(assignee, new_init_state);
			changed = TRUE;
		}

		//Bump up to the next one
		cursor = cursor->next_statement;
	}

	return changed;
}


/**
 * Perform dataflow analysis for a given function. The entire point of this helper
 * is to make sure that every eligible variable has all of its SSA generations populated
 * with correct initialization state information before we go and do mutability/definite
 * assignment analysis on it
 */
static inline void perform_dataflow_analysis_for_function(basic_block_t* function_entry, dynamic_array_t* postorder_traversal){
	/**
	 * Get the post order traversal for this function. We will iterate over
	 * it backwards to get the reverse post order traversal(level order)
	 */
	get_post_order_traversal(&(function_entry->function_defined_in->function_blocks), function_entry, postorder_traversal);

	/**
	 * Before we can perform the actual dataflow analysis, we need to go through and populate the initialization
	 * states for all SSA generations. We do this by flagging every single SSA generation that gets assigned
	 * to as "definitely initialized". This works because our algorithm is an "optimistic" algorithm, meaning
	 * that we assume that everything is initialized properly and need to be proven wrong by the dataflow
	 * analysis
	 */
	populate_all_initialization_states(function_entry->function_defined_in, postorder_traversal);

	/**
	 * While changed algorithm ensures that we allow all state changes to
	 * fully propogate over the CFG. In practice this will always converge
	 * because SSA is monotonic.
	 */
	u_int8_t changed;
	do {
		//Assume no change will happen at the start of each iteration
		changed = FALSE;

		/**
		 * Run through everything in reverse postorder(just backwards over the
		 * postorder array). We do this because dataflow is a forward flowing
		 * operation so this converges faster
		 */
		for(int32_t i = postorder_traversal->current_index - 1; i >= 0; i--){
			//Grab our block and let the helper update the phi functions in it
			basic_block_t* block = dynamic_array_get_at(postorder_traversal, i);
			u_int8_t block_changed = update_initialization_states_in_block(block);

			//If at any point one block changes we need to recompute the whole thing
			changed |= block_changed;
		}
	} while(changed == TRUE);
}


/**
 * Perform our dataflow analysis to populate the intiailization state information
 * for all eligible variables. This information will be used by the mutation and
 * definite assignment checker later on to catch "use uninitialized" and "may be
 * used uninitialized" errors, as well as mutability violations
 */
static inline void perform_dataflow_analysis(cfg_t* cfg){
	//Allocate a reusable holder for the postorder traversal
	dynamic_array_t postorder_traversal = dynamic_array_alloc();

	/**
	 * Run through all of the function entry blocks and invoke the per-function
	 * dataflow helper. We will use the reusable postorder traversal dynamic
	 * array and just clear it upon each new function
	 */
	for(int32_t i = 0; i < cfg->function_entry_blocks.current_index; i++){
		basic_block_t* function_entry = dynamic_array_get_at(&(cfg->function_entry_blocks), i);
		perform_dataflow_analysis_for_function(function_entry, &postorder_traversal);

		//This array will be reused - we just need to clear it out
		clear_dynamic_array(&postorder_traversal);
	}

	//We can scrap this now that we're done
	dynamic_array_dealloc(&postorder_traversal);
}


/**
 * Does a given variable comply with the definite assignment rule? This function is
 * blindly called by the instruction analyzer so we will guard against NULL variables
 * and variables that are ineligible for SSA in here. This function also handles
 * all of the error printing for variables that may not have been initialized
 *
 * We map initialization states to SSA inside of the symtab variable itself
 * while we're doing it
 *
 * In other words
 * x[ssa_gen = 0] = UNINITIALIZED
 * x[ssa_gen = 1] = INITIALIZED
 *
 * We need to perform a lookup using the SSA generation inside of the symtab variable's
 * hashmap to get the state that we're using it in
 */
static u_int8_t check_variable_for_definite_assignment(instruction_t* instruction, three_addr_var_t* variable){
	/**
	 * First guard - if the variable is NULL(common) or it's not 
	 * SSA eligible(temp var, etc.) we can leave now. This is still
	 * a SUCCESS because there's nothing wrong with this
	 */
	if(variable == NULL || is_variable_ssa_eligible(variable) == FALSE){
		return SUCCESS;
	}

	/**
	 * The stack and instruction pointer are special cases that are exempt from this
	 * kind of checking so leave if we see it
	 */
	if(variable == stack_pointer_var || variable == instruction_pointer_var){
		return SUCCESS;
	}

	switch(get_variable_initialization_state(variable)){
		/**
		 * Obvious case - it's never been initialized 
		 * so this is a pure use before initialization
		 */
		case VARIABLE_STATE_UNINITIALIZED:
			sprintf(error_info, "Variable %s is used before initialization. First defined here: ", variable->linked_var->var_name.string);
			print_variable_name_to_buffer(error_info, variable->linked_var);
			print_static_analyzer_message(MESSAGE_TYPE_ERROR, error_info, instruction->line_number);
			(*error_count)++;
			return FAILURE;

		/**
		 * Not so obvious case - there are some paths where this variable
		 * is initialized and some where it is not. So, we will
		 *
		 */
		case VARIABLE_STATE_MAYBE_INITIALIZED:
			sprintf(error_info, "Variable %s may be used before initialization. First defined here: ", variable->linked_var->var_name.string);
			print_variable_name_to_buffer(error_info, variable->linked_var);
			print_static_analyzer_message(MESSAGE_TYPE_ERROR, error_info, instruction->line_number);
			(*error_count)++;
			return FAILURE;

		/**
		 * It definitely was initialized so we should be good to go here
		 */
		case VARIABLE_STATE_DEFINITELY_INITIALIZED:
			return SUCCESS;

		//Should never happen
		default:
			fprintf(stderr, "Fatal internal compiler error: variable found to be in an impossible initialization state\n");
			exit(1);
	}
}


/**
 * Does the given instruction comply with the definite assignment rules? We will check 
 * every single eligible variable for compliance. If one variable fails, the whole thing
 * fails out
 * 
 * NOTE: we assume that the caller will never pass a phi function. Phi functions should never
 * be included in definite assignment analysis because they are not real from the programmer's
 * perspective
 */
static inline u_int8_t does_instruction_comply_with_definite_assignment(instruction_t* instruction){
	//By default assume SUCCESS(1)
	u_int8_t overall_result = SUCCESS;

	/**
	 * We only check if we're not a phi function(phi functions have already
	 * been handled by the initializer at this point). We will crawl
	 * through every single variable in use. If at any point a variable
	 * in use is not definitely initialized, we'll display the failure message
	 * and record that this instruction violates definite assignment
	 */
	overall_result &= check_variable_for_definite_assignment(instruction, instruction->operands.oir.operand1);
	overall_result &= check_variable_for_definite_assignment(instruction, instruction->operands.oir.operand2);
	overall_result &= check_variable_for_definite_assignment(instruction, instruction->operands.oir.address_operand1);
	overall_result &= check_variable_for_definite_assignment(instruction, instruction->operands.oir.address_operand2);

	//Check all parameters as well
	for(int32_t i = 0; i < instruction->parameters.current_index; i++){
		three_addr_var_t* parameter = dynamic_array_get_at(&(instruction->parameters), i);

		overall_result &= check_variable_for_definite_assignment(instruction, parameter);
	}

	return overall_result;
}


/**
 *
 * TODO DOC
 * NOTE: we assume that the caller will never pass a phi function as a parameter
 */
static inline u_int8_t does_instruction_comply_with_mutability_constraints(instruction_t* instruction){
	three_addr_var_t* assignee = instruction->operands.oir.assignee;

	/**
	 * First guard - if the variable is NULL(common) or it's not 
	 * SSA eligible(temp var, etc.) we can leave now. This is still
	 * a SUCCESS because there's nothing wrong with this
	 */
	if(assignee == NULL || is_variable_ssa_eligible(assignee) == FALSE){
		return SUCCESS;
	}

	/**
	 * The stack and instruction pointer are special cases that are exempt from this
	 * kind of checking so leave if we see it
	 */
	if(assignee == stack_pointer_var || assignee == instruction_pointer_var){
		return SUCCESS;
	}

	/**
	 * If the assignee is mutable, then it can be assigned to all we want
	 * so we always succeed here
	 */
	if(assignee->type->mutability == MUTABLE){
		return SUCCESS;
	}

	/**
	 * Once we get here, we'll need to get what this instruction overwrites. We do this
	 * by indexing into the overwritten generation map using the ssa generation on the
	 * three address variable. The result will either be a valid generation that we can
	 * use to see if it was initialized or it's -1
	 */
	symtab_variable_record_t* linked_var = assignee->linked_var;
	int32_t overwritten_generation = linked_var->ssa_overwritten_generation_map.internal_array[assignee->ssa_generation];

	/**
	 * This should actually never happen because Ollie uses
	 * synthetic initialization values. However if it did
	 * happen, this would count as an initialization so
	 * we will take it
	 */
	if(overwritten_generation == OVERWRITES_NOTHING){
		return SUCCESS;
	}

	//TODO DELETE
	//printf("MADE IT HERE FOR %s\n", linked_var->var_name.string);

	//printf("GENERATION %d overwrites GENERATION %d", assignee->ssa_generation, overwritten_generation);

	/**
	 * Otherwise, we'll need to lookup what the state of the overwritten
	 * generation was. If it was definitely or maybe initialized, then
	 * this is a mutation and therefore not allowed
	 */
	variable_initialization_state_t overwritten_init_state = linked_var->initialization_state_map[overwritten_generation];
	switch(overwritten_init_state){
		/**
		 * Uninitialized - all good for us
		 */
		case VARIABLE_STATE_UNINITIALIZED:
			return SUCCESS;

		/**
		 * This is a potential mutation - still a violation for us
		 */
		case VARIABLE_STATE_MAYBE_INITIALIZED:
			sprintf(error_info, "Variable %s was declared with an immutable type but may be mutated. First defined here:", linked_var->var_name.string);
			print_variable_name_to_buffer(error_info, linked_var);
			print_static_analyzer_message(MESSAGE_TYPE_ERROR, error_info, instruction->line_number);
			(*error_count)++;
			return FAILURE;

		/**
		 * This is a definite mutation - definite violation
		 */
		case VARIABLE_STATE_DEFINITELY_INITIALIZED:
			sprintf(error_info, "Variable %s was declared with an immutable type but is mutated. First defined here:", linked_var->var_name.string);
			print_variable_name_to_buffer(error_info, linked_var);
			print_static_analyzer_message(MESSAGE_TYPE_ERROR, error_info, instruction->line_number);
			(*error_count)++;
			return FAILURE;

		//Should never happen but just in case
		default:
			fprintf(stderr, "Fatal internal compiler error: unknown initialization type found on variable\n");
			exit(1);
	}
}


/**
 * Perform definite assignment analysis on every instruction in a given block. We will scan
 * through every instruction and call the helper. Note that one failure will not stop the
 * entire analysis and we will always scan the entire thing no matter way
 *
 * This function recursively calls out to the dominator children of the block that it's analyzing.
 * To run the full analysis you need to call this function initially with the function entry
 * block
 *
 * NOTE: this function is recursive
 */
static u_int8_t perform_initialization_and_mutability_analysis_for_block(basic_block_t* block){
	//Assume success off the bat
	u_int8_t result = SUCCESS;

	//Grab a leader statement out
	instruction_t* cursor = block->leader_statement;

	/**
	 *
	 * TODO DOC
	 */
	while(cursor != NULL){
		if(cursor->statement_type == THREE_ADDR_CODE_PHI_FUNC){
			cursor = cursor->next_statement;
			continue;
		}

		/**
		 * TODO DOC
		 */
		result &= does_instruction_comply_with_definite_assignment(cursor);
		result &= does_instruction_comply_with_mutability_constraints(cursor);

		cursor = cursor->next_statement;
	}

	/**
	 * For all dominator children of this block, go through and perform the
	 * definite assignment analysis
	 */
	for(int32_t i = 0; i < block->dominator_children.current_index; i++){
		basic_block_t* child = dynamic_array_get_at(&(block->dominator_children), i);

		/**
		 * If anything in this child fails, our overall result is failure. We will
		 * keep going to scan everything though
		 */
		if(perform_initialization_and_mutability_analysis_for_block(child) == FALSE){
			result = FAILURE;
		}
	}

	return result;
}


/**
 * TODO DOC
 */
static inline u_int8_t perform_definite_assignment_and_mutability_analysis(cfg_t* cfg){
	//Assume success off the bat
	u_int8_t result = SUCCESS;

	//Run through all functions
	for(int32_t i = 0; i < cfg->function_entry_blocks.current_index; i++){
		//Use the function entry to seed the search
		basic_block_t* function_entry = dynamic_array_get_at(&(cfg->function_entry_blocks), i);

		/**
		 * Update the dependency node so that we get accurate error printouts
		 */
		current_dependency_node = function_entry->function_defined_in->dependency_graph_node;

		/**
		 * Call into the recursive analyzer. If we have a failure, then the entire thing
		 * goes into failure, but we will keep scanning to get all errors in at once
		 */
		if(perform_initialization_and_mutability_analysis_for_block(function_entry) == FAILURE){
			result = FAILURE;
		}
	}

	return result;
}


/**
 * Perform mutability checking on the variable symtab. This will check
 * and display warnings if we are labeling a variable as mutable
 * but then never mutating it
 */
static void perform_mutability_checking(variable_symtab_t* symtab){
	char info[ERROR_SIZE * 3];

	//Run through all sheafs(lexical scopes) in the symtab
	for(int32_t i = 0; i < symtab->sheafs.current_index; i++){
		symtab_variable_sheaf_t* sheaf = dynamic_array_get_at(&(symtab->sheafs), i);

		//For each scope go through all record slots
		for(int32_t j = 0; j  < VARIABLE_KEYSPACE; j++){
			//For each record slot grab a cursor and crawl
			symtab_variable_record_t* cursor = sheaf->records[j];

			//Keep going so long as we have things to drill into
			while(cursor != NULL){
				//If it's not mutable we don't care to check it
				if(cursor->type_defined_as->mutability == NOT_MUTABLE){
					cursor = cursor->next;
					continue;
				}

				//We do not currently support memory SSA so we have to skip
				if(is_memory_address_type(cursor->type_defined_as) == TRUE || cursor->stack_variable == TRUE){
					cursor = cursor->next;
					continue;
				}

				/**
				 * If the symtab variable is not SSA eligible then this
				 * is not going to work, we will move on
				 *
				 * We also don't bother with function parameters
				 */
				if(is_symtab_variable_ssa_eligible(cursor) == FALSE || cursor->membership == FUNCTION_PARAMETER){
					cursor = cursor->next;
					continue;
				}

				/**
				 * If the SSA counter is at 2, that means that the *next* LHS generation would have been
				 * 2 *IF* it was ever hit. Since we are at 2, it means that we only did one LHS operation
				 * with this value. Therefore, this tells us that the variable was never mutated
				 */
				if(cursor->ssa_counter == 2){
					sprintf(info, "Variable \"%s\" is declared as mutable but never mutated. Consider removing the \"mut\" keyword. First defined here:", cursor->var_name.string);
					print_variable_name_to_buffer(info, cursor);
					print_static_analyzer_message(MESSAGE_TYPE_WARNING, info, cursor->line_number);
					(*warning_count)++;
				}

				//Bump up to the next record
				cursor = cursor->next;
			}
		}
	}
}


/**
 * Crawl the function symtab and check for functions that 
 * are unused. We will generate warnings for every function that
 * is defined but never called
 */
static void perform_function_usage_analysis(function_symtab_t* symtab){
	//Run thorugh all of the namespaces
	for(int32_t _ = 0; _ < symtab->namespaces.current_index; _++){
		//Grab the current sheaf to check
		function_namespace_t* current_sheaf = dynamic_array_get_at(&(symtab->namespaces), _);

		//Now run through the keyspace in this sheaf
		for(int32_t i = 0; i < FUNCTION_KEYSPACE; i++){
			symtab_function_record_t* record = current_sheaf->records[i];

			while(record != NULL){
				/**
				 * Warn case 1: we have a function that was declared
				 * but never defined and never called
				 */
				if(record->called == FALSE && record->defined == FALSE){
					sprintf(error_info, "Function \"%s\" is never defined and never called. First defined here:", record->func_name.string);
					print_function_name_to_buffer(error_info, record);
					print_static_analyzer_message(MESSAGE_TYPE_WARNING, error_info, record->line_number);
					(*warning_count)++;

				/**
				 * If a function is defined but never called that's another kind of warning
				 */
				} else if(record->called == FALSE && record->defined == TRUE && record->visibility == VISIBILITY_TYPE_PRIVATE){
					sprintf(error_info, "Function \"%s\" is defined but never called. First defined here:", record->func_name.string);
					print_function_name_to_buffer(error_info, record);
					print_static_analyzer_message(MESSAGE_TYPE_WARNING, error_info, record->line_number);
					(*warning_count)++;

				/**
				 * If a function is called but never defined that's another kind of issue 
				 */
				} else if(record->called == TRUE && record->defined == FALSE){
					sprintf(error_info, "Function \"%s\" is called but never explicitly defined. If you are using Ollie in the standard way this will cause a runtime error. First declared here:", record->func_name.string);
					print_function_name_to_buffer(error_info, record);
					print_static_analyzer_message(MESSAGE_TYPE_WARNING, error_info, record->line_number);
					(*warning_count)++;
				}

				//Advance record up
				record = record->next;
			}
		}
	}
}



/**
 * Perform all static analysis on a given CFG. The functions
 * performed herein are:
 * 	1.) Mangling static variable names
 * 	2.) Converting the CFG into SSA form
 * 	3.) Populate all initialization states in preparation for
 * 		later analysis
 * 	3.) Perform definite assignment analysis & mutation analysis
 * 		- This is a potential failure point
 * 	4.) Perform function call analysis
 * 	5.) Perform mutability checking for variables that we never mutated
 */
cfg_construction_result_type_t perform_all_static_analysis(cfg_t* cfg, front_end_results_package_t* results, u_int32_t* num_errors, u_int32_t* num_warnings){
	//By default assume success
	cfg_construction_result_type_t result = CFG_RESULT_SUCCESS;

	//Cache these two for later use
	instruction_pointer_var = cfg->instruction_pointer;
	stack_pointer_var = cfg->stack_pointer;

	//Cache these as well
	error_count = num_errors;
	warning_count = num_warnings;

	/**
	 * 1.) Mangle all static variable names with a unique number identifier at the very end
	 * to avoid name collisions
	 */
	mangle_static_variable_names(&(cfg->global_variables));

	/**
	 * 2.) Convert the CFG into static single assignment(SSA) form. This form is the
	 * basis for all of our future checks & optimizations
	 */
	convert_cfg_to_ssa_form(cfg, results->variable_symtab);

	/**
	 * 3.) Populate the intialization states for all variables in
	 * the CFG using a forward dataflow analysis for each and every
	 * function. When done, all eligible variables will have thier
	 * initialization maps fully populated
	 */
	perform_dataflow_analysis(cfg);

	/**
	 * 4.) Perform definite assignment and mutability analysi
	 *
	 * NOTE: this is a potential fail point for the CFG
	 */
	if(perform_definite_assignment_and_mutability_analysis(cfg) == FAILURE){
		result = CFG_RESULT_FAILURE;
	}

	/**
	 * 4.) Crawl the function symtab and generate warnings for functions
	 * that are defined but not used
	 */
	perform_function_usage_analysis(results->function_symtab);

	/**
	 * 5.) Perform mutability checking. Unlike definite assignment
	 * analysis there is no chance for failure here, this
	 * just generates warnings
	 */
	perform_mutability_checking(results->variable_symtab);

	//Give back whatever result we've have
	return result;
}
