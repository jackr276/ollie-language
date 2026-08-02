/**
 * Author: Jack Robbins
 * This file implements the APIs for the static analyzer that were defined in the
 * header file of the same name
 */

#include "static_analyzer.h"

//========================================= General Utilities =============================================
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
 * Perform all static analysis on a given CFG. The functions
 * performed herein are:
 * 	1.) Mangling static variable names
 * 	2.) Converting the CFG into SSA form
 * 	3.) Perform definite assignment analysis
 * 		- This is a potential failure point
 * 	4.) Perform variable mutation analysis
 */
cfg_construction_result_type_t perform_all_static_analysis(cfg_t* cfg, front_end_results_package_t* results){
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

	//TODO DUMMY FOR NOW
	return CFG_RESULT_SUCCESS;

}
