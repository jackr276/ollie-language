/**
 * The implementation file for all CFG related operations
*/

#include "cfg.h"
#include <bits/types/stack_t.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

//Our atomically incrementing integer
//If at any point a block has an ID of (-1), that means that it is in error and can be dealt with as such
static int32_t current_block_id = 0;
//Keep global references to the number of errors and warnings
u_int32_t* num_errors_ref;
u_int32_t* num_warnings_ref;

//We predeclare up here to avoid needing any rearrangements
static basic_block_t* visit_declaration_statement(generic_ast_node_t* decl_node);
static basic_block_t* visit_compound_statement(generic_ast_node_t* compound_stmt_node, basic_block_t* function_block_end);
static basic_block_t* visit_let_statement(generic_ast_node_t* let_node);
static basic_block_t* visit_expression_statement(generic_ast_node_t* decl_node);
static basic_block_t* visit_if_statement(generic_ast_node_t* if_stmt_node, basic_block_t* function_block_end);
static basic_block_t* visit_while_statement(generic_ast_node_t* while_stmt_node, basic_block_t* function_block_end);
static basic_block_t* visit_do_while_statement(generic_ast_node_t* do_while_stmt_node, basic_block_t* function_block_end);
static basic_block_t* visit_for_statement(generic_ast_node_t* for_stmt_node, basic_block_t* function_block_end);


/**
 * Simply prints a parse message in a nice formatted way. For the CFG, there
 * are no parser line numbers
*/
static void print_cfg_message(parse_message_type_t message_type, char* info, u_int16_t line_number){
	//Build and populate the message
	parse_message_t parse_message;
	parse_message.message = message_type;
	parse_message.info = info;

	//Fatal if error
	if(message_type == PARSE_ERROR){
		parse_message.fatal = 1;
	}

	//Now print it
	//Mapped by index to the enum values
	char* type[] = {"WARNING", "ERROR", "INFO"};

	//Print this out on a single line
	fprintf(stderr, "\n[LINE %d: COMPILER %s]: %s\n", line_number, type[parse_message.message], parse_message.info);
}


/**
 * A helper function that makes a new block id. This ensures we have an atomically
 * increasing block ID
 */
static int32_t increment_and_get(){
	current_block_id++;
	return current_block_id;
}


/**
 * Create the memory necessary for a statement
 */
static top_level_statement_node_t* create_statement(generic_ast_node_t* node){
	//Dynamically allocated, will be freed later
	top_level_statement_node_t* stmt = calloc(1, sizeof(top_level_statement_node_t));
	
	//Add the node in here
	stmt->node = node;

	return stmt;
}


/**
 * Allocate a basic block using calloc. NO data assignment
 * happens in this function
*/
static basic_block_t* basic_block_alloc(){
	//Allocate the block
	basic_block_t* created = calloc(1, sizeof(basic_block_t));
	//Grab the unique ID for this block
	created->block_id = increment_and_get();

	return created;
}


/**
 * Deallocate a basic block
*/
static void basic_block_dealloc(basic_block_t* block){
	//Just in case
	if(block == NULL){
		printf("ERROR: Attempt to deallocate a null block");
		exit(1);
	}

	//We'll need to go through here and free the statement linked list
	top_level_statement_node_t* cursor = block->leader_statement;
	top_level_statement_node_t* temp;

	//So long as we see statements, we keep going
	while(cursor != NULL){
		temp = cursor;
		//Move onto the next node
		cursor = cursor->next;

		//Free the temp var
		free(temp);
	}

	//Otherwise its fine so
	free(block);
}


/**
 * Helper for returning error blocks. Error blocks always have an ID of -1
 */
static basic_block_t* create_and_return_err(){
	basic_block_t* err_block = basic_block_alloc();
	err_block->block_id = -1;
	return err_block;
}


/**
 * Add a predecessor to the target block. When we add a predecessor, the target
 * block is also implicitly made a successor of said predecessor
 */
static void add_predecessor(basic_block_t* target, basic_block_t* predecessor, linked_direction_t directedness){
	//Let's check this
	if(target->num_predecessors == MAX_PREDECESSORS){
		//Internal error for the programmer
		printf("CFG ERROR. YOU MUST INCREASE THE NUMBER OF PREDECESSORS");
		exit(1);
	}

	//Otherwise we're set here
	//Add this in
	target->predecessors[target->num_predecessors] = predecessor;
	//Increment how many we have
	(target->num_predecessors)++;

	//If we are trying to do a bidirectional link
	if(directedness == LINKED_DIRECTION_BIDIRECTIONAL){
		//We also need to reverse the roles and add target as a successor to "predecessor"
		//Let's check this
		if(predecessor->num_successors == MAX_SUCCESSORS){
			//Internal error for the programmer
			printf("CFG ERROR. YOU MUST INCREASE THE NUMBER OF SUCCESSORS");
			exit(1);
		}

		//Otherwise we're set here
		//Add this in
		predecessor->successors[predecessor->num_successors] = target;
		//Increment how many we have
		(predecessor->num_successors)++;
	}
}


/**
 * Add a successor to the target block
 */
static void add_successor(basic_block_t* target, basic_block_t* successor, linked_direction_t directedness){
	//Let's check this
	if(target->num_successors == MAX_SUCCESSORS){
		//Internal error for the programmer
		printf("CFG ERROR. YOU MUST INCREASE THE NUMBER OF SUCCESSORS");
		exit(1);
	}

	//If it's the very first successor, we'll also mark it as the "direct successor" for convenience
	if(target->num_successors == 0){
		target->direct_successor = successor;
	}
	//We'll of course still add it in in the bottom

	//Otherwise we're set here
	//Add this in
	target->successors[target->num_successors] = successor;
	//Increment how many we have
	(target->num_successors)++;

	//If we are trying to do a bidirectional link
	if(directedness == LINKED_DIRECTION_BIDIRECTIONAL){
		//Now we'll also need to add in target as a predecessor of successor
		//Let's check this
		if(successor->num_predecessors == MAX_PREDECESSORS){
			//Internal error for the programmer
			printf("CFG ERROR. YOU MUST INCREASE THE NUMBER OF PREDECESSORS");
			exit(1);
		}

		//Otherwise we're set here
		//Add this in
		successor->predecessors[successor->num_predecessors] = target;
		//Increment how many we have
		(successor->num_predecessors)++;
	}
}


/**
 * Add a statement to the target block, following all standard linked-list protocol
 */
static void add_statement(basic_block_t* target, top_level_statement_node_t* statement_node){
	//Generic fail case
	if(target == NULL){
		print_parse_message(PARSE_ERROR, "NULL BASIC BLOCK FOUND", 0);
		exit(1);
	}

	//Special case--we're adding the head
	if(target->leader_statement == NULL || target->exit_statement == NULL){
		//Assign this to be the head and the tail
		target->leader_statement = statement_node;
		target->exit_statement = statement_node;
		return;
	}

	//Otherwise, we are not dealing with the head. We'll simply tack this on to the tail
	target->exit_statement->next = statement_node;
	//Update the tail reference
	target->exit_statement = statement_node;
}


/**
 * Merge two basic blocks. We always return a pointer to a, b will be deallocated
 */
static basic_block_t* merge_blocks(basic_block_t* a, basic_block_t* b){
	//Just to double check
	if(a == NULL || b == NULL){
		print_cfg_message(PARSE_ERROR, "Fatal error. Attempting to merge null block", 0);
		exit(1);
	}

	//What if a was never even assigned?
	if(a->exit_statement == NULL){
		a->leader_statement = b->leader_statement;
		a->exit_statement = b->exit_statement;
	} else {
		//Otherwise it's a "true merge"
		//The leader statement in b will be connected to a's tail
		a->exit_statement->next = b->leader_statement;
		//Now once they're connected we'll set a's exit to be b's exit
		a->exit_statement = b->exit_statement;
	}

	//If we're gonna merge two blocks, then they'll share all the same successors and predecessors
	//Let's merge predecessors first
	for(u_int8_t i = 0; i < b->num_predecessors; i++){
		//Tie it in
		a->predecessors[a->num_predecessors] = b->predecessors[i];
		//Increment how many predecessors a has
		(a->num_predecessors)++;
	}

	//Now merge successors
	for(u_int8_t i = 0; i < b->num_successors; i++){
		//Tie it in
		a->successors[a->num_successors] = b->successors[i];
		//Increment how many successors a has
		(a->num_successors)++;
	}

	//Also make note of any direct succession
	a->direct_successor = b->direct_successor;
	a->is_exit_block = b->is_exit_block;
	//If we're merging return statements
	a->is_return_stmt = b->is_return_stmt;

	//We will not deallocate here, we will merely free the block itself
	free(b);

	//Give back the pointer to a
	return a;
}


/**
 * We will perform reachability analysis on the function CFG. We wish to know if the function
 * returns from every control path
 */
static void perform_function_reachability_analysis(generic_ast_node_t* function_node, basic_block_t* entry_block){
	//For error printing
	char info[1000];
	//The number of dead-ends
	u_int32_t dead_ends = 0;

	//If the function returns void, there is no need for any reachability analysis, it will return when 
	//the function runs off anyway
	if(strcmp(((func_def_ast_node_t*)(function_node->node))->func_record->return_type->type_name, "void") == 0){
		return;
	}

	//We'll need a stack for our DFS
	heap_stack_t* stack = create_stack();

	//The idea here is very simple. If we can walk the function tree and every control path leads 
	//to a return statement, we return null from every control path
	
	//We'll need a cursor to walk the tree
	basic_block_t* block_cursor;

	//Push the source node
	push(stack, entry_block);
	u_int8_t num_blocks = 0;

	//So long as the stack is not empty
	while(is_empty(stack) == 0){
		//Grab the current one off of the stack
		block_cursor = pop(stack);

		//If this wasn't visited
		if(block_cursor->visited == 0){
			num_blocks++;
			/**
			 * Now we can perform our checks. 
			 */
			//If the direct successor is the exit, but it's not a return statement
			if(block_cursor->direct_successor != NULL && block_cursor->direct_successor->is_exit_block == 1
			  && block_cursor->is_return_stmt == 0){
				//One more dead end
				dead_ends++;
				//Go to the next iteration
				continue;
			}

			//If it is a return statement none of its children are relevant
			if(block_cursor->is_return_stmt == 1){
				continue;
			}
		}

		//Mark this one as seen
		block_cursor->visited = 1;

		//We'll now add in all of the childen
		for(u_int8_t i = 0; i < block_cursor->num_successors; i++){
			//If we haven't seen it yet, add it to the list
			if(block_cursor->successors[i]->visited == 0){
				push(stack, block_cursor->successors[i]);
			}
		}
	}
	
	printf("FOUND %d BLOCKS\n", num_blocks);

	//Once we escape our while loop, we can actually see what the analysis said
	if(dead_ends > 0){
		//Extract the function name
		char* func_name = ((func_def_ast_node_t*)(function_node->node))->func_record->func_name;
		sprintf(info, "Function \"%s\" does not return in %d control paths", func_name, dead_ends);
		print_cfg_message(WARNING, info, function_node->line_number);
		(*num_warnings_ref)+=dead_ends;
	}
}


static basic_block_t* visit_function_definition(generic_ast_node_t* prog_node){

}


/**
 * Visit a declaration statement
 */
static basic_block_t* visit_declaration_statement(generic_ast_node_t* decl_node){
	//Create the basic block
	basic_block_t* decl_stmt_block = basic_block_alloc();

	//Create the top level statement for this
	top_level_statement_node_t* stmt = create_statement(decl_node);

	//Add it into the block
	add_statement(decl_stmt_block, stmt);

	//Give the block back
	return decl_stmt_block;
}


/**
 * Visit a let statement
 */
static basic_block_t* visit_let_statement(generic_ast_node_t* let_node){
	//Create the basic block
	basic_block_t* let_stmt_node = basic_block_alloc();

	//Create the top level statement for this
	top_level_statement_node_t* stmt = create_statement(let_node);

	//Add it into the block
	add_statement(let_stmt_node, stmt);

	//Give the block back
	return let_stmt_node;
}


/**
 * Visit the prog node for our CFG. This rule will simply multiplex to all other rules
 * between functions, let statements and declaration statements
 */
static basic_block_t* visit_prog_node(generic_ast_node_t* prog_node){
	//Maintain a start and current block here
	basic_block_t* start_block = NULL;
	basic_block_t* current_block = start_block;

	//A prog node can decay into a function definition, a let statement or otherwise
	generic_ast_node_t* ast_cursor = prog_node->first_child;

	//So long as the AST cursor is not null
	while(ast_cursor != NULL){
		if(ast_cursor->CLASS == AST_NODE_CLASS_FUNC_DEF){

		} else if(ast_cursor->CLASS == AST_NODE_CLASS_LET_STMT){
			//We'll visit the block here
			basic_block_t* let_block = visit_let_statement(ast_cursor);

			//If the start block is null, then this is the start block. Otherwise, we merge it in
			if(start_block == NULL){
				start_block = let_block;
				current_block = let_block;
			//Just merge with current
			} else {
				merge_blocks(current_block, let_block); 
			}

		//Visit a declaration statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_DECL_STMT){
			//We'll visit the block here
			basic_block_t* decl_block = visit_declaration_statement(ast_cursor);

			//If the start block is null, then this is the start block. Otherwise, we merge it in
			if(start_block == NULL){
				start_block = decl_block;
				current_block = decl_block;
			//Just merge with current
			} else {
				merge_blocks(current_block, decl_block); 
			}

		//Some weird error here
		} else {
			print_parse_message(PARSE_ERROR, "Unrecognizable node found as child to prog node", ast_cursor->line_number);
			(*num_errors_ref)++;
			return create_and_return_err();
		}
		
		//Advance to the next child
		ast_cursor = ast_cursor->next_sibling;
	}

	//Always return the start block
	return start_block;
}


/**
 * Build a cfg from the ground up
*/
cfg_t* build_cfg(front_end_results_package_t results, u_int32_t* num_errors, u_int32_t* num_warnings){
	//Store our references here
	num_errors_ref = num_errors;
	num_warnings_ref = num_warnings;

	//We'll first create the fresh CFG here
	cfg_t* cfg = calloc(1, sizeof(cfg_t));

	//For dev use here
	if(results.root->CLASS != AST_NODE_CLASS_PROG){
		print_parse_message(PARSE_ERROR, "Expected prog node as first node", results.root->line_number);
		exit(1);
	}

	//We'll visit the prog node, and let everything else do the rest
	cfg->root = visit_prog_node(results.root);

	//If we get a -1 block ID, this means that the whole thing failed
	if(cfg->root->block_id == -1){
		print_parse_message(PARSE_ERROR, "CFG was unable to be constructed", 0);
		(*num_errors_ref)++;
	}
	
	//Give back the reference
	return cfg;
}
