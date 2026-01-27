/**
 * Author: Jack Robbins
 *
 * This is the implementation file for the ollie optimizer. Currently
 * it is implemented as one monolothic block
*/
#include "optimizer.h"
#include "../utils/queue/heap_queue.h"
#include "../utils/constants.h"
#include <sys/select.h>
#include <sys/types.h>

/**
 * Combine two blocks into one
 */
static void combine(cfg_t* cfg, basic_block_t* a, basic_block_t* b){
	//If b is null, we just return a. This in reality should never happen
	if(b == NULL){
		return;
	}

	//What if a was never even assigned?
	if(a->exit_statement == NULL){
		a->leader_statement = b->leader_statement;
		a->exit_statement = b->exit_statement;

	//If the leader statement is NULL - we really don't need to do anything. If it's not however, we
	//will need to add everything in
	} else if(b->leader_statement != NULL){
		//Otherwise it's a "true merge"
		//The leader statement in b will be connected to a's tail
		a->exit_statement->next_statement = b->leader_statement;
		//Connect backwards too
		b->leader_statement->previous_statement = a->exit_statement;
		//Now once they're connected we'll set a's exit to be b's exit
		a->exit_statement = b->exit_statement;
	}

	//In our case for "combine" - we know for a fact that "b" only had one predecessor - which is "a"
	//As such, we won't even bother looking at the predecessors

	//Now merge successors
	for(u_int16_t i = 0; i < b->successors.current_index; i++){
		basic_block_t* successor = dynamic_array_get_at(&(b->successors), i);

		//Add b's successors to be a's successors
		add_successor_only(a, successor);

		//Now for each of the predecessors that equals b, it needs to now point to A
		for(u_int16_t j = 0; j < successor->predecessors.current_index; j++){
			//If it's pointing to b, it needs to be updated
			if(successor->predecessors.internal_array[j] == b){
				//Update it to now be correct
				successor->predecessors.internal_array[j] = a;
			}
		}
	}

	//Copy over the block type and terminal type
	if(a->block_type != BLOCK_TYPE_FUNC_ENTRY){
		a->block_type = b->block_type;
	}

	//If b is a switch statment start block, we'll copy the jump table
	if(b->jump_table != NULL){
		a->jump_table = b->jump_table;
	}

	//Increment the number of instructions in here
	a->number_of_instructions += b->number_of_instructions;

	//For each statement in b, all of it's old statements are now "defined" in a
	instruction_t* b_stmt = b->leader_statement;

	//Modify these "block contained in" references to be A
	while(b_stmt != NULL){
		b_stmt->block_contained_in = a;

		//Push it up
		b_stmt = b_stmt->next_statement;
	}
	
	//We'll remove this from the list of created blocks
	dynamic_array_delete(&(cfg->created_blocks), b);
}


/**
 * Remove a statement from a block. This is more like a soft deletion, we are
 * not actually deleting the statement, just moving it from once place to another
 */
void remove_statement(instruction_t* stmt){
	//Grab the block out
	basic_block_t* block = stmt->block_contained_in;

	//We are losing a statement here
	block->number_of_instructions--;

	//If it's the leader statement, we'll just update the references
	if(block->leader_statement == stmt){
		//Special case - it's the only statement. We'll just delete it here
		if(block->leader_statement->next_statement == NULL){
			//Just remove it entirely
			block->leader_statement = NULL;
			block->exit_statement = NULL;
		//Otherwise it is the leader, but we have more
		} else {
			//Update the reference
			block->leader_statement = stmt->next_statement;
			//Set this to NULL
			block->leader_statement->previous_statement = NULL;
		}

	//What if it's the exit statement?
	} else if(block->exit_statement == stmt){
		instruction_t* previous = stmt->previous_statement;
		//Nothing at the end
		previous->next_statement = NULL;

		//This now is the exit statement
		block->exit_statement = previous;
		
	//Otherwise, we have one in the middle
	} else {
		//Regular middle deletion here
		instruction_t* previous = stmt->previous_statement;
		instruction_t* next = stmt->next_statement;
		previous->next_statement = next;
		next->previous_statement = previous;
	}

	//This statement is listless(for now)
	stmt->previous_statement = NULL;
	stmt->next_statement = NULL;
	stmt->block_contained_in = NULL;
}


/**
 * Split a block, taking all statements beginning at start(inclusive) until
 * the end and putting them into the new block
 *
 * .L1
 *   A
 *   B
 *   C <----- split start
 *   D
 *   E
 *
 *  .L1
 *   A
 *   B
 *   
 *  .L2
 *   C
 *   D
 *   E
 *
 * NOTE: this rule does *no* successor management or branch insertion
 *
 */
static void bisect_block(basic_block_t* new, instruction_t* bisect_start){
	//Grab a cursor to the start statement
	instruction_t* cursor = bisect_start;

	//So long as this is not NULL
	while(cursor != NULL){
		//What we'll actually use
		instruction_t* holder = cursor;

		//Push this one up
		cursor = cursor->next_statement;

		//Remove the holder from the original block
		remove_statement(holder);

		//Add it to the new block
		add_statement(new, holder);
	}
}


/**
 * Mark definitions(assignment) of a three address variable within a given
 * function. The current_function parameter is an optimization step designed to help
 * us weed out useless blocks. Note that the variable passed in may be null. If it is,
 * we just leave immediately
 */
static void mark_and_add_definition(cfg_t* cfg, three_addr_var_t* variable, symtab_function_record_t* current_function, dynamic_array_t* worklist){
	//If the variable is NULL, we leave
	if(variable == NULL){
		return;
	}

	//There is no point in trying to mark a variable like this, we will
	//never find the definition since they exist by default
	if(variable == cfg->stack_pointer 
		|| variable == cfg->instruction_pointer
		|| variable->variable_type == VARIABLE_TYPE_LOCAL_CONSTANT
		|| variable->variable_type == VARIABLE_TYPE_FUNCTION_ADDRESS){
		return;
	}

	//If this variable has a stack region, then we will be marking
	//said stack region. We know that this discriminating union is a stack
	//region because of the if-check above that rules out local constants
	if(variable->associated_memory_region.stack_region != NULL){
		mark_stack_region(variable->associated_memory_region.stack_region);
	}

	//Run through everything here
	for(u_int16_t _ = 0; _ < cfg->created_blocks.current_index; _++){
		//Grab the block out
		basic_block_t* block = dynamic_array_get_at(&(cfg->created_blocks), _);

		//If it's not in the current function and it's temporary, get rid of it
		if(block->function_defined_in != current_function){
			continue;
		}

		//This is always where we start
		instruction_t* stmt = block->exit_statement;

		switch(variable->variable_type){
			case VARIABLE_TYPE_NON_TEMP:
			case VARIABLE_TYPE_MEMORY_ADDRESS:
				stmt = block->exit_statement;

				//So long as this isn't NULL
				while(stmt != NULL){
					//If it's marked we're out of here
					if(stmt->mark == TRUE || stmt->assignee == NULL){
						stmt = stmt->previous_statement;
						continue;
					}

					//Is the assignee our variable AND it's unmarked?
					if(stmt->assignee->linked_var == variable->linked_var
						&& stmt->assignee->ssa_generation == variable->ssa_generation){
						//Add this in
						dynamic_array_add(worklist, stmt);
						//Mark it
						stmt->mark = TRUE;
						//Mark it
						block->contains_mark = TRUE;
						return;
					}

					//Advance the statement
					stmt = stmt->previous_statement;
				}

				break;

			case VARIABLE_TYPE_TEMP:
				//So long as this isn't NULL
				while(stmt != NULL){
					//If this is the case, we'll just go onto the next one
					if(stmt->mark == TRUE || stmt->assignee == NULL){
						stmt = stmt->previous_statement;
						continue;
					}

					//Is the assignee our variable AND it's unmarked?
					if(stmt->assignee->temp_var_number == variable->temp_var_number){
						//Add this in
						dynamic_array_add(worklist, stmt);
						//Mark it
						stmt->mark = TRUE;
						//Mark the block
						block->contains_mark = TRUE;
						return;
					}

					//Advance the statement
					stmt = stmt->previous_statement;
				}

				break;

			default:
				printf("Fatal internal compiler error: attempting to mark invalid variable type\n");
				exit(1);
		}
	}
}


/**
 * The mark algorithm will go through and mark every operation(three address code statement) as
 * critical or noncritical. We will then go back through and see which operations are setting
 * those critical values
 *
 * for each operation i:
 * 	clear i's mark
 * 	if i is critical then
 * 		mark i
 * 		add i to the worklist
 * 	while worklist not empty
 * 		remove i from the worklist i is x <- y op z
 * 		if def(y) is not marked then
 * 			mark def(y)
 * 			add def(y) to worklist
 * 		if def(z) is not marked then
 * 			mark def(z)
 * 			add def(y) to worklist
 * 		for each block b in RDF(block(i))
 * 			let j be the branch that ends b
 * 			if j is unmarked then
 * 				mark j
 * 				add j to worklist
 */
static void mark(cfg_t* cfg){
	//First we'll need a worklist
	dynamic_array_t worklist = dynamic_array_alloc();

	//Now we'll go through every single operation in every single block
	for(u_int16_t _ = 0; _ < cfg->created_blocks.current_index; _++){
		//Grab the block we'll work on
		basic_block_t* current = dynamic_array_get_at(&(cfg->created_blocks), _);

		//Grab a cursor to the current statement
		instruction_t* current_stmt = current->leader_statement;

		/**
		 * We'll now go through and mark every statement that we
		 * deem to be critical in the block. Statements are critical
		 * if they:
		 * 	1.) set a return value
		 * 	2.) is an input/output statement
		 * 	3.) affects the value in a storage location that could be
		 * 		accessed outside of the procedure(i.e. a parameter that is a pointer)
		 */
		while(current_stmt != NULL){
			//Clear it's mark
			current_stmt->mark = FALSE;

			/**
			 * We will go through every operation and determine its importance based on our rules
			 */
			switch(current_stmt->statement_type){
				/**
				 * Return statements are always considered important
				 */
				case THREE_ADDR_CODE_RET_STMT:
					//Mark this as useful
					current_stmt->mark = TRUE;
					//Add it to the list
					dynamic_array_add(&worklist, current_stmt);
					//The block now has a mark
					current->contains_mark = TRUE;
					break;

				/**
				 * Asm inline statements are also
				 * always important because we don't 
				 * analyze them, so the user assumes that
				 * their direct code will be executed
				 */
				case THREE_ADDR_CODE_ASM_INLINE_STMT:
					current_stmt->mark = TRUE;
					//Add it to the list
					dynamic_array_add(&worklist, current_stmt);
					//The block now has a mark
					current->contains_mark = TRUE;
					break;

				/**
				 * Since we don't know whether or not a function
				 * that is being called performs an important task,
				 * we also always consider it to be important
				 */
				case THREE_ADDR_CODE_FUNC_CALL:
					current_stmt->mark = TRUE;
					//Add it to the list
					dynamic_array_add(&worklist, current_stmt);
					//The block now has a mark
					current->contains_mark = TRUE;
					break;

				/**
				 * Indirect function calls are the same as function calls. They will 
				 * always count becuase we do not know whether or not the indirectly
				 * called function performs some important task. As such, we will 
				 * mark it as important
				 */
				case THREE_ADDR_CODE_INDIRECT_FUNC_CALL:
					current_stmt->mark = TRUE;
					//Add it to the list
					dynamic_array_add(&worklist, current_stmt);
					//The block now has a mark
					current->contains_mark = TRUE;
					break;

				/**
				 * And finally idle statements are considered important
				 * because they literally do nothing, so if the user
				 * put them there, we'll assume that it was for a good reason
				 */
				case THREE_ADDR_CODE_IDLE_STMT:
					current_stmt->mark = TRUE;
					//Add it to the list
					dynamic_array_add(&worklist, current_stmt);
					//The block now has a mark
					current->contains_mark = TRUE;
					break;

				/**
				 * All store statements are considered useful by the ollie optimizer,
				 * regardless of the actual use count tracking
				 */
				case THREE_ADDR_CODE_STORE_STATEMENT:
				case THREE_ADDR_CODE_STORE_WITH_CONSTANT_OFFSET:
				case THREE_ADDR_CODE_STORE_WITH_VARIABLE_OFFSET:
					current_stmt->mark = TRUE;
					//Add it to the list
					dynamic_array_add(&worklist, current_stmt);
					//The block now has a mark
					current->contains_mark = TRUE;
					break;

				//Let's see what other special cases we have
				default:
					break;
			}

			//Advance the current statement up
			current_stmt = current_stmt->next_statement;
		}
	}

	//Now that we've marked everything that is initially critical, we'll go through and trace
	//these values back through the code
	while(dynamic_array_is_empty(&worklist) == FALSE){
		//Grab out the operation from the worklist(delete from back-most efficient)
		instruction_t* stmt = dynamic_array_delete_from_back(&worklist);
		//Generic array for holding parameters
		dynamic_array_t params;

		//There are several unique cases that require extra attention
		switch(stmt->statement_type){
			//If it's a phi function, now we need to go back and mark everything that it came from
			case THREE_ADDR_CODE_PHI_FUNC:
				params = stmt->parameters;

				//Add this in here
				for(u_int16_t i = 0; i < params.current_index; i++){
					//Grab the param out
					three_addr_var_t* phi_func_param = dynamic_array_get_at(&params, i);

					//Add the definitions in
					mark_and_add_definition(cfg, phi_func_param, stmt->function, &worklist);
				}

				break;

			//If we have a function call, everything in the function call
			//is important
			case THREE_ADDR_CODE_FUNC_CALL:
				//Grab the parameters out
				params = stmt->parameters;

				//Run through them all and mark them
				for(u_int16_t i = 0; i < params.current_index; i++){
					mark_and_add_definition(cfg, dynamic_array_get_at(&params, i), stmt->function, &worklist);
				}

				break;

			/**
			 * An indirect function call behaves similarly to a function call, but we'll also
			 * need to mark it's "op1" value as important. This is the value that stores
			 * the memory address of the function that we're calling
			 */
			case THREE_ADDR_CODE_INDIRECT_FUNC_CALL:
				//Mark the op1 of this function as being important
				mark_and_add_definition(cfg, stmt->op1, stmt->function, &worklist);

				//Grab the parameters out
				params = stmt->parameters;

				//Run through them all and mark them
				for(u_int16_t i = 0; i < params.current_index; i++){
					mark_and_add_definition(cfg, dynamic_array_get_at(&params, i), stmt->function, &worklist);
				}

				break;

			/**
			 * There will be special rules for store statements because we have assignees
			 * that are not really assignees, they are more like operands
			 */
			case THREE_ADDR_CODE_STORE_STATEMENT:
			case THREE_ADDR_CODE_STORE_WITH_CONSTANT_OFFSET:
			case THREE_ADDR_CODE_STORE_WITH_VARIABLE_OFFSET:
				//Add the assignee as if it was a variable itself
				mark_and_add_definition(cfg, stmt->assignee, stmt->function, &worklist);

				//We need to mark the place where each definition is set
				mark_and_add_definition(cfg, stmt->op1, stmt->function, &worklist);
				mark_and_add_definition(cfg, stmt->op2, stmt->function, &worklist);
				break;

			//In all other cases, we'll just mark and add the two operands 
			default:
				//We need to mark the place where each definition is set
				mark_and_add_definition(cfg, stmt->op1, stmt->function, &worklist);
				mark_and_add_definition(cfg, stmt->op2, stmt->function, &worklist);

				break;
		}

		//Grab this out for convenience
		basic_block_t* block = stmt->block_contained_in;

		/**
		 * Now we'll apply this logic to the branching/indirect jumping
		 * statements here
		 *
		 * for each block b in RDF(block(i))
		 * 	let j be the branch that ends b
		 * 	if j is unmarked then
		 * 		mark j
		 * 		add j to worklist
		 */
		//If this block even has an RDF(it may now)
		if(block->reverse_dominance_frontier.internal_array != NULL){
			for(u_int16_t i = 0; i < block->reverse_dominance_frontier.current_index; i++){
				//Grab the block out of the RDF
				basic_block_t* rdf_block = dynamic_array_get_at(&(block->reverse_dominance_frontier), i);

				//Grab out the exit statement
				instruction_t* exit_statement = rdf_block->exit_statement;

				//We'll now go based on what the exit statement is
				switch(exit_statement->statement_type){
					/**
					 * An indirect jump means that we had some kind of switch statement. This
					 * will be marked as important
					 */
					case THREE_ADDR_CODE_INDIRECT_JUMP_STMT:
						//Avoids infinite loops
						if(exit_statement->mark == FALSE){
							//Mark it
							exit_statement->mark = TRUE;
							//Add it to the worklist
							dynamic_array_add(&worklist, exit_statement);
							//This now has a mark
							rdf_block->contains_mark = TRUE;
						}

						break;

					/**
					 * This is the most common case, we'll have a branch that
					 * ends the predecessor
					 */
					case THREE_ADDR_CODE_BRANCH_STMT:
						//Avoids infinite loops
						if(exit_statement->mark == FALSE){
							//Mark it
							exit_statement->mark = TRUE;
							//Add it to the worklist
							dynamic_array_add(&worklist, exit_statement);
							//This now has a mark
							rdf_block->contains_mark = TRUE;
						}

						break;

					//By default just leave
					default:
						break;
				}
			}
		}
	}

	//And get rid of the worklist
	dynamic_array_dealloc(&worklist);
}


/**
 * Replace all targets that jump to "empty block" with "replacement". This is a helper 
 * function for the "Empty Block Removal" step of clean()
 */
static void replace_all_branch_targets(basic_block_t* empty_block, basic_block_t* replacement){
	//Use a clone since we are mutating
	dynamic_array_t clone = clone_dynamic_array(&(empty_block->predecessors));

	//For everything in the predecessor set of the empty block
	for(u_int16_t _ = 0; _ < clone.current_index; _++){
		//Grab a given predecessor out
		basic_block_t* predecessor = dynamic_array_get_at(&clone, _);

		//The empty block is no longer a successor of this predecessor
		delete_successor(predecessor, empty_block);
		
		//Run through the jump table and replace all of those targets as well. Most of the time,
		//we won't hit this because num_nodes will be 0. In the times that we do though, this is
		//what will ensure that switch statements are not corrupted by the optimization process
		if(predecessor->jump_table != NULL){
			for(u_int16_t idx = 0; idx < predecessor->jump_table->num_nodes; idx++){
				//If this equals the other node, we'll need to replace it
				if(dynamic_array_get_at(&(predecessor->jump_table->nodes), idx) == empty_block){
					//This now points to the replacement
					dynamic_array_set_at(&(predecessor->jump_table->nodes), replacement, idx);

					//The replacement is now a successor of this predecessor
					add_successor(predecessor, replacement);
				}
			}
		}

		//We always will be starting at the exit statement. Branches/jumps
		//can only happen at the end
		instruction_t* exit_statement = predecessor->exit_statement;

		//This can happen - and if it's the case, we move along
		if(exit_statement == NULL){
			continue;
		}

		//Go based on the type
		switch(exit_statement->statement_type){
			//One type of block exit
			case THREE_ADDR_CODE_JUMP_STMT:
				//If this is the right target, then replace it
				if(exit_statement->if_block == empty_block){
					exit_statement->if_block = replacement;
					//Counts as a successor
					add_successor(predecessor, replacement);
				}

				break;

			//Other type of block exit
			case THREE_ADDR_CODE_BRANCH_STMT:
				//If this is the right target, then replace it
				if(exit_statement->if_block == empty_block){
					exit_statement->if_block = replacement;
					//Counts as a successor
					add_successor(predecessor, replacement);
				}

				//Same for the else block
				if(exit_statement->else_block == empty_block){
					exit_statement->else_block = replacement;
					//Counts as a successor
					add_successor(predecessor, replacement);
				}

				break;

			//By default do nothing
			default:
				break;
		}

	}

	//The empty block now no longer has the replacement as a successor
	delete_successor(empty_block, replacement);

	//Destroy the clone array
	dynamic_array_dealloc(&clone);
}


/**
 * To find the nearest marked postdominator, we can do a breadth-first
 * search starting at our block B. Whenever we find a node that is both:
 * 	a.) A postdominator of B
 * 	b.) marked
 * we'll have our answer
 */
static basic_block_t* nearest_marked_postdominator(cfg_t* cfg, basic_block_t* B){
	//We'll need a queue for the BFS
	heap_queue_t queue = heap_queue_alloc();

	//First, we'll reset every single block here
	reset_visited_status(cfg, FALSE);

	//Seed the search with B
	enqueue(&queue, B);

	//The nearest marked postdominator and a holder for our candidates
	basic_block_t* nearest_marked_postdominator = NULL;
	basic_block_t* candidate;

	//So long as the queue is not empty
	while(queue_is_empty(&queue) == FALSE){
		//Grab the block off
		candidate = dequeue(&queue);
		
		//If we've been here before, continue;
		if(candidate->visited == TRUE){
			continue;
		}

		//Mark this for later
		candidate->visited = TRUE;

		//Now let's check for our criterion.
		//We want:
		//	it to be in the postdominator set
		//	it to have a mark
		//	it to not equal itself
		if(dynamic_array_contains(&(B->postdominator_set), candidate) != NOT_FOUND
		  && candidate->contains_mark == TRUE && B != candidate){
			//We've found it, so we're done
			nearest_marked_postdominator = candidate;
			//Get out
			break;
		}

		//Enqueue all of the successors
		for(u_int16_t i = 0; i < candidate->successors.current_index; i++){
			//Grab the successor out
			basic_block_t* successor = dynamic_array_get_at(&(candidate->successors), i);

			//If it's already been visited, we won't bother with it. If it hasn't been visited, we'll add it in
			if(successor->visited == FALSE){
				enqueue(&queue, successor);
			}
		}
	}

	//Destroy the queue when done
	heap_queue_dealloc(&queue);

	//And give this back
	return nearest_marked_postdominator;
}


/**
 * The sweep algorithm will go through and remove every operation that has not been marked
 *
 * procedure sweep:
 * 	for each operation i:
 * 		if is is unmarked then:
 * 			if i is a branch then
 * 			  rewrite i with a jump to i's nearest
 * 			  marked postdominator
 *
 * 			if i is not a jump then:
 * 			  delete i
 *
 */
static void sweep(cfg_t* cfg){
	//For each and every operation in every basic block
	for(u_int16_t _ = 0; _ < cfg->created_blocks.current_index; _++){
		//Grab the block out
		basic_block_t* block = dynamic_array_get_at(&(cfg->created_blocks), _);

		//Holder for the postdom
		basic_block_t* nearest_marked_postdom;

		//Grab the statement out
		instruction_t* stmt = block->leader_statement;

		//For each statement in the block
		while(stmt != NULL){
			//If it's useful, ignore it
			if(stmt->mark == TRUE){
				stmt = stmt->next_statement;
				continue;
			}

			//A holder for when we perform deletions
			instruction_t* temp;

			/**
			 * Some statements like jumps and branches
			 * require special attention
			 */
			switch(stmt->statement_type){
				//We *never* delete jump statements because
				//they are critical to the control flow. They
				//may be cleaned up by other optimizations, but for
				//here we leave them
				case THREE_ADDR_CODE_JUMP_STMT:
					stmt = stmt->next_statement;

					//Break out of the switch
					break;

				//If we have a branch that is now useless,
				//we'll need to replace it with a jump to
				//it's nearest marked postdominator
				case THREE_ADDR_CODE_BRANCH_STMT:
					//We'll first find the nearest marked postdominator
					nearest_marked_postdom = nearest_marked_postdominator(cfg, block);

					//This is now useless
					delete_statement(stmt);

					//Emit the jump statement to the nearest marked postdominator
					//NOTE: the emit jump adds the successor in for us, so we don't need to
					//do so here
					stmt = emit_jump(block, nearest_marked_postdom);

					//Break out of the switch
					break;

				/**
				 * By default no special treatment, we're just deleting
				 */
				default:
					//Perform the deletion and advancement
					temp = stmt;

					//If we are deleting an indirect jump address calculation statement,
					//then this statements jump table is useless
					if(temp->statement_type == THREE_ADDR_CODE_INDIR_JUMP_ADDR_CALC_STMT){
						//We'll need to deallocate this jump table
						jump_table_dealloc(block->jump_table);

						//Flag it as null
						block->jump_table = NULL;
					}
					
					//Advance the statement
					stmt = stmt->next_statement;
					//Delete the statement, now that we know it is not a jump
					delete_statement(temp);

					//Break out of the switch
					break;
			}
		}
	}

	//Once we've done all of the actual sweeping inside of the blocks, we will now also clean up
	//the stack from any unmarked regions. If a region is unmarked, it is entirely useless and as such
	//we'll just get rid of it
	for(u_int16_t i = 0; i < cfg->function_entry_blocks.current_index; i++){
		//Extract the block
		basic_block_t* function_entry = dynamic_array_get_at(&(cfg->function_entry_blocks), i);

		//We really want this one's stack
		stack_data_area_t* stack =  &(function_entry->function_defined_in->data_area);

		//Invoke the stack sweeper. This function will go through an remove any stack regions
		//that have been flagged as unimportant
		sweep_stack_data_area(stack);

		//Now we will sweep the local constants out of here
		sweep_local_constants(function_entry->function_defined_in);
	}
}


/**
 * Delete all branching statements in the current block. We know if a statement is branching if it is 
 * marks as branch ending. 
 *
 * NOTE: This should only be called after we have identified this block as a candidate for block folding
 */
static void delete_all_branching_statements(basic_block_t* block){
	//We'll always start from the end and work our way up
	instruction_t* current = block->exit_statement;
	//To hold while we delete
	instruction_t* temp;

	//So long as this is NULL and it's branch ending
	while(current != NULL && current->is_branch_ending == TRUE){
		temp = current;
		//Advance this
		current = current->previous_statement;
		//Then we delete
		delete_statement(temp);
	}

	//After we've gotten here we're all done
}


/**
 * The branch reduce function is what we use on each pass of the function
 * postorder
 *
 * Procedure branch_reduce():
 * 	for each block in postorder
 * 		if i ends in a conditional branch
 * 			if both targets are identical then
 * 				replace branch with a jump
 *
 * 		if i ends in a jump to j then
 * 			if i is empty then
 * 				replace transfers to i with transfers to j
 * 			if j has only one predecessor then
 * 				merge i and j
 * 			if j is empty and ends in a conditional branch then
 * 				overwrite i's jump with a copy of j's branch
 */
static u_int8_t branch_reduce(cfg_t* cfg, dynamic_array_t* postorder){
	//Have we seen a change? By default we assume not
	u_int8_t changed = FALSE;

	//Our current block
	basic_block_t* current;

	/**
	 * For each block in postorder
	 */
	for(u_int16_t _ = 0; _ < postorder->current_index; _++){
		//Grab the current block out
		current = dynamic_array_get_at(postorder, _);

		/**
		 * If block i ends in a conditional branch
		 */
		if(current->exit_statement != NULL 
			&& current->exit_statement->statement_type == THREE_ADDR_CODE_BRANCH_STMT){
			//Extract the branch statement
			instruction_t* branch = current->exit_statement;

			/**
			 * If both targets are identical(j) then:
			 * 	replace branch with a jump to j
			 */
			if(branch->if_block == branch->else_block){
				//Remove these all
				delete_all_branching_statements(current);

				//Emit a jump here instead
				emit_jump(current, branch->if_block);

				//This counts as a change
				changed = TRUE;
			}
		}


		/**
		 * If block i ends in a jump to j then..
		 */
		if(current->exit_statement != NULL
			&& current->exit_statement->statement_type == THREE_ADDR_CODE_JUMP_STMT){
			//Extract the block(j) that we're going to
			basic_block_t* jumping_to_block = current->exit_statement->if_block;

			/**
			 * If i is empty then
			 * 	replace transfers to i with transfers to j
			 */
			//We know it's empty if these are the same
			if(current->exit_statement == current->leader_statement
				&& current->block_type != BLOCK_TYPE_FUNC_ENTRY){
				//Replace all jumps to the current block with those to the jumping block
				replace_all_branch_targets(current, jumping_to_block);

				//Current is no longer in the picture
				dynamic_array_delete(&(cfg->created_blocks), current);

				//Counts as a change
				changed = TRUE;

				//We are done here, no need to continue on
				continue;
			}

			/**
			 * If j only has one predecessor then
			 * 	merge i and j
			 */
			if(jumping_to_block->predecessors.current_index == 1){
				//Delete the jump statement because it's now useless
				delete_statement(current->exit_statement);

				//Decouple these as predecessors/successors
				delete_successor(current, jumping_to_block);

				//Combine the two
				combine(cfg, current, jumping_to_block);

				//Counts as a change 
				changed = TRUE;

				//And we're done here
				continue;
			}

			/**
			 * If j is empty(except for the branch) and ends in a conditional branch then
			 * 	overwrite i's jump with a copy of j's branch
			 */
			if(jumping_to_block->leader_statement->is_branch_ending == TRUE
				&& jumping_to_block->exit_statement->statement_type == THREE_ADDR_CODE_BRANCH_STMT){
				//Delete the jump statement in i
				delete_statement(current->exit_statement);

				//These are also no longer successors
				delete_successor(current, jumping_to_block);

				//Run through every statement in the jumping to block and 
				//copy them into current
				instruction_t* current_stmt = jumping_to_block->leader_statement;

				//So long as there is more to copy
				while(current_stmt != NULL){
					//Copy it
					instruction_t* copy = copy_instruction(current_stmt);

					//Add it to the current block
					add_statement(current, copy);

					//Add as assigned
					if(copy->assignee != NULL){
						add_assigned_variable(current, copy->assignee);
					}

					//Add these as used
					add_used_variable(current, copy->op1);
					add_used_variable(current, copy->op2);

					//Add it over
					current_stmt = current_stmt->next_statement;
				}

				//Once we get to the very end here, we'll need to do the bookkeeping
				//from the branch
				basic_block_t* if_destination = jumping_to_block->exit_statement->if_block;
				basic_block_t* else_destination = jumping_to_block->exit_statement->else_block;
				
				//These both count as successor
				add_successor(current, if_destination);
				add_successor(current, else_destination);

				//This counts as a change
				changed = TRUE;
			}
		}
	}

	//Give back whether or not we changed
	return changed;
}


/**
 * Emit a test instruction. Note that this is different depending on what kind of testing that we're doing(GP vs SSE)
 *
 * Note that for the operator input, we will use this to modify the given operator *if* we have a floating point operation.
 * This is because the eventual selected code for floating point will turn if(x) into if(x != 0) essentially, so we need to
 * have that logic already in for when the branch statements are selected
 */
static inline instruction_t* emit_test_not_zero_instruction(three_addr_var_t* destination_variable, three_addr_var_t* tested_variable, ollie_token_t* operator){
	//Emit the instruction
	instruction_t* test_if_not_zero = emit_test_if_not_zero_statement(destination_variable, tested_variable);

	//If this is a floating point variable, update the pass-by-reference
	//operator
	if(IS_FLOATING_POINT(tested_variable->type) == TRUE){
		*operator = NOT_EQUALS;
	}

	//Give back the final assignee
	return test_if_not_zero;
}


/**
 * Handle a logical or inverse branch statement optimization
 *
 * These statement will take what was once one block, and split it into 
 * 2 successive blocks
 *
 * .L2
 * t5 <- 0
 * t6 <- b_1
 * t7 <- t6 != t5
 * t8 <- 33
 * t9 <- a_1
 * t10 <- t9 < t8
 * t11 <- t7 || t10
 * cbranch_z .L9 else .L13 <-- Notice how it goes to if on failure
 *
 * Turn this into:
 *
 * .L2:
 * t5 <- 0
 * t6 <- b_1
 * t7 <- t6 != t5 <--- If this does work, we know that t11 will *not* be zero, so jump to else
 * cbranch_ne .L13 else .L3
 *
 * .L3: <----- We only get here if the first one is false
 * t8 <- 33
 * t9 <- a_1
 * t10 <- t9 < t8
 * cbranch_ge .L9 else .L13 <-- If this also fails, we've satisfied the initial condition
 */
static void optimize_logical_or_inverse_branch_logic(instruction_t* short_circuit_statment, basic_block_t* if_target, basic_block_t* else_target){
	//Grab out the block that we're using
	basic_block_t* original_block = short_circuit_statment->block_contained_in;
	//The new block that we'll need for our second half
	basic_block_t* second_half_block = basic_block_alloc(original_block->estimated_execution_frequency);
	//VERY important that we copy this on over
	second_half_block->function_defined_in = original_block->function_defined_in;

	//Some bookkeeping - all of the original blocks successors should no longer point to it
	for(u_int16_t i = 0; i < original_block->successors.current_index; i++){
		basic_block_t* successor = dynamic_array_get_at(&(original_block->successors), i);

		//Remove the successor/predecessor link
		delete_successor(original_block, successor);
	}

	//Extract the op1, we'll need to traverse
	three_addr_var_t* op1 = short_circuit_statment->op1;
	three_addr_var_t* op2 = short_circuit_statment->op2;

	//The cursor for our first half
	instruction_t* first_half_cursor = short_circuit_statment->previous_statement;

	//Trace our way up to where op1 was assigned
	while(variables_equal(op1, first_half_cursor->assignee, FALSE) == FALSE){
		//Keep advancing backward
		first_half_cursor = first_half_cursor->previous_statement;
	}

	//The cursor for our second half
	instruction_t* second_half_cursor = short_circuit_statment->previous_statement;

	//Trace our way up to where op2 was assigned
	while(variables_equal(op2, second_half_cursor->assignee, FALSE) == FALSE){
		//Keep advancing backward
		second_half_cursor = second_half_cursor->previous_statement;
	}

	//Now we've found where we need to effectively split the block into 2 pieces
	//Everything after this op1 assignment needs to be removed from this block
	//and put into the new block. The split starts at the first half cursor's *next statement*
	bisect_block(second_half_block, first_half_cursor->next_statement);

	/**
	 * Now starting at the second half cursor's next statement, we'll *delete* everything
	 * after it because we no longer need it
	 */
	instruction_t* delete_cursor = second_half_cursor->next_statement;
	
	//Delete until we run out
	while(delete_cursor != NULL){
		//Hold onto it
		instruction_t* holder = delete_cursor;

		//Move it along up
		delete_cursor = delete_cursor->next_statement;

		//Delete the holder. This is a full delete, this statement
		//isn't ever coming back
		delete_statement(holder);
	}

	//Now we have 2 blocks, split nicely in half for us to work with
	//The first block contains the first condition, and the second
	//block contains the second condition and nothing else after it
	//The old branch and the compound and condition is now gone
	
	/**
	 * HANDLING THE FIRST BLOCK
	 *
	 * The first block will exploit the logical or property that if the
	 * first condition works, the second *should never execute*. We have
	 * a normal branch here
	 */
	//We need the operator
	ollie_token_t first_condition_op = first_half_cursor->op;
	//And if the type is signed
	u_int8_t first_half_signed = is_type_signed(first_half_cursor->assignee->type);
	//Does the first half using float logic?
	u_int8_t first_half_float = IS_FLOATING_POINT(first_half_cursor->op1->type);

	//The conditional decider is by default the assignee
	three_addr_var_t* first_branch_conditional_decider = first_half_cursor->assignee;

	//This is possible - if it happens we need to emit test code
	if(first_condition_op == BLANK){
		//This is now the first half's conditional decider
		first_branch_conditional_decider = emit_temp_var(first_half_cursor->assignee->type);

		//Test instruction, we're just testing against ourselves here
		instruction_t* test = emit_test_not_zero_instruction(first_branch_conditional_decider, first_half_cursor->assignee, &first_condition_op);

		//Throw it into the block
		add_statement(original_block, test);

		//This now counts as a use
		add_used_variable(original_block, first_half_cursor->assignee);
	}

	//Determine an appropriate branch. Remember, if this *fails* the if condition
	//succeeds, so this is an *inverse* jump
	branch_type_t first_half_branch = select_appropriate_branch_statement(first_condition_op, BRANCH_CATEGORY_NORMAL, first_half_signed);

	//Now we'll emit our branch at the very end of the first block. Remember it's:
	//if condition works:
	//	goto else
	//else
	//	goto second_half_block
	emit_branch(original_block, else_target, second_half_block, first_half_branch, first_branch_conditional_decider, BRANCH_CATEGORY_NORMAL, first_half_float);


	/**
	 * HANDLING THE SECOND BLOCK
	 *
	 * The second block is only reachable if the first condition is false. Therefore, if the second condition
	 * is true, we can jump to our if target. Otherwise, go to the else target
	 */
	ollie_token_t second_condition_op = second_half_cursor->op;
	//And if the type is signed
	u_int8_t second_half_signed = is_type_signed(second_half_cursor->assignee->type);

	//The conditional decider is by default the assignee
	three_addr_var_t* second_branch_conditional_decider = second_half_cursor->assignee;

	//This is possible - if it happens we need to emit test code
	if(second_condition_op == BLANK){
		//This is now the first half's conditional decider
		second_branch_conditional_decider = emit_temp_var(second_half_cursor->assignee->type);

		//Test instruction, we're just testing against ourselves here
		instruction_t* test = emit_test_not_zero_instruction(second_branch_conditional_decider, second_half_cursor->assignee, &second_condition_op);

		//Throw it into the block
		add_statement(second_half_block, test);

		//This now counts as a use
		add_used_variable(original_block, second_half_cursor->assignee);
	}

	//Determine the appropriate inverse jump here
	branch_type_t second_half_branch = select_appropriate_branch_statement(second_condition_op, BRANCH_CATEGORY_INVERSE, second_half_signed);

	//Now we'll emit our final branch at the end of the first block. Remember it's:
	//if condition fails:
	// goto if_block
	//else 
	// goto else_block
	emit_branch(second_half_block, if_target, else_target, second_half_branch, second_branch_conditional_decider, BRANCH_CATEGORY_INVERSE);
}


/**
 * Handle a compound or statement optimization
 *
 * These statement will take what was once one block, and split it into 
 * 2 successive blocks
 *
 * .L2
 * t5 <- x_0
 * t6 <- 3
 * t5 <- t5 < t6
 * t7 <- x_0
 * t8 <- 1
 * t7 <- t7 != t8
 * t5 <- t5 || t7
 * cbranch_nz .L12 else .L13
 *
 *
 * Turn this into:
 *
 * .L2:
 * t5 <- x_0
 * t6 <- 3
 * t5 <- t5 < t6 <---- if this is true, we leave(to if case)
 * cbranch_l .L13 else .L3
 *
 * .L3 <----- The *only* way we get here is if the first condition is false 
 * t7 <- x_0
 * t8 <- 1
 * t7 <- t7 != t8 <------- If this is true, jump to if
 * cbranch_ne .L12 else .L13
 */
static void optimize_logical_or_branch_logic(instruction_t* short_circuit_statment, basic_block_t* if_target, basic_block_t* else_target){
	//Grab out the block that we're using
	basic_block_t* original_block = short_circuit_statment->block_contained_in;
	//The new block that we'll need for our second half
	basic_block_t* second_half_block = basic_block_alloc(original_block->estimated_execution_frequency);
	//VERY important that we copy this on over
	second_half_block->function_defined_in = original_block->function_defined_in;

	//Some bookkeeping - all of the original blocks successors should no longer point to it
	for(u_int16_t i = 0; i < original_block->successors.current_index; i++){
		basic_block_t* successor = dynamic_array_get_at(&(original_block->successors), i);

		//Remove the successor/predecessor link
		delete_successor(original_block, successor);
	}

	//Extract the op1, we'll need to traverse
	three_addr_var_t* op1 = short_circuit_statment->op1;
	three_addr_var_t* op2 = short_circuit_statment->op2;

	//The cursor for our first half
	instruction_t* first_half_cursor = short_circuit_statment->previous_statement;

	//Trace our way up to where op1 was assigned
	while(variables_equal(op1, first_half_cursor->assignee, FALSE) == FALSE){
		//Keep advancing backward
		first_half_cursor = first_half_cursor->previous_statement;
	}

	//The cursor for our second half
	instruction_t* second_half_cursor = short_circuit_statment->previous_statement;

	//Trace our way up to where op2 was assigned
	while(variables_equal(op2, second_half_cursor->assignee, FALSE) == FALSE){
		//Keep advancing backward
		second_half_cursor = second_half_cursor->previous_statement;
	}

	//Now we've found where we need to effectively split the block into 2 pieces
	//Everything after this op1 assignment needs to be removed from this block
	//and put into the new block. The split starts at the first half cursor's *next statement*
	bisect_block(second_half_block, first_half_cursor->next_statement);

	/**
	 * Now starting at the second half cursor's next statement, we'll *delete* everything
	 * after it because we no longer need it
	 */
	instruction_t* delete_cursor = second_half_cursor->next_statement;
	
	//Delete until we run out
	while(delete_cursor != NULL){
		//Hold onto it
		instruction_t* holder = delete_cursor;

		//Move it along up
		delete_cursor = delete_cursor->next_statement;

		//Delete the holder. This is a full delete, this statement
		//isn't ever coming back
		delete_statement(holder);
	}

	//Now we have 2 blocks, split nicely in half for us to work with
	//The first block contains the first condition, and the second
	//block contains the second condition and nothing else after it
	//The old branch and the compound and condition is now gone
	
	/**
	 * HANDLING THE FIRST BLOCK
	 *
	 * The first block will exploit the logical or property that if the
	 * first condition works, the second *should never execute*. We have
	 * a normal branch here
	 */
	//We need the operator
	ollie_token_t first_condition_op = first_half_cursor->op;
	//And if the type is signed
	u_int8_t first_half_signed = is_type_signed(first_half_cursor->assignee->type);

	//The conditional decider is by default the assignee
	three_addr_var_t* first_branch_conditional_decider = first_half_cursor->assignee;

	//This is possible - if it happens we need to emit test code
	if(first_condition_op == BLANK){
		//This is now the first half's conditional decider
		first_branch_conditional_decider = emit_temp_var(first_half_cursor->assignee->type);

		//Test instruction, we're just testing against ourselves here
		instruction_t* test = emit_test_not_zero_instruction(first_branch_conditional_decider, first_half_cursor->assignee, &first_condition_op);

		//Throw it into the block
		add_statement(original_block, test);

		//This now counts as a use
		add_used_variable(original_block, first_half_cursor->assignee);
	}

	//Determine an appropriate branch. Remember, if this *fails* the if condition
	//succeeds, so this is an *inverse* jump
	branch_type_t first_half_branch = select_appropriate_branch_statement(first_condition_op, BRANCH_CATEGORY_NORMAL, first_half_signed);

	//Now we'll emit our branch at the very end of the first block. Remember it's:
	//if condition works:
	//	goto if
	//else
	//	goto second_half_block
	emit_branch(original_block, if_target, second_half_block, first_half_branch, first_branch_conditional_decider, BRANCH_CATEGORY_NORMAL);

	/**
	 * HANDLING THE SECOND BLOCK
	 *
	 * The second block is only reachable if the first condition is false. Therefore, if the second condition
	 * is true, we can jump to our if target. Otherwise, go to the else target
	 */
	ollie_token_t second_condition_op = second_half_cursor->op;
	//And if the type is signed
	u_int8_t second_half_signed = is_type_signed(second_half_cursor->assignee->type);

	//The conditional decider is by default the assignee
	three_addr_var_t* second_branch_conditional_decider = second_half_cursor->assignee;

	//This is possible - if it happens we need to emit test code
	if(second_condition_op == BLANK){
		//This is now the first half's conditional decider
		second_branch_conditional_decider = emit_temp_var(second_half_cursor->assignee->type);

		//Test instruction, we're just testing against ourselves here
		instruction_t* test = emit_test_not_zero_instruction(second_branch_conditional_decider, second_half_cursor->assignee, &second_condition_op);

		//Throw it into the block
		add_statement(second_half_block, test);

		//This now counts as a use
		add_used_variable(original_block, second_half_cursor->assignee);
	}

	//Determine an appropriate branch. Remember, if this *succeeds* the if condition
	//succeeds, so this is a *regular* jump
	branch_type_t second_half_branch = select_appropriate_branch_statement(second_condition_op, BRANCH_CATEGORY_NORMAL, second_half_signed);

	//Now we'll emit our final branch at the end of the first block. Remember it's:
	//if condition succeeds:
	// goto if_block
	//else 
	// goto else_block
	emit_branch(second_half_block, if_target, else_target, second_half_branch, second_branch_conditional_decider, BRANCH_CATEGORY_NORMAL);
}


/**
 * Handle an inverse-branching logical and condition
 *
 * These statement will take what was once one block, and split it into 
 * 2 successive blocks
 *
 * .L2
 * t5 <- x_0
 * t6 <- 3
 * t5 <- t5 < t6
 * t7 <- x_0
 * t8 <- 1
 * t7 <- t7 != t8
 * t5 <- t5 && t7
 * cbranch_z .L12 else .L13 <--- notice how it's branch if zero, we go to if if this fails(hence inverse)
 *
 * Turn this into:
 *
 * .L2:
 * t5 <- x_0
 * t6 <- 3
 * t5 <- t5 < t6 <---- if this doesn't work, we're done. We can go to *if case*
 * cbranch_ge .L12 else .L3
 *
 * .L3 <----- The *only* way we get here is if the first condition is true
 * t7 <- x_0
 * t8 <- 1
 * t7 <- t7 != t8 <------- Remember we're looking for a failure, so if this fails to go *if*, otherwise *else*
 * cbranch_e .L12 else .L13
 */
static void optimize_logical_and_inverse_branch_logic(instruction_t* short_circuit_statment, basic_block_t* if_target, basic_block_t* else_target){
	//Grab out the block that we're using
	basic_block_t* original_block = short_circuit_statment->block_contained_in;
	//The new block that we'll need for our second half
	basic_block_t* second_half_block = basic_block_alloc(original_block->estimated_execution_frequency);
	//VERY important that we copy this on over
	second_half_block->function_defined_in = original_block->function_defined_in;

	//Some bookkeeping - all of the original blocks successors should no longer point to it
	for(u_int16_t i = 0; i < original_block->successors.current_index; i++){
		basic_block_t* successor = dynamic_array_get_at(&(original_block->successors), i);

		//Remove the successor/predecessor link
		delete_successor(original_block, successor);
	}

	//Extract the op1, we'll need to traverse
	three_addr_var_t* op1 = short_circuit_statment->op1;
	three_addr_var_t* op2 = short_circuit_statment->op2;

	//The cursor for our first half
	instruction_t* first_half_cursor = short_circuit_statment->previous_statement;

	//Trace our way up to where op1 was assigned
	while(variables_equal(op1, first_half_cursor->assignee, FALSE) == FALSE){
		//Keep advancing backward
		first_half_cursor = first_half_cursor->previous_statement;
	}

	//The cursor for our second half
	instruction_t* second_half_cursor = short_circuit_statment->previous_statement;

	//Trace our way up to where op2 was assigned
	while(variables_equal(op2, second_half_cursor->assignee, FALSE) == FALSE){
		//Keep advancing backward
		second_half_cursor = second_half_cursor->previous_statement;
	}

	//Now we've found where we need to effectively split the block into 2 pieces
	//Everything after this op1 assignment needs to be removed from this block
	//and put into the new block. The split starts at the first half cursor's *next statement*
	bisect_block(second_half_block, first_half_cursor->next_statement);

	/**
	 * Now starting at the second half cursor's next statement, we'll *delete* everything
	 * after it because we no longer need it
	 */
	instruction_t* delete_cursor = second_half_cursor->next_statement;
	
	//Delete until we run out
	while(delete_cursor != NULL){
		//Hold onto it
		instruction_t* holder = delete_cursor;

		//Move it along up
		delete_cursor = delete_cursor->next_statement;

		//Delete the holder. This is a full delete, this statement
		//isn't ever coming back
		delete_statement(holder);
	}

	//Now we have 2 blocks, split nicely in half for us to work with
	//The first block contains the first condition, and the second
	//block contains the second condition and nothing else after it
	//The old branch and the compound and condition is now gone
	
	/**
	 * HANDLING THE FIRST BLOCK
	 *
	 * The first block will exploit the logical and property that if the
	 * first condition fails, the second *should never execute*. We have
	 * an inverse jump of sorts here
	 */
	//We need the operator
	ollie_token_t first_condition_op = first_half_cursor->op;
	//And if the type is signed
	u_int8_t first_half_signed = is_type_signed(first_half_cursor->assignee->type);

	//The conditional decider is by default the assignee
	three_addr_var_t* first_branch_conditional_decider = first_half_cursor->assignee;

	//This is possible - if it happens we need to emit test code
	if(first_condition_op == BLANK){
		//This is now the first half's conditional decider
		first_branch_conditional_decider = emit_temp_var(first_half_cursor->assignee->type);

		//Test instruction, we're just testing against ourselves here
		instruction_t* test = emit_test_not_zero_instruction(first_branch_conditional_decider, first_half_cursor->assignee, &first_condition_op);

		//Throw it into the block
		add_statement(original_block, test);

		//This now counts as a use
		add_used_variable(original_block, first_half_cursor->assignee);
	}

	//Determine the appropriate branch using an inverse jump
	branch_type_t first_half_branch = select_appropriate_branch_statement(first_condition_op, BRANCH_CATEGORY_INVERSE, first_half_signed);

	//Now we'll emit our branch at the very end of the first block. Remember it's:
	//if condition fails:
	//	goto if 
	//else
	//	goto second_half_block 
	emit_branch(original_block, if_target, second_half_block, first_half_branch, first_branch_conditional_decider, BRANCH_CATEGORY_NORMAL);

	/**
	 * HANDLING THE SECOND BLOCK
	 *
	 * The second block is only reachable if the first condition is true. Therefore, if the second condition
	 * is also true, we can jump to our if target. Otherwise, go to the else target
	 */
	ollie_token_t second_condition_op = second_half_cursor->op;
	//And if the type is signed
	u_int8_t second_half_signed = is_type_signed(second_half_cursor->assignee->type);

	//The conditional decider is by default the assignee
	three_addr_var_t* second_branch_conditional_decider = second_half_cursor->assignee;

	//This is possible - if it happens we need to emit test code
	if(second_condition_op == BLANK){
		//This is now the first half's conditional decider
		second_branch_conditional_decider = emit_temp_var(second_half_cursor->assignee->type);

		//Test instruction, we're just testing against ourselves here
		instruction_t* test = emit_test_not_zero_instruction(second_branch_conditional_decider, second_half_cursor->assignee, &second_condition_op);

		//Throw it into the block
		add_statement(second_half_block, test);

		//This now counts as a use
		add_used_variable(original_block, second_half_cursor->assignee);
	}

	//Determine the appropriate branch using an inverse jump
	branch_type_t second_half_branch = select_appropriate_branch_statement(second_condition_op, BRANCH_CATEGORY_INVERSE, second_half_signed);

	//Now we'll emit our final branch at the end of the first block. Remember it's:
	//if condition fails:
	// goto if_block
	//else 
	// goto else_block
	emit_branch(second_half_block, if_target, else_target, second_half_branch, second_branch_conditional_decider, BRANCH_CATEGORY_NORMAL);
}


/**
 * Handle a compound and statement optimization
 *
 * These statement will take what was once one block, and split it into 
 * 2 successive blocks
 *
 * .L2
 * t5 <- x_0
 * t6 <- 3
 * t5 <- t5 < t6
 * t7 <- x_0
 * t8 <- 1
 * t7 <- t7 != t8
 * t5 <- t5 && t7
 * cbranch_nz .L12 else .L13
 *
 *
 * Turn this into:
 *
 * .L2:
 * t5 <- x_0
 * t6 <- 3
 * t5 <- t5 < t6 <---- if this is false, we leave(to else case)
 * cbranch_ge .L13 else .L3
 *
 * .L3 <----- The *only* way we get here is if the first condition is true
 * t7 <- x_0
 * t8 <- 1
 * t7 <- t7 != t8 <------- If this is true, jump to if
 * cbranch_ne .L12 else .L13
 */
static void optimize_logical_and_branch_logic(instruction_t* short_circuit_statment, basic_block_t* if_target, basic_block_t* else_target){
	//Grab out the block that we're using
	basic_block_t* original_block = short_circuit_statment->block_contained_in;
	//The new block that we'll need for our second half
	basic_block_t* second_half_block = basic_block_alloc(original_block->estimated_execution_frequency);
	//VERY important that we copy this on over
	second_half_block->function_defined_in = original_block->function_defined_in;

	//Some bookkeeping - all of the original blocks successors should no longer point to it
	for(u_int16_t i = 0; i < original_block->successors.current_index; i++){
		basic_block_t* successor = dynamic_array_get_at(&(original_block->successors), i);

		//Remove the successor/predecessor link
		delete_successor(original_block, successor);
	}

	//Extract the op1, we'll need to traverse
	three_addr_var_t* op1 = short_circuit_statment->op1;
	three_addr_var_t* op2 = short_circuit_statment->op2;

	//The cursor for our first half
	instruction_t* first_half_cursor = short_circuit_statment->previous_statement;

	//Trace our way up to where op1 was assigned
	while(variables_equal(op1, first_half_cursor->assignee, FALSE) == FALSE){
		//Keep advancing backward
		first_half_cursor = first_half_cursor->previous_statement;
	}

	//The cursor for our second half
	instruction_t* second_half_cursor = short_circuit_statment->previous_statement;

	//Trace our way up to where op2 was assigned
	while(variables_equal(op2, second_half_cursor->assignee, FALSE) == FALSE){
		//Keep advancing backward
		second_half_cursor = second_half_cursor->previous_statement;
	}

	//Now we've found where we need to effectively split the block into 2 pieces
	//Everything after this op1 assignment needs to be removed from this block
	//and put into the new block. The split starts at the first half cursor's *next statement*
	bisect_block(second_half_block, first_half_cursor->next_statement);

	/**
	 * Now starting at the second half cursor's next statement, we'll *delete* everything
	 * after it because we no longer need it
	 */
	instruction_t* delete_cursor = second_half_cursor->next_statement;
	
	//Delete until we run out
	while(delete_cursor != NULL){
		//Hold onto it
		instruction_t* holder = delete_cursor;

		//Move it along up
		delete_cursor = delete_cursor->next_statement;

		//Delete the holder. This is a full delete, this statement
		//isn't ever coming back
		delete_statement(holder);
	}

	//Now we have 2 blocks, split nicely in half for us to work with
	//The first block contains the first condition, and the second
	//block contains the second condition and nothing else after it
	//The old branch and the compound and condition is now gone
	
	/**
	 * HANDLING THE FIRST BLOCK
	 *
	 * The first block will exploit the logical and property that if the
	 * first condition fails, the second *should never execute*. We have
	 * an inverse jump of sorts here
	 */
	//We need the operator
	ollie_token_t first_condition_op = first_half_cursor->op;
	//And if the type is signed
	u_int8_t first_half_signed = is_type_signed(first_half_cursor->assignee->type);

	//The conditional decider is by default the assignee
	three_addr_var_t* first_branch_conditional_decider = first_half_cursor->assignee;

	//This is possible - if it happens we need to emit test code
	if(first_condition_op == BLANK){
		//This is now the first half's conditional decider
		first_branch_conditional_decider = emit_temp_var(first_half_cursor->assignee->type);

		//Test instruction, we're just testing against ourselves here
		instruction_t* test = emit_test_not_zero_instruction(first_branch_conditional_decider, first_half_cursor->assignee, &first_condition_op);

		//Throw it into the block
		add_statement(original_block, test);

		//This now counts as a use
		add_used_variable(original_block, first_half_cursor->assignee);
	}

	//Determine an appropriate branch. Remember, if this *fails* the if condition
	//succeeds, so this is an *inverse* jump
	branch_type_t first_half_branch = select_appropriate_branch_statement(first_condition_op, BRANCH_CATEGORY_INVERSE, first_half_signed);

	//Now we'll emit our branch at the very end of the first block. Remember it's:
	//if condition fails:
	//	goto else
	//else
	//	goto second_half_block 
	emit_branch(original_block, else_target, second_half_block, first_half_branch, first_branch_conditional_decider, BRANCH_CATEGORY_NORMAL);

	/**
	 * HANDLING THE SECOND BLOCK
	 *
	 * The second block is only reachable if the first condition is true. Therefore, if the second condition
	 * is also true, we can jump to our if target. Otherwise, go to the else target
	 */
	ollie_token_t second_condition_op = second_half_cursor->op;
	//And if the type is signed
	u_int8_t second_half_signed = is_type_signed(second_half_cursor->assignee->type);

	//The conditional decider is by default the assignee
	three_addr_var_t* second_branch_conditional_decider = second_half_cursor->assignee;

	//This is possible - if it happens we need to emit test code
	if(second_condition_op == BLANK){
		//This is now the first half's conditional decider
		second_branch_conditional_decider = emit_temp_var(second_half_cursor->assignee->type);

		//Test instruction, we're just testing against ourselves here
		instruction_t* test = emit_test_not_zero_instruction(second_branch_conditional_decider, second_half_cursor->assignee, &second_condition_op);

		//Throw it into the block
		add_statement(second_half_block, test);

		//This now counts as a use
		add_used_variable(original_block, second_half_cursor->assignee);
	}

	//Determine an appropriate branch. Remember, if this *succeeds* the if condition
	//succeeds, so this is a *regular* jump
	branch_type_t second_half_branch = select_appropriate_branch_statement(second_condition_op, BRANCH_CATEGORY_NORMAL, second_half_signed);

	//Now we'll emit our final branch at the end of the first block. Remember it's:
	//if condition succeeds:
	// goto if_block
	//else 
	// goto else_block
	emit_branch(second_half_block, if_target, else_target, second_half_branch, second_half_cursor->assignee, BRANCH_CATEGORY_NORMAL);
}


/**
 * The compound logic optimizer will go through and look for compound and or or statements
 * that are parts of branch endings and see if they're able to be short-circuited. These
 * statements have been pre-marked by the cfg constructor, so whichever survive until here are going to 
 * be optimized
 *
 * KEY ASSUMPTION: The basic block that contains a branch will contain all of the necessary
 * information for this to happen. This means that the actual branch must contain straight
 * line code
 *
 *
 * Here is a brief example:
 * t9 <- 0x2
 * t10 <- x_0 < t9
 * t11 <- 0x1
 * t12 <- x_0 != t11
 * t13 <- t10 && t12 <-------- COMPOUND JUMP
 * cbranch_nz .L8 else .L9
 *
 * .L8():
 * t14 <- 0x2
 * t15 <- x_0 * t14
 * x_2 <- t15
 * jmp .L5
 *
 * .L9():
 * t16 <- 0x3
 * t17 <- x_0 + t16
 * x_1 <- t17
 * jmp .L5	
 *
 * We could optimize this statement by realizing that if the first condition fails(t10), there is no chance
 * for the next one to succeed, and as such we can jump immediately after t10 is defined
 *
 * TURNS INTO THIS:
 *  
 *  .L1
 * t9 <- 0x2
 * t10 <- x_0 < t9
 * cbranch_ge .L8 else .L3 ---------------- If it's more than it can't work, so we leave
 *
 * .L33
 * t11 <- 0x1
 * t12 <- x_0 != t11
 * cbranch_ne .L8 else .L9
 * --------------------------------t13 <- t10 && t12 <-------------------- No longer a need for this one
 * -------------------------------- No longer a need for the original branch at all --------------------
 *
 * .L8():
 * t14 <- 0x2
 * t15 <- x_0 * t14
 * x_2 <- t15
 * jmp .L5
 *
 * .L9():
 * t16 <- 0x3
 * t17 <- x_0 + t16
 * x_1 <- t17
 * jmp .L5	
 */
static void optimize_short_circuit_logic(cfg_t* cfg){
	//For every single block in the CFG
	for(u_int16_t _ = 0; _ < cfg->created_blocks.current_index; _++){
		//Grab the block out
		basic_block_t* block = dynamic_array_get_at(&(cfg->created_blocks), _);

		//If it's empty then leave
		if(block->leader_statement == NULL){
			continue;
		}

		//The branch is the block's exit statement
		instruction_t* branch_statement = block->exit_statement;

		//If the exit statement is not a branch, then we're done here
		if(branch_statement->statement_type != THREE_ADDR_CODE_BRANCH_STMT){
			continue;
		}

		//Extract both of these values - we will need them
		basic_block_t* if_target = branch_statement->if_block;
		basic_block_t* else_target = branch_statement->else_block;

		//Is this an inverse jumping branch?
		u_int8_t inverse_branch = branch_statement->inverse_branch;

		//Grab a statement cursor
		instruction_t* cursor = block->exit_statement->previous_statement;

		//Store all of our eligible statements in this block. This will be done in a FIFO
		//fashion
		dynamic_array_t eligible_statements = dynamic_array_alloc();

		//Let's run through and see if we can find a statement that's eligible for short circuiting.
		while(cursor != NULL){
			//Not branch ending - move on
			if(cursor->is_branch_ending == FALSE){
				cursor = cursor->previous_statement;
				continue;
			}

			//If we make it here, then we've found something that is eligible for a compound logic optimization
			if(cursor->op == DOUBLE_AND || cursor->op == DOUBLE_OR){
				//Add the cursor. We will iterate over these statements in the order we found them,
				//so going through in
				dynamic_array_add(&eligible_statements, cursor);
			}

			//move it back
			cursor = cursor->previous_statement;
		}

		//Now we'll iterate over the array and process what we have
		for(u_int16_t i = 0; i < eligible_statements.current_index; i++){
			//Grab the block out
			instruction_t* short_circuit_statement = dynamic_array_get_at(&eligible_statements, i);

			//Make the helper call. These are treated differently based on what their
			//operators are, so we'll need to use the appropriate call
			if(short_circuit_statement->op == DOUBLE_AND){
				//Most common case
				if(inverse_branch == FALSE){
					optimize_logical_and_branch_logic(short_circuit_statement, if_target, else_target);
				} else {
					optimize_logical_and_inverse_branch_logic(short_circuit_statement, if_target, else_target);
				}

			//Otherwise we have the double or
			} else {
				//Most common case
				if(inverse_branch == FALSE){
					optimize_logical_or_branch_logic(short_circuit_statement, if_target, else_target);
				} else {
					optimize_logical_or_inverse_branch_logic(short_circuit_statement, if_target, else_target);
				}
			}
		}

		//Deallocate the array
		dynamic_array_dealloc(&eligible_statements);
	}
}


/**
 * The clean algorithm will remove all useless control flow structures, ideally
 * resulting in a simplified CFG. This should be done after we use mark and sweep to get rid of useless code,
 * because that may lead to empty blocks that we can clean up here
 *
 * Algorithm:
 *
 * Procedure clean():
 * 	while changed
 * 	 compute Postorder of CFG
 * 	 branch_reduce()
 *
 * Procedure branch_reduce():
 * 	for each block in postorder
 * 	
 * 	if i ends in a conditional branch
 * 		if both targets are identical then
 * 			replace branch with a jump
 *
 * 	if i ends in a jump to j then
 * 		if i is empty then
 * 			replace transfers to i with transfers to j
 * 		if j has only one predecessor then
 * 			merge i and j
 * 		if j is empty and ends in a conditional branch then
 * 			overwrite i's jump with a copy of j's branch
 */
static void clean(cfg_t* cfg){
	//For each function in the CFG
	for(u_int16_t _ = 0; _ < cfg->function_entry_blocks.current_index; _++){
		//Have we seen change(modification) at all?
		u_int8_t changed;

		//Grab the function block out
		basic_block_t* function_entry = dynamic_array_get_at(&(cfg->function_entry_blocks), _);

		//The postorder traversal array
		dynamic_array_t postorder;

		//Now we'll do the actual clean algorithm
		do {
			//Compute the new postorder
			postorder = compute_post_order_traversal(function_entry);

			//Call onepass() for the reduction
			changed = branch_reduce(cfg, &postorder);

			//We can free up the old postorder now
			dynamic_array_dealloc(&postorder);
			
		//We keep going so long as branch_reduce changes something 
		} while(changed == TRUE);
	}
}


/**
 * After mark and sweep and clean run, we'll almost certainly have a litany of blocks in all
 * of the dominance relations that are now useless. As such, we'll need to completely recompute all
 * of these key values
 */
static void recompute_all_dominance_relations(cfg_t* cfg){
	//First, we'll go through and completely blow away anything related to
	//a dominator in the entirety of the cfg
	for(u_int16_t _ = 0; _ < cfg->created_blocks.current_index; _++){
		//Grab the given block out
		basic_block_t* block = dynamic_array_get_at(&(cfg->created_blocks), _);

		//Now we're going to reset everything about this block
		block->immediate_dominator = NULL;
		block->immediate_postdominator = NULL;

		if(block->dominator_set.internal_array != NULL){
			dynamic_array_dealloc(&(block->dominator_set));
		}

		if(block->postdominator_set.internal_array != NULL){
			dynamic_array_dealloc(&(block->postdominator_set));
		}

		if(block->dominance_frontier.internal_array != NULL){
			dynamic_array_dealloc(&(block->dominance_frontier));
		}

		if(block->dominator_children.internal_array != NULL){
			dynamic_array_dealloc(&(block->dominator_children));
		}

		if(block->reverse_dominance_frontier.internal_array != NULL){
			dynamic_array_dealloc(&(block->reverse_dominance_frontier));
		}
	}

	//Now that that's finished, we can go back and calculate all of the control relations again
	calculate_all_control_relations(cfg);
}


/**
 * After everything runs, it is possible that we'll have blocks leftover
 * with no predecessors. These blocks are useless, and just gunk up our pipeline.
 * We will remove them all now.
 */
static void delete_unreachable_blocks(cfg_t* cfg){
	//Clone all blocks here - we will be messing with the original
	//array, so we can't count on it for an accurate count
	dynamic_array_t all_blocks = clone_dynamic_array(&(cfg->created_blocks));

	//Run through all blocks
	for(u_int16_t i = 0; i < all_blocks.current_index; i++){
		//Extract this
		basic_block_t* current = dynamic_array_get_at(&all_blocks, i);

		//Nothing we can do about this
		if(current->block_type == BLOCK_TYPE_FUNC_ENTRY){
			continue;
		}

		//This is our deletion case - this block is unreachable
		if(current->predecessors.internal_array == NULL || current->predecessors.current_index == 0){
			//Scrap it from here
			dynamic_array_delete(&(cfg->created_blocks), current);
		}
	}

	//Once we're done, deallocate the all_blocks array
	dynamic_array_dealloc(&all_blocks);
}


/**
 * The generic optimize function. We have the option to specific how many passes the optimizer
 * runs for in it's current iteration
*/
cfg_t* optimize(cfg_t* cfg){
	//First thing we'll do is reset the visited status of the CFG. This just ensures
	//that we won't have any issues with the CFG in terms of traversal
	reset_visited_status(cfg, FALSE);

	//PASS 1: Mark algorithm
	//The mark algorithm marks all useful operations. It will perform one full pass of the program
	mark(cfg);

	//PASS 2: Sweep algorithm
	//Sweep follows directly after mark because it eliminates anything that is unmarked. If sweep
	//comes across branch ending statements that are unmarked, it will replace them with a jump to the
	//nearest marked postdominator
	sweep(cfg);

	//PASS 3: compound logic optimization
	//Now that we've sweeped everything, we know that what branches are left must be useful. This means
	//that we can expend the compute of optimizing the short circuit logic on them, and we will do so here
	optimize_short_circuit_logic(cfg);

	//PASS 4: Clean algorithm
	//Clean follows after sweep because during the sweep process, we will likely delete the contents of
	//entire blocks. Clean uses 4 different steps in a specific order to eliminate control flow
	//that has been made useless by sweep()
	clean(cfg);
	
	//PASS 5: Delete all unreachable blocks
	//There is a chance that we have some blocks who are now unreachable. We will
	//remove them now
	delete_unreachable_blocks(cfg);

	//PASS 6: Recalculate everything
	//Now that we've marked, sweeped and cleaned, odds are that all of our control relations will be off due to deletions of blocks, statements,
	//etc. So, to remedy this, we will recalculate everything in the CFG
	//cleanup_all_control_relations(cfg);
	recompute_all_dominance_relations(cfg);

	//Give back the CFG
	return cfg;
}
