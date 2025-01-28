/**
 * The implementation file for all CFG related operations
*/

#include "cfg.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

//Our atomically incrementing integer
static u_int16_t current_block_id = 0;

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
static u_int16_t increment_and_get(){
	current_block_id++;
	return current_block_id;
}


/**
 * Allocate a basic block using calloc. NO data assignment
 * happens in this function
*/
basic_block_t* basic_block_alloc(cfg_t* cfg){
	//Allocate the block
	basic_block_t* created = calloc(1, sizeof(basic_block_t));
	//Grab the unique ID for this block
	created->block_id = increment_and_get();
	//Increment the number of blocks
	(cfg->num_blocks)++;

	return created;
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

	//Once we're done here, b is no longer of use to us
	basic_block_dealloc(b);

	//Give back the pointer to a
	return a;
}


/**
 * Build a CFG from the top-down using information derived from the parser front-end. This mainly consists
 * of the AST, but is also helped by the call graph, and symbol tables
 *
 * If at any point we return NULL here, that represents a failure in construction
 */
cfg_t* build_cfg(front_end_results_package_t results, u_int32_t* num_errors, u_int32_t* num_warnings){
	//Create the global cfg root
	cfg_t* cfg = calloc(1, sizeof(cfg_t));

	//We build the CFG from the ground up here
	//We have the AST root
	generic_ast_node_t* cursor = results.root->first_child;

	//If this is null, we had an empty program of some kind
	if(cursor == NULL){
		print_cfg_message(PARSE_ERROR, "No top level CFG node has been detected");
		(*num_errors)++;
		return NULL;
	}

	//The AST root is the "PROG" node. It has children that are
	//declarations, "lets" or functions. We will iterate through here and visit
	//items so long as we aren't null
	while(cursor != NULL){
		if(cursor->CLASS == AST_NODE_CLASS_FUNC_DEF){

		} else if(cursor->CLASS == AST_NODE_CLASS_DECL_STMT){

		} else if(cursor->CLASS == AST_NODE_CLASS_LET_STMT){

		} else {
			print_cfg_message(PARSE_ERROR, "Invalid top level node detected in AST");
			(*num_errors)++;
			return NULL;
		}
	}


}


/**
 * Deallocate a basic block
*/
void basic_block_dealloc(basic_block_t* block){
	//Just in case
	if(block == NULL){
		printf("ERROR: Attempt to deallocate a null block");
		exit(1);
	}

	//Otherwise its fine so
	free(block);
}


