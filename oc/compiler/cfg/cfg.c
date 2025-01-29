/**
 * The implementation file for all CFG related operations
*/

#include "cfg.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

//Our atomically incrementing integer
//If at any point a block has an ID of (-1), that means that it is in error and can be dealt with as such
static int32_t current_block_id = 0;
//Do we need to see a leader statement?
static u_int8_t need_leader = 0;
//Keep global references to the number of errors and warnings
u_int32_t* num_errors_ref;
u_int32_t* num_warnings_ref;

/**
 * Simply prints a parse message in a nice formatted way. For the CFG, there
 * are no parser line numbers
*/
static void print_cfg_message(parse_message_type_t message_type, char* info){
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
	fprintf(stderr, "\n[COMPILER %s]: %s\n", type[parse_message.message], parse_message.info);
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
 * Add a predecessor to the target block. When we add a predecessor, the target
 * block is also implicitly made a successor of said predecessor
 */
void add_predecessor(basic_block_t* target, basic_block_t* predecessor, linked_direction_t directedness){
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
void add_successor(basic_block_t* target, basic_block_t* successor, linked_direction_t directedness){
	//Let's check this
	if(target->num_successors == MAX_SUCCESSORS){
		//Internal error for the programmer
		printf("CFG ERROR. YOU MUST INCREASE THE NUMBER OF SUCCESSORS");
		exit(1);
	}

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
void add_statement(basic_block_t* target, top_level_statement_node_t* statement_node){
	//Special case--we're adding the head
	if(target->leader_statement == NULL){
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
basic_block_t* merge_blocks(basic_block_t* a, basic_block_t* b){
	//The leader statement in b will be connected to a's tail
	a->exit_statement->next = b->leader_statement;
	//Now once they're connected we'll set a's exit to be b's exit
	a->exit_statement = b->exit_statement;

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

	//We will not deallocate here, we will merely free the block itself
	free(b);

	//Give back the pointer to a
	return a;
}


/**
 * Visit a declaration statement
 */
basic_block_t* visit_declaration_statement(generic_ast_node_t* decl_node){
	//This will likely be merged later, but we will still create it
	basic_block_t* decl_node_block = basic_block_alloc();
	
	//Create the needed 
	decl_node_block->leader_statement = create_statement(decl_node);

	return decl_node_block;
}


/**
 * Visit a top-level let statement
 * Let statements contain a root node that anchors a subtree containing everything that
 * we need to know about them. All type info is stored here as well
 */
basic_block_t* visit_let_statement(generic_ast_node_t* let_stmt){
	//Create the block
	basic_block_t* let_stmt_block = basic_block_alloc();

	//We'll add the let statement node in as a statement
	let_stmt_block->leader_statement = create_statement(let_stmt);
	
	//This block will most likely be merged, but still we will return it
	return let_stmt_block;
}



/**
 * Visit a function declaration. The start of a function declaration is
 * always a leader statement. We expect that we see a function definition
 * node here. If we do not, we fail immediately
 *
 * Since a function always has a compound statement in it, we will essentially
 * pass control off here to the compound statement rule
 */
basic_block_t* visit_function_declaration(generic_ast_node_t* func_def_node){
	basic_block_t* func_def_block = basic_block_alloc();

	return func_def_block;
}



/**
 * Build a CFG from the top-down using information derived from the parser front-end. This mainly consists
 * of the AST, but is also helped by the call graph, and symbol tables
 *
 * If at any point we return NULL here, that represents a failure in construction
 */
cfg_t* build_cfg(front_end_results_package_t results, u_int32_t* num_errors, u_int32_t* num_warnings){
	//Store these locally so that we can reference them wherever we are
	num_errors_ref = num_errors;
	num_warnings_ref = num_warnings;

	//Create the global cfg root
	cfg_t* cfg = calloc(1, sizeof(cfg_t));

	//We build the CFG from the ground up here
	//We have the AST root
	generic_ast_node_t* cursor = results.root->first_child;
	//By default we need to see a leader her
	need_leader = 1;

	//If this is null, we had an empty program of some kind
	if(cursor == NULL){
		print_cfg_message(PARSE_ERROR, "No top level CFG node has been detected");
		(*num_errors_ref)++;
		return NULL;
	}

	//The AST root is the "PROG" node. It has children that are
	//declarations, "lets" or functions. We will iterate through here and visit
	//items so long as we aren't null
	while(cursor != NULL){
		//We can encounter a function definition first thing
		if(cursor->CLASS == AST_NODE_CLASS_FUNC_DEF){
			//Visit our function declaration here
			basic_block_t* func_def_block = visit_function_declaration(cursor);

			//If we have a -1, this means that the whole block is an error
			if(func_def_block->block_id == -1){
				print_cfg_message(PARSE_ERROR, "Invalid function definition block encountered");
				(*num_errors_ref)++;
				return NULL;
			}

			//If the CFG root is null, then this becomes the root
			if(cfg->root == NULL){
				cfg->root = func_def_block;
				//We also maintain a reference to the current block
				cfg->current = func_def_block;
			//Otherwise, this block is a successor to the current block
			} else {
				//Add the successor in
				add_successor(cfg->current, func_def_block, LINKED_DIRECTION_UNIDIRECTIONAL);
				//Update the reference to whatever the current block is
				cfg->current = func_def_block;
			}

			//When we get out of a function, the next block will itself be a leader block
			need_leader = 1;

		//We can also encounter a declarative statement
		} else if(cursor->CLASS == AST_NODE_CLASS_DECL_STMT){
			//Let's visit the decl statement node here
			basic_block_t* decl_block = visit_declaration_statement(cursor);

			//If we have -1, that means that this whole block is an error
			if(decl_block->block_id == -1){
				print_cfg_message(PARSE_ERROR, "Invalid top level declaration block encountered");
				(*num_errors_ref)++;
				return NULL;
			}

			//If the root is null, then this becomes the root
			if(cfg->root == NULL){
				cfg->root = decl_block;
				//We also maintain a reference to the current block
				cfg->current = decl_block;
			//If we need a leader, then this block will be added as a successor
			} else if(need_leader == 1){
				add_successor(cfg->current, decl_block, LINKED_DIRECTION_UNIDIRECTIONAL);
				//Update the current reference
				cfg->current = decl_block;

			//Otherwise, this block will be "merged" with whoever the current block is
			} else {
				//Merge blocks and maintain the CFG's current pointer
				cfg->current = merge_blocks(cfg->current, decl_block);
			}

		//We can also encounter a let statement
		} else if(cursor->CLASS == AST_NODE_CLASS_LET_STMT){
			//Let's visit the decl statement node here
			basic_block_t* let_block = visit_let_statement(cursor);

			//If we have -1, that means that this whole block is an error
			if(let_block->block_id == -1){
				print_cfg_message(PARSE_ERROR, "Invalid top level let block encountered");
				(*num_errors_ref)++;
				return NULL;
			}

			//If the root is null then this becomes the root
			if(cfg->root == NULL){
				cfg->root = let_block;
				//We also maintain a reference to the current block
				cfg->current = let_block;
			//If we need a leader, then this block will be added as a successor
			} else  if(need_leader == 1){
				add_successor(cfg->current, let_block, LINKED_DIRECTION_UNIDIRECTIONAL);
				//Update the current reference
				cfg->current = let_block;
			//Otherwise, this block will be "merged" with whoever the current block is
			} else {
				//Merge blocks and maintain the CFG's current pointer
				cfg->current = merge_blocks(cfg->current, let_block);
			}

		//We really should never get here, but we'll catch this if we do
		} else {
			print_cfg_message(PARSE_ERROR, "Invalid top level node detected in AST");
			(*num_errors_ref)++;
			return NULL;
		}

		//At the very end here, we refresh cursor with its next sibling
		cursor = cursor->next_sibling;
	}

	return cfg;
}
