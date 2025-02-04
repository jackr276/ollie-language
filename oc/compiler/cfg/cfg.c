/**
 * The implementation file for all CFG related operations
*/

#include "cfg.h"
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
//Keep a stack of deferred statements for each function
heap_stack_t* deferred_stmts;

//A package of values that each visit function uses
typedef struct {
	generic_ast_node_t* initial_node;
	basic_block_t* function_end_block;
	basic_block_t* loop_stmt_start;
	basic_block_t* if_stmt_end_block;
	top_level_statement_node_t* for_loop_update_clause;
} values_package_t;


//We predeclare up here to avoid needing any rearrangements
static basic_block_t* visit_declaration_statement(values_package_t* values);
static basic_block_t* visit_compound_statement(values_package_t* values);
static basic_block_t* visit_let_statement(values_package_t* values);
static basic_block_t* visit_expression_statement(values_package_t* values);
static basic_block_t* visit_if_statement(values_package_t* values);
static basic_block_t* visit_while_statement(values_package_t* values);
static basic_block_t* visit_do_while_statement(values_package_t* values);
static basic_block_t* visit_for_statement(values_package_t* values);


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
 *
 * IMPORTANT NOTE: ONCE BLOCKS ARE MERGED, BLOCK B IS GONE
 */
static basic_block_t* merge_blocks(basic_block_t* a, basic_block_t* b){
	//Just to double check
	if(a == NULL){
		print_cfg_message(PARSE_ERROR, "Fatal error. Attempting to merge null block", 0);
		exit(1);
	}

	//If b is null, we just return a
	if(b == NULL){
		return a;
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

	//So long as the stack is not empty
	while(is_empty(stack) == 0){
		//Grab the current one off of the stack
		block_cursor = pop(stack);

		//If this wasn't visited
		if(block_cursor->visited == 0){
			//Mark this one as seen
			block_cursor->visited = 1;

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
		sprintf(info, "Non-void function \"%s\" does not return a value in all control paths", func_name);
		print_cfg_message(WARNING, info, function_node->line_number);
		(*num_warnings_ref)+=dead_ends;
	}

	//Destroy the stack once we're done
	destroy_stack(stack);
}


/**
 * A for-statement is another kind of control flow construct. As always the direct successor is the path that reliably
 * leads us down and out
 */
static basic_block_t* visit_for_statement(values_package_t* values){
	//Create our entry block
	basic_block_t* for_stmt_entry_block = basic_block_alloc();
	//Create our exit block
	basic_block_t* for_stmt_exit_block = basic_block_alloc();
	
	//Grab the reference to the for statement node
	generic_ast_node_t* for_stmt_node = values->initial_node;

	//Grab a cursor for walking the sub-tree
	generic_ast_node_t* ast_cursor = for_stmt_node->first_child;

	//We will always see 3 nodes here to start out with, of the type for_loop_cond_ast_node_t. These
	//nodes contain an "is_blank" field that will alert us if this is just a placeholder. The first 2 parts of a for

	//If the very first one is not blank
	if(((for_loop_condition_ast_node_t*)(ast_cursor->node))->is_blank == 0){
		//Add it's child in as a statement to the entry block
		top_level_statement_node_t* first_cond = create_statement(ast_cursor->first_child);
		//Add it in to the entry block
		add_statement(for_stmt_entry_block, first_cond);
	}

	//We'll now need to create our repeating node. This is the node that will actually repeat from the for loop.
	//The second and third condition in the for loop are the ones that execute continously. The third condition
	//always executes at the end of each iteration
	basic_block_t* condition_block = basic_block_alloc();

	//The condition block is always a successor to the entry block
	add_successor(for_stmt_entry_block, condition_block, LINKED_DIRECTION_UNIDIRECTIONAL);

	//The condition block also has another direct successor, the exit block
	add_successor(condition_block, for_stmt_exit_block, LINKED_DIRECTION_UNIDIRECTIONAL);
	//Ensure it is the direct successor
	condition_block->direct_successor = for_stmt_exit_block;

	//Move along to the next node
	ast_cursor = ast_cursor->next_sibling;
	
	//If the second one is not blank
	if(((for_loop_condition_ast_node_t*)(ast_cursor->node))->is_blank == 0){
		//Add it's child in as a statement to the entry block
		top_level_statement_node_t* second_cond = create_statement(ast_cursor->first_child);

		//This is always the first part of the repeating block
		add_statement(condition_block,  second_cond);
	
	//It is impossible for the second one to be blank
	} else {
		print_parse_message(PARSE_ERROR, "Fatal internal compiler error. Should not have gotten here if blank", for_stmt_node->line_number);
		exit(0);
	}

	//Now move it along to the third condition
	ast_cursor = ast_cursor->next_sibling;

	//Allocate and set to NULL as a warning for later
	top_level_statement_node_t* third_cond = NULL;

	//If the third one is not blank
	if(((for_loop_condition_ast_node_t*)(ast_cursor->node))->is_blank == 0){
		//Just make the statement for now, it will be added in later
		third_cond = create_statement(ast_cursor->first_child);
	}

	//Advance to the next sibling
	ast_cursor = ast_cursor->next_sibling;
	
	//If this is not a compound statement, we have a serious error
	if(ast_cursor->CLASS != AST_NODE_CLASS_COMPOUND_STMT){
		print_parse_message(PARSE_ERROR, "Fatal internal compiler error. Expected compound statement in for loop, but did not find one.", for_stmt_node->line_number);
		//Immediate failure here
		exit(0);
	}
	
	//Create a copy of our values here
	values_package_t compound_stmt_values;
	compound_stmt_values.initial_node = ast_cursor;
	//The loop starts at the condition block
	compound_stmt_values.loop_stmt_start = condition_block;
	//Store the end block
	compound_stmt_values.function_end_block = values->function_end_block;
	//Store this as well
	compound_stmt_values.for_loop_update_clause = third_cond;

	//Otherwise, we will allow the subsidiary to handle that. The loop statement here is the condition block,
	//because that is what repeats on continue
	basic_block_t* compound_stmt_start = visit_compound_statement(&compound_stmt_values);

	//If it's null, that's actually ok here
	if(compound_stmt_start == NULL){
		//We'll make our own
		basic_block_t* repeating_block = basic_block_alloc();
		//Add the third conditional to it
		if(third_cond != NULL){
			add_statement(repeating_block, third_cond);
		}

		//We'll make sure that the start points to this block
		add_successor(condition_block, repeating_block, LINKED_DIRECTION_UNIDIRECTIONAL);
		//And we'll add the conditional block as a successor to this
		add_successor(repeating_block, condition_block, LINKED_DIRECTION_UNIDIRECTIONAL);

		//And we're done
		return for_stmt_entry_block;
	}

	//This will always be a successor to the conditional statement
	add_successor(condition_block, compound_stmt_start, LINKED_DIRECTION_UNIDIRECTIONAL);

	//However if it isn't NULL, we'll need to find the end of this compound statement
	basic_block_t* compound_stmt_end = compound_stmt_start;

	//So long as we don't see the end or a return
	while(compound_stmt_end->direct_successor != NULL && compound_stmt_end->is_return_stmt == 0
		  && compound_stmt_end->is_cont_stmt == 0){
		compound_stmt_end = compound_stmt_end->direct_successor;
	}

	//Once we get here, if it is a return statement, that means that we always return
	if(compound_stmt_end->is_return_stmt == 1){
		//We should warn here
		print_cfg_message(WARNING, "For loop internal returns through every control block, will only execute once", for_stmt_node->line_number);
		(*num_warnings_ref)++;
		//There's nothing to add here, we just return
		return for_stmt_entry_block;
	}

	//Otherwise, we'll need to add the third condition into this block at the very end
	if(third_cond != NULL){
		add_statement(compound_stmt_end, third_cond);
	}

	//The successor of the end block is the conditional block
	add_successor(compound_stmt_end, condition_block, LINKED_DIRECTION_UNIDIRECTIONAL);

	//Give back the entry block
	return for_stmt_entry_block;
}


/**
 * A do-while statement is a simple control flow construct. As always, the direct successor path is the path that reliably
 * leads us down and out
 */
static basic_block_t* visit_do_while_statement(values_package_t* values){
	//Create our entry block. This in reality will be the compound statement
	basic_block_t* do_while_stmt_entry_block = basic_block_alloc();
	//The true ending block
	basic_block_t* do_while_stmt_exit_block = basic_block_alloc();

	//Grab the initial node
	generic_ast_node_t* do_while_stmt_node = values->initial_node;

	//Grab a cursor for walking the subtree
	generic_ast_node_t* ast_cursor = do_while_stmt_node->first_child;

	//If this is not a compound statement, something here is very wrong
	if(ast_cursor->CLASS != AST_NODE_CLASS_COMPOUND_STMT){
		print_cfg_message(PARSE_ERROR, "Expected compound statement in do-while, but did not find one", do_while_stmt_node->line_number);
		exit(0);
	}

	//Create and populate all needed values
	values_package_t compound_stmt_values;
	compound_stmt_values.initial_node = ast_cursor;
	compound_stmt_values.function_end_block = values->function_end_block;
	compound_stmt_values.loop_stmt_start = do_while_stmt_entry_block;

	//We go right into the compound statement here
	basic_block_t* do_while_compound_stmt_entry = visit_compound_statement(&compound_stmt_values);

	//If this is NULL, it means that we really don't have a compound statement there
	if(do_while_compound_stmt_entry == NULL){
		print_parse_message(PARSE_ERROR, "Do-while statement has empty clause, statement has no effect", do_while_stmt_node->line_number);
		(*num_warnings_ref)++;
	}

	//No matter what, this will get merged into the top statement
	do_while_stmt_entry_block = merge_blocks(do_while_stmt_entry_block, do_while_compound_stmt_entry);

	//We will drill to the bottom of the compound statement
	basic_block_t* compound_stmt_end = do_while_stmt_entry_block;

	//So long as we don't see NULL or return
	while(compound_stmt_end->direct_successor != NULL && compound_stmt_end->is_return_stmt == 0
		  && compound_stmt_end->is_cont_stmt == 0){
		compound_stmt_end = compound_stmt_end->direct_successor;
	}

	//Once we get here, if it's a return statement, everything below is unreachable
	if(compound_stmt_end->is_return_stmt == 1){
		print_cfg_message(WARNING, "Do-while returns through all internal control paths. All following code is unreachable", do_while_stmt_node->line_number);
		(*num_warnings_ref)++;
		//Just return the block here
		return do_while_stmt_entry_block;
	}

	//Otherwise, we'll need to add one more statement to the end block
	top_level_statement_node_t* do_while_cond_stmt = create_statement(ast_cursor->next_sibling);

	//Add this in to the ending block
	add_statement(compound_stmt_end, do_while_cond_stmt);

	//Now we'll make do our necessary connnections. The direct successor of this end block is the true
	//exit block
	add_successor(compound_stmt_end, do_while_stmt_exit_block, LINKED_DIRECTION_UNIDIRECTIONAL);
	//Make sure it's the direct successor
	compound_stmt_end->direct_successor = do_while_stmt_exit_block;

	//It's other successor though is the loop entry
	add_successor(compound_stmt_end, do_while_stmt_entry_block, LINKED_DIRECTION_UNIDIRECTIONAL);

	//Always return the entry block
	return do_while_stmt_entry_block;
}


/**
 * A while statement is a very simple control flow construct. As always, the "direct successor" path is the path
 * that reliably leads us down and out
 */
static basic_block_t* visit_while_statement(values_package_t* values){
	//Create our entry block
	basic_block_t* while_statement_entry_block = basic_block_alloc();
	//And create our exit block
	basic_block_t* while_statement_end_block = basic_block_alloc();

	//The direct successor to the entry block is the end block
	add_successor(while_statement_entry_block, while_statement_end_block, LINKED_DIRECTION_UNIDIRECTIONAL);
	//Just to be sure
	while_statement_entry_block->direct_successor = while_statement_end_block;

	//Grab this for convenience
	generic_ast_node_t* while_stmt_node = values->initial_node;

	//Grab a cursor to the while statement node
	generic_ast_node_t* ast_cursor = while_stmt_node->first_child;

	//The very first child will the expression that we have
	top_level_statement_node_t* expr_statement = create_statement(ast_cursor);	
	//The entry block contains our expression statement
	add_statement(while_statement_entry_block, expr_statement);

	//The very next node is a compound statement
	ast_cursor = ast_cursor->next_sibling;

	//If it isn't, we'll error out. This is really only for dev use
	if(ast_cursor->CLASS != AST_NODE_CLASS_COMPOUND_STMT){
		print_cfg_message(PARSE_ERROR, "Found node that is not a compound statement in while-loop subtree", while_stmt_node->line_number);
		exit(0);
	}

	//Create a values package to send in
	values_package_t compound_stmt_values;
	compound_stmt_values.initial_node = ast_cursor;
	compound_stmt_values.function_end_block = values->function_end_block;
	compound_stmt_values.loop_stmt_start = while_statement_entry_block;

	//Now that we know it's a compound statement, we'll let the subsidiary handle it
	basic_block_t* compound_stmt_start = visit_compound_statement(&compound_stmt_values);

	//If it's null, that means that we were given an empty while loop here
	if(compound_stmt_start == NULL){
		//For the user to see
		print_cfg_message(WARNING, "While loop has empty body, has no effect", while_stmt_node->line_number);
		(*num_warnings_ref)++;
		//We'll just return now
		return while_statement_entry_block;
	}

	//Otherwise it isn't null, so we can add it as a successor
	add_successor(while_statement_entry_block, compound_stmt_start, LINKED_DIRECTION_UNIDIRECTIONAL);

	//Let's now find the end of the compound statement
	basic_block_t* compound_stmt_end = compound_stmt_start;

	//So long as it isn't null or return
	while (compound_stmt_end->direct_successor != NULL && compound_stmt_end->is_return_stmt == 0
		   && compound_stmt_end->is_cont_stmt == 0) {
		compound_stmt_end = compound_stmt_end->direct_successor;
	}
	
	//If we make it to the end and this ending statement is a return, that means that we always return
	//Throw a warning
	if(compound_stmt_end->is_return_stmt == 1){
		//It is only an error though -- the user is allowed to do this
		print_cfg_message(WARNING, "While loop body returns in all control paths. It will only execute at most once", while_stmt_node->line_number);
		(*num_warnings_ref)++;
	}

	//No matter what, the successor to this statement is the top of the loop
	add_successor(compound_stmt_end, while_statement_entry_block, LINKED_DIRECTION_UNIDIRECTIONAL);

	//Now we're done, so
	return while_statement_entry_block;
}


/**
 * Process the if-statement subtree into the equivalent CFG form
 *
 * We make use of the "direct successor" nodes as a direct path through the if statements. We ensure that 
 * these direct paths always exist in such if-statements. 
 *
 * The sub-structure that this tree creates has only two options:
 * 	1.) Every node flows through a return, in which case nobody hits the exit block
 * 	2.) The main path flows through the end block, out of the structure
 */
static basic_block_t* visit_if_statement(values_package_t* values){
	//We always have an entry block here -- the end block is made for us
	basic_block_t* entry_block = basic_block_alloc();
	
	//Mark if we have a return statement
	u_int8_t returns_through_main_path = 0;
	//Mark if we have a continue statement
	u_int8_t continues_through_main_path = 0;
	//Mark if we return through an else path
	u_int8_t returns_through_second_path = 0;
	//Mark if we have a continue statement
	u_int8_t continues_through_second_path = 0;

	//Let's grab a cursor to walk the tree
	generic_ast_node_t* cursor = values->initial_node->first_child;

	//The very first child should be an expression, so we'll just add it in to the top
	top_level_statement_node_t* if_expr = create_statement(cursor);

	//Add it into the start block
	add_statement(entry_block, if_expr);

	//No we'll move one step beyond, the next node must be a compound statement
	cursor = cursor->next_sibling;

	//If it isn't, that's an issue
	if(cursor->CLASS != AST_NODE_CLASS_COMPOUND_STMT){
		print_cfg_message(PARSE_ERROR, "Expected compound statement in if node", cursor->line_number);
		exit(1);
	}

	//Create and send in a values package
	values_package_t if_compound_stmt_values;
	if_compound_stmt_values.initial_node = cursor;
	if_compound_stmt_values.function_end_block = values->function_end_block;
	if_compound_stmt_values.loop_stmt_start = values->loop_stmt_start;
	if_compound_stmt_values.if_stmt_end_block = values->if_stmt_end_block;
	if_compound_stmt_values.for_loop_update_clause = values->for_loop_update_clause;

	//Now that we know it is, we'll invoke the compound statement rule
	basic_block_t* if_compound_stmt_entry = visit_compound_statement(&if_compound_stmt_values);

	//If this is null, whole thing fails
	if(if_compound_stmt_entry == NULL){
		print_cfg_message(WARNING, "Empty if clause in if-statement", cursor->line_number);
		(*num_warnings_ref)++;
	} else {
		//Add the if statement node in as a direct successor
		add_successor(entry_block, if_compound_stmt_entry, LINKED_DIRECTION_UNIDIRECTIONAL);

		//Now we'll find the end of this statement
		basic_block_t* if_compound_stmt_end = if_compound_stmt_entry;

		//Once we've visited, we'll need to drill to the end of this compound statement
		while(if_compound_stmt_end->direct_successor != NULL && if_compound_stmt_end->is_return_stmt == 0
			 && if_compound_stmt_end->is_cont_stmt == 0){
			if_compound_stmt_end = if_compound_stmt_end->direct_successor;
		}

		//Once we get here, we either have an end block or a return statement. Which one we have will influence decisions
		returns_through_main_path = if_compound_stmt_end->is_return_stmt;
		//Mark this too
		continues_through_main_path = if_compound_stmt_end->is_cont_stmt;

		//If it doesn't return through the main path, the successor is the end node
		if(returns_through_main_path == 0){
			add_successor(if_compound_stmt_end, values->if_stmt_end_block, LINKED_DIRECTION_UNIDIRECTIONAL);
		}
	}

	//This is the end if we have a lone "if"
	if(cursor->next_sibling == NULL){
		//If this is the case, the end block is a direct successor
		add_successor(entry_block, values->if_stmt_end_block, LINKED_DIRECTION_UNIDIRECTIONAL);

		//If it is the parent, then we want there to be an exit here. If it's not the parent, then
		//we want the other area to handle it
		//For traversal reasons, we want this as the direct successor
		entry_block->direct_successor = values->if_stmt_end_block;

		//We can leave now
		return entry_block;
	}

	//If we make it here, we know that we have either an if or an if-else block
	//Advance the cursor
	cursor = cursor->next_sibling;
	
	//If we have a compound statement, we'll handle it as an "else" clause
	if(cursor->CLASS == AST_NODE_CLASS_COMPOUND_STMT){
		//Create the values package 
		values_package_t else_values_package;
		else_values_package.initial_node = cursor;
		else_values_package.function_end_block = values->function_end_block;
		else_values_package.loop_stmt_start = values->loop_stmt_start;
		else_values_package.if_stmt_end_block = values->if_stmt_end_block;
		else_values_package.for_loop_update_clause = values->for_loop_update_clause;

		//Visit the else statement
		basic_block_t* else_compound_stmt_entry = visit_compound_statement(&else_values_package);

		//If this is NULL, we'll send a warning and hop out -- no need for more processing here
		if(else_compound_stmt_entry == NULL){
			print_cfg_message(WARNING, "Empty else clause in if-else statement", cursor->line_number);
			(*num_warnings_ref)++;

			//The entry block's direct successor is the end statement
			entry_block->direct_successor = values->if_stmt_end_block;

			//Just get out if this happens
			return entry_block;
		}

		//Otherwise, we'll add this in as a successor
		add_successor(entry_block, else_compound_stmt_entry, LINKED_DIRECTION_UNIDIRECTIONAL);

		//Now we'll find the end of this statement
		basic_block_t* else_compound_stmt_end = else_compound_stmt_entry;

		//Once we've visited, we'll need to drill to the end of this compound statement
		while(else_compound_stmt_end->direct_successor != NULL && else_compound_stmt_end->is_return_stmt == 0
			  && else_compound_stmt_end->is_cont_stmt == 0){
			else_compound_stmt_end = else_compound_stmt_end->direct_successor;
		}

		//Once we get here, we either have an end block or a return statement. Which one we have will influence decisions
		returns_through_second_path = else_compound_stmt_end->is_return_stmt;
		//Mark this too
		continues_through_second_path = else_compound_stmt_end->is_cont_stmt;

		//If it isn't a return statement, then it's successor is the entry block
		if(returns_through_second_path == 0){
			add_successor(else_compound_stmt_end, values->if_stmt_end_block, LINKED_DIRECTION_UNIDIRECTIONAL);
		}

		/**
		 * Rules for a direct successor
		 * 	1.) If both statements are return statements, the entire thing is a return statement
		 * 	2.) If one or the other does not return, we flow through the one that does NOT return
		 * 	3.) If both don't return, we default to the "if" clause
		 */
		if(returns_through_main_path == 0 && continues_through_main_path == 0){
			//The direct successor is the main path
			entry_block->direct_successor = if_compound_stmt_entry;
		//We favor this one if not
		} else if(returns_through_second_path == 0 && continues_through_second_path == 0){
			entry_block->direct_successor = else_compound_stmt_entry;
		} else if(returns_through_main_path == 1 && returns_through_second_path == 0){
			//The direct successor is the else path
			entry_block->direct_successor = else_compound_stmt_entry;
		} else {
			//If there's anything else, we default to the first path
			entry_block->direct_successor = if_compound_stmt_entry;
		}

		//We're done here, send it back
		return entry_block;
	
	//Otherwise we have an "else if" clause 
	} else if(cursor->CLASS == AST_NODE_CLASS_IF_STMT){
		//Create the hours package
		values_package_t else_if_values_package;
		else_if_values_package.initial_node = cursor;
		else_if_values_package.if_stmt_end_block = values->if_stmt_end_block;
		else_if_values_package.loop_stmt_start = values->loop_stmt_start;
		else_if_values_package.for_loop_update_clause = values->for_loop_update_clause;
		else_if_values_package.function_end_block = values->function_end_block;

		//Visit the if statment, this one is not a parent
		basic_block_t* else_if_entry = visit_if_statement(&else_if_values_package);

		//Add this as a successor to the entrant
		add_successor(entry_block, else_if_entry, LINKED_DIRECTION_UNIDIRECTIONAL);
	
		//Once we visit this, we'll navigate to the end
		basic_block_t* else_if_end = else_if_entry;

		//We'll drill down to the end -- so long as we don't hit the end block and we don't hit a return statement
		while(else_if_end->direct_successor != NULL && else_if_end->is_return_stmt == 0
			 && else_if_end->is_cont_stmt == 0){
			//Keep track of the immediate predecessor
			else_if_end = else_if_end->direct_successor;
		}

		//Once we get here, we either have an end block or a return statement
		returns_through_second_path = else_if_end->is_return_stmt;
		//Mark this too
		continues_through_second_path = else_if_end->is_cont_stmt;

		//If it doesnt return through the second path, then the end better be the original end
		if(returns_through_second_path == 0 && else_if_end != values->if_stmt_end_block){
			printf("DOES NOT TRACK END BLOCK\n");
		}

		/**
		 * Rules for a direct successor
		 * 	1.) If both statements are return statements, the entire thing is a return statement
		 * 	2.) If one or the other does not return, we flow through the one that does NOT return
		 * 	3.) If both don't return, we default to the "if" clause
		 */
		if(returns_through_main_path == 0 && continues_through_main_path == 0){
			//The direct successor is the main path
			entry_block->direct_successor = if_compound_stmt_entry;
		} else if(continues_through_second_path == 0 && returns_through_second_path == 0){
			entry_block->direct_successor = else_if_entry;
		} else if(returns_through_main_path == 1 && returns_through_second_path == 0){
			//The direct successor is the else path
			entry_block->direct_successor = else_if_entry;
		} else {
			//If there's anything else, we default to the first path
			entry_block->direct_successor = if_compound_stmt_entry;
		}

		return entry_block;
	
	//Some weird error here
	} else {
		print_cfg_message(PARSE_ERROR, "Improper node found after if-statement", cursor->line_number);
		(*num_errors_ref)++;
		exit(0);
	}
}

/**
 * A compound statement also acts as a sort of multiplexing block. It runs through all of it's statements, calling
 * the appropriate functions and making the appropriate additions
 *
 * We make use of the "direct successor" nodes as a direct path through the compound statement, if such a path exists
 */
static basic_block_t* visit_compound_statement(values_package_t* values){
	//The global starting block
	basic_block_t* starting_block = NULL;
	//The current block
	basic_block_t* current_block = starting_block;

	//Grab the initial node
	generic_ast_node_t* compound_stmt_node = values->initial_node;

	//Grab our very first thing here
	generic_ast_node_t* ast_cursor = compound_stmt_node->first_child;
	
	//Roll through the entire subtree
	while(ast_cursor != NULL){
		//We've found a declaration statement
		if(ast_cursor->CLASS == AST_NODE_CLASS_DECL_STMT){
			values_package_t values;
			values.initial_node = ast_cursor;

			//We'll visit the block here
			basic_block_t* decl_block = visit_declaration_statement(&values);

			//If the start block is null, then this is the start block. Otherwise, we merge it in
			if(starting_block == NULL){
				starting_block = decl_block;
				current_block = decl_block;
			//Just merge with current
			} else {
				current_block = merge_blocks(current_block, decl_block); 
			}

		//If we've found an assignment expression
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_ASNMNT_EXPR){
			//Create the statement
			top_level_statement_node_t* asn_expr_stmt = create_statement(ast_cursor);

			//If the starting block is null, we'll make it
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
				//Add the statement
				add_statement(current_block, asn_expr_stmt);

			//Otherwise just add it in to current
			} else {
				add_statement(current_block, asn_expr_stmt);
			}

		//We've found a generic statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_EXPR_STMT){
			//Create the statement
			top_level_statement_node_t* expr_stmt = create_statement(ast_cursor);

			//If the starting block is null, we'll make it
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
				//Add the statement
				add_statement(current_block, expr_stmt);

			//Otherwise just add it in to current
			} else {
				add_statement(current_block, expr_stmt);
			}

		//We've found a let statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_LET_STMT){
			values_package_t values;
			values.initial_node = ast_cursor;

			//We'll visit the block here
			basic_block_t* let_block = visit_let_statement(&values);

			//If the start block is null, then this is the start block. Otherwise, we merge it in
			if(starting_block == NULL){
				starting_block = let_block;
				current_block = let_block;
			//Just merge with current
			} else {
				current_block = merge_blocks(current_block, let_block); 
			}

		//If we have a return statement -- SPECIAL CASE HERE
		} else if (ast_cursor->CLASS == AST_NODE_CLASS_RET_STMT){
			//If for whatever reason the block is null, we'll create it
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
			}

			//Whatever the current block is, this is being added to it
			//If it's a non-blank return
			if(ast_cursor->first_child != NULL){
				//Create the statement for the return
				top_level_statement_node_t* ret_expr = create_statement(ast_cursor->first_child);
				//Add it into current
				add_statement(current_block, ret_expr);
			}

			//The current block will now be marked as a return statement
			current_block->is_return_stmt = 1;

			//The current block's direct and only successor is the function exit block
			add_successor(current_block, values->function_end_block, LINKED_DIRECTION_UNIDIRECTIONAL);

			//If there is anything after this statement, it is UNREACHABLE
			if(ast_cursor->next_sibling != NULL){
				print_cfg_message(WARNING, "Unreachable code detected after return statement", ast_cursor->next_sibling->line_number);
				(*num_warnings_ref)++;
			}

			//We're completely done here
			return starting_block;

		//We've found an if-statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_IF_STMT){
			//Create the end block here for pointer reasons
			basic_block_t* if_end_block = basic_block_alloc();

			//Create the values package
			values_package_t if_stmt_values;
			if_stmt_values.initial_node = ast_cursor;
			if_stmt_values.function_end_block = values->function_end_block;
			if_stmt_values.for_loop_update_clause = values->for_loop_update_clause;
			if_stmt_values.if_stmt_end_block = if_end_block;
			if_stmt_values.loop_stmt_start = values->loop_stmt_start;

			//We'll now enter the if statement
			basic_block_t* if_stmt_start = visit_if_statement(&if_stmt_values);
			
			//Once we have the if statement start, we'll add it in as a successor
			if(starting_block == NULL){
				starting_block = if_stmt_start;
				current_block = if_stmt_start;
			} else {
				//Add this in as the current block
				current_block = merge_blocks(current_block, if_stmt_start);
			}

			//Now we'll find the end of the if statement block
			//So long as we haven't hit the end and it isn't a return statement
			while (current_block->direct_successor != NULL && current_block->is_return_stmt == 0
				  && current_block->is_cont_stmt == 0){
				current_block = current_block->direct_successor;
			}
			
			/*
			 * DEVELOPER USE MESSAGE
			 */
			if(current_block->is_return_stmt == 0 && current_block->is_cont_stmt == 0 && current_block != if_end_block){
				printf("END BLOCK REFERENCE LOST");
			}

			//If it is a return statement, that means that this if statement returns through every path. We'll leave 
			//if this is the case
			if(current_block->is_return_stmt == 1){
				//Throw a warning if this happens
				if(ast_cursor->next_sibling != NULL){
					print_cfg_message(WARNING, "Unreachable code detected after if-else block that returns through every control path", ast_cursor->line_number);
					(*num_warnings_ref)++;
				}
				//Give it back
				return starting_block;
			}

			//If it's a continue statement, that means that this if statement continues through every path. We'll leave if this
			//is the case
			if(current_block->is_cont_stmt == 1){
				//Throw a warning if this happens
				if(ast_cursor->next_sibling != NULL){
					print_cfg_message(WARNING, "Unreachable code detected after if-else block that continues through every control path", ast_cursor->line_number);
					(*num_warnings_ref)++;
				}

				//Give it back
				return starting_block;
			}
		
		//Handle a while statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_WHILE_STMT){
			//Create the values here
			values_package_t while_stmt_values;
			while_stmt_values.initial_node = ast_cursor;
			while_stmt_values.for_loop_update_clause = values->for_loop_update_clause;
			while_stmt_values.loop_stmt_start = NULL;
			while_stmt_values.if_stmt_end_block = values->if_stmt_end_block;
			while_stmt_values.function_end_block = values->function_end_block;

			//Visit the while statement
			basic_block_t* while_stmt_entry_block = visit_while_statement(&while_stmt_values);

			//We'll now add it in
			if(starting_block == NULL){
				starting_block = while_stmt_entry_block;
				current_block = starting_block;
			//We never merge while statements -- it will always be a successor
			} else {
				//Add as a successor
				add_successor(current_block, while_stmt_entry_block, LINKED_DIRECTION_UNIDIRECTIONAL);
			}

			//Now we'll drill to the end here. This is easier than before, because the direct successor to
			//the entry block of a while statement is always the end block
			current_block = while_stmt_entry_block->direct_successor;
	
		//Handle a do-while statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_DO_WHILE_STMT){
			//Create the values package
			values_package_t do_while_values;
			do_while_values.initial_node = ast_cursor;
			do_while_values.function_end_block = values->function_end_block;
			do_while_values.if_stmt_end_block = values->if_stmt_end_block;
			do_while_values.loop_stmt_start = values->loop_stmt_start;
			do_while_values.for_loop_update_clause = values->for_loop_update_clause;

			//Visit the statement
			basic_block_t* do_while_stmt_entry_block = visit_do_while_statement(&do_while_values);

			//We'll now add it in
			if(starting_block == NULL){
				starting_block = do_while_stmt_entry_block;
				current_block = starting_block;
			//We never merge do-while's, they are strictly successors
			} else {
				add_successor(current_block, do_while_stmt_entry_block, LINKED_DIRECTION_UNIDIRECTIONAL);
			}

			//Now we'll need to reach the end-point of this statement
			current_block = do_while_stmt_entry_block;

			//So long as we have successors and don't see returns
			while(current_block->direct_successor != NULL && current_block->is_return_stmt == 0
				  && current_block->is_cont_stmt == 0){
				current_block = current_block->direct_successor;
			}

			//If we make it here and we had a return statement, we need to get out
			if(current_block->is_return_stmt == 1){
				//Everything beyond this point is unreachable, no point in going on
				print_cfg_message(WARNING, "Unreachable code detected after block that returns in all control paths", ast_cursor->next_sibling->line_number);
				(*num_warnings_ref)++;
				//Get out now
				return starting_block;
			}

			//Otherwise, we're all set to go to the next iteration

		//Handle a for statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_FOR_STMT){
			//Create the values package
			values_package_t for_stmt_values;
			for_stmt_values.initial_node = ast_cursor;
			for_stmt_values.function_end_block = values->function_end_block;
			for_stmt_values.for_loop_update_clause = values->for_loop_update_clause;
			for_stmt_values.loop_stmt_start = values->loop_stmt_start;
			for_stmt_values.if_stmt_end_block = values->if_stmt_end_block;

			//First visit the statement
			basic_block_t* for_stmt_entry_block = visit_for_statement(&for_stmt_values);

			//Now we'll add it in
			if(starting_block == NULL){
				starting_block = for_stmt_entry_block;
				current_block = starting_block;
			//We ALWAYS merge for statements into the current block
			} else {
				current_block = merge_blocks(current_block, for_stmt_entry_block);
			}
			
			//Once we're here the start is in current
			while(current_block->direct_successor != NULL && current_block->is_return_stmt == 0 && current_block->is_cont_stmt == 0){
				current_block = current_block->direct_successor;
			}

			//This should never happen, so if it does we have a problem
			if(current_block->is_return_stmt == 1){
				print_parse_message(PARSE_ERROR, "It should be impossible to have a for statement that returns in all control paths", ast_cursor->line_number);
				exit(0);
			}

			//But if we don't then this is the current node

		//Handle a defer statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_DEFER_STMT){
			/**
			 * Defer statements are a special case. The are supposed to be executed 
			 * "after" the function returns. Of course, in assembly, there is no such thing.
			 * As such, deferred statements are executed immediately after a "ret" statement
			 * in the assembly
			 */
			//Add this in as a top level statement
			top_level_statement_node_t* defer_stmt = create_statement(ast_cursor);

			//We'll now add this into the stack
			push(deferred_stmts, defer_stmt);

		//Handle a continue statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_CONTINUE_STMT){
			//Let's first see if we're in a loop or not
			if(values->loop_stmt_start == NULL){
				print_cfg_message(PARSE_ERROR, "Continue statement was not found in a loop", ast_cursor->line_number);
				(*num_errors_ref)++;
				return create_and_return_err();
			}

			//This could happen where we have nothing here
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
			}

			//Mark this for later
			current_block->is_cont_stmt = 1;

			//Otherwise we are in a loop, so this means that we need to point the continue statement to
			//the loop entry block
			add_successor(current_block, values->loop_stmt_start, LINKED_DIRECTION_UNIDIRECTIONAL);

			//If we have for loops
			if(values->for_loop_update_clause != NULL){
				add_statement(current_block, values->for_loop_update_clause);
			}

			//Further, anything after this is unreachable
			if(ast_cursor->next_sibling != NULL){
				print_cfg_message(WARNING, "Unreachable code detected after continue statement", ast_cursor->next_sibling->line_number);
				(*num_warnings_ref)++;
			}

			//We're done here, so return the starting block
			return starting_block;
		}

		//Advance to the next child
		ast_cursor = ast_cursor->next_sibling;
	}

	//We always return the starting block
	//It is possible that we have a completely NULL compound statement. This returns
	//NULL in that event
	return starting_block;
}


/**
 * A function definition will always be considered a leader statement. As such, it
 * will always have it's own separate block
 */
static basic_block_t* visit_function_definition(generic_ast_node_t* function_node){
	//For error printing
	char info[1000];
	//The starting block
	basic_block_t* function_starting_block = basic_block_alloc();
	//The ending block
	basic_block_t* function_ending_block = basic_block_alloc();
	//We very clearly mark this as an ending block
	function_ending_block->is_exit_block = 1;

	//We don't care about anything until we reach the compound statement
	generic_ast_node_t* func_cursor = function_node->first_child;

	//Let's get to the compound statement
	while(func_cursor->CLASS != AST_NODE_CLASS_COMPOUND_STMT){
		func_cursor = func_cursor->next_sibling;
	}

	//Create our values package
	values_package_t compound_stmt_values;
	compound_stmt_values.initial_node = func_cursor;
	compound_stmt_values.function_end_block = function_ending_block;
	compound_stmt_values.loop_stmt_start = NULL;
	compound_stmt_values.if_stmt_end_block = NULL;
	compound_stmt_values.for_loop_update_clause = NULL;

	//Once we get here, we know that func cursor is the compound statement that we want
	basic_block_t* compound_stmt_block = visit_compound_statement(&compound_stmt_values);

	//If this compound statement is NULL(which is possible) we just add the starting and ending
	//blocks as successors
	if(compound_stmt_block == NULL){
		add_successor(function_starting_block, function_ending_block, LINKED_DIRECTION_UNIDIRECTIONAL);
		
		//We'll also throw a warning
		sprintf(info, "Function \"%s\" was given no body", ((func_def_ast_node_t*)(function_node->node))->func_record->func_name);
		print_cfg_message(WARNING, info, func_cursor->line_number);
		//One more warning
		(*num_warnings_ref)++;

	//Otherwise we merge them
	} else {
		//Once we're done with the compound statement, we will merge it into the function
		merge_blocks(function_starting_block, compound_stmt_block);
	}

	//Let's see if we actually made it all the way through and found a return
	basic_block_t* compound_stmt_cursor = function_starting_block;

	//Until we hit the end
	while(compound_stmt_cursor->direct_successor != NULL){
		compound_stmt_cursor = compound_stmt_cursor->direct_successor;
	}

	//Once we hit the end, if this isn't an exit block, we'll make it one
	if(compound_stmt_cursor->is_exit_block == 0){
		//We'll add this in as the ending block
		add_successor(compound_stmt_cursor, function_ending_block, LINKED_DIRECTION_UNIDIRECTIONAL);
		compound_stmt_cursor->direct_successor = function_ending_block;
	}

	//Once we get here, we'll now add in any deferred statements to the function ending block
	
	//So long as they aren't empty
	while(is_empty(deferred_stmts) == 0){
		//Add them in one by one
		add_statement(function_ending_block, pop(deferred_stmts));
	}

	perform_function_reachability_analysis(function_node, function_starting_block);

	//We always return the start block
	return function_starting_block;
}


/**
 * Visit a declaration statement
 */
static basic_block_t* visit_declaration_statement(values_package_t* values){
	//Create the basic block
	basic_block_t* decl_stmt_block = basic_block_alloc();

	//Create the top level statement for this
	top_level_statement_node_t* stmt = create_statement(values->initial_node);

	//Add it into the block
	add_statement(decl_stmt_block, stmt);

	//Give the block back
	return decl_stmt_block;
}


/**
 * Visit a let statement
 */
static basic_block_t* visit_let_statement(values_package_t* values){
	//Create the basic block
	basic_block_t* let_stmt_node = basic_block_alloc();

	//Create the top level statement for this
	top_level_statement_node_t* stmt = create_statement(values->initial_node);

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
		//Process a function statement
		if(ast_cursor->CLASS == AST_NODE_CLASS_FUNC_DEF){
			//Visit the function definition
			basic_block_t* function_block = visit_function_definition(ast_cursor);
			
			//If the start block is null, this becomes the start block
			if(start_block == NULL){
				start_block = function_block;
			//We could have a case where the current block is entirely empty. If this happens,
			//we'll merge the two blocks
			} else if(current_block->leader_statement == NULL) {
				current_block = merge_blocks(current_block, function_block);
			//Otherwise, we'll add this as a successor to the current block
			} else {
				add_successor(current_block, function_block, LINKED_DIRECTION_UNIDIRECTIONAL);
			}

			//We now need to find where the end of the function block is to have that as our current reference
			current_block = function_block;

			//So long as we don't see the exit statement, we keep going
			while(current_block->is_exit_block == 0){
				//Always follow the path of the direct successor
				current_block = current_block->direct_successor;
			}

			//Finally once we get down here, we have our proper current block

		//Process a let statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_LET_STMT){
			values_package_t values;
			values.initial_node = ast_cursor;

			//We'll visit the block here
			basic_block_t* let_block = visit_let_statement(&values);

			//If the start block is null, then this is the start block. Otherwise, we merge it in
			if(start_block == NULL){
				start_block = let_block;
				current_block = let_block;
			//Just merge with current
			} else {
				current_block =	merge_blocks(current_block, let_block); 
			}

		//Visit a declaration statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_DECL_STMT){
			values_package_t values;
			values.initial_node = ast_cursor;

			//We'll visit the block here
			basic_block_t* decl_block = visit_declaration_statement(&values);

			//If the start block is null, then this is the start block. Otherwise, we merge it in
			if(start_block == NULL){
				start_block = decl_block;
				current_block = decl_block;
			//Just merge with current
			} else {
				current_block = merge_blocks(current_block, decl_block); 
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

	//Create the stack here
	deferred_stmts = create_stack();

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

	//Destroy the deferred statements stack
	destroy_stack(deferred_stmts);
	
	//Give back the reference
	return cfg;
}
