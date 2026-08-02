/**
 * Author: Jack Robbins
 * This file implements the APIs for the static analyzer that were defined in the
 * header file of the same name
 */

#include "static_analyzer.h"
#include <sys/types.h>

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
 * This simple utility will scan a dynamic array of variables and invoke the variables_equal() function
 * on each of them for the given variable
 */
static inline u_int8_t does_variable_dynamic_array_contain_variable(dynamic_array_t* array, three_addr_var_t* variable){
	for(int32_t i = 0; i < array->current_index; i++){
		three_addr_var_t* candidate = dynamic_array_get_at(array, i);

		//If we have one equals then the whole thing works
		if(variables_equal(candidate, variable) == TRUE){
			return TRUE;
		}
	}

	//If we made it here then no match
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
 * 	push the current SSA generation number onto the counter stack
 * 	variable's SSA generation is the current number
 * 	bump the SSA generation number for the next go 
 */
static inline void lhs_new_name(three_addr_var_t* var){
	//Grab the linked variable out
	symtab_variable_record_t* linked_var = var->linked_var;

	//Grab the name out of the counter
	int32_t generation_level = linked_var->ssa_counter;

	//Now we increment the counter for the next go around
	(linked_var->ssa_counter)++;

	//We'll also push this generation level onto the stack
	lightstack_push(&(linked_var->counter_stack), generation_level);

	//Store the generation level in here
	var->ssa_generation = generation_level;
}


/**
 * For a left hand side(assignment) new name:
 * 	push the current SSA generation number onto the counter stack
 * 	bump the SSA generation number
 */
static inline void lhs_new_name_direct(symtab_variable_record_t* variable){
	//Store the old generation level
	u_int16_t generation_level = variable->ssa_counter;

	//Increment the counter
	(variable->ssa_counter)++;

	//Push the old generation level onto here
	lightstack_push(&(variable->counter_stack), generation_level);
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

				//Emit the LHS new name directly here
				lhs_new_name_direct(cursor);

				//Emit the three address representation
				three_addr_var_t* starting_variable = emit_var(cursor);

				/**
				 * This counts as a definition for this variable inside of our
				 * given function's entry block
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
				 * Since we use the "visited" tag to keep track of whether or not a block
				 * was ever on the worklist, we'll need to reset this here
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

						//If this already has a phi function for this run we skip it
						if(df_node->already_has_phi_func == TRUE){
							continue;
						}

						/**
						 * ----------------------------------------
						 *  CRITERION:
						 *  If a variable is NOT Live-out at the join node,
						 *  that means that it is not LIVE-IN at any of
						 *  the successors of that block. If a variable
						 *  is not active(used) at the join node either,
						 *  that means that the phi function is useless.
						 *
						 * So, we will skip inserting a phi function
						 * if the variable is not used and not LIVE_OUT
						 * at N
						 * ----------------------------------------
						 */
						if(does_variable_dynamic_array_contain_symtab_variable(&(df_node->used_before_definition), record) == FALSE 
							&& does_variable_dynamic_array_contain_symtab_variable(&(df_node->live_out), record) == FALSE){
							continue;
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

						//Flag that this already has a phi function 
						df_node->already_has_phi_func = TRUE;

						/**
						 * If we haven't visited this block yet then we'll add it to our worklist
						 * for the next go around
						 */
						if(df_node->visited == FALSE){
							dynamic_array_add(&worklist, df_node);
						}
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
				lhs_new_name(cursor->operands.oir.assignee);
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
}


/**
 * Does a given variable comply with the definite assignment rule? This function is
 * blindly called by the instruction analyzer so we will guard against NULL variables
 * and variables that are ineligible for SSA in here. This function also handles
 * all of the error printing for variables that may not have been initialized
 */
static u_int8_t check_variable_for_definite_assignment(instruction_t* instruction, three_addr_var_t* variable, dynamic_array_t* may_not_have_been_initialized){
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

	/**
	 * Obvious case - generation of 0 means it's never
	 * been initialized so this is a pure use before
	 * initialization
	 */
	if(variable->ssa_generation == 0){
		sprintf(error_info, "Variable %s is used before initialization", variable->linked_var->var_name.string);
		print_static_analyzer_message(MESSAGE_TYPE_ERROR, error_info, instruction->line_number);
		(*error_count)++;
		return FAILURE;

	/**
	 * Not so obvious case - it being in this array means that it comes from a phi function
	 * that itself had a parameter of generation 0, meaning that it's not a guarantee that
	 * this wasn't initialized but it may not have been, which is still an error
	 */
	} else if(does_variable_dynamic_array_contain_variable(may_not_have_been_initialized, variable) == TRUE){
		sprintf(error_info, "Variable %s may be used before initialization", variable->linked_var->var_name.string);
		print_static_analyzer_message(MESSAGE_TYPE_ERROR, error_info, instruction->line_number);
		(*error_count)++;
		return FAILURE;

	/**
	 * If we made it here then this worked and the variable is clean
	 */
	} else {
		return SUCCESS;
	}
}


/**
 * Does the given instruction comply with the definite assignment rules? There are two paths
 * that this check will take:
 * 
 * 1.) We are not a phi function - in this case just check every variable in the RHS to see if
 * 	  it's either completely uninitialized or may be uninitialized(see below). If any one of
 * 	  the variables fails then the whole things fails
 * 2.) We are a phi function - we must check every variable in the parameter list. If any
 * 	   one in the parameter list is an _0 variable *OR* is in our "may_not_have_been_initialized"
 * 	   list, then we will flag that LHS variable as potentially being uninitialized for future checks.
 * 	   Since we do this scan from top to bottom in the function using dominators this check will work
 */
static inline u_int8_t does_instruction_comply_with_definite_assignment(instruction_t* instruction, dynamic_array_t* may_not_have_been_initialized){
	//By default assume SUCCESS(1)
	u_int8_t overall_result = SUCCESS;

	/**
	 * Regular non-phi function handling involves us checking every
	 * single variable to see if we have any "_0" variables in use.
	 * "_0" is our canary SSA value that represents an uninitialzed
	 * variable. We will also need to make sure that each variable
	 * is not a member of the "may_not_have_been_initialized" array.
	 * This gets built up from phi functions who have values that may
	 * have never been initialized
	 *
	 * We bitwise and the results together for this. One false in the chain
	 * will make the whole thing 0(FAILURE)
	 */
	if(instruction->statement_type != THREE_ADDR_CODE_PHI_FUNC){
		overall_result &= check_variable_for_definite_assignment(instruction, instruction->operands.oir.operand1, may_not_have_been_initialized);
		overall_result &= check_variable_for_definite_assignment(instruction, instruction->operands.oir.operand2, may_not_have_been_initialized);
		overall_result &= check_variable_for_definite_assignment(instruction, instruction->operands.oir.address_operand1, may_not_have_been_initialized);
		overall_result &= check_variable_for_definite_assignment(instruction, instruction->operands.oir.address_operand2, may_not_have_been_initialized);

		//Check all parameters as well
		for(int32_t i = 0; i < instruction->parameters.current_index; i++){
			three_addr_var_t* parameter = dynamic_array_get_at(&(instruction->parameters), i);

			overall_result &= check_variable_for_definite_assignment(instruction, parameter, may_not_have_been_initialized);
		}


	/**
	 * Phi-functions have special handling. If we have a phi
	 * function that has at least one _0 variable in it, then the
	 * LHS value may not have been initialized
	 *
	 * x_3 <- phi(x_2, x_1, x_0)
	 * x_4 <- x_3 + 1
	 *
	 * Fail there, x_3 may not have been initialized. We will maintain
	 * a list of variables that may be uninitialized that will be cross
	 * referenced by all other checks. The value x_3 in this case would go
	 * into there
	 */
	} else {
		/**
		 * Run through all of the parameters - all it takes is for one
		 *
		 * declare x:mut i32;
		 *
		 * if(<cond>){
		 * 		x = 3;
		 * } 
		 *
		 * ret x; <--- x may be used uninitialized here
		 *
		 */
		for(int32_t i = 0; i < instruction->parameters.current_index; i++){
			three_addr_var_t* parameter = dynamic_array_get_at(&(instruction->parameters), i);

			/**
			 * If we see a parameter with a generation of 0, that means that it's 
			 * never been initialized. We will add the LHS to the "may_not_have_been_initialized"
			 * list for later checks
			 */
			if(parameter->ssa_generation == 0){
				dynamic_array_add(may_not_have_been_initialized, instruction->operands.oir.assignee);
				overall_result = FAILURE;
				
				//We don't need to check any further for this
				break;

			/**
			 * Just because it's not 0 doesn't mean we're safe. We must also check if this value
			 * is inside of the "may not be initialized" list. If it is, then subsequently the
			 * LHS of this instruction is also potentially uninitialized
			 */
			} else if(does_variable_dynamic_array_contain_variable(may_not_have_been_initialized, parameter) == TRUE){
				dynamic_array_add(may_not_have_been_initialized, instruction->operands.oir.assignee);
				overall_result = FAILURE;
				
				//We don't need to check any further for this
				break;
			}
		}
	}

	return overall_result;
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
static u_int8_t perform_definite_assignment_analysis_for_block(basic_block_t* block, dynamic_array_t* may_not_have_been_initialized){
	//Assume success off the bat
	u_int8_t result = SUCCESS;

	//Grab a leader statement out
	instruction_t* cursor = block->leader_statement;

	/**
	 * Go through every single statement and analyze every
	 * variable within. If any of the statements is using
	 * a _0 version, that counts as a use-before-initialize
	 * and it is an error. If we have a phi-function that
	 * has a _0 parameter, that is "maybe" use before initialize
	 * and it is still an error
	 *
	 * NOTE: we do *NOT* check anything to do with the so-called "rip_offset_var". This 
	 * is not a variable in the true sense so it's not worth it to mess around with it
	 */
	while(cursor != NULL){
		/**
		 * Any one instruction failing definite assignment means that the whole
		 * thing fails. We will process all instructions to get a full picture of the
		 * errors though
		 */
		if(does_instruction_comply_with_definite_assignment(cursor, may_not_have_been_initialized) == FALSE){
			result = FALSE;
		}

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
		if(perform_definite_assignment_analysis_for_block(child, may_not_have_been_initialized) == FALSE){
			result = FAILURE;
		}
	}

	return result;
}


/**
 * Perform the Ollie analyzer's version of definite assignment analysis.
 *
 * We will scan all functions at once. If one function fails, we will still keep going to analyze
 * the rest of the program. However, one function failing does mean that the entire program fails
 * to compile in the end. All functions are scanned in dominator order meaning that we start
 * from the top and work our way down through the dominator children
 */
static inline u_int8_t perform_definite_assignment_analysis(cfg_t* cfg, variable_symtab_t* variables){
	//Assume success off the bat
	u_int8_t result = SUCCESS;

	/**
	 * Keep an array of variables that may not have been initialized in each function to make
	 * scanning easier and less intensive. We'll allocate once and just wipe every time
	 */
	dynamic_array_t may_not_have_been_initialized = dynamic_array_alloc();

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
		if(perform_definite_assignment_analysis_for_block(function_entry, &may_not_have_been_initialized) == FAILURE){
			result = FAILURE;
		}

		//Clear it now that we're done with this function
		clear_dynamic_array(&may_not_have_been_initialized);
	}

	//Done with this so scrap it now
	dynamic_array_dealloc(&may_not_have_been_initialized);

	return result;
}


/**
 * Perform mutability checking on the variable symtab. This will check
 * and display warnings if we are labeling a variable as mutable
 * but then never mutating it
 */
static void perform_mutability_checking(cfg_t* cfg, variable_symtab_t* symtab){

	for(int32_t i = 0; i < symtab->sheafs.current_index; i++){
		symtab_variable_sheaf_t* sheaf = dynamic_array_get_at(&(symtab->sheafs), i);

		for(int32_t i = 0; i < VARIABLE_KEYSPACE; i++){
			symtab_variable_record_t* cursor = sheaf->records[i];

			while(cursor != NULL){

				//TODO DEPENDENCY NODE
				//TODO


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
				 * If a function is called but never defined that's another kind of issue TODO IS THIS JUST A WARNING???
				 */
				} else if(record->called == TRUE && record->defined == FALSE){
					sprintf(error_info, "Function \"%s\" is called but never explicitly defined. First declared here:", record->func_name.string);
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
 * 	3.) Perform definite assignment analysis
 * 		- This is a potential failure point
 * 	4.) Perform variable mutation analysis
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
	 * 3.) perform definite assignment analysis on the entire
	 * CFG. This process will verify that all variables are only
	 * used after they are guaranteed to have been assigned
	 *
	 * NOTE: this is a potential fail point for the CFG
	 */
	if(perform_definite_assignment_analysis(cfg, results->variable_symtab) == FAILURE){
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
	perform_mutability_checking(cfg, results->variable_symtab);

	//Give back whatever result we've have
	return result;
}
