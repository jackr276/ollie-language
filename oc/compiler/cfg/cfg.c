/**
 * The implementation file for all CFG related operations
*/

#include "cfg.h"
#include <bits/types/stack_t.h>
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

//We predeclare up here to avoid needing any rearrangements
static basic_block_t* visit_declaration_statement(generic_ast_node_t* decl_node);
static basic_block_t* visit_compound_statement(generic_ast_node_t* compound_stmt_node, basic_block_t* function_block_end);
static basic_block_t* visit_let_statement(generic_ast_node_t* let_stmt);
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
static basic_block_t* merge_blocks(basic_block_t* a, basic_block_t* b){
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

	//We will not deallocate here, we will merely free the block itself
	free(b);

	//Give back the pointer to a
	return a;
}


/**
 * A for statement is another control flow statement that loops
 */
static basic_block_t* visit_for_statement(generic_ast_node_t* for_stmt_node, basic_block_t* function_block_end){
	//Create our entry block
	basic_block_t* for_stmt_entry_block = basic_block_alloc();
	//The current block we're operating with
	basic_block_t* current = for_stmt_entry_block;	
	
	//Create our end block
	basic_block_t* end_block = basic_block_alloc();
	//This end block is ok to merge
	end_block->good_to_merge = 1;

	//Grab a cursor for the for statemeent
	generic_ast_node_t* cursor = for_stmt_node->first_child;

	//The first node that we see could be a let_statement
	if(cursor->CLASS == AST_NODE_CLASS_LET_STMT || cursor->CLASS == AST_NODE_CLASS_ASNMNT_EXPR){
		//This block is OK to merge
		current->good_to_merge = 1;
		
		//Create the statement
		top_level_statement_node_t* expr_stmt = create_statement(cursor);

		//We'll add it in as a statement to our block
		add_statement(current, expr_stmt);

		//Since the next thing that we see will be the loop, we need a fresh block
		basic_block_t* next_block = basic_block_alloc();

		//This block is a strict successor of current
		add_successor(current, next_block, LINKED_DIRECTION_UNIDIRECTIONAL);
		
		//Current is this block
		current = next_block;

		//Move the cursor up
		cursor = cursor->next_sibling;
	}

	//So long as we don't see the compound statement, keep going
	while(cursor->CLASS != AST_NODE_CLASS_COMPOUND_STMT){
		//We add the expression as a statement
		top_level_statement_node_t* current_stmt = create_statement(cursor);

		//Add this statement into the current block
		add_statement(current, current_stmt);

		//Move cursor up
		cursor = cursor->next_sibling;
	}

	//We know that the current block points directly to the end block
	add_successor(current, end_block, LINKED_DIRECTION_UNIDIRECTIONAL);

	//Once we get here, the cursor will be on the compound statement
	basic_block_t* compound_stmt_block = visit_compound_statement(cursor, function_block_end);

	//We now need to navigate all the way down to this block's end
	basic_block_t* compound_block_end = compound_stmt_block;
	
	//So long as this isn't 0, we haven't reached the end
	while(compound_block_end->is_return_stmt == 0 && compound_block_end->num_successors != 0){
		//Drill down some more
		compound_block_end = compound_block_end->successors[0];
	}

	//The current block goes right to the compound statement
	add_successor(current, compound_stmt_block, LINKED_DIRECTION_UNIDIRECTIONAL);

	//We also know that the end of the compound statement points directly to the beginning(current)
	add_successor(compound_block_end, current, LINKED_DIRECTION_UNIDIRECTIONAL);

	//All done now
	return for_stmt_entry_block;
}


/**
 * A do-while statement is another control flow statement that can also point back to itself
 */
static basic_block_t* visit_do_while_statement(generic_ast_node_t* do_while_stmt_node, basic_block_t* function_block_end){
	//Create our entry block
	basic_block_t* do_while_stmt_block = basic_block_alloc();
	//Also create the ending bloc
	basic_block_t* end_block = basic_block_alloc();

	//Let's grab a cursor to crawl the do-while statement
	generic_ast_node_t* cursor = do_while_stmt_node->first_child;

	//The very first thing that we will see is a compound statement
	basic_block_t* compound_stmt_block = visit_compound_statement(cursor, function_block_end);

	//We now need to navigate all the way down to this block's end
	basic_block_t* compound_block_end = compound_stmt_block;
	
	//So long as this isn't 0, we haven't reached the end
	while(compound_block_end->is_return_stmt == 0 && compound_block_end->num_successors != 0){
		//Drill down some more
		compound_block_end = compound_block_end->successors[0];
	}

	//This compound statement will be merged with the entry statement
	do_while_stmt_block = merge_blocks(do_while_stmt_block, compound_stmt_block);

	//From here we will see a statement that is inside of the while block
	cursor = cursor->next_sibling;
	
	//Create a new block just for our statement
	basic_block_t* statement_block = basic_block_alloc();

	//Create the conditional statement
	top_level_statement_node_t* conditional_stmt = create_statement(cursor);

	//Add this into the next block
	add_statement(statement_block, conditional_stmt);

	//This block is a successor to the end block in the compound statement
	add_successor(compound_block_end, statement_block, LINKED_DIRECTION_UNIDIRECTIONAL);

	//This block can point to our ending block
	add_successor(statement_block, end_block, LINKED_DIRECTION_UNIDIRECTIONAL);

	//This block also points all the way back up to the beginning
	add_successor(statement_block, do_while_stmt_block, LINKED_DIRECTION_UNIDIRECTIONAL);

	//Finally, we give the starting block back
	return do_while_stmt_block;
}


/**
 * A while statement is a looping control flow statement that can point back to itself
 */
static basic_block_t* visit_while_statement(generic_ast_node_t* while_stmt_node, basic_block_t* function_block_end){
	//Create our entry block
	basic_block_t* while_stmt_block = basic_block_alloc();
	//Also create our end block
	basic_block_t* end_block = basic_block_alloc();

	//Due to the way a while loop works, the end block is also a direct successor to the first block
	add_successor(while_stmt_block, end_block, LINKED_DIRECTION_UNIDIRECTIONAL);

	//We'll use a cursor to crawl through the statements
	generic_ast_node_t* cursor = while_stmt_node->first_child;

	//The very first thing that we will see here is a statement
	top_level_statement_node_t* expr_statement = create_statement(cursor);

	//This statement will be added to our block here
	add_statement(while_stmt_block, expr_statement);

	//Advance the cursor
	cursor = cursor->next_sibling;

	//Following this, the only thing left to see is a compound statment
	basic_block_t* compound_stmt_block = visit_compound_statement(cursor, function_block_end);

	//The compound statement block is a successor to the first block
	add_successor(while_stmt_block, compound_stmt_block, LINKED_DIRECTION_UNIDIRECTIONAL);

	//We now need to navigate all the way down to this block's end
	basic_block_t* compound_block_end = compound_stmt_block;
	
	//So long as this isn't 0, we haven't reached the end
	while(compound_block_end->is_return_stmt == 0 && compound_block_end->num_successors != 0){
		//Drill down some more
		compound_block_end = compound_block_end->successors[0];
	}

	//Once we get here, we have the very end of the compound statement block
	//This block's direct successor is the top block
	add_successor(compound_block_end, while_stmt_block, LINKED_DIRECTION_UNIDIRECTIONAL);


	//Finally we are all set here, so we will return
	return while_stmt_block;
}


/**
 * An if statement always invokes some kind of control flow
 */
static basic_block_t* visit_if_statement(generic_ast_node_t* if_stmt_node, basic_block_t* function_block_end){
	//Create our basic block
	basic_block_t* if_stmt_block = basic_block_alloc();
	//Create an "end block". This is very important because the end block here
	//creates an anchor where all of our other statements go
	basic_block_t* end_block = basic_block_alloc();

	//Grab a cursor that we will use to crawl the if statement
	generic_ast_node_t* cursor = if_stmt_node->first_child;

	//We need to first see the expression inside of the if statement
	top_level_statement_node_t* expr_stmt = create_statement(cursor);
	
	//Add this statement into the if_statement block
	add_statement(if_stmt_block, expr_stmt);

	//Move the cursor up
	cursor = cursor->next_sibling;

	//Following this, we'll see a compound statement
	basic_block_t* compound_stmt_block = visit_compound_statement(cursor, function_block_end);

	//This compount_statement is always a control-flow successor to the condition
	add_successor(if_stmt_block, compound_stmt_block, LINKED_DIRECTION_UNIDIRECTIONAL);
	
	//We now need to navigate all the way down to this block's end
	basic_block_t* compound_block_end = compound_stmt_block;
	
	//So long as this isn't 0, we haven't reached the end
	while(compound_block_end->is_return_stmt == 0 && compound_block_end->num_successors != 0){
		//Drill down some more
		compound_block_end = compound_block_end->successors[0];
	}

	//This block will have it's own successor, the end statement block
	add_successor(compound_block_end, end_block, LINKED_DIRECTION_UNIDIRECTIONAL);

	//Now moving forward here, we do have the option to see an "else" section
	cursor = cursor->next_sibling;

	//If this is null, then we're done here
	if(cursor == NULL){
		return if_stmt_block;
	}

	//But if it isn't null, it's our else node -- which could be an else or another if(if-else) 
	if(cursor->CLASS == AST_NODE_CLASS_COMPOUND_STMT){
		//We'll invoke the compound statement again here
		compound_stmt_block = visit_compound_statement(cursor, function_block_end);
		//We'll then add this in same as before
		//This compount_statement is always a control-flow successor to the condition
		add_successor(if_stmt_block, compound_stmt_block, LINKED_DIRECTION_UNIDIRECTIONAL);

		//We now need to navigate all the way down to this block's end
		compound_block_end = compound_stmt_block;
	
		//So long as this isn't 0, we haven't reached the end
		while(compound_block_end->is_return_stmt == 0 && compound_block_end->num_successors != 0){
			//Drill down some more
			compound_block_end = compound_block_end->successors[0];
		}

		//This block will have it's own successor, the end statement block
		add_successor(compound_block_end, end_block, LINKED_DIRECTION_UNIDIRECTIONAL);

		//Once we're done get out
		return if_stmt_block;

	//If we see another if statement
	} else if(cursor->CLASS == AST_NODE_CLASS_IF_STMT){
		//Otherwise we'll invoke this very rule
		basic_block_t* else_if_block = visit_if_statement(cursor, function_block_end);

		//This block is a successor to the parent
		add_successor(if_stmt_block, else_if_block, LINKED_DIRECTION_UNIDIRECTIONAL);

		//Again, we'll need to drill all the way to the very end of this block
		//We now need to navigate all the way down to this block's end
		basic_block_t* if_block_end = else_if_block;
	
		//So long as this isn't 0, we haven't reached the end
		while(if_block_end->is_return_stmt == 0 && if_block_end->num_successors != 0){
			//Drill down some more
			if_block_end = if_block_end->successors[0];
		}

		//Merge these two blocks
		merge_blocks(if_block_end, end_block);

		//And bail out
		return if_stmt_block;

	} else {
		print_cfg_message(PARSE_ERROR, "Fatal internal compiler error. Found unknown non-null block in if statement", cursor->line_number);
		return create_and_return_err();
	}
}


/**
 * Visit an assignment expression. This should be quite simple, as there are only two children
 */
basic_block_t* visit_assignment_expression(generic_ast_node_t* assignment_expr_node){
	//Create our basic block
	basic_block_t* asn_expr_block = basic_block_alloc();

	//We'll need to walk this node's subtree with a cursor
	generic_ast_node_t* cursor = assignment_expr_node->first_child;

	//If this first child is not a unary expression, we bail out
	if(cursor->CLASS != AST_NODE_CLASS_UNARY_EXPR){
		print_cfg_message(PARSE_ERROR, "Fatal internal compiler error. Expected unary expression as the first child of assignment expression", cursor->line_number);
		return create_and_return_err();
	}

	//We'll walk the subtree creating statements
	while(cursor != NULL){
		//Create the statement
		top_level_statement_node_t* stmt = create_statement(cursor);

		//Add it into the node
		add_statement(asn_expr_block, stmt);

		//Advance the cursor
		cursor = cursor->next_sibling;
	}

	return asn_expr_block;
}


/**
 * Visit an expression statement. This can decay into a variety of non-control flow cases
 */
static basic_block_t* visit_expression_statement(generic_ast_node_t* expr_statement_node){
	//This will probably be merged
	basic_block_t* expression_stmt_block = basic_block_alloc();

	//We can either have an assignment expression or a given non-assigned expression
	if(expr_statement_node->first_child->CLASS == AST_NODE_CLASS_ASNMNT_EXPR){
		//Let the assignment expression rule handle it
		basic_block_t* asn_expr_block = visit_assignment_expression(expr_statement_node->first_child);

		//Merge the two statements here
		expression_stmt_block = merge_blocks(expression_stmt_block, asn_expr_block);

	} else {
		//Otherwise, for now, we'll just add this in as a statement
		top_level_statement_node_t* stmt = create_statement(expr_statement_node->first_child);

		//Add this into the current block
		add_statement(expression_stmt_block, stmt);
	}


	return expression_stmt_block;
}


/**
 * Visit a compound statement. This is usually a jumping off point for various other nodes
 *
 * TODO RETHINK HOW THIS IS STRUCTURED
 */
static basic_block_t* visit_compound_statement(generic_ast_node_t* compound_stmt_node, basic_block_t* function_block_end){
	//For error printing
	char info[1000];
	//This will probably not be merged
	basic_block_t* compound_stmt_block = basic_block_alloc();
	//The end block
	basic_block_t* compound_stmt_block_end = basic_block_alloc();
	//We will keep track of what the current "end" block is
	basic_block_t* current_block = compound_stmt_block;

	//We will iterate over all of the children in this compound statement
	generic_ast_node_t* cursor = compound_stmt_node->first_child;

	//By default, we do not immediately need a leader block here(compound_stmt_block is the leader block)
	need_leader = 0;

	//So long as we have stuff in here
	while(cursor != NULL){
		//If we encounter a declaration statement
		if(cursor->CLASS == AST_NODE_CLASS_DECL_STMT){
			basic_block_t* decl_block = visit_declaration_statement(cursor); 

			//If we need a leader, then we'll add this onto current
			if(need_leader == 1){
				//Add the decl block as a successor
				add_successor(current_block, decl_block, LINKED_DIRECTION_UNIDIRECTIONAL);
				//We'll also update the current reference
				current_block = decl_block;
			//Otherwise, we will just merge this block in
			} else {
				//Merge the block in, the current block pointer is unchanged
				current_block = merge_blocks(current_block, decl_block);
			}
		//If we encounter a let statement
		} else if(cursor->CLASS == AST_NODE_CLASS_LET_STMT){
			//Let the subsidiary handle
			basic_block_t* let_block = visit_let_statement(cursor); 

			//If we need a leader, then we'll add this onto current
			if(need_leader == 1){
				//Add the decl block as a successor
				add_successor(current_block, let_block, LINKED_DIRECTION_UNIDIRECTIONAL);
				//We'll also update the current reference
				current_block = let_block;
			//Otherwise, we will just merge this block in
			} else {
				//Merge the block in, the current block pointer is unchanged
				current_block = merge_blocks(current_block, let_block);
			}

		//If we encounter an expression statement
		} else if(cursor->CLASS == AST_NODE_CLASS_EXPR_STMT){
			//Let the subsidiary handle
			basic_block_t* expr_block = visit_expression_statement(cursor); 

			//If we need a leader, then we'll add this onto current
			if(need_leader == 1){
				//Add the decl block as a successor
				add_successor(current_block, expr_block, LINKED_DIRECTION_UNIDIRECTIONAL);
				//We'll also update the current reference
				current_block = expr_block;
			//Otherwise, we will just merge this block in
			} else {
				//Merge the block in, the current block pointer is unchanged
				current_block = merge_blocks(current_block, expr_block);
			}

		//If this is the case we're just gonna call ourself recursively
		} else if(cursor->CLASS == AST_NODE_CLASS_COMPOUND_STMT){
			//Let the subsidiary handle
			basic_block_t* compound_stmt_block = visit_compound_statement(cursor, function_block_end); 

			//We'll also need a reference to the end block
			basic_block_t* block_cursor = compound_stmt_block;

			//If there are more than 0 successors, we need to keep going
			while(block_cursor->is_return_stmt == 0 && block_cursor->num_successors != 0){
				block_cursor = block_cursor->successors[0];
			}

			//If we need a leader
			if(need_leader == 1){
				//Add the decl block as a successor
				add_successor(current_block, compound_stmt_block, LINKED_DIRECTION_UNIDIRECTIONAL);
				//We'll also update the current reference to be the very end of this bloc
				current_block = block_cursor;
			//Otherwise, we will just merge this block in
			} else {
				//Merge the block in, the current block pointer is unchanged
				merge_blocks(current_block, compound_stmt_block);
				//Update the current block here
				current_block = block_cursor;
			}
		
		//This is the first kind of block where any actual control flow happens
		} else if(cursor->CLASS == AST_NODE_CLASS_IF_STMT){
			
			//Let the subsidiary handle
			basic_block_t* if_stmt_block = visit_if_statement(cursor, function_block_end);

			//We'll also need a reference to the end block
			basic_block_t* cursor = if_stmt_block;

			//If there are more than 0 successors, we need to keep going
			while(cursor->is_return_stmt == 0 && cursor->num_successors != 0){
				cursor = cursor->successors[0];
			}

			//If we need to see a leader statement
			if(need_leader == 1){
				//Add the if block as a successor
				add_successor(current_block, if_stmt_block, LINKED_DIRECTION_UNIDIRECTIONAL);
				//We'll also update the current reference
				current_block = cursor;
			//Otherwise, we will just merge this block in
			} else {
				//Merge the block in, the current block pointer is unchanged
				merge_blocks(current_block, if_stmt_block);
				//Update the current reference
				current_block = cursor;
			}
			
		//We've encountered a while statement
		} else if(cursor->CLASS == AST_NODE_CLASS_WHILE_STMT){
			//Let the subsidiary handle
			basic_block_t* while_stmt_block = visit_while_statement(cursor, function_block_end);

			//We'll also need a reference to the end block
			basic_block_t* cursor = while_stmt_block;

			//If there are more than 0 successors, we need to keep going
			while(cursor->is_return_stmt == 0 && cursor->num_successors != 0){
				cursor = cursor->successors[0];
			}

			//If we see a while statement, we actually never merge it
			//Add the if block as a successor
			add_successor(current_block, while_stmt_block, LINKED_DIRECTION_UNIDIRECTIONAL);
			//We'll also update the current reference
			current_block = cursor;

		//We've encountered a do-while statement
		} else if(cursor->CLASS == AST_NODE_CLASS_DO_WHILE_STMT){
			//Let the subsidiary handle
			basic_block_t* do_while_stmt_block = visit_do_while_statement(cursor, function_block_end);

			//We'll also need a reference to the end block
			basic_block_t* cursor = do_while_stmt_block;

			//If there are more than 0 successors, we need to keep going
			while(cursor->is_return_stmt == 0 && cursor->num_successors != 0){
				cursor = cursor->successors[0];
			}

			//We don't merge do-whiles, they will always be a successor
			add_successor(current_block, do_while_stmt_block, LINKED_DIRECTION_UNIDIRECTIONAL);
			//Update the current cursor
			current_block = cursor;
		//We've encountered a for-statement
		} else if(cursor->CLASS == AST_NODE_CLASS_FOR_STMT){
			//Let the subsidiary handle it
			basic_block_t* for_stmt_block = visit_for_statement(cursor, function_block_end);

			//We'll also need a reference to the end block
			basic_block_t* cursor = for_stmt_block;

			//If there are more than 0 successors, we need to keep going
			while(cursor->is_return_stmt == 0 && cursor->num_successors != 0){
				cursor = cursor->successors[0];
			}

			//If the for_stmt_block is ok to merge, we'll do that
			if(for_stmt_block->good_to_merge == 1){
				current_block = merge_blocks(current_block, for_stmt_block);
			//Otherwise, we'll add it in as a successor
			} else {
				add_successor(current_block, for_stmt_block, LINKED_DIRECTION_UNIDIRECTIONAL);
			}

			//Whatever happened, the new current block is the end of the for statement
			current_block = cursor;

		//We've encountered a return statement
		} else if(cursor->CLASS == AST_NODE_CLASS_RET_STMT){
			//There are really two options here -- we could see a blank return statement
			//or an expression one
			//If this isn't null, we'll add the expression in
			if(cursor->first_child != NULL){
				//Create the statement
				top_level_statement_node_t* ret_stmt = create_statement(cursor->first_child);
				//Add it in
				add_statement(current_block, ret_stmt);
			}

			//No matter what, this current block now points to the function's end
			add_successor(current_block, function_block_end, LINKED_DIRECTION_UNIDIRECTIONAL);

			//Mark it as a return statement
			current_block->is_return_stmt = 1;
			
			//If the next sibling isn't null, we have unreachable code
			if(cursor->next_sibling != NULL){
				print_cfg_message(WARNING, "Unreachable code detected after a return statement", cursor->next_sibling->line_number);
				(*num_warnings_ref)++;
			}

			break;
		}

		//Go to the next sibling
		cursor = cursor->next_sibling;
	} 



	//We always give back the very first block here
	return compound_stmt_block;
}


/**
 * Visit a declaration statement
 */
static basic_block_t* visit_declaration_statement(generic_ast_node_t* decl_node){
	//This will likely be merged later, but we will still create it
	basic_block_t* decl_node_block = basic_block_alloc();

	//There is only one statement here
	top_level_statement_node_t* stmt = create_statement(decl_node);

	//Add the statement in
	add_statement(decl_node_block, stmt);

	return decl_node_block;
}


/**
 * Visit a top-level let statement
 * Let statements contain a root node that anchors a subtree containing everything that
 * we need to know about them. All type info is stored here as well
 */
static basic_block_t* visit_let_statement(generic_ast_node_t* let_stmt){
	//Create the block
	basic_block_t* let_stmt_block = basic_block_alloc();

	//There is only one statement here
	top_level_statement_node_t* stmt = create_statement(let_stmt);

	//Add the statement in
	add_statement(let_stmt_block, stmt);
	
	//This block will most likely be merged, but still we will return it
	return let_stmt_block;
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

	//So long as the stack is not empty
	while(is_empty(stack) == 0){
		//Grab the current one off of the stack
		block_cursor = pop(stack);

		//If this wasn't visited
		if(block_cursor->visited == 0){
			/**
			 * Now we can perform our checks. 
			 */
			if(block_cursor->is_exit_block == 0 && block_cursor->num_successors == 0){
				dead_ends++;
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

	//Once we escape our while loop, we can actually see what the analysis said
	if(dead_ends > 0){
		//Extract the function name
		char* func_name = ((func_def_ast_node_t*)(function_node->node))->func_record->func_name;
		sprintf(info, "Function \"%s\" does not return in %d control paths", func_name, dead_ends);
		print_cfg_message(WARNING, info, function_node->line_number);
		(*num_warnings_ref)+=dead_ends;
	}
}


/**
 * Visit a function declaration. The start of a function declaration is
 * always a leader statement. We expect that we see a function definition
 * node here. If we do not, we fail immediately
 *
 * Since a function always has a compound statement in it, we will essentially
 * pass control off here to the compound statement rule
 */
static basic_block_t* visit_function_declaration(generic_ast_node_t* func_def_node){
	//Create the block for ourselves here
	basic_block_t* func_def_block = basic_block_alloc();
	//Also create the ending block that we'll need
	basic_block_t* end_block = basic_block_alloc();

	//The compound statement is always the last child of a function statement
	generic_ast_node_t* func_cursor = func_def_node->first_child;	

	//Get to the end for now
	while(func_cursor->next_sibling != NULL){
		func_cursor = func_cursor->next_sibling;
	}

	//The next thing that a function declaration sees is a compound statement
	if(func_cursor->CLASS != AST_NODE_CLASS_COMPOUND_STMT){
		print_cfg_message(PARSE_ERROR, "Fatal internal compiler error. Did not find compound statement as the last node in a function block.", func_cursor->line_number);
		(*num_errors_ref)++;
		//Create and give back an erroneous block
		return create_and_return_err();
	}

	//Otherwise, we can visit the compound statement
	basic_block_t* compound_stmt_block = visit_compound_statement(func_cursor, end_block);

	//Again, we'll need to drill all the way to the very end of this block
	//We now need to navigate all the way down to this block's end
	basic_block_t* compound_block_end = compound_stmt_block;

	//So long as this isn't 0, we haven't reached the end
	while(compound_block_end->num_successors == 0 && compound_block_end->is_return_stmt == 1){
		//Drill down some more
		compound_block_end = compound_block_end->successors[0];
	}

	//If it's a failure, we error out
	if(compound_stmt_block->block_id == -1){
		//Print our message out
		print_cfg_message(PARSE_ERROR, "Invalid compound statement encountered in function definition", func_cursor->line_number);
		//Send this up the chain
		return compound_stmt_block;
	}

	//At the end, we'll merge the compound statement block with the function definition block
	merge_blocks(func_def_block, compound_stmt_block);
	
	//The end of the compound statement points to the end block
	add_successor(compound_block_end, end_block, LINKED_DIRECTION_UNIDIRECTIONAL);
	
	//The compound block end is an exit block
	end_block->is_exit_block = 1;

	//Perform our reachability analysis here--this will produce appropriate warnings
	perform_function_reachability_analysis(func_def_node, func_def_block);

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
		print_cfg_message(PARSE_ERROR, "No top level CFG node has been detected", 0);
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
				print_cfg_message(PARSE_ERROR, "Invalid function definition block encountered", cursor->line_number);
				(*num_errors_ref)++;
				return NULL;
			}

			//We need to drill to the bottom of this block
			basic_block_t* cursor = func_def_block;

			//So long as we aren't at the end
			while(cursor->num_successors != 0){
				cursor = cursor->successors[0];
			}
			//Once we get here, we'll have the bottom

			//If the CFG root is null, then this becomes the root
			if(cfg->root == NULL){
				cfg->root = func_def_block;
				//We also maintain a reference to the current block
				cfg->current = cursor;
			//Otherwise, this block is a successor to the current block
			} else {
				//Add the successor in
				add_successor(cursor, func_def_block, LINKED_DIRECTION_UNIDIRECTIONAL);
				//Update the reference to whatever the current block is
				cfg->current = cursor;
			}

		//We can also encounter a declarative statement
		} else if(cursor->CLASS == AST_NODE_CLASS_DECL_STMT){
			//Let's visit the decl statement node here
			basic_block_t* decl_block = visit_declaration_statement(cursor);

			//If we have -1, that means that this whole block is an error
			if(decl_block->block_id == -1){
				print_cfg_message(PARSE_ERROR, "Invalid top level declaration block encountered", cursor->line_number);
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
				print_cfg_message(PARSE_ERROR, "Invalid top level let block encountered", cursor->line_number);
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
			print_cfg_message(PARSE_ERROR, "Invalid top level node detected in AST", cursor->line_number);
			(*num_errors_ref)++;
			return NULL;
		}

		//At the very end here, we refresh cursor with its next sibling
		cursor = cursor->next_sibling;
	}

	return cfg;
}
