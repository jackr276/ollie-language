/**
 * The implementation file for all CFG related operations
 *
 * The CFG will translate the higher level code into something referred to as 
 * "Ollie Intermediate Representation Language"(OIR). This intermediary form 
 * is a hybrid of abstract machine code and assembly. Some operations, like 
 * jump commands, are able to be deciphered at this stage, and as such we do
 * so in the OIR
 *
 *
 * SSA code notes:
 *
 * - A "live" variable, in the context of a block, is one that is defined in that block.
 *   We keep track of these here, and they are appended to the headers of the blocks for ease of understanding
 *   to the programmer
 *
*/

#include "cfg.h"
//For switch statement ordering
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "../queue/heap_queue.h"

//For magic number removal
#define TRUE 1
#define FALSE 0

//This can always be reupped dynamically
#define MAX_LIVE_VARS 5
#define MAX_ASSIGNED_VARS 5
#define MAX_DF_BLOCKS 5

//Our atomically incrementing integer
//If at any point a block has an ID of (-1), that means that it is in error and can be dealt with as such
static int32_t current_block_id = 0;
//Keep global references to the number of errors and warnings
u_int32_t* num_errors_ref;
u_int32_t* num_warnings_ref;
//Keep a stack of deferred statements for each function
heap_stack_t* deferred_stmts;
//Keep a variable symtab of temporary variables
variable_symtab_t* temp_vars;
//Keep the type symtab up and running
type_symtab_t* type_symtab;
//The CFG that we're working with
cfg_t* cfg_ref;

//A package of values that each visit function uses
typedef struct {
	//The initial node
	generic_ast_node_t* initial_node;
	//The ending block of the function
	basic_block_t* function_end_block;
	//For continue statements
	basic_block_t* loop_stmt_start;
	//For break statements
	basic_block_t* loop_stmt_end;
	//For break statements as well
	basic_block_t* switch_statement_end;
	//For congruety in if-statements
	basic_block_t* if_stmt_end_block;
	//For any time we need to do for-loop operations
	basic_block_t* for_loop_update_block;
} values_package_t;


//Define a package return struct that is used by the binary op expression code
typedef struct{
	three_addr_var_t* assignee;
	Token operator;
} expr_ret_package_t;


//We predeclare up here to avoid needing any rearrangements
static basic_block_t* visit_declaration_statement(values_package_t* values);
static basic_block_t* visit_compound_statement(values_package_t* values);
static basic_block_t* visit_let_statement(values_package_t* values);
static basic_block_t* visit_if_statement(values_package_t* values);
static basic_block_t* visit_while_statement(values_package_t* values);
static basic_block_t* visit_do_while_statement(values_package_t* values);
static basic_block_t* visit_for_statement(values_package_t* values);
static basic_block_t* visit_case_statement(values_package_t* values);
static basic_block_t* visit_default_statement(values_package_t* values);
static basic_block_t* visit_switch_statement(values_package_t* values);


//Return a three address code variable
static expr_ret_package_t emit_binary_op_expr_code(basic_block_t* basic_block, generic_ast_node_t* logical_or_expr);
static three_addr_var_t* emit_function_call_code(basic_block_t* basic_block, generic_ast_node_t* function_call_node);


//An enum for jump types
typedef enum{
	JUMP_CATEGORY_INVERSE,
	JUMP_CATEGORY_NORMAL,
} jump_category_t;


//Are we emitting the dominance frontier or not?
typedef enum{
	EMIT_DOMINANCE_FRONTIER,
	DO_NOT_EMIT_DOMINANCE_FRONTIER
} emit_dominance_frontier_selection_t;


//A type for which side we're on
typedef enum{
	SIDE_TYPE_LEFT,
	SIDE_TYPE_RIGHT,
} side_type_t;


//An enum for temp variable selection
typedef enum{
	USE_TEMP_VAR,
	PRESERVE_ORIG_VAR,
} temp_selection_t;


/**
 * This is a very simple helper function that will pack values for us. This is done to avoid repeated code
 */
static values_package_t pack_values(generic_ast_node_t* initial_node, basic_block_t* function_end_block, basic_block_t* loop_stmt_start, basic_block_t* loop_stmt_end, basic_block_t* switch_statement_end, basic_block_t* if_statement_end_block, basic_block_t* for_loop_update_block){
	//Allocate it
	values_package_t values;

	//Pack with all of our values
	values.initial_node = initial_node;
	values.function_end_block = function_end_block;
	values.loop_stmt_start = loop_stmt_start;
	values.loop_stmt_end = loop_stmt_end;
	values.switch_statement_end = switch_statement_end;
	values.if_stmt_end_block = if_statement_end_block;
	values.for_loop_update_block = for_loop_update_block;

	//And give the copy back
	return values;
}


/**
 * Select the appropriate jump type to use. We can either use
 * inverse jumps or direct jumps
 */
static jump_type_t select_appropriate_jump_stmt(Token operator, jump_category_t jump_type){
	//Let's see what we have here
	switch(operator){
		case G_THAN:
			if(jump_type == JUMP_CATEGORY_INVERSE){
				return JUMP_TYPE_JLE;
			} else {
				return JUMP_TYPE_JG;
			}
		case L_THAN:
			if(jump_type == JUMP_CATEGORY_INVERSE){
				return JUMP_TYPE_JGE;
			} else {
				return JUMP_TYPE_JL;
			}
		case L_THAN_OR_EQ:
			if(jump_type == JUMP_CATEGORY_INVERSE){
				return JUMP_TYPE_JG;
			} else {
				return JUMP_TYPE_JLE;
			}
		case G_THAN_OR_EQ:
			if(jump_type == JUMP_CATEGORY_INVERSE){
				return JUMP_TYPE_JL;
			} else {
				return JUMP_TYPE_JGE;
			}
		case D_EQUALS:
			if(jump_type == JUMP_CATEGORY_INVERSE){
				return JUMP_TYPE_JNE;
			} else {
				return JUMP_TYPE_JE;
			}
		case NOT_EQUALS:
			if(jump_type == JUMP_CATEGORY_INVERSE){
				return JUMP_TYPE_JE;
			} else {
				return JUMP_TYPE_JE;
			}


		//If we get here, it was some kind of
		//non relational operator. In this case,
		//we default to 0 = false non zero = true
		default:
			if(jump_type == JUMP_CATEGORY_INVERSE){
				return JUMP_TYPE_JZ;
			} else {
				return JUMP_TYPE_JNZ;
			}
	}
}


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
 * A simple helper function that allows us to add a live variable into the block's
 * header. It is important to note that only actual variables(not temp variables) count
 * as live
 */
static void add_live_variable(basic_block_t* basic_block, three_addr_var_t* var){
	//If we haven't even allocated one of these yet, we will now
	if(basic_block->live_variables == NULL){
		//Let's allocate it with the default size for now
		basic_block->live_variables = calloc(sizeof(three_addr_var_t*), MAX_LIVE_VARS);
		//Set this for later on
		basic_block->max_live_variable_count = MAX_LIVE_VARS;
	//Otherwise, there is another case where we may have to readjust
	} else if(basic_block->live_variable_count == basic_block->max_live_variable_count){
		//We will double here, seems to be a good strategy
		basic_block->max_live_variable_count *= 2;
		//Realloc here
		basic_block->live_variables = realloc(basic_block->live_variables, basic_block->max_live_variable_count * sizeof(three_addr_var_t*));
	}

	//Check the entire thing for duplicates
	for(u_int16_t i = 0; i < basic_block->live_variable_count; i++){
		//These addresses matching means that we have a duplicate. This
		//is perfectly fine, we just won't add it in
		if(basic_block->live_variables[i]->linked_var == var->linked_var){
			return;
		}
	}

	//Now that we've handled any case where we could run out of memory, we can do this here
	//Add the given variable in
	basic_block->live_variables[basic_block->live_variable_count] = var;
	//We've seen one more
	(basic_block->live_variable_count)++;

	//This variable has been live
	var->linked_var->has_ever_been_live = 1;
}


/**
 * A simple helper function that allows us to add an assigned-to variable into the block's
 * header. It is important to note that only actual variables(not temp variables) count
 * as live
 */
static void add_assigned_variable(basic_block_t* basic_block, three_addr_var_t* var){
	//If we haven't even allocated one of these yet, we will now
	if(basic_block->assigned_variables == NULL){
		//Let's allocate it with the default size for now
		basic_block->assigned_variables = calloc(sizeof(three_addr_var_t*), MAX_ASSIGNED_VARS);
		//Set this for later on
		basic_block->max_assigned_variable_count = MAX_ASSIGNED_VARS;
	//Otherwise, there is another case where we may have to readjust
	} else if(basic_block->assigned_variable_count == basic_block->max_assigned_variable_count){
		//We will double here, seems to be a good strategy
		basic_block->max_assigned_variable_count *= 2;
		//Realloc here
		basic_block->assigned_variables = realloc(basic_block->assigned_variables, basic_block->max_assigned_variable_count * sizeof(three_addr_var_t*));
	}

	//Check the entire thing for duplicates
	for(u_int16_t i = 0; i < basic_block->assigned_variable_count; i++){
		//These addresses matching means that we have a duplicate. This
		//is perfectly fine, we just won't add it in
		if(basic_block->assigned_variables[i]->linked_var == var->linked_var){
			return;
		}
	}

	//Now that we've handled any case where we could run out of memory, we can do this here
	//Add the given variable in
	basic_block->assigned_variables[basic_block->assigned_variable_count] = var;
	//We've seen one more
	(basic_block->assigned_variable_count)++;
}


/**
 * Print a block our for reading
*/
static void print_block_three_addr_code(basic_block_t* block, emit_dominance_frontier_selection_t print_df){
	//If this is empty, don't print anything
	//For now only, this probably won't stay
	if(block->leader_statement == NULL && block->block_type != BLOCK_TYPE_CASE){
		//return;
	}

	//Print the block's ID or the function name
	if(block->block_type == BLOCK_TYPE_FUNC_ENTRY){
		printf("%s", block->func_record->func_name);
	} else {
		printf(".L%d", block->block_id);
	}

	//Now, we will print all of the active variables that this block has
	if(block->live_variable_count != 0){
		printf("(");

		//Run through all of the live variables and print them out
		for(u_int16_t i = 0; i < block->live_variable_count; i++){
			//Print it out
			print_variable(block->live_variables[i], PRINTING_VAR_BLOCK_HEADER);

			//If it isn't the very last one, we need a comma
			if(i != block->live_variable_count - 1){
				printf(", ");
			}
		}

		//Close it out here
		printf(")");
	}

	//We always need the colon and newline
	printf(":\n");

	printf("Predecessors: {");

	for(u_int16_t i = 0; i < block->num_predecessors; i++){
		basic_block_t* predecessor = block->predecessors[i];

		//Print the block's ID or the function name
		if(predecessor->block_type == BLOCK_TYPE_FUNC_ENTRY){
			printf("%s", predecessor->func_record->func_name);
		} else {
			printf(".L%d", predecessor->block_id);
		}

		if(i != block->num_predecessors - 1){
			printf(", ");
		}
	}

	printf("}\n");

	printf("Successors: {");

	for(u_int16_t i = 0; i < block->num_successors; i++){
		basic_block_t* predecessor = block->successors[i];

		//Print the block's ID or the function name
		if(predecessor->block_type == BLOCK_TYPE_FUNC_ENTRY){
			printf("%s", predecessor->func_record->func_name);
		} else {
			printf(".L%d", predecessor->block_id);
		}

		if(i != block->num_successors - 1){
			printf(", ");
		}
	}

	printf("}\n");



	//If we have some assigned variables, we will dislay those for debugging
	if(block->assigned_variables != NULL){
		printf("Assigned: (");

		for(u_int16_t i = 0; i < block->assigned_variable_count; i++){
			print_variable(block->assigned_variables[i], PRINTING_VAR_BLOCK_HEADER);

			//If it isn't the very last one, we need a comma
			if(i != block->assigned_variable_count - 1){
				printf(", ");
			}
		}
		printf("):\n");
	}

	//Print out the dominance frontier if we're in DEBUG mode
	if(print_df == EMIT_DOMINANCE_FRONTIER){
		printf("Dominance frontier: {");

		//Run through and print them all out
		for(u_int16_t i = 0; i < block->next_df_index; i++){
			basic_block_t* printing_block = block->dominance_frontier[i];

			//Print the block's ID or the function name
			if(printing_block->block_type == BLOCK_TYPE_FUNC_ENTRY){
				printf("%s", printing_block->func_record->func_name);
			} else {
				printf(".L%d", printing_block->block_id);
			}

			//If it isn't the very last one, we need a comma
			if(i != block->next_df_index - 1){
				printf(", ");
			}
		}

		//And close it out
		printf("}\n");
	}


	//Now grab a cursor and print out every statement that we 
	//have
	three_addr_code_stmt_t* cursor = block->leader_statement;

	//So long as it isn't null
	while(cursor != NULL){
		//Hand off to printing method
		print_three_addr_code_stmt(cursor);
		//Move along to the next one
		cursor = cursor->next_statement;
	}

	//Some spacing
	printf("\n");
}


/**
 * Add a statement to the target block, following all standard linked-list protocol
 */
static void add_statement(basic_block_t* target, three_addr_code_stmt_t* statement_node){
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
	target->exit_statement->next_statement = statement_node;
	//Update the tail reference
	target->exit_statement = statement_node;
}


/**
 * Add a block to the dominance frontier of the first block
 */
static void add_block_to_dominance_frontier(basic_block_t* block, basic_block_t* df_block){
	//If it's NULL, we'll need to allocate
	if(block->dominance_frontier == NULL){
		//Allocate it
		block->dominance_frontier = calloc(MAX_DF_BLOCKS, sizeof(basic_block_t*));
		//Set this value too for later
		block->max_df_index = MAX_DF_BLOCKS;

	//If these two are equal, we need to realloc
	} else if(block->max_df_index == block->next_df_index){
		//Double it
		block->max_df_index *= 2;

		//And reallocate the whole thing
		block->dominance_frontier = realloc(block->dominance_frontier, sizeof(basic_block_t*) * block->max_df_index);
	}

	//Let's just check - is this already in there. If it is, we will not add it
	for(u_int16_t i = 0; i < block->next_df_index; i++){
		//This is not a problem at all, we just won't add it
		if(block->dominance_frontier[i] == df_block){
			return;
		}
	}

	//Now we can add into it
	block->dominance_frontier[block->next_df_index] = df_block;
	//And increment this for the next go around
	(block->next_df_index)++;
}


/**
 * Simple helper function to tell us whether or not a dominance frontier contains
 * something
 *
 * In other words, is "df_block" in the dominance frontier of "block"
 */
static u_int8_t dominance_frontier_contains(basic_block_t* block, basic_block_t* df_block){
	//Run through everything in here
	for(u_int8_t i = 0; i < block->next_df_index; i++){
		//We've found it
		if(block->dominance_frontier[i] == df_block){
			return TRUE;
		}
	}

	//Otherwise we make it here and nothing was found
	return FALSE;
}


/**
 * Does the block assign this variable? We'll do a simple linear scan to find out
 */
static u_int8_t does_block_assign_variable(basic_block_t* block, symtab_variable_record_t* variable){
	//Run through the entirety of the assigned variables in the block
	for(u_int16_t i = 0; i < block->assigned_variable_count; i++){
		//If we can find this in the block, we've got one
		if(block->assigned_variables[i]->linked_var == variable){
			return TRUE;
		}
	}

	//If we make it here, the block was empty or we found nothing
	return FALSE;
}


/**
 * Grab the immediate dominator of the block
 * A IDOM B if A SDOM B and there does not exist a node C 
 * such that C ≠ A, C ≠ B, A dom C, and C dom B
 *
 * THIS IS WRONG CURRENTLY
 */
static basic_block_t* immediate_dominator(basic_block_t* B){
	basic_block_t* A; 
	basic_block_t* C;
	u_int8_t A_is_IDOM;

	/*
	//Print the block's ID or the function name
	if(B->block_type == BLOCK_TYPE_FUNC_ENTRY){
		printf("%s: IDOM = ", B->func_record->func_name);
	} else {
		printf(".L%d: IDOM = ", B->block_id);
	}
	*/

	//For each node in B's Dominance Frontier set(we call this node A)
	//These nodes are our candidates for immediate dominator
	for(u_int16_t i = 0; i < B->next_df_index; i++){
		//By default we assume A is an IDOM
		A_is_IDOM = TRUE;

		//A is our "candidate"
		A = B->dominance_frontier[i];

		//If A == B, that means that A does NOT strictly dominate(SDOM)
		//B, so it's disqualified
		if(A == B){
			continue;
		}

		//If we get here, we know that A SDOM B
		//Now we must check, is there any "C" in the way.
		//We can tell if this is the case by checking every other
		//node in the dominance frontier of B, and seeing if that
		//node is also dominated by A
		
		//For everything in B's dominator set that IS NOT A, we need
		//to check if this is an intermediary. As in, does C get in-between
		//A and B in the dominance chain
		for(u_int16_t j = 0 ; j < B->next_df_index; j++){
			//If it's aleady B or A, we're skipping
			C = B->dominance_frontier[j];

			//If this is the case, disqualified
			if(C == B || C == A){
				continue;
			}

			//We can now see that C dominates B. The true test now is
			//if C is dominated by A. If that's the case, then we do NOT
			//have an immediate dominator in A.
			//
			//This would look like A -Doms> C -Doms> B, so A is not an immediate dominator
			if(dominance_frontier_contains(C, A) == TRUE){
				//A is disqualified, it's not an IDOM
				A_is_IDOM = FALSE;
				break;
			}
		}

		//If we found some node inbetween, we need to go onto the next one
		if(A_is_IDOM == FALSE){
			continue;
		} else {
			/*
			if(A->block_type == BLOCK_TYPE_FUNC_ENTRY){
				printf("%s\n", A->func_record->func_name);
			} else {
				printf(".L%d\n", A->block_id);
			}	
			*/

			return A;
		}
	}
 
	//printf("NONE\n");

	return NULL;
}


/**
 * Calculate the dominance frontiers of every block in the CFG
 *
 * Standard dominance frontier algorithm:
 * 	for all nodes b in the CFG
 * 		if b has less than 2 predecessors
 * 			continue
 * 		else
 * 			for all predecessors p of b
 * 				cursor = p
 * 				while cursor is not IDOM(b)
 * 					add b to cursor DF set
 * 					cursor = IDOM(cursor)
 * 	
 */
static void calculate_dominance_frontiers(cfg_t* cfg){
	//Our metastructure will come in handy here, we'll be able to run through 
	//node by node and ensure that we've gotten everything
	
	//Run through every block
	for(u_int16_t i = 0; i < cfg->created_blocks->current_index; i++){
		//Grab this from the array
		basic_block_t* block = dynamic_array_get_at(cfg->created_blocks, i);
		
		//If we have less than 2 successors,the rest of 
		//the search here is useless
		if(block->num_predecessors < 2){
			//Hop out here, there is no need to analyze further
			continue;
		}

		//A cursor for traversing our predecessors
		basic_block_t* cursor;

		//Now we run through every predecessor of the block
		for(u_int8_t i = 0; i < block->num_predecessors; i++){
			//Grab it out
			cursor = block->predecessors[i];

			//While cursor is not the immediate dominator of block
			while(cursor != immediate_dominator(block)){
				//Add block to predecessor's dominance frontier set
				add_block_to_dominance_frontier(cursor, block);
				
				//Cursor now becomes it's own immediate dominator
				cursor = immediate_dominator(cursor);
			}
		}
	}
}


/**
 * if(x0 == 0){
 * 	asn x1 := 2;
 * } else {
 * 	asn x2 := 3;
 * }
 * 
 * x3 <- phi(x1, x2)
 *
 * This means that x3 is x1 if it comes from the first branch and x2 if it comes
 * from the second branch
 *
 * To insert phi functions, we take the following approach:
 * 	For each variable
 * 		Find all basic blocks that define this variable
 * 		For each of these basic blocks that contains the variable as live
 * 			add it onto the "worklist"
 *
 * 		Then:
 * 			While worklist is not empty
 * 			Remove some node from the worklist
 * 			for each dominance frontier d
 * 				if it does not already have one of these
 * 					Insert a phi function for v at d
 *
 */
static void insert_phi_functions(cfg_t* cfg, variable_symtab_t* var_symtab){
	//We'll run through the variable symtab, finding every single variable in it
	symtab_variable_sheaf_t* sheaf_cursor;
	symtab_variable_record_t* record;

	//We need to calculate the dominance frontier of every single block before
	//we go any further
	calculate_dominance_frontiers(cfg);

	//We'll need a "worklist" here - just a dynamic array 
	dynamic_array_t* worklist = dynamic_array_alloc();
	dynamic_array_t* has_been_on_worklist = dynamic_array_alloc();

	//------------------------------------------
	// FIRST STEP: FOR EACH variable we have
	//------------------------------------------
	//Run through all of the sheafs
	for	(u_int16_t i = 0; i < var_symtab->num_sheafs; i++){
		sheaf_cursor = var_symtab->sheafs[i];

		//Now we'll free all non-null records
		for(u_int16_t j = 0; j < KEYSPACE; j++){
			//Grab the record
			record = sheaf_cursor->records[j];

			//We could have chaining here, so run through just in case
			while(record != NULL){
				//----------------------------------
				// SECOND STEP: For each block that 
				// defines(assigns) said variable
				//----------------------------------

				/*
				//Just run through the entire thing
				while(block_cursor != NULL){
					//Does this block define(assign) our variable?
					if(does_block_assign_variable(block_cursor, record) == TRUE){
						//Then we add this block onto the "worklist"
						dynamic_array_add(worklist, block_cursor);
						dynamic_array_add(has_been_on_worklist, block_cursor);
					}

					//Iterate over to the next one
					block_cursor = block_cursor->next_created;
				}
				*/

				//Advance to the next record in the chain
				record = record->next;
			}
		}
	}

	//We're done with it, so now we can free
	dynamic_array_dealloc(worklist);
	dynamic_array_dealloc(has_been_on_worklist);
}


/**
 * Emit a statement that fits the definition of a lea statement. This usually takes the
 * form of address computations
 */
static three_addr_var_t* emit_lea_stmt(basic_block_t* basic_block, three_addr_var_t* base_addr, three_addr_var_t* offset, generic_type_t* base_type){
	//We need a new temp var for the assignee
	three_addr_var_t* assignee = emit_temp_var(base_type);

	//If the base addr is not temporary, this counts as a read
	if(base_addr->is_temporary == FALSE){
		add_live_variable(basic_block, base_addr);
	}

	//Now we leverage the helper to emit this
	three_addr_code_stmt_t* stmt = emit_lea_stmt_three_addr_code(assignee, base_addr, offset, base_type->type_size);

	//Now add the statement into the block
	add_statement(basic_block, stmt);

	//And give back the assignee
	return assignee;
}


/**
 * Directly emit the assembly nop instruction
 */
static void emit_idle_stmt(basic_block_t* basic_block){
	//Use the helper
	three_addr_code_stmt_t* idle_stmt = emit_idle_statement_three_addr_code();
	
	//Add it into the block
	add_statement(basic_block, idle_stmt);

	//And that's all
}


/**
 * Directly emit the assembly code for an inlined statement. Users who write assembly inline
 * want it directly inserted in order, nothing more, nothing less
 */
static void emit_asm_inline_stmt(basic_block_t* basic_block, generic_ast_node_t* asm_inline_node){
	//First we allocate the whole thing
	three_addr_code_stmt_t* asm_inline_stmt = emit_asm_statement_three_addr_code(asm_inline_node->node); 
	
	//Once done we add it into the block
	add_statement(basic_block, asm_inline_stmt);
	
	//And that's all
}


/**
 * Emit the abstract machine code for a return statement
 */
static void emit_ret_stmt(basic_block_t* basic_block, generic_ast_node_t* ret_node){
	//For holding our temporary return variable
	expr_ret_package_t package;

	//Is null by default
	package.assignee = NULL;

	//If the ret node's first child is not null, we'll let the expression rule
	//handle it
	if(ret_node->first_child != NULL){
		package = emit_binary_op_expr_code(basic_block, ret_node->first_child);

	}

	//We'll use the ret stmt feature here
	three_addr_code_stmt_t* ret_stmt = emit_ret_stmt_three_addr_code(package.assignee);

	//Once it's been emitted, we'll add it in as a statement
	add_statement(basic_block, ret_stmt);
}


/**
 * Emit the abstract machine code for a label statement
 */
static void emit_label_stmt_code(basic_block_t* basic_block, generic_ast_node_t* label_node){
	//Emit the appropriate variable
	three_addr_var_t* label_var = emit_var(label_node->variable, 0, 1);

	//This is a special case here -- these don't really count as variables
	//in the way that most do. As such, we will not add it in as live

	//We'll just use the helper to emit this
	three_addr_code_stmt_t* stmt = emit_label_stmt_three_addr_code(label_var);

	//Add this statement into the block
	add_statement(basic_block, stmt);
}


/**
 * Emit the abstract machine code for a jump statement
 */
static void emit_jump_stmt_code(basic_block_t* basic_block, generic_ast_node_t* jump_statement){
	//Emit the appropriate variable
	three_addr_var_t* label_var = emit_var(jump_statement->variable, 0, 1);

	//This is a special case here -- these don't really count as variables
	//in the way that most do. As such, we will not add it in as live
	
	//We'll just use the helper to do this
	three_addr_code_stmt_t* stmt = emit_dir_jmp_stmt_three_addr_code(label_var);

	//Add this statement into the block
	add_statement(basic_block, stmt);
}


/**
 * Emit a jump statement jumping to the destination block, using the jump type that we
 * provide
 */
static void emit_jmp_stmt(basic_block_t* basic_block, basic_block_t* dest_block, jump_type_t type){
	//Use the helper function to emit the statement
	three_addr_code_stmt_t* stmt = emit_jmp_stmt_three_addr_code(dest_block->block_id, type);

	//Add this into the first block
	add_statement(basic_block, stmt);
}


/**
 * Emit the abstract machine code for a constant to variable assignment. 
 */
static three_addr_var_t* emit_constant_code(basic_block_t* basic_block, generic_ast_node_t* constant_node){
	//We'll use the constant var feature here
	three_addr_code_stmt_t* const_var = emit_assn_const_stmt_three_addr_code(emit_temp_var(constant_node->inferred_type), emit_constant(constant_node));
	
	//Add this into the basic block
	add_statement(basic_block, const_var);

	//Now give back the assignee variable
	return const_var->assignee;
}


/**
 * Emit the abstract machine code for a constant to variable assignment. 
 *
 * No live variables can be generated from here, because everything that we use is a temp
 */
static three_addr_var_t* emit_constant_code_direct(basic_block_t* basic_block, three_addr_const_t* constant, generic_type_t* inferred_type){
	//We'll use the constant var feature here
	three_addr_code_stmt_t* const_var = emit_assn_const_stmt_three_addr_code(emit_temp_var(inferred_type), constant);
	
	//Add this into the basic block
	add_statement(basic_block, const_var);

	//Now give back the assignee variable
	return const_var->assignee;
}


/**
 * Emit the identifier machine code. This function is to be used in the instance where we want
 * to move an identifier to some temporary location
 */
static three_addr_var_t* emit_ident_expr_code(basic_block_t* basic_block, generic_ast_node_t* ident_node, temp_selection_t use_temp, side_type_t side){
	//Just give back the name
	if(use_temp == PRESERVE_ORIG_VAR || side == SIDE_TYPE_RIGHT){
		//If it's an enum constant
		if(ident_node->variable->is_enumeration_member == TRUE){
			return emit_constant_code_direct(basic_block, emit_int_constant_direct(ident_node->variable->enum_member_value), lookup_type(type_symtab, "u32")->type);
		}

		//Emit the variable
		three_addr_var_t* var = emit_var(ident_node->variable, use_temp, 0);

		//This variable now is live
		ident_node->variable->has_ever_been_live = TRUE;
		
		//Add it as a live variable to the block, because we've used it
		add_live_variable(basic_block, var);

		//This variable has been assigned to, so we'll add that too
		if(side == SIDE_TYPE_LEFT){
			//We only do this if it's the LHS
			add_assigned_variable(basic_block, var);
		}

		//Give it back
		return var;

	//We will do an on-the-fly conversion to a number
	} else if(ident_node->inferred_type->type_class == TYPE_CLASS_ENUMERATED) {
		//Look up the type
		symtab_type_record_t* type_record = lookup_type(type_symtab, "u32");
		generic_type_t* type = type_record->type;
		//Just create a constant here with the enum
		return emit_constant_code_direct(basic_block, emit_int_constant_direct(ident_node->variable->enum_member_value), type);

	} else {
		//First we'll create the non-temp var here
		three_addr_var_t* non_temp_var = emit_var(ident_node->variable, 0, 0);

		//THis has been live
		ident_node->variable->has_ever_been_live = TRUE;

		//Add it into the block
		add_live_variable(basic_block, non_temp_var);

		//This variable has been assigned to, so we'll add that too
		add_assigned_variable(basic_block, non_temp_var);

		//Let's first create the assignment statement
		three_addr_code_stmt_t* temp_assnment = emit_assn_stmt_three_addr_code(emit_temp_var(ident_node->inferred_type), non_temp_var);

		//Add the statement in
		add_statement(basic_block, temp_assnment);

		//Just give back the temp var here
		return temp_assnment->assignee;
	}
}


/**
 * Emit increment three adress code
 */
static three_addr_var_t* emit_inc_code(basic_block_t* basic_block, three_addr_var_t* incrementee){
	//Create the code
	three_addr_code_stmt_t* inc_code = emit_inc_stmt_three_addr_code(incrementee);

	//This will count as live if we read from it
	if(incrementee->is_temporary == FALSE){
		add_live_variable(basic_block, incrementee);
	}

	//Add it into the block
	add_statement(basic_block, inc_code);

	//Return the incrementee
	return incrementee;
}


/**
 * Emit decrement three address code
 */
static three_addr_var_t* emit_dec_code(basic_block_t* basic_block, three_addr_var_t* decrementee){
	//Create the code
	three_addr_code_stmt_t* dec_code = emit_dec_stmt_three_addr_code(decrementee);

	//This will count as live if we read from it
	if(decrementee->is_temporary == FALSE){
		add_live_variable(basic_block, decrementee);
	}

	//Add it into the block
	add_statement(basic_block, dec_code);

	//Return the incrementee
	return decrementee;
}


/**
 * Emit memory indirection three address code
 */
static three_addr_var_t* emit_mem_code(basic_block_t* basic_block, three_addr_var_t* assignee){
	//No actual code here, we are just accessing this guy's memory
	//Create a new variable with an indirection level
	three_addr_var_t* indirect_var = emit_var_copy(assignee);

	//This will count as live if we read from it
	if(indirect_var->is_temporary == FALSE){
		add_live_variable(basic_block, indirect_var);
	}

	//Increment the indirection
	indirect_var->indirection_level++;
	//Temp or not same deal
	indirect_var->is_temporary = assignee->is_temporary;

	//And get out
	return indirect_var;
}


/**
 * Emit a bitwise not statement 
 */
static three_addr_var_t* emit_bitwise_not_expr_code(basic_block_t* basic_block, three_addr_var_t* var, temp_selection_t use_temp){
	//First we'll create it here
	three_addr_code_stmt_t* not_stmt = emit_not_stmt_three_addr_code(var);

	//This is also a case where the variable is read from, so it counts as live
	if(var->is_temporary == FALSE){
		add_live_variable(basic_block, var);
	}

	//Now if we need to use a temp, we'll make one here
	if(use_temp == USE_TEMP_VAR){
		//Emit a temp var
		three_addr_var_t* temp = emit_temp_var(var->type);

		//The assignee is the temp
		not_stmt->assignee = temp;
	}
	//Otherwise nothing else needed here

	//Add this into the block
	add_statement(basic_block, not_stmt);

	//Give back the assignee
	return not_stmt->assignee;
}


/**
 * Emit a binary operation statement with a constant built in
 */
static three_addr_var_t* emit_binary_op_with_constant_code(basic_block_t* basic_block, three_addr_var_t* assignee, three_addr_var_t* op1, Token op, three_addr_const_t* constant){
	//If these variables are not temporary, then we have read from them
	if(assignee->is_temporary == FALSE){
		add_live_variable(basic_block, assignee);
	}

	//Add this one in too
	if(op1->is_temporary == FALSE){
		add_live_variable(basic_block, assignee);
	}

	//First let's create it
	three_addr_code_stmt_t* stmt = emit_bin_op_with_const_three_addr_code(assignee, op1, op, constant);

	//Then we'll add it into the block
	add_statement(basic_block, stmt);

	//Finally we'll return it
	return assignee;
}


/**
 * Emit a bitwise negation statement
 */
static three_addr_var_t* emit_neg_stmt_code(basic_block_t* basic_block, three_addr_var_t* negated, temp_selection_t use_temp){
	three_addr_var_t* var;

	//We make our temp selection based on this
	if(use_temp == USE_TEMP_VAR){
		var = emit_temp_var(negated->type);
	} else {
		var = negated;
	}

	//If this isn't a temp var, we'll add it in as live
	if(negated->is_temporary == FALSE){
		add_live_variable(basic_block, negated);
	}

	//Now let's create it
	three_addr_code_stmt_t* stmt = emit_neg_stmt_three_addr_code(var, negated);
	
	//Add it into the block
	add_statement(basic_block, stmt);

	//We always return the assignee
	return var;
}


/**
 * Emit a logical negation statement
 */
static three_addr_var_t* emit_logical_neg_stmt_code(basic_block_t* basic_block, three_addr_var_t* negated){
	//We ALWAYS use a temp var here
	three_addr_code_stmt_t* stmt = emit_logical_not_stmt_three_addr_code(emit_temp_var(negated->type), negated);
	
	//If negated isn't temp, it also counts as a read
	if(negated->is_temporary == FALSE){
		add_live_variable(basic_block, negated);
	}

	//From here, we'll add the statement in
	add_statement(basic_block, stmt);

	//We'll give back the assignee temp variable
	return stmt->assignee;
}


/**
 * Emit the abstract machine code for a primary expression. Remember that a primary
 * expression could be an identifier, a constant, a function call, or a nested expression
 * tree
 */
static three_addr_var_t* emit_primary_expr_code(basic_block_t* basic_block, generic_ast_node_t* primary_parent, temp_selection_t use_temp, side_type_t side){
	if(primary_parent->CLASS == AST_NODE_CLASS_IDENTIFIER){
		//If it's an identifier, emit this and leave
		 return emit_ident_expr_code(basic_block, primary_parent, use_temp, side);
	//If it's a constant, emit this and leave
	} else if(primary_parent->CLASS == AST_NODE_CLASS_CONSTANT){
		return emit_constant_code(basic_block, primary_parent);
	} else if(primary_parent->CLASS == AST_NODE_CLASS_BINARY_EXPR){
		return emit_binary_op_expr_code(basic_block, primary_parent).assignee;
	//Handle a function call
	} else if(primary_parent->CLASS == AST_NODE_CLASS_FUNCTION_CALL){
		return emit_function_call_code(basic_block, primary_parent);
	} else {
		//Throw some error here, really this should never occur
		print_parse_message(PARSE_ERROR, "Did not find identifier, constant, expression or function call in primary expression", primary_parent->line_number);
		(*num_errors_ref)++;
		exit(0);
	}
}


/**
 * Emit the abstract machine code for the various different kinds of postfix expressions
 * that we could see(array access, decrement/increment, etc)
 */
static three_addr_var_t* emit_postfix_expr_code(basic_block_t* basic_block, generic_ast_node_t* postfix_parent, temp_selection_t use_temp, side_type_t side){
	//The very first child should be some kind of prefix expression
	generic_ast_node_t* cursor = postfix_parent->first_child;

	//In theory the first child should always be some kind of postfix expression. As such, we'll first call that helper
	//to get what we need
	three_addr_var_t* current_var = emit_primary_expr_code(basic_block, cursor, use_temp, side);

	//Let's now advance to the next child. We will keep advancing until we hit the very end,
	//or we hit some kind of terminal node
	cursor = cursor->next_sibling;
	while(cursor != NULL){
		/**
		 * There are several things that could happen in a postfix expression, two of them
		 * being the post increment and postdecrement. These are unique operations in that they occur after
		 * user. So for example: my_arr[i++] := 23; would have i increment after it was used as the index.
		 * To achieve this, the variable that we return will always be a temp variable containing the 
		 * pre-operation result of i. Then i will be incremented itself. The same goes for decrementing
		 *
		 */

		//We have post-increment or decrement here. This
		if(cursor->CLASS == AST_NODE_CLASS_UNARY_OPERATOR){
			//We either have a postincrement or postdecrement here. Either way,
			//we'll need to first save the current variable
			//Declare a temporary here
			three_addr_var_t* temp_var = emit_temp_var(current_var->type);
			
			//Save the current variable into this new temporary one. This is what allows
			//us to achieve the "Increment/decrement after use" effect
			three_addr_code_stmt_t* assignment =  emit_assn_stmt_three_addr_code(temp_var, current_var);

			//This counts as a read relationship, so we'll need to add it in as live
			if(current_var->is_temporary == FALSE){
				add_live_variable(basic_block, current_var);
			}

			//Ensure that we add this into the block
			add_statement(basic_block, assignment);

			//We'll now perform the actual operation
			if(cursor->unary_operator == PLUSPLUS){
				//Use the helper for this
				emit_inc_code(basic_block, current_var);
			//Otherwise we know that it has to be minusminus
			} else {
				//Use the helper here as well
				emit_dec_code(basic_block, current_var);
			}

			//We are officially done. What we actually give back here
			//is not the current var, but whatever temp was assigned to it
			return temp_var;
		//Otherwise we have some kind of array access here
		} else if(cursor->CLASS == AST_NODE_CLASS_ARRAY_ACCESSOR){
			//Let's find the logical or expression that we have here. It should
			//be the first child of this node
			three_addr_var_t* offset = emit_binary_op_expr_code(basic_block, cursor->first_child).assignee;

			//Now we'll need the current variable type to know the base address and the size
			//This should be guaranteed to be a pointer or array. Current var is either
			//an array or pointer type. We'll need to extract the base type here

			generic_type_t* base_type;

			//We can either have an array or pointer, extract either or accordingly
			if(current_var->type->type_class == TYPE_CLASS_ARRAY){
				base_type = current_var->type->array_type->member_type;
			} else {
				base_type = current_var->type->pointer_type->points_to;
			}

			/**
			 * The formula for array subscript is: base_address + type_size * subscript
			 *
			 * This can be done using a lea instruction, so we will emit that directly
			 */
			three_addr_var_t* address = emit_lea_stmt(basic_block, current_var, offset, base_type);

			//Now to actually access this address, we need to emit the memory access
			current_var = emit_mem_code(basic_block, address);

			//Do we need to do more memoery work? We can tell if the array accessor node is next
			if(cursor->next_sibling != NULL && cursor->next_sibling->CLASS == AST_NODE_CLASS_ARRAY_ACCESSOR){
				//We will perform the deref here, as we can't do it in the lea 
				three_addr_code_stmt_t* deref_stmt = emit_assn_stmt_three_addr_code(emit_temp_var(current_var->type), current_var);
				add_statement(basic_block, deref_stmt);

				//Update the current bar too
				current_var = deref_stmt->assignee;
			}
	
		//Fail out here, not yet implemented
		} else if(cursor->CLASS == AST_NODE_CLASS_CONSTRUCT_ACCESSOR){
			print_parse_message(PARSE_ERROR, "THIS HAS NOT BEEN IMPLEMENTED", cursor->line_number);
			exit(0);
		//We have hit something unknown here
		} else {
			print_parse_message(PARSE_ERROR, "UNKOWN EXPRESSION TYPE DETECTED", cursor->line_number);
			exit(0);
		}

		//Advance the pointer up here
		cursor = cursor->next_sibling;
	}

	return current_var;
}

/**
 * Emit the abstract machine code for a unary expression
 * Unary expressions come in the following forms:
 * 	
 * 	<postfix-expression> | <unary-operator> <cast-expression> | typesize(<type-specifier>) | sizeof(<logical-or-expression>) 
 */
static three_addr_var_t* emit_unary_expr_code(basic_block_t* basic_block, generic_ast_node_t* unary_expr_parent, temp_selection_t use_temp, side_type_t side){
	//The last two instances return a constant node. If that's the case, we'll just emit a constant
	//node here
	if(unary_expr_parent->CLASS == AST_NODE_CLASS_CONSTANT){
		//Let the helper deal with it
		return emit_constant_code(basic_block, unary_expr_parent);
	}

	//If it isn't a constant, then this node should have children
	generic_ast_node_t* first_child = unary_expr_parent->first_child;

	//This could be a postfix expression
	if(first_child->CLASS == AST_NODE_CLASS_POSTFIX_EXPR){
		//We will leverage the helper here
		return emit_postfix_expr_code(basic_block, first_child, use_temp, side);
		
	//If we have some kind of unary operator here
	} else if(first_child->CLASS == AST_NODE_CLASS_UNARY_OPERATOR){
		//Grab this internal reference for ease
		generic_ast_node_t* unary_operator = first_child;

		//No matter what here, the next sibling will also be some kind of unary expression.
		//We'll need to handle that first before going forward
		three_addr_var_t* assignee = emit_unary_expr_code(basic_block, first_child->next_sibling, use_temp, side);

		//What kind of unary operator do we have?
		//Handle plus plus case
		if(unary_operator->unary_operator == PLUSPLUS){
			//What if the assignee is a complex type(pointer, array, etc)
			if(assignee->type->type_class != TYPE_CLASS_BASIC){
				//Emit the constant size
				three_addr_const_t* constant = emit_int_constant_direct(assignee->type->type_size);
				//Now we'll make the statement
				return emit_binary_op_with_constant_code(basic_block, assignee, assignee, PLUS, constant);
			} else {
				//We really just have an "inc" instruction here
				return emit_inc_code(basic_block, assignee);
			}
		} else if(unary_operator->unary_operator == MINUSMINUS){
			//What if the assignee is a complex type(pointer, array, etc)
			if(assignee->type->type_class != TYPE_CLASS_BASIC){
				//Emit the constant size
				three_addr_const_t* constant = emit_int_constant_direct(assignee->type->type_size);
				//Now we'll make the statement
				return emit_binary_op_with_constant_code(basic_block, assignee, assignee, MINUS, constant);
			} else {
				//We really just have an "inc" instruction here
				return emit_dec_code(basic_block, assignee);
			}
		} else if (unary_operator->unary_operator == STAR){
			//Memory address
			return emit_mem_code(basic_block, assignee);
		} else if (unary_operator->unary_operator == B_NOT){
			//Bitwise not -- this does need to be assigned from
			return emit_bitwise_not_expr_code(basic_block, assignee, use_temp);

		/**
		 * Uses strategy of:
		 * 	test rdx, rdx
		 * 	sete rdx
		 * 	mov rdx, rdx //this specifically exists to set flags
		 * for implementation
		 */
		} else if(unary_operator->unary_operator == L_NOT){
			return emit_logical_neg_stmt_code(basic_block, assignee);

		/**
		 * x = -a;
		 * t <- a;
		 * negl t;
		 * x <- t;
		 *
		 * Uses strategy of: negl rdx
		 */
		} else if(unary_operator->unary_operator == MINUS){
			//We will emit the negation code here
			return emit_neg_stmt_code(basic_block, assignee, use_temp);
		}

		//FOR NOW ONLY
		return assignee;
	} else {
		return emit_primary_expr_code(basic_block, first_child, use_temp, side);
	}
}


/**
 * Emit the abstract machine code needed for a binary expression. The lowest possible
 * thing that we could have here is a unary expression. If we have that, we just emit the
 * unary expression
 *
 * We need to convert these into straight line binary expression code(two operands, one operator) each.
 * For each binary expression, we compute
 *
 */
static expr_ret_package_t emit_binary_op_expr_code(basic_block_t* basic_block, generic_ast_node_t* logical_or_expr){
	//The return package here
	expr_ret_package_t package;
	//Operator is blank by default
	package.operator = BLANK;

	//Is the cursor a unary expression? If so just emit that. This is our base case 
	//essentially
	if(logical_or_expr->CLASS == AST_NODE_CLASS_UNARY_EXPR){
		//Return the temporary character from here
		package.assignee = emit_unary_expr_code(basic_block, logical_or_expr, USE_TEMP_VAR, SIDE_TYPE_RIGHT);
		return package;
	}

	//Otherwise we actually have a binary operation of some kind
	//Grab a cursor
	generic_ast_node_t* cursor = logical_or_expr->first_child;
	
	//Emit the binary expression on the left first
	expr_ret_package_t left_hand_temp = emit_binary_op_expr_code(basic_block, cursor);

	//Advance up here
	cursor = cursor->next_sibling;

	//Then grab the right hand temp
	expr_ret_package_t right_hand_temp = emit_binary_op_expr_code(basic_block, cursor);

	//Let's see what binary operator that we have
	Token binary_operator = logical_or_expr->binary_operator;
	//Store this binary operator
	package.operator = binary_operator;

	//Emit the binary operator expression using our helper
	three_addr_code_stmt_t* bin_op_stmt = emit_bin_op_three_addr_code(emit_temp_var(logical_or_expr->inferred_type), left_hand_temp.assignee, binary_operator, right_hand_temp.assignee);

	//If these are not temporary, they also count as live
	if(left_hand_temp.assignee->is_temporary == FALSE){
		add_live_variable(basic_block, left_hand_temp.assignee);
	}

	if(right_hand_temp.assignee->is_temporary == FALSE){
		add_live_variable(basic_block, right_hand_temp.assignee);
	}

	//Add this statement to the block
	add_statement(basic_block, bin_op_stmt);

	//Store the temporary var as the assignee
	package.assignee = bin_op_stmt->assignee;
	
	//Return the temp variable that we assigned to
	return package;
}


/**
 * Emit abstract machine code for an expression. This is a top level statement.
 * These statements almost always involve some kind of assignment "<-" and generate temporary
 * variables
 */
static expr_ret_package_t emit_expr_code(basic_block_t* basic_block, generic_ast_node_t* expr_node){
	//A cursor for tree traversal
	generic_ast_node_t* cursor;
	symtab_variable_record_t* assigned_var;
	//The return package
	expr_ret_package_t ret_package;
	//By default, last seen op is blank
	ret_package.operator = BLANK;

	//If we have a declare statement,
	if(expr_node->CLASS == AST_NODE_CLASS_DECL_STMT){
		//What kind of declarative statement do we have here?
		//TODO


	//Convert our let statement into abstract machine code 
	} else if(expr_node->CLASS == AST_NODE_CLASS_LET_STMT){
		//Let's grab the associated variable record here
		symtab_variable_record_t* var =  expr_node->variable;

		//Create the variable associated with this
	 	three_addr_var_t* left_hand_var = emit_var(var, 1, 0);

		//Mark that this has been live
		var->has_ever_been_live = TRUE;

		//Add it in as a live variable
		add_live_variable(basic_block, left_hand_var);

		//This has been assigned to
		add_assigned_variable(basic_block, left_hand_var);

		//Now emit whatever binary expression code that we have
		expr_ret_package_t package = emit_binary_op_expr_code(basic_block, expr_node->first_child);

		//The actual statement is the assignment of right to left
		three_addr_code_stmt_t* assn_stmt = emit_assn_stmt_three_addr_code(left_hand_var, package.assignee);

		//Finally we'll add this into the overall block
		add_statement(basic_block, assn_stmt);
	
	//An assignment statement
	} else if(expr_node->CLASS == AST_NODE_CLASS_ASNMNT_EXPR) {
		//In our tree, an assignment statement decays into a unary expression
		//on the left and a binary op expr on the right
		
		//This should always be a unary expression
		cursor = expr_node->first_child;

		//If it is not one, we fail out
		if(cursor->CLASS != AST_NODE_CLASS_UNARY_EXPR){
			print_parse_message(PARSE_ERROR, "Expected unary expression as first child to assignment expression", cursor->line_number);
			exit(0);
		}

		//Emit the left hand unary expression
		three_addr_var_t* left_hand_var = emit_unary_expr_code(basic_block, cursor, PRESERVE_ORIG_VAR, SIDE_TYPE_LEFT);

		//Advance the cursor up
		cursor = cursor->next_sibling;

		//Now emit the right hand expression
		expr_ret_package_t package = emit_binary_op_expr_code(basic_block, cursor);

		//Finally we'll construct the whole thing
		three_addr_code_stmt_t* stmt = emit_assn_stmt_three_addr_code(left_hand_var, package.assignee);
		
		//Now add this statement in here
		add_statement(basic_block, stmt);

		//Now pack the return value here
		ret_package.operator = BLANK;
		ret_package.assignee = left_hand_var;
		
		//Return what we had
		return ret_package;

	} else if(expr_node->CLASS == AST_NODE_CLASS_BINARY_EXPR){
		//Emit the binary expression node
		return emit_binary_op_expr_code(basic_block, expr_node);
	} else if(expr_node->CLASS == AST_NODE_CLASS_FUNCTION_CALL){
		//Emit the function call statement
		ret_package.assignee = emit_function_call_code(basic_block, expr_node);
		return ret_package;
	} else if(expr_node->CLASS == AST_NODE_CLASS_UNARY_EXPR){
		//Let this rule handle it
		ret_package.assignee = emit_unary_expr_code(basic_block, expr_node, PRESERVE_ORIG_VAR, SIDE_TYPE_RIGHT);
		return ret_package;
	} else {
		return ret_package;

	}

	return ret_package;
}


/**
 * Emit a function call node. In this iteration of a function call, we will still be parameterized, so the actual 
 * node will record what needs to be passed into the function
 */
static three_addr_var_t* emit_function_call_code(basic_block_t* basic_block, generic_ast_node_t* function_call_node){
	//Grab this out first
	symtab_function_record_t* func_record = function_call_node->func_record;

	//May be NULL or not based on what we have as the return type
	three_addr_var_t* assignee = NULL;

	//If the function does not return void, we will be assigning it to a temporary variable
	if(strcmp(func_record->return_type->type_name, "void") != 0){
		//This means that we have a temp variable here
		assignee = emit_temp_var(func_record->return_type);
	}

	//Once we get here we can create the function statement
	three_addr_code_stmt_t* func_call_stmt = emit_func_call_three_addr_code(func_record, assignee);

	//Let's grab a param cursor for ourselves
	generic_ast_node_t* param_cursor = function_call_node->first_child;

	//The current param of the index
	u_int8_t current_func_param_idx = 0;

	//So long as this isn't NULL
	while(param_cursor != NULL){
		//Emit whatever we have here into the basic block
		expr_ret_package_t package = emit_expr_code(basic_block, param_cursor);
		
		//The temporary variable that we get will be our parameter
		func_call_stmt->params[current_func_param_idx] = package.assignee;
		current_func_param_idx++;

		//And move up
		param_cursor = param_cursor->next_sibling;
	}

	//Once we make it here, we should have all of the params stored in temp vars
	//We can now add the function call statement in
	add_statement(basic_block, func_call_stmt);

	//Give back what we assigned to
	return assignee;
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
 * Allocate a basic block using calloc. NO data assignment
 * happens in this function
*/
static basic_block_t* basic_block_alloc(){
	//Allocate the block
	basic_block_t* created = calloc(1, sizeof(basic_block_t));

	//Put the block ID in
	created->block_id = increment_and_get();

	//Our sane defaults here - normal termination and normal type
	created->block_terminal_type = BLOCK_TERM_TYPE_NORMAL;
	//By default we're normal here
	created->block_type = BLOCK_TYPE_NORMAL;

	//Usually these are, in special cases they aren't
	created->good_to_merge = TRUE;

	//Add this into the dynamic array
	dynamic_array_add(cfg_ref->created_blocks, created);

	return created;
}


/**
 * Print out the whole program in order. This is done using an
 * iterative DFS
 */
static void emit_blocks_dfs(cfg_t* cfg, emit_dominance_frontier_selection_t print_df){
	//We'll need a stack for our DFS
	heap_stack_t* stack = heap_stack_alloc();

	//The idea here is very simple. If we can walk the function tree and every control path leads 
	//to a return statement, we return null from every control path
	
	//We'll need a cursor to walk the tree
	basic_block_t* block_cursor;

	//Push the source node
	push(stack, cfg->root);

	//So long as the stack is not empty
	while(is_empty(stack) == HEAP_STACK_NOT_EMPTY){
		//Grab the current one off of the stack
		block_cursor = pop(stack);

		//If this wasn't visited
		if(block_cursor->visited != 2){
			//Mark this one as seen
			block_cursor->visited = 2;

			print_block_three_addr_code(block_cursor, print_df);
		}

		//We'll now add in all of the childen
		for(int8_t i = block_cursor->num_successors-1; i > -1; i--){
			//If we haven't seen it yet, add it to the list
			if(block_cursor->successors[i]->visited != 2){
				push(stack, block_cursor->successors[i]);
			}
		}
	}

	//Deallocate our stack once done
	heap_stack_dealloc(stack);
}


/**
 * Print out the whole program in order. Done using an iterative
 * bfs
 */
static void emit_blocks_bfs(cfg_t* cfg, emit_dominance_frontier_selection_t print_df){
	//We'll need a queue for our BFS
	heap_queue_t* queue = heap_queue_alloc();
	//To be enqueued dynamic array
	dynamic_array_t* to_be_enqueued = dynamic_array_alloc();

	//Enqueue the very first node
	enqueue(queue, cfg->root);

	//For holding our blocks
	basic_block_t* block;

	//So long as the queue isn't empty
	while(queue_is_empty(queue) == HEAP_QUEUE_NOT_EMPTY){
		//Pop off of the queue
		block = dequeue(queue);

		//If this wasn't visited, we'll print
		if(block->visited != 3){
			print_block_three_addr_code(block, print_df);	
		}

		//Now we'll mark this as visited
		block->visited = 3;

		//And finally we'll add all of these onto the queue
		for(u_int16_t i = 0; i < block->num_successors; i++){
			if(block->successors[i]->visited != 3){
				if(block->successors[i]->block_terminal_type == BLOCK_TERM_TYPE_RET){
					//Store this for later, we don't want it now
				//	dynamic_array_add(to_be_enqueued, block->successors[i]);
					//Don't add it now
				//	continue;
				}

				enqueue(queue, block->successors[i]);
			}

			//enqueue the ones that are terminal blocks at the very end
			while(dynamic_array_is_empty(to_be_enqueued) == FALSE){
				enqueue(queue, dynamic_array_delete_at(to_be_enqueued, 0));
			}
		}
	}

	//Destroy the heap queue when done
	heap_queue_dealloc(queue);
	//Destroy the dynamic array when done
	dynamic_array_dealloc(to_be_enqueued);
}

/**
 * Print out the whole program in order by using the direct successor
 */
static void emit_blocks_direct_successor(cfg_t* cfg, emit_dominance_frontier_selection_t print_df){
	//Grab a cursor
	basic_block_t* cursor = cfg->root;

	//So long as we have something to print, we print
	while(cursor != NULL){
		if(cursor->visited != 5){
			print_block_three_addr_code(cursor, print_df);
		}

		cursor->visited = 5;

		cursor = cursor->direct_successor;
	}
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

	//Deallocate the live variable array
	if(block->live_variables != NULL){
		free(block->live_variables);
	}

	//Deallocate the assigned variable array
	if(block->assigned_variables != NULL){
		free(block->assigned_variables);
	}

	//Grab a statement cursor here
	three_addr_code_stmt_t* cursor = block->leader_statement;
	//We'll need a temp block too
	three_addr_code_stmt_t* temp = cursor;

	//So long as the cursor is not NULL
	while(cursor != NULL){
		temp = cursor;
		cursor = cursor->next_statement;
		//Destroy temp
		three_addr_stmt_dealloc(temp);
	}
	

	//Otherwise its fine so
	free(block);
}


/**
 * Memory management code that allows us to deallocate the entire CFG
 */
void dealloc_cfg(cfg_t* cfg){
	//Run through all of the blocks here and delete them
	for(u_int16_t i = 0; i < cfg->created_blocks->current_index; i++){
		//Use this to deallocate
		basic_block_dealloc(dynamic_array_get_at(cfg->created_blocks, i));
	}

	//Destroy all variables
	deallocate_all_vars();
	//Destroy all constants
	deallocate_all_consts();

	//At the very end, be sure to destroy this too
	free(cfg);
}


/**
 * Helper for returning error blocks. Error blocks always have an ID of -1
 */
static basic_block_t* create_and_return_err(){
	//Create the error
	basic_block_t* err_block = basic_block_alloc();
	//Set the ID to -1
	err_block->block_id = -1;

	return err_block;
}


/**
 * Add a successor to the target block
 */
static void add_successor(basic_block_t* target, basic_block_t* successor){
	//Let's check this
	if(target->num_successors == MAX_SUCCESSORS){
		//Internal error for the programmer
		printf("CFG ERROR. YOU MUST INCREASE THE NUMBER OF SUCCESSORS");
		exit(1);
	}

	//If there are no successors here, add it in as the direct one
	if(target->num_successors == 0){
		target->direct_successor = successor;
	}

	//Is this thing already a successor? If so we won't add it
	for(u_int16_t i = 0; i < target->num_successors; i++){
		//It's fine, we don't need to add it
		if(target->successors[i] == successor){
			return;
		}
	}

	//Otherwise we're set here
	//Add this in
	target->successors[target->num_successors] = successor;
	//Increment how many we have
	(target->num_successors)++;

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
		a->exit_statement->next_statement = b->leader_statement;
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

	//FOR EACH Successor of B, it will have a reference to B as a predecessor.
	//This is now wrong though. So, for each successor of B, it will need
	//to have A as predecessor
	for(u_int8_t i = 0; i < b->num_successors; i++){
		//Grab the block first
		basic_block_t* successor_block = b->successors[i];

		//Now for each of the predecessors that equals b, it needs to now point to A
		for(u_int8_t i = 0; i < successor_block->num_predecessors; i++){
			//If it's pointing to b, it needs to be updated
			if(successor_block->predecessors[i] == b){
				//Update it to now be correct
				successor_block->predecessors[i] = a;
			}
		}
	}

	//Also make note of any direct succession
	a->direct_successor = b->direct_successor;
	a->is_exit_block = b->is_exit_block;
	//Copy over the block type and terminal type
	if(a->block_type != BLOCK_TYPE_FUNC_ENTRY){
		a->block_type = b->block_type;
	}

	a->block_terminal_type = b->block_terminal_type;

	//IMPORTANT--wipe b's statements out
	b->leader_statement = NULL;
	b->exit_statement = NULL;

	//We'll now need to ensure that all of the live variables in B are also in A
	for(u_int16_t i = 0; i < b->live_variable_count; i++){
		//Add these in one by one to A
		add_live_variable(a, b->live_variables[i]);
	}

	//Copy over all of the assigned variables too
	for(u_int16_t i = 0; i < b->assigned_variable_count; i++){
		//Add these in one by one
		add_assigned_variable(a, b->assigned_variables[i]);
	}

	//Very last thing here, we need to be completely rid of B. It can
	//no longer be in the dynamic array. As such, we'll have to remove it
	u_int16_t index = dynamic_array_contains(cfg_ref->created_blocks, b);

	//Now we'll delete at this index
	dynamic_array_delete_at(cfg_ref->created_blocks, index);

	//And finally we'll free B -- MEMORY LEAKS
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
	if(strcmp(function_node->func_record->return_type->type_name, "void") == 0){
		return;
	}

	//We'll need a stack for our DFS
	heap_stack_t* stack = heap_stack_alloc();

	//The idea here is very simple. If we can walk the function tree and every control path leads 
	//to a return statement, we return null from every control path
	
	//We'll need a cursor to walk the tree
	basic_block_t* block_cursor;

	//Push the source node
	push(stack, entry_block);

	//So long as the stack is not empty
	while(is_empty(stack) == HEAP_STACK_NOT_EMPTY){
		//Grab the current one off of the stack
		block_cursor = pop(stack);

		//If this wasn't visited
		if(block_cursor->visited == FALSE){
			//Mark this one as seen
			block_cursor->visited = TRUE;

			/**
			 * Now we can perform our checks. If we find a block that:
			 * 	a.) Has a direct successor
			 * 	b.) That direct successor POINTS to a function ending block
			 * 	c.) It is NOT a return statement
			 *
			 * Then we have a function that does not return in all paths
			 */
			//If the direct successor is the exit, but it's not a return statement
			if(block_cursor->direct_successor != NULL && block_cursor->direct_successor->is_exit_block == TRUE
			  && block_cursor->block_terminal_type != BLOCK_TERM_TYPE_RET){
				//One more dead end
				dead_ends++;
				//Go to the next iteration
				continue;
			}

			//If it is a return statement none of its children are relevant
			if(block_cursor->block_terminal_type == BLOCK_TERM_TYPE_RET){
				continue;
			}
		}

		//We'll now add in all of the childen
		for(u_int8_t i = 0; i < block_cursor->num_successors; i++){
			//If we haven't seen it yet, add it to the list
			if(block_cursor->successors[i]->visited == FALSE){
				push(stack, block_cursor->successors[i]);
			}
		}
	}
	
	//Once we escape our while loop, we can actually see what the analysis said
	if(dead_ends > 0){
		//Extract the function name
		char* func_name = function_node->func_record->func_name;
		sprintf(info, "Non-void function \"%s\" does not return a value in all control paths", func_name);
		print_cfg_message(WARNING, info, function_node->line_number);
		(*num_warnings_ref) += dead_ends;
	}

	//Destroy the stack once we're done
	heap_stack_dealloc(stack);
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
	//We will explicitly declare that this is an exit here
	for_stmt_exit_block->block_type = BLOCK_TYPE_FOR_STMT_END;
	
	//Grab the reference to the for statement node
	generic_ast_node_t* for_stmt_node = values->initial_node;

	//Grab a cursor for walking the sub-tree
	generic_ast_node_t* ast_cursor = for_stmt_node->first_child;

	//We will always see 3 nodes here to start out with, of the type for_loop_cond_ast_node_t. These
	//nodes contain an "is_blank" field that will alert us if this is just a placeholder. 

	//If the very first one is not blank
	if(ast_cursor->first_child != NULL){
		//Add it's child in as a statement to the entry block
		emit_expr_code(for_stmt_entry_block, ast_cursor->first_child);
	}

	//We'll now need to create our repeating node. This is the node that will actually repeat from the for loop.
	//The second and third condition in the for loop are the ones that execute continously. The third condition
	//always executes at the end of each iteration
	basic_block_t* condition_block = basic_block_alloc();

	//The condition block is always a successor to the entry block
	add_successor(for_stmt_entry_block, condition_block);

	//Move along to the next node
	ast_cursor = ast_cursor->next_sibling;

	//The condition block values package
	expr_ret_package_t condition_block_vals;
	//By default, make this blank
	condition_block_vals.operator = BLANK;
	
	//If the second one is not blank
	if(ast_cursor->first_child != NULL){
		//This is always the first part of the repeating block
		condition_block_vals = emit_expr_code(condition_block, ast_cursor->first_child);

	//It is impossible for the second one to be blank
	} else {
		print_parse_message(PARSE_ERROR, "Fatal internal compiler error. Should not have gotten here if blank", for_stmt_node->line_number);
		exit(0);
	}

	//Now move it along to the third condition
	ast_cursor = ast_cursor->next_sibling;

	//Create the update block
	basic_block_t* for_stmt_update_block = basic_block_alloc();

	//If the third one is not blank
	if(ast_cursor->first_child != NULL){
		//Emit the update expression
		emit_expr_code(for_stmt_update_block, ast_cursor->first_child);
	}

	//This is the first and foremost successor
	add_successor(for_stmt_update_block, for_stmt_exit_block);
	//This node will always jump right back to the start
	add_successor(for_stmt_update_block, condition_block);
	//The direct successor of the update block is the exit block
	for_stmt_update_block->direct_successor = for_stmt_exit_block;
	
	//Unconditional jump to condition block
	emit_jmp_stmt(for_stmt_update_block, condition_block, JUMP_TYPE_JMP);

	//Advance to the next sibling
	ast_cursor = ast_cursor->next_sibling;
	
	//If this is not a compound statement, we have a serious error
	if(ast_cursor->CLASS != AST_NODE_CLASS_COMPOUND_STMT){
		print_parse_message(PARSE_ERROR, "Fatal internal compiler error. Expected compound statement in for loop, but did not find one.", for_stmt_node->line_number);
		//Immediate failure here
		exit(0);
	}
	
	//Create a copy of our values here
	values_package_t compound_stmt_values = pack_values(ast_cursor, //Initial Node
													    values->function_end_block, //Function end block
													 	condition_block, //Loop statement start -- for loops start at their condition
													 	for_stmt_exit_block, //Exit block of loop
													 	NULL,
													 	NULL, 
													 	for_stmt_update_block); //For loop update block

	//Otherwise, we will allow the subsidiary to handle that. The loop statement here is the condition block,
	//because that is what repeats on continue
	basic_block_t* compound_stmt_start = visit_compound_statement(&compound_stmt_values);

	//For our eventual token
	expr_ret_package_t expression_package;

	//If it's null, that's actually ok here
	if(compound_stmt_start == NULL){
		//We'll make sure that the start points to this block
		add_successor(condition_block, for_stmt_update_block);

		//And we're done
		return for_stmt_entry_block;
	}

	//This will always be a successor to the conditional statement
	add_successor(condition_block, compound_stmt_start);
	//The condition block also has another direct successor, the exit block
	//add_successor(condition_block, for_stmt_exit_block);
	//The direct successor of the condition block is the compound statement start
	condition_block->direct_successor = compound_stmt_start;

	//We'll use our inverse jumping("jump out") strategy here
	jump_type_t jump_type = select_appropriate_jump_stmt(condition_block_vals.operator, JUMP_CATEGORY_INVERSE);

	//Make the condition block jump to the compound stmt start
	emit_jmp_stmt(condition_block, for_stmt_exit_block, jump_type);

	//However if it isn't NULL, we'll need to find the end of this compound statement
	basic_block_t* compound_stmt_end = compound_stmt_start;

	//So long as we don't see the end or a return
	while(compound_stmt_end->direct_successor != NULL && compound_stmt_end->block_terminal_type == BLOCK_TERM_TYPE_NORMAL){
		compound_stmt_end = compound_stmt_end->direct_successor;
	}

	//Once we get here, if it is a return statement, that means that we always return
	if(compound_stmt_end->block_terminal_type == BLOCK_TERM_TYPE_RET){
		//We should warn here
		print_cfg_message(WARNING, "For loop internal returns through every control block, will only execute once", for_stmt_node->line_number);
		(*num_warnings_ref)++;
	}

	//The successor to the end block is the update block
	add_successor(compound_stmt_end, for_stmt_update_block);
	//The direct successor is the update block
	compound_stmt_end->direct_successor = for_stmt_update_block;
	//We also need an uncoditional jump right to the update block
	emit_jmp_stmt(compound_stmt_end, for_stmt_update_block, JUMP_TYPE_JMP);

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
	//We will explicitly mark that this is an exit block
	do_while_stmt_exit_block->block_type = BLOCK_TYPE_DO_WHILE_END;

	//Grab the initial node
	generic_ast_node_t* do_while_stmt_node = values->initial_node;

	//Grab a cursor for walking the subtree
	generic_ast_node_t* ast_cursor = do_while_stmt_node->first_child;

	//If this is not a compound statement, something here is very wrong
	if(ast_cursor->CLASS != AST_NODE_CLASS_COMPOUND_STMT){
		print_cfg_message(PARSE_ERROR, "Expected compound statement in do-while, but did not find one", do_while_stmt_node->line_number);
		exit(0);
	}

	//Create a copy of our values here
	values_package_t compound_stmt_values = pack_values(ast_cursor, //Initial Node
													    values->function_end_block, //Function end block
													 	do_while_stmt_entry_block, //Loop statement start
													 	do_while_stmt_exit_block, //Exit block of loop
													 	NULL, //Switch statement end
													 	NULL, //If statement end
													 	NULL); //For loop update block

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
	while(compound_stmt_end->direct_successor != NULL && compound_stmt_end->block_terminal_type == BLOCK_TERM_TYPE_NORMAL){
		compound_stmt_end = compound_stmt_end->direct_successor;
	}

	//Once we get here, if it's a return statement, everything below is unreachable
	if(compound_stmt_end->block_terminal_type == BLOCK_TERM_TYPE_RET){
		print_cfg_message(WARNING, "Do-while returns through all internal control paths. All following code is unreachable", do_while_stmt_node->line_number);
		(*num_warnings_ref)++;
		//Just return the block here
		return do_while_stmt_entry_block;
	}

	//Add the conditional check into the end here
	expr_ret_package_t package = emit_expr_code(compound_stmt_end, ast_cursor->next_sibling);

	//Now we'll make do our necessary connnections. The direct successor of this end block is the true
	//exit block
	add_successor(compound_stmt_end, do_while_stmt_exit_block);

	//Make sure it's the direct successor
	compound_stmt_end->direct_successor = do_while_stmt_exit_block;

	//It's other successor though is the loop entry. This is for flow analysis
	add_successor(compound_stmt_end, do_while_stmt_entry_block);

	//Discern the jump type here--This is a direct jump
	jump_type_t jump_type = select_appropriate_jump_stmt(package.operator, JUMP_CATEGORY_NORMAL);
		
	//We'll need a jump statement here to the entrance block
	emit_jmp_stmt(compound_stmt_end, do_while_stmt_entry_block, jump_type);

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
	//We will specifically mark the end block here as an ending block
	while_statement_end_block->block_type = BLOCK_TYPE_WHILE_END;

	//Grab this for convenience
	generic_ast_node_t* while_stmt_node = values->initial_node;

	//Grab a cursor to the while statement node
	generic_ast_node_t* ast_cursor = while_stmt_node->first_child;

	//The entry block contains our expression statement
	expr_ret_package_t package = emit_expr_code(while_statement_entry_block, ast_cursor);

	//The very next node is a compound statement
	ast_cursor = ast_cursor->next_sibling;

	//If it isn't, we'll error out. This is really only for dev use
	if(ast_cursor->CLASS != AST_NODE_CLASS_COMPOUND_STMT){
		print_cfg_message(PARSE_ERROR, "Found node that is not a compound statement in while-loop subtree", while_stmt_node->line_number);
		exit(0);
	}

	//Create a copy of our values here
	values_package_t compound_stmt_values = pack_values(ast_cursor, //Initial Node
													    values->function_end_block, //Function end block
													 	while_statement_entry_block, //Loop statement start
													 	while_statement_end_block, //Exit block of loop
													 	NULL, //Switch statement end
													 	NULL, //If statement end
													 	NULL); //For loop update block

	//Now that we know it's a compound statement, we'll let the subsidiary handle it
	basic_block_t* compound_stmt_start = visit_compound_statement(&compound_stmt_values);

	//If it's null, that means that we were given an empty while loop here
	if(compound_stmt_start == NULL){
		//For the user to see
		print_cfg_message(WARNING, "While loop has empty body, has no effect", while_stmt_node->line_number);
		(*num_warnings_ref)++;

		//We do still need to have our successor be the ending block
		add_successor(while_statement_entry_block, while_statement_end_block);
		//It is our direct successor
		while_statement_entry_block->direct_successor = while_statement_end_block;

		//We'll just return now
		return while_statement_entry_block;
	}


	//We'll now determine what kind of jump statement that we have here. We want to jump to the exit if
	//we're bad, so we'll do an inverse jump
	jump_type_t jump_type = select_appropriate_jump_stmt(package.operator, JUMP_CATEGORY_INVERSE);
	//"Jump over" the body if it's bad
	emit_jmp_stmt(while_statement_entry_block, while_statement_end_block, jump_type);

	//Otherwise it isn't null, so we can add it as a successor
	add_successor(while_statement_entry_block, compound_stmt_start);
	//The successor to the entry block is the starting block
	while_statement_entry_block->direct_successor = compound_stmt_start;

	//Let's now find the end of the compound statement
	basic_block_t* compound_stmt_end = compound_stmt_start;

	//So long as it isn't null or return
	while (compound_stmt_end->direct_successor != NULL && compound_stmt_end->block_terminal_type == BLOCK_TERM_TYPE_NORMAL){
		compound_stmt_end = compound_stmt_end->direct_successor;
	}
	
	//If we make it to the end and this ending statement is a return, that means that we always return
	//Throw a warning
	if(compound_stmt_end->block_terminal_type == BLOCK_TERM_TYPE_RET){
		//It is only an error though -- the user is allowed to do this
		print_cfg_message(WARNING, "While loop body returns in all control paths. It will only execute at most once", while_stmt_node->line_number);
		(*num_warnings_ref)++;
	}

	//A successor to the end block is the block at the top of the loop
	add_successor(compound_stmt_end, while_statement_entry_block);
	//His direct successor is the end block
	add_successor(compound_stmt_end, while_statement_end_block);
	//Set this to make sure
	compound_stmt_end->direct_successor = while_statement_end_block;

	//The compound statement end will jump right back up to the entry block
	emit_jmp_stmt(compound_stmt_end, while_statement_entry_block, JUMP_TYPE_JMP);

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
 *
 * 
 * =========== JUMP INSTRUCTION SELECTION =========================
 *
 * This implementation will make use of a "jump-to-else" scheme. This means
 * that the "if" part of the statement is always directly underneath the entry block. These else,
 * if such a statement exists, needs to be jumped to to get to it
 */
static basic_block_t* visit_if_statement(values_package_t* values){
	//We always have an entry block here -- the end block is made for us
	basic_block_t* entry_block = basic_block_alloc();

	//Let's grab a cursor to walk the tree
	generic_ast_node_t* cursor = values->initial_node->first_child;

	//Add whatever our conditional is into the starting block
	expr_ret_package_t package = emit_expr_code(entry_block, cursor);

	//No we'll move one step beyond, the next node must be a compound statement
	cursor = cursor->next_sibling;

	//If it isn't, that's an issue
	if(cursor->CLASS != AST_NODE_CLASS_COMPOUND_STMT){
		print_cfg_message(PARSE_ERROR, "Fatal internal compiler error: Expected compound statement in if node", cursor->line_number);
		exit(1);
	}

	//Create a copy of our values here
	values_package_t if_compound_stmt_values = pack_values(cursor, //Initial Node
													    values->function_end_block, //Function end block
													 	values->loop_stmt_start, //Loop statement start
													 	values->loop_stmt_end, //Exit block of loop
													 	values->switch_statement_end, //Switch statement end
													 	values->if_stmt_end_block, //If statement end
													 	values->for_loop_update_block); //For loop update block

	//Now that we know it is, we'll invoke the compound statement rule
	basic_block_t* if_compound_stmt_entry = visit_compound_statement(&if_compound_stmt_values);

	//The ending block of the compound statement
	basic_block_t* if_compound_stmt_end;

	//If this is null, it's fine, but we should throw a warning
	if(if_compound_stmt_entry == NULL){
		print_cfg_message(WARNING, "Empty if clause in if-statement", cursor->line_number);
		(*num_warnings_ref)++;
		//No direct successor here as of yet
		//We'll set the end ot be the entrance
		if_compound_stmt_end = entry_block;

	//We expect this to be the most likely option
	} else {
		//Add the if statement node in as a direct successor
		add_successor(entry_block, if_compound_stmt_entry);
		//Add as direct successor. We want this to pass through the entry block
		entry_block->direct_successor = if_compound_stmt_entry;

		//Now we'll find the end of this statement
		if_compound_stmt_end = if_compound_stmt_entry;

		//Once we've visited, we'll need to drill to the end of this compound statement
		while(if_compound_stmt_end->direct_successor != NULL && if_compound_stmt_end->block_terminal_type == BLOCK_TERM_TYPE_NORMAL){
			if_compound_stmt_end = if_compound_stmt_end->direct_successor;
		}

		//The successor to the if-stmt end path is the if statement end block
		emit_jmp_stmt(if_compound_stmt_end, values->if_stmt_end_block, JUMP_TYPE_JMP);
		//If this is the case, the end block is a direct successor
		add_successor(if_compound_stmt_end, values->if_stmt_end_block);
	}

	//This is the end if we have a lone "if"
	if(cursor->next_sibling == NULL){
		//Ensure that we have a direct path to the end
		if_compound_stmt_end->direct_successor = values->if_stmt_end_block;
		//If this is the case, another successor of the entry block is
		//the end block
		add_successor(entry_block, values->if_stmt_end_block);

		/**
		 * The "if" path is always our direct path, we only need to jump to the else, so we jump
		 * in an inverse fashion
		 */
		//Let's determine the appropriate jump statement
		jump_type_t jump_type = select_appropriate_jump_stmt(package.operator, JUMP_CATEGORY_INVERSE);
		emit_jmp_stmt(entry_block, values->if_stmt_end_block, jump_type);

		//We can leave now
		return entry_block;
	}

	//If we make it here, we know that we have either an if or an if-else block
	//Advance the cursor
	cursor = cursor->next_sibling;
	
	//If we have a compound statement, we'll handle it as an "else" clause
	if(cursor->CLASS == AST_NODE_CLASS_COMPOUND_STMT){
		//Create a copy of our values here
		values_package_t else_values_package = pack_values(cursor, //Initial Node
													    values->function_end_block, //Function end block
													 	values->loop_stmt_start, //Loop statement start
													 	values->loop_stmt_end, //Exit block of loop
													 	values->switch_statement_end, //Switch statement end
													 	values->if_stmt_end_block, //If statement end
													 	values->for_loop_update_block); //For loop update block

		//Visit the else statement
		basic_block_t* else_compound_stmt_entry = visit_compound_statement(&else_values_package);

		//If this is NULL, we'll send a warning and hop out -- no need for more processing here
		if(else_compound_stmt_entry == NULL){
			print_cfg_message(WARNING, "Empty else clause in if-else statement", cursor->line_number);
			(*num_warnings_ref)++;

			//Just get out if this happens
			return entry_block;
		}

		//Otherwise, we'll add this in as a successor
		add_successor(entry_block, else_compound_stmt_entry);
		//However, we want this to be the direct successor of the end of the if compound statement
		if_compound_stmt_end->direct_successor = else_compound_stmt_entry;

		//Let's determine the appropriate jump statement
		jump_type_t jump_type = select_appropriate_jump_stmt(package.operator, JUMP_CATEGORY_INVERSE);
		//Add in our "jump to else" clause
		emit_jmp_stmt(entry_block, else_compound_stmt_entry, jump_type);

		//Now we'll find the end of this statement
		basic_block_t* else_compound_stmt_end = else_compound_stmt_entry;

		//Once we've visited, we'll need to drill to the end of this compound statement
		while(else_compound_stmt_end->direct_successor != NULL && else_compound_stmt_end->block_terminal_type == BLOCK_TERM_TYPE_NORMAL){
			else_compound_stmt_end = else_compound_stmt_end->direct_successor;
		}

		//The successor to this block is the ending block
		add_successor(else_compound_stmt_end, values->if_stmt_end_block);
		//Ensure is direct successor
		else_compound_stmt_end->direct_successor = values->if_stmt_end_block;
		//Add in our jump to end here
		emit_jmp_stmt(else_compound_stmt_end, values->if_stmt_end_block, JUMP_TYPE_JMP);

		//We're done here, send it back
		return entry_block;
	
	//Otherwise we have an "else if" clause 
	} else if(cursor->CLASS == AST_NODE_CLASS_IF_STMT){
		//Create a copy of our values here
		values_package_t else_if_values_package = pack_values(cursor, //Initial Node
													    values->function_end_block, //Function end block
													 	values->loop_stmt_start, //Loop statement start
													 	values->loop_stmt_end, //Exit block of loop
													 	values->switch_statement_end, //Switch statement end
													 	values->if_stmt_end_block, //If statement end
													 	values->for_loop_update_block); //For loop update block

		//Visit the if statment, this one is not a parent
		basic_block_t* else_if_entry = visit_if_statement(&else_if_values_package);

		//Add this as a successor to the entrant
		add_successor(entry_block, else_if_entry);
		//However, we want this to be a direct successor to the if end block
		if_compound_stmt_end->direct_successor = else_if_entry;

		//Let's determine the appropriate jump statement
		jump_type_t jump_type = select_appropriate_jump_stmt(package.operator, JUMP_CATEGORY_INVERSE);
		//Emit our jump to else if here
		emit_jmp_stmt(entry_block, else_if_entry, jump_type);

		//In theory, the if block already has an end appropriately made, so we don't need to do anything here
		return entry_block;
	
	//Some weird error here
	} else {
		print_cfg_message(PARSE_ERROR, "Improper node found after if-statement", cursor->line_number);
		(*num_errors_ref)++;
		exit(0);
	}
}


/**
 * This statement block acts as a multiplexing rule. It will go through and make
 * appropriate calls based no what kind of statement that we have
 *
 * Since this is largely intended for case and default statements, we assume that
 * the statements in here are chained together as siblings with the initial node
 */
static basic_block_t* visit_statement_sequence(values_package_t* values){
	//The global starting block
	basic_block_t* starting_block = NULL;
	//The current block
	basic_block_t* current_block = starting_block;

	//Grab the initial node
	generic_ast_node_t* current_node = values->initial_node;
	
	//Roll through the entire subtree
	while(current_node != NULL){
		//We've found a declaration statement
		if(current_node->CLASS == AST_NODE_CLASS_DECL_STMT){
			//Create our values package
			values_package_t decl_values = pack_values(current_node, //Initial Node
														NULL, //Function end block
													 	NULL, //Loop statement start
													 	NULL, //Exit block of loop
													 	NULL, //Switch statement end
													 	NULL, //If statement end
													 	NULL); //For loop update block
			//We'll visit the block here
			basic_block_t* decl_block = visit_declaration_statement(&decl_values);

			//There is nothing to merge here

		//We've found a let statement
		} else if(current_node->CLASS == AST_NODE_CLASS_LET_STMT){
			//Create our values package
			values_package_t let_values = pack_values(current_node, //Initial Node
														NULL, //Function end block
													 	NULL, //Loop statement start
													 	NULL, //Exit block of loop
													 	NULL, //Switch statement end
													 	NULL, //If statement end
													 	NULL); //For loop update block


			//We'll visit the block here
			basic_block_t* let_block = visit_let_statement(&let_values);

			//If the start block is null, then this is the start block. Otherwise, we merge it in
			if(starting_block == NULL){
				starting_block = let_block;
				current_block = let_block;
			//Just merge with current
			} else {
				current_block = merge_blocks(current_block, let_block); 
			}

		//If we have a return statement -- SPECIAL CASE HERE
		} else if (current_node->CLASS == AST_NODE_CLASS_RET_STMT){
			//If for whatever reason the block is null, we'll create it
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
			}

			//Emit the return statement, let the sub rule handle
			emit_ret_stmt(current_block, current_node);

			//The current block will now be marked as a return statement
			current_block->block_terminal_type = BLOCK_TERM_TYPE_RET;

			//The current block's direct and only successor is the function exit block
			add_successor(current_block, values->function_end_block);

			//If there is anything after this statement, it is UNREACHABLE
			if(current_node->next_sibling != NULL){
				print_cfg_message(WARNING, "Unreachable code detected after return statement", current_node->next_sibling->line_number);
				(*num_warnings_ref)++;
			}

			//We're completely done here
			return starting_block;

		//We could also have a compound statement inside of here as well
		} else if(current_node->CLASS == AST_NODE_CLASS_COMPOUND_STMT){
			//Pack up all of our values
			values_package_t compound_stmt_values = pack_values(current_node, //Initial Node
														values->function_end_block, //Function end block
													 	values->loop_stmt_start, //Loop statement start
													 	values->loop_stmt_end, //Exit block of loop
													 	values->switch_statement_end, //Switch statement end
													 	values->if_stmt_end_block, //If statement end
													 	values->for_loop_update_block); //For loop update block
			
			//We'll simply recall this function and let it handle it
			basic_block_t* compound_stmt_entry_block = visit_compound_statement(&compound_stmt_values);

			//Add in everything appropriately here
			if(starting_block == NULL){
				starting_block = compound_stmt_entry_block;
			} else {
				//TODO MAY OR MAY NOT KEEP
				add_successor(current_block, compound_stmt_entry_block);
			}

			//We need to drill to the end
			//Set this to be current
			current_block = compound_stmt_entry_block;

			//Once we're here the start is in current, we'll need to drill to the end
			while(current_block->direct_successor != NULL && current_block->block_terminal_type != BLOCK_TERM_TYPE_RET){
				current_block = current_block->direct_successor;
			}

			//If we did hit a return block here and there are nodes after this one in the chain, then we have
			//unreachable code
			if(current_block->block_terminal_type == BLOCK_TERM_TYPE_RET && current_node->next_sibling != NULL){
				print_parse_message(WARNING, "Unreachable code detected after ret statement", current_node->next_sibling->line_number);

			}

		//We've found an if-statement
		} else if(current_node->CLASS == AST_NODE_CLASS_IF_STMT){
			//Create the end block here for pointer reasons
			basic_block_t* if_end_block = basic_block_alloc();
			//We will explicitly denote that this is an if statement ending block
			if_end_block->block_type = BLOCK_TYPE_IF_STMT_END;

			values_package_t if_stmt_values = pack_values(current_node, //Initial Node
														values->function_end_block, //Function end block
													 	values->loop_stmt_start, //Loop statement start
													 	values->loop_stmt_end, //Exit block of loop
													 	values->switch_statement_end, //Switch statement end
														if_end_block, //If statement end
													 	values->for_loop_update_block); //For loop update block
			
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
			//Compare the end addresses -- as long as we don't hit we're fine
			while(current_block->block_type != BLOCK_TYPE_IF_STMT_END){
				current_block = current_block->direct_successor;
			}

		//Handle a while statement
		} else if(current_node->CLASS == AST_NODE_CLASS_WHILE_STMT){
			//Create the values here
			values_package_t while_stmt_values;
			while_stmt_values.initial_node = current_node;
			while_stmt_values.for_loop_update_block = values->for_loop_update_block;
			while_stmt_values.loop_stmt_start = NULL;
			while_stmt_values.loop_stmt_end = NULL;
			while_stmt_values.if_stmt_end_block = values->if_stmt_end_block;
			while_stmt_values.function_end_block = values->function_end_block;
			while_stmt_values.switch_statement_end = values->switch_statement_end;

			//Visit the while statement
			basic_block_t* while_stmt_entry_block = visit_while_statement(&while_stmt_values);

			//We'll now add it in
			if(starting_block == NULL){
				starting_block = while_stmt_entry_block;
				current_block = starting_block;
			//We never merge while statements -- it will always be a successor
			} else {
				//Add as a successor
				add_successor(current_block, while_stmt_entry_block);
			}

			//Set the current block here
			current_block = while_stmt_entry_block;

			//Drill down to the very end
			while(current_block->block_type != BLOCK_TYPE_WHILE_END){
				current_block = current_block->direct_successor;
			}
	
		//Handle a do-while statement
		} else if(current_node->CLASS == AST_NODE_CLASS_DO_WHILE_STMT){
			//Create the values package
			values_package_t do_while_values;
			do_while_values.initial_node = current_node;
			do_while_values.function_end_block = values->function_end_block;
			do_while_values.if_stmt_end_block = values->if_stmt_end_block;
			do_while_values.loop_stmt_start = NULL;
			do_while_values.loop_stmt_end = NULL;
			do_while_values.for_loop_update_block = values->for_loop_update_block;
			do_while_values.switch_statement_end = values->switch_statement_end;

			//Visit the statement
			basic_block_t* do_while_stmt_entry_block = visit_do_while_statement(&do_while_values);

			//We'll now add it in
			if(starting_block == NULL){
				starting_block = do_while_stmt_entry_block;
				current_block = starting_block;
			//We never merge do-while's, they are strictly successors
			} else {
				add_successor(current_block, do_while_stmt_entry_block);
			}

			//Now we'll need to reach the end-point of this statement
			current_block = do_while_stmt_entry_block;

			//So long as we have successors and don't see returns
			while(current_block->direct_successor != NULL && current_block->block_type != BLOCK_TYPE_DO_WHILE_END){
				current_block = current_block->direct_successor;
			}

			//If we make it here and we had a return statement, we need to get out
			if(current_block->block_terminal_type == BLOCK_TERM_TYPE_RET){
				//Everything beyond this point is unreachable, no point in going on
				print_cfg_message(WARNING, "Unreachable code detected after block that returns in all control paths", current_node->next_sibling->line_number);
				(*num_warnings_ref)++;
				//Get out nowk
				return starting_block;
			}

			//Otherwise, we're all set to go to the next iteration

		//Handle a for statement
		} else if(current_node->CLASS == AST_NODE_CLASS_FOR_STMT){
			//Create the values package
			values_package_t for_stmt_values;
			for_stmt_values.initial_node = current_node;
			for_stmt_values.function_end_block = values->function_end_block;
			for_stmt_values.for_loop_update_block = values->for_loop_update_block;
			for_stmt_values.loop_stmt_start = NULL;
			for_stmt_values.loop_stmt_end = NULL;
			for_stmt_values.if_stmt_end_block = values->if_stmt_end_block;
			for_stmt_values.switch_statement_end = values->switch_statement_end;

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
			while(current_block->block_type != BLOCK_TYPE_FOR_STMT_END){
				current_block = current_block->direct_successor;
			}

		//Handle a continue statement
		} else if(current_node->CLASS == AST_NODE_CLASS_CONTINUE_STMT){
			//Let's first see if we're in a loop or not
			if(values->loop_stmt_start == NULL){
				print_cfg_message(PARSE_ERROR, "Continue statement was not found in a loop", current_node->line_number);
				(*num_errors_ref)++;
				return create_and_return_err();
			}

			//This could happen where we have nothing here
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
			}

			//There are two options here. We could see a regular continue or a conditional
			//continue. If the child is null, then it is a regular continue
			if(current_node->first_child == NULL){
				//Mark this for later
				current_block->block_terminal_type = BLOCK_TERM_TYPE_CONTINUE;

				//Let's see what kind of loop we're in
				//NON for loop
				if(values->for_loop_update_block == NULL){
					//Otherwise we are in a loop, so this means that we need to point the continue statement to
					//the loop entry block
					add_successor(current_block, values->loop_stmt_start);
					//We always jump to the start of the loop statement unconditionally
					emit_jmp_stmt(current_block, values->loop_stmt_start, JUMP_TYPE_JMP);

				//We are in a for loop
				} else {
					//Otherwise we are in a for loop, so we just need to point to the for loop update block
					add_successor(current_block, values->for_loop_update_block);
					//Emit a direct unconditional jump statement to it
					emit_jmp_stmt(current_block, values->for_loop_update_block, JUMP_TYPE_JMP);
				}

				//Further, anything after this is unreachable
				if(current_node->next_sibling != NULL){
					print_cfg_message(WARNING, "Unreachable code detected after continue statement", current_node->next_sibling->line_number);
					(*num_warnings_ref)++;
				}

				//We're done here, so return the starting block. There is no 
				//point in going on
				return starting_block;

			//Otherwise, we have a conditional continue here
			} else {
				//Emit the expression code into the current statement
				expr_ret_package_t package = emit_expr_code(current_block, current_node->first_child);
				//Decide the appropriate jump statement -- direct path here
				jump_type_t jump_type = select_appropriate_jump_stmt(package.operator, JUMP_CATEGORY_NORMAL);

				//Two divergent paths here -- whether or not we have a for loop
				//Not a for loop
				if(values->for_loop_update_block == NULL){
					//Otherwise we are in a loop, so this means that we need to point the continue statement to
					//the loop entry block
					basic_block_t* successor = current_block->direct_successor;
					//Add the successor in
					add_successor(current_block, values->loop_stmt_start);
					//Restore the direct successor
					current_block->direct_successor = successor;
					//We always jump to the start of the loop statement unconditionally
					emit_jmp_stmt(current_block, values->loop_stmt_start, jump_type);

				//We are in a for loop
				} else {
					//Otherwise we are in a for loop, so we just need to point to the for loop update block
					basic_block_t* successor = current_block->direct_successor;
					add_successor(current_block, values->for_loop_update_block);
					//Restore the direct successor
					current_block->direct_successor = successor;
					//Emit a direct unconditional jump statement to it
					emit_jmp_stmt(current_block, values->for_loop_update_block, jump_type);
				}
			}

		//Handle a break out statement
		} else if(current_node->CLASS == AST_NODE_CLASS_BREAK_STMT){
			//Let's first see if we're in a loop or not
			if(values->loop_stmt_start == NULL && values->switch_statement_end == NULL){
				print_cfg_message(PARSE_ERROR, "Break statement was not found in a loop or switch statement", current_node->line_number);
				(*num_errors_ref)++;
				return create_and_return_err();
			}

			//This could happen where we have nothing here
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
			}

			//There are two options here: We could have a conditional break
			//or a normal break. If there is no child node, we have a normal break
			if(current_node->first_child == NULL){
				//Mark this for later
				current_block->block_terminal_type = BLOCK_TERM_TYPE_BREAK;

				//There are two options here. If we're in a loop, that takes precedence. If we're in
				//a switch statement, then that takes precedence
				if(values->loop_stmt_start != NULL){
					//Otherwise we need to break out of the loop
					add_successor(current_block, values->loop_stmt_end);
					//We will jump to it -- this is always an uncoditional jump
					emit_jmp_stmt(current_block, values->loop_stmt_end, JUMP_TYPE_JMP);
				//We must've seen a switch statement then
				} else {
					//Save this for later on
					current_block->case_block_breaks_to = values->switch_statement_end;
					//We will jump to it -- this is always an uncoditional jump
					emit_jmp_stmt(current_block, values->switch_statement_end, JUMP_TYPE_JMP);
				}

				//If we see anything after this, it is unreachable so throw a warning
				if(current_node->next_sibling != NULL){
					print_cfg_message(WARNING, "Unreachable code detected after break statement", current_node->next_sibling->line_number);
					(*num_errors_ref)++;
				}

				//For a regular break statement, this is it, so we just get out
				//Give back the starting block
				return starting_block;

			//Otherwise, we have a conditional break, which will generate a conditional jump instruction
			} else {
				//First let's emit the conditional code
				expr_ret_package_t ret_package = emit_expr_code(current_block, current_node->first_child);

				//There are two options here. If we're in a loop, that takes precedence. If we're in
				//a switch statement, then that takes precedence
				if(values->loop_stmt_end != NULL){
					//This block can jump right out of the loop
					basic_block_t* successor = current_block->direct_successor;
					add_successor(current_block, values->loop_stmt_end);
					//Restore
					current_block->direct_successor = successor;

					//Now based on whatever we have in here, we'll emit the appropriate jump type(direct jump)
					jump_type_t jump_type = select_appropriate_jump_stmt(ret_package.operator, JUMP_CATEGORY_NORMAL);

					//Emit our conditional jump now
					emit_jmp_stmt(current_block, values->loop_stmt_end, jump_type);			

				//We must've seen a switch statement then
				} else {
					//We'll save this in for later
					current_block->case_block_breaks_to = values->switch_statement_end;

					//Now based on whatever we have in here, we'll emit the appropriate jump type(direct jump)
					jump_type_t jump_type = select_appropriate_jump_stmt(ret_package.operator, JUMP_CATEGORY_NORMAL);

					//Emit our conditional jump now
					emit_jmp_stmt(current_block, values->switch_statement_end, jump_type);			
				}
			}

		//Handle a defer statement. Remember that a defer statment is one monolithic
		//node with a bunch of sub-nodes underneath that are all handleable by "expr"
		} else if(current_node->CLASS == AST_NODE_CLASS_DEFER_STMT){
			//This really shouldn't happen, but it can't hurt
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
			}

			//Grab a cursor here
			generic_ast_node_t* defer_stmt_cursor = current_node->first_child;

			//Ollie lang uniquely allows the user to defer assembly statements. 
			//This can be useful IF you know what you're doing when it comes to assembly.
			//Since, if you defer assembly, that is the entire statement, we only
			//need to worry about emitting it once
			if(defer_stmt_cursor->CLASS == AST_NODE_CLASS_ASM_INLINE_STMT){
				//Emit the inline assembly that we need here
				emit_asm_inline_stmt(current_block, defer_stmt_cursor);

			//Otherwise it's just a regular deferral
			} else {
				//Run through all of the children, emitting their respective
				//expr codes
				while(defer_stmt_cursor != NULL){
					//Let the helper deal with it
					emit_expr_code(current_block, defer_stmt_cursor);
					//Move this up
					defer_stmt_cursor = defer_stmt_cursor->next_sibling;
				}			
			}

		//Handle a labeled statement
		} else if(current_node->CLASS == AST_NODE_CLASS_LABEL_STMT){
			//Thisreally shouldn't happen, but it can't hurt
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
			}
			
			//We rely on the helper to do it for us
			emit_label_stmt_code(current_block, current_node);

		//Handle a jump statement
		} else if(current_node->CLASS == AST_NODE_CLASS_JUMP_STMT){
			//This really shouldn't happen, but it can't hurt
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
			}

			//We rely on the helper to do it for us
			emit_jump_stmt_code(current_block, current_node);

		//These are 100% user generated,
		} else if(current_node->CLASS == AST_NODE_CLASS_ASM_INLINE_STMT){
			//If we find an assembly inline statement, the actuality of it is
			//incredibly easy. All that we need to do is literally take the 
			//user's statement and insert it into the code

			//We'll need a new block here regardless
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
			}

			//Let the helper handle
			emit_asm_inline_stmt(current_block, current_node);

		//Handle a nop statement
		} else if(current_node->CLASS == AST_NODE_CLASS_IDLE_STMT){
			//Do we need a new block?
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
			}

			//Let the helper handle -- doesn't even need the cursor
			emit_idle_stmt(current_block);
	
		//This means that we have some kind of expression statement
		} else {
			//This could happen where we have nothing here
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
			}
			
			//Also emit the simplified machine code
			emit_expr_code(current_block, current_node);
		}

		//Advance to the next child
		current_node = current_node->next_sibling;
	}

	//We always return the starting block
	//It is possible that we have a completely NULL compound statement. This returns
	//NULL in that event
	return starting_block;
}


/**
 * Visit a default statement.  These statements are also handled like individual blocks that can 
 * be jumped to
 */
static basic_block_t* visit_default_statement(values_package_t* values){
	//For a default statement, it performs very similarly to a case statement. 
	//It will be handled slightly differently in the jump table, but we'll get to that 
	//later on

	//Grab a cursor to our default statement
	generic_ast_node_t* default_stmt_cursor = values->initial_node;
	//Create it
	basic_block_t* default_stmt = basic_block_alloc();
	//Treated as case statements
	default_stmt->block_type = BLOCK_TYPE_CASE;

	//Now that we've actually packed up the value of the case statement here, we'll use the helper method to go through
	//any/all statements that are below it
	values_package_t statement_values = *values;
	//Only difference here is the starting place
	statement_values.initial_node = default_stmt_cursor->first_child;

	//Let this take care of it
	basic_block_t* statement_section_start = visit_statement_sequence(&statement_values);

	//Once we get this back, we'll add it in to the main block
	merge_blocks(default_stmt, statement_section_start);

	//Give the block back
	return default_stmt;
}


/**
 * Visit a case statement. It is very important that case statements know
 * where the end of the switch statement is, in case break statements are used
 */
static basic_block_t* visit_case_statement(values_package_t* values){
	//We need to make the block first
	basic_block_t* case_stmt = basic_block_alloc();
	case_stmt->block_type = BLOCK_TYPE_CASE;

	//The case statement should have some kind of constant value here, whether
	//it's an enum value or regular const. All validation should have been
	//done by the parser, so we're guaranteed to see something
	//correct here
	
	//The first child is our enum value
	generic_ast_node_t* case_stmt_cursor = values->initial_node;
	//Grab the value -- this should've already been done by the parser
	case_stmt->case_stmt_val = case_stmt_cursor->case_statement_value;

	//Now that we've actually packed up the value of the case statement here, we'll use the helper method to go through
	//any/all statements that are below it
	values_package_t statement_values = *values;
	//Only difference here is the starting place
	statement_values.initial_node = case_stmt_cursor->first_child;

	//Let this take care of it
	basic_block_t* statement_section_start = visit_statement_sequence(&statement_values);

	//Once we get this back, we'll add it in to the main block
	merge_blocks(case_stmt, statement_section_start);

	//Give the block back
	return case_stmt;
}


/**
 * Visit a switch statement. In Ollie's current implementation, 
 * the values here will not be reordered at all. Instead, they
 * will be put in the exact orientation that the user wants
 */
static basic_block_t* visit_switch_statement(values_package_t* values){
	//The starting block for the switch statement - we'll want this in a new
	//block
	basic_block_t* starting_block = basic_block_alloc();
	starting_block->block_type = BLOCK_TYPE_SWITCH;
	//We also need to know the ending block here -- Knowing
	//this is important for break statements
	basic_block_t* ending_block = basic_block_alloc();

	//We need a quick reference to the starting block ID
	u_int16_t starting_block_id = starting_block->block_id;

	//We also need a jump table block
	basic_block_t* jump_table_block = basic_block_alloc();
	//We need a reference to this block ID too
	u_int16_t jump_table_block_id = starting_block->block_id;


	//If this is empty, serious issue
	if(values->initial_node == NULL){
		//Print this message
		print_cfg_message(WARNING, "Empty switch statement detected", values->initial_node->line_number);
		(*num_warnings_ref)++;
		//It's just going to be empty
		return starting_block;
	}

	//Grab a cursor to the case statements
	generic_ast_node_t* case_stmt_cursor = values->initial_node->first_child;

	//The very first thing should be an expression telling us what to switch on
	//There should be some kind of expression here
	emit_expr_code(starting_block, case_stmt_cursor);

	//Get to the next statement. This is the first actual case 
	//statement
	case_stmt_cursor = case_stmt_cursor->next_sibling;

	//The values package that we have
	values_package_t passing_values = *values;
	//Set the ending block here, so that any break statements
	//know where to point
	passing_values.switch_statement_end = ending_block;
	//Send this in as the initial value
	passing_values.initial_node = case_stmt_cursor;

	//Keep a reference to whatever the current switch statement block is
	basic_block_t* current_block = starting_block;
	
	//The current block(case or default) that we're on
	basic_block_t* case_block;

	//So long as this isn't null
	while(case_stmt_cursor != NULL){
		//Handle a case statement
		if(case_stmt_cursor->CLASS == AST_NODE_CLASS_CASE_STMT){
			//Update this
			passing_values.initial_node = case_stmt_cursor;
			//Visit our case stmt here
			case_block = visit_case_statement(&passing_values);

		//Handle a default statement
		} else if(case_stmt_cursor->CLASS == AST_NODE_CLASS_DEFAULT_STMT){
			//Update this
			passing_values.initial_node = case_stmt_cursor;
			//Visit the default statement
			case_block = visit_default_statement(&passing_values);

		//Otherwise we fail out here
		} else {
			print_cfg_message(PARSE_ERROR, "Switch statements are only allowed \"case\" and \"default\" statements", case_stmt_cursor->line_number);
		}

		//Now we'll add this one into the overall structure
		add_successor(current_block, case_block);

		//We'll also add in what it breaks to, if anything. This MUST come last
		if(case_block->case_block_breaks_to != NULL){
			add_successor(current_block, case_block->case_block_breaks_to);
		}

		//Ensure that the current block points right here
		current_block->direct_successor = case_block;

		//Now we'll drill down to the bottom to prime the next pass
		while(current_block->direct_successor != NULL && current_block->block_terminal_type == BLOCK_TERM_TYPE_NORMAL){
			current_block = current_block->direct_successor;
		}

		//Move the cursor up
		case_stmt_cursor = case_stmt_cursor->next_sibling;
	}

	//Once we escape here, we should have a reference to the end of the last case or default statement. We'll
	//now ensure that this points directly to the switch statement ending block
	add_successor(current_block, ending_block);
	current_block->direct_successor = ending_block;

	//Give back the starting block
	return starting_block;
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

			//Emit the return statement, let the sub rule handle
			emit_ret_stmt(current_block, ast_cursor);

			//The current block will now be marked as a return statement
			current_block->block_terminal_type = BLOCK_TERM_TYPE_RET;

			//The current block's direct and only successor is the function exit block
			add_successor(current_block, values->function_end_block);

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
			//Explicitly mark this as an if end block
			if_end_block->block_type = BLOCK_TYPE_IF_STMT_END;

			//Create the values package
			values_package_t if_stmt_values;
			if_stmt_values.initial_node = ast_cursor;
			if_stmt_values.function_end_block = values->function_end_block;
			if_stmt_values.for_loop_update_block = values->for_loop_update_block;
			if_stmt_values.if_stmt_end_block = if_end_block;
			if_stmt_values.loop_stmt_start = values->loop_stmt_start;
			if_stmt_values.loop_stmt_end = values->loop_stmt_end;

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
			while(current_block->block_type != BLOCK_TYPE_IF_STMT_END){
				current_block = current_block->direct_successor;
			}

		//Handle a while statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_WHILE_STMT){
			//Create the values here
			values_package_t while_stmt_values;
			while_stmt_values.initial_node = ast_cursor;
			while_stmt_values.for_loop_update_block = values->for_loop_update_block;
			while_stmt_values.loop_stmt_start = NULL;
			while_stmt_values.loop_stmt_end = NULL;
			while_stmt_values.if_stmt_end_block = values->if_stmt_end_block;
			while_stmt_values.function_end_block = values->function_end_block;

			//Visit the while statement
			basic_block_t* while_stmt_entry_block = visit_while_statement(&while_stmt_values);

			//We'll now add it in
			if(starting_block == NULL){
				starting_block = while_stmt_entry_block;
				current_block = starting_block;
			//We never merge these
			} else {
				//Add as a successor
				add_successor(current_block, while_stmt_entry_block);
			}

			//Let's now drill to the bottom
			current_block = while_stmt_entry_block;

			//So long as we don't see the end
			while(current_block->block_type != BLOCK_TYPE_WHILE_END){
				current_block = current_block->direct_successor;
			}
	
		//Handle a do-while statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_DO_WHILE_STMT){
			//Create the values package
			values_package_t do_while_values;
			do_while_values.initial_node = ast_cursor;
			do_while_values.function_end_block = values->function_end_block;
			do_while_values.if_stmt_end_block = values->if_stmt_end_block;
			do_while_values.loop_stmt_start = NULL;
			do_while_values.loop_stmt_end = NULL;
			do_while_values.for_loop_update_block = values->for_loop_update_block;

			//Visit the statement
			basic_block_t* do_while_stmt_entry_block = visit_do_while_statement(&do_while_values);

			//We'll now add it in
			if(starting_block == NULL){
				starting_block = do_while_stmt_entry_block;
				current_block = starting_block;
			//We never merge do-while's, they are strictly successors
			} else {
				add_successor(current_block, do_while_stmt_entry_block);
			}

			//Now we'll need to reach the end-point of this statement
			current_block = do_while_stmt_entry_block;

			//As long as we aren't at the very end
			while(current_block->direct_successor != NULL && current_block->block_type != BLOCK_TYPE_DO_WHILE_END){
				current_block = current_block->direct_successor;
			}

			//If we make it here and we had a return statement, we need to get out
			if(current_block->block_terminal_type == BLOCK_TERM_TYPE_RET){
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
			for_stmt_values.for_loop_update_block = values->for_loop_update_block;
			for_stmt_values.loop_stmt_start = NULL;
			for_stmt_values.loop_stmt_end = NULL;
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
			while(current_block->block_type != BLOCK_TYPE_FOR_STMT_END){
				current_block = current_block->direct_successor;
			}

			//But if we don't then this is the current node
			//This current node is the for statement exit block. It will always be ok to merge this node
			current_block->good_to_merge = TRUE;

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

			//There are two options here. We could see a regular continue or a conditional
			//continue. If the child is null, then it is a regular continue
			if(ast_cursor->first_child == NULL){
				//Mark this for later
				current_block->block_terminal_type = BLOCK_TERM_TYPE_CONTINUE;

				//Let's see what kind of loop we're in
				//NON for loop
				if(values->for_loop_update_block == NULL){
					//Otherwise we are in a loop, so this means that we need to point the continue statement to
					//the loop entry block
					add_successor(current_block, values->loop_stmt_start);
					//We always jump to the start of the loop statement unconditionally
					emit_jmp_stmt(current_block, values->loop_stmt_start, JUMP_TYPE_JMP);

				//We are in a for loop
				} else {
					//Otherwise we are in a for loop, so we just need to point to the for loop update block
					add_successor(current_block, values->for_loop_update_block);
					//Emit a direct unconditional jump statement to it
					emit_jmp_stmt(current_block, values->for_loop_update_block, JUMP_TYPE_JMP);
				}

				//Further, anything after this is unreachable
				if(ast_cursor->next_sibling != NULL){
					print_cfg_message(WARNING, "Unreachable code detected after continue statement", ast_cursor->next_sibling->line_number);
					(*num_warnings_ref)++;
				}

				//We're done here, so return the starting block. There is no 
				//point in going on
				return starting_block;

			//Otherwise, we have a conditional continue here
			} else {
				//Emit the expression code into the current statement
				expr_ret_package_t package = emit_expr_code(current_block, ast_cursor->first_child);
				//Decide the appropriate jump statement -- direct path here
				jump_type_t jump_type = select_appropriate_jump_stmt(package.operator, JUMP_CATEGORY_NORMAL);

				//Two divergent paths here -- whether or not we have a for loop
				//Not a for loop
				if(values->for_loop_update_block == NULL){
					//Otherwise we are in a loop, so this means that we need to point the continue statement to
					//the loop entry block
					basic_block_t* successor = current_block->direct_successor;
					//Add the successor in
					add_successor(current_block, values->loop_stmt_start);
					//Restore the direct successor
					current_block->direct_successor = successor;
					//We always jump to the start of the loop statement unconditionally
					emit_jmp_stmt(current_block, values->loop_stmt_start, jump_type);

				//We are in a for loop
				} else {
					//Otherwise we are in a for loop, so we just need to point to the for loop update block
					basic_block_t* successor = current_block->direct_successor;
					add_successor(current_block, values->for_loop_update_block);
					//Restore the direct successor
					current_block->direct_successor = successor;
					//Emit a direct unconditional jump statement to it
					emit_jmp_stmt(current_block, values->for_loop_update_block, jump_type);
				}
			}

		//Hand le a break out statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_BREAK_STMT){
			//Let's first see if we're in a loop or switch statement or not
			if(values->loop_stmt_start == NULL && values->switch_statement_end == NULL){
				print_cfg_message(PARSE_ERROR, "Break statement was not found in a loop or switch statement", ast_cursor->line_number);
				(*num_errors_ref)++;
				return create_and_return_err();
			}

			//This could happen where we have nothing here
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
			}

			//There are two options here: We could have a conditional break
			//or a normal break. If there is no child node, we have a normal break
			if(ast_cursor->first_child == NULL){
				//Mark this for later
				current_block->block_terminal_type = BLOCK_TERM_TYPE_BREAK;

				//Loops always take precedence over switching, so we'll break out of that if it isn't null
				if(values->loop_stmt_end != NULL){
					//Special case for for loops
					if(values->for_loop_update_block != NULL){
						//In this special case, we don't want to add this as a direct successor
						//We will jump to it -- this is always an uncoditional jump
						emit_jmp_stmt(current_block, values->loop_stmt_end, JUMP_TYPE_JMP);
					} else {
						//Otherwise we need to break out of the loop
						add_successor(current_block, values->loop_stmt_end);
						//We will jump to it -- this is always an uncoditional jump
						emit_jmp_stmt(current_block, values->loop_stmt_end, JUMP_TYPE_JMP);
					}

				} else {
					//Save this for later on. We won't add it in now to preserve the
					//order of block chaining
					current_block->case_block_breaks_to = values->switch_statement_end;
					//We will jump to it -- this is always an uncoditional jump
					emit_jmp_stmt(current_block, values->switch_statement_end, JUMP_TYPE_JMP);
				}

				//If we see anything after this, it is unreachable so throw a warning
				if(ast_cursor->next_sibling != NULL){
					print_cfg_message(WARNING, "Unreachable code detected after break statement", ast_cursor->next_sibling->line_number);
					(*num_errors_ref)++;
				}

				//For a regular break statement, this is it, so we just get out
				//Give back the starting block
				return starting_block;

			//Otherwise, we have a conditional break, which will generate a conditional jump instruction
			} else {
				//Mark this for later again
				current_block->block_terminal_type = BLOCK_TERM_TYPE_BREAK;

				//First let's emit the conditional code
				expr_ret_package_t ret_package = emit_expr_code(current_block, ast_cursor->first_child);

				//There are two options here. If we're in a loop, that takes precedence. If we're in
				//a switch statement, then that takes precedence
				if(values->loop_stmt_start != NULL){
					//Now based on whatever we have in here, we'll emit the appropriate jump type(direct jump)
					jump_type_t jump_type = select_appropriate_jump_stmt(ret_package.operator, JUMP_CATEGORY_NORMAL);

					//Special case for for loops
					if(values->for_loop_update_block != NULL){
						//In this special case, we don't want to add anything as a direct successor
						//We will jump to it -- this is always an uncoditional jump
						emit_jmp_stmt(current_block, values->loop_stmt_end, jump_type);
					} else {
						//Otherwise we need to break out of the loop
						add_successor(current_block, values->loop_stmt_end);
						//We will jump to it -- this is always an uncoditional jump
						emit_jmp_stmt(current_block, values->loop_stmt_end, jump_type);
					}

				//We must've seen a switch statement then
				} else {
					//We'll save this in for later
					current_block->case_block_breaks_to = values->switch_statement_end;

					//Now based on whatever we have in here, we'll emit the appropriate jump type(direct jump)
					jump_type_t jump_type = select_appropriate_jump_stmt(ret_package.operator, JUMP_CATEGORY_NORMAL);

					//Emit our conditional jump now
					emit_jmp_stmt(current_block, values->switch_statement_end, jump_type);			
				}
			}

		//Handle a defer statement. Remember that a defer statment is one monolithic
		//node with a bunch of sub-nodes underneath that are all handleable by "expr"
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_DEFER_STMT){
			//This really shouldn't happen, but it can't hurt
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
			}

			//Grab a cursor here
			generic_ast_node_t* defer_stmt_cursor = ast_cursor->first_child;

			//Ollie lang uniquely allows the user to defer assembly statements. 
			//This can be useful IF you know what you're doing when it comes to assembly.
			//Since, if you defer assembly, that is the entire statement, we only
			//need to worry about emitting it once
			if(defer_stmt_cursor->CLASS == AST_NODE_CLASS_ASM_INLINE_STMT){
				//Emit the inline assembly that we need here
				emit_asm_inline_stmt(current_block, defer_stmt_cursor);

			//Otherwise it's just a regular deferral
			} else {
				//Run through all of the children, emitting their respective
				//expr codes
				while(defer_stmt_cursor != NULL){
					//Let the helper deal with it
					emit_expr_code(current_block, defer_stmt_cursor);
					//Move this up
					defer_stmt_cursor = defer_stmt_cursor->next_sibling;
				}			
			}


		//Handle a labeled statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_LABEL_STMT){
			//This really shouldn't happen, but it can't hurt
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
			}
			
			//We rely on the helper to do it for us
			emit_label_stmt_code(current_block, ast_cursor);

		//Handle a jump statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_JUMP_STMT){
			//This really shouldn't happen, but it can't hurt
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
			}

			//We rely on the helper to do it for us
			emit_jump_stmt_code(current_block, ast_cursor);

		//A very unique case exists in the switch statement. For a switch 
		//statement, we leverage some very unique properties of the enumerable
		//types that it uses
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_SWITCH_STMT){
			//Set the initial node
			values->initial_node = ast_cursor;

			//Visit the switch statement
			basic_block_t* switch_stmt_entry = visit_switch_statement(values);

			//If the starting block is NULL, then this is the starting block. Otherwise, it's the 
			//starting block's direct successor
			if(starting_block == NULL){
				starting_block = switch_stmt_entry;
			} else if(current_block->good_to_merge == TRUE){
				//Merge them if we can
				merge_blocks(current_block, switch_stmt_entry);
			} else {
				//Otherwise this is a direct successor
				add_successor(current_block, switch_stmt_entry);
			}

			//We need to drill to the end
			//Set this to be current
			current_block = switch_stmt_entry;

			//Once we're here the start is in current, we'll need to drill to the end
			while(current_block->direct_successor != NULL){
				current_block = current_block->direct_successor;
			}
			
		//We could also have a compound statement inside of here as well
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_COMPOUND_STMT){
			//Prime this here
			values->initial_node = ast_cursor;

			//We'll simply recall this function and let it handle it
			basic_block_t* compound_stmt_entry_block = visit_compound_statement(values);

			//Add in everything appropriately here
			if(starting_block == NULL){
				starting_block = compound_stmt_entry_block;
			} else {
				//TODO MAY OR MAY NOT KEEP
				add_successor(current_block, compound_stmt_entry_block);
			}

			//We need to drill to the end
			//Set this to be current
			current_block = compound_stmt_entry_block;

			//Once we're here the start is in current, we'll need to drill to the end
			while(current_block->direct_successor != NULL && current_block->block_terminal_type != BLOCK_TERM_TYPE_RET){
				current_block = current_block->direct_successor;
			}

			//If we did hit a return block here and there are nodes after this one in the chain, then we have
			//unreachable code
			if(current_block->block_terminal_type == BLOCK_TERM_TYPE_RET && ast_cursor->next_sibling != NULL){
				print_parse_message(WARNING, "Unreachable code detected after ret statement", ast_cursor->next_sibling->line_number);

			}

		//These are 100% user generated,
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_ASM_INLINE_STMT){
			//If we find an assembly inline statement, the actuality of it is
			//incredibly easy. All that we need to do is literally take the 
			//user's statement and insert it into the code

			//We'll need a new block here regardless
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
			}

			//Let the helper handle
			emit_asm_inline_stmt(current_block, ast_cursor);
		//Handle a nop statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_IDLE_STMT){
			//Do we need a new block?
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
			}

			//Let the helper handle -- doesn't even need the cursor
			emit_idle_stmt(current_block);
	
		//This means that we have some kind of expression statement
		} else {
			//This could happen where we have nothing here
			if(starting_block == NULL){
				starting_block = basic_block_alloc();
				current_block = starting_block;
			}
			
			//Also emit the simplified machine code
			emit_expr_code(current_block, ast_cursor);
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
	//Mark that this is a starting block
	function_starting_block->block_type = BLOCK_TYPE_FUNC_ENTRY;
	//The ending block
	basic_block_t* function_ending_block = basic_block_alloc();
	//We very clearly mark this as an ending block
	function_ending_block->is_exit_block = TRUE;

	//Grab the function record
	symtab_function_record_t* func_record = function_node->func_record;
	//Store this in the entry block
	function_starting_block->func_record = func_record;

	//We don't care about anything until we reach the compound statement
	generic_ast_node_t* func_cursor = function_node->first_child;

	//Developer error here
	if(func_cursor->CLASS != AST_NODE_CLASS_COMPOUND_STMT){
		print_parse_message(PARSE_ERROR, "Expected compound statement as only child to function declaration", func_cursor->line_number);
		exit(0);
	}

	//Package the values up
	values_package_t compound_stmt_values = pack_values(func_cursor, //Initial Node
											    	function_ending_block, //Function end block
											 		NULL, //Loop statement start
											 		NULL, //Exit block of loop
											 		NULL, //Switch statement end
											 		NULL, //If statement end
											 		NULL); //For loop update block

	//Once we get here, we know that func cursor is the compound statement that we want
	basic_block_t* compound_stmt_block = visit_compound_statement(&compound_stmt_values);

	//If this compound statement is NULL(which is possible) we just add the starting and ending
	//blocks as successors
	if(compound_stmt_block == NULL){
		add_successor(function_starting_block, function_ending_block);
		
		//We'll also throw a warning
		sprintf(info, "Function \"%s\" was given no body", function_node->func_record->func_name);
		print_cfg_message(WARNING, info, func_cursor->line_number);
		//One more warning
		(*num_warnings_ref)++;

	//Otherwise we merge them
	} else {
		//Once we're done with the compound statement, we will merge it into the function
		merge_blocks(function_starting_block, compound_stmt_block);
		//add_successor(function_starting_block, compound_stmt_block);
	}

	//Let's see if we actually made it all the way through and found a return
	basic_block_t* compound_stmt_cursor = function_starting_block;

	//Until we hit the end
	while(compound_stmt_cursor->direct_successor != NULL){
		compound_stmt_cursor = compound_stmt_cursor->direct_successor;
	}

	//Once we hit the end, if this isn't an exit block, we'll make it one
	compound_stmt_cursor->direct_successor = function_ending_block;

	//Once we get here, we'll now add in any deferred statements to the function ending block
	
	//So long as they aren't empty
	while(is_empty(deferred_stmts) == HEAP_STACK_NOT_EMPTY){
		//Add them in one by one
		add_statement(function_ending_block, pop(deferred_stmts));
	}

	//Now we'll analyze the reachability of the function
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

	//Emit the expression code
	emit_expr_code(decl_stmt_block, values->initial_node);

	//Give the block back
	return decl_stmt_block;
}


/**
 * Visit a let statement
 */
static basic_block_t* visit_let_statement(values_package_t* values){
	//Create the basic block
	basic_block_t* let_stmt_node = basic_block_alloc();

	emit_expr_code(let_stmt_node, values->initial_node);

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

	//Is the current block empty
	u_int8_t current_block_is_empty = FALSE;

	//So long as the AST cursor is not null
	while(ast_cursor != NULL){
		//Process a function statement
		if(ast_cursor->CLASS == AST_NODE_CLASS_FUNC_DEF){
			//Visit the function definition
			basic_block_t* function_block = visit_function_definition(ast_cursor);
			
			//If the start block is null, this becomes the start block
			if(start_block == NULL){
				start_block = function_block;
			//Otherwise, we'll add this as a successor to the current block
			} else {
				add_successor(current_block, function_block);
			}

			//We now need to find where the end of the function block is to have that as our current reference
			current_block = function_block;

			//So long as we don't see the exit statement, we keep going
			while(current_block->direct_successor->is_exit_block == FALSE){
				//Always follow the path of the direct successor
				current_block = current_block->direct_successor;
			}

			//If the current block is empty, we're going to merge it later on
			if(current_block->leader_statement == NULL){
				current_block_is_empty = TRUE;
			} else {
				current_block_is_empty = FALSE;
			}

			//Finally once we get down here, we have our proper current block

		//Process a let statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_LET_STMT){
			//Package the values up
			values_package_t values = pack_values(ast_cursor, //Initial Node
											    NULL, //Function end block
											 	NULL, //Loop statement start
											 	NULL, //Exit block of loop
											 	NULL, //Switch statement end
											 	NULL, //If statement end
											 	NULL); //For loop update block

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

			//These are not empty
			current_block_is_empty = FALSE;

		//Visit a declaration statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_DECL_STMT){
			//Package the values up
			values_package_t values = pack_values(ast_cursor, //Initial Node
											    NULL, //Function end block
											 	NULL, //Loop statement start
											 	NULL, //Exit block of loop
											 	NULL, //Switch statement end
											 	NULL, //If statement end
											 	NULL); //For loop update block

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

			//Not empty here either
			current_block_is_empty = FALSE;

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

	//Add this in
	type_symtab = results.type_symtab;

	//Create the stack here
	deferred_stmts = heap_stack_alloc();
	
	//Create the temp vars symtab
	temp_vars = variable_symtab_alloc();

	//We'll first create the fresh CFG here
	cfg_t* cfg = calloc(1, sizeof(cfg_t));

	//Create the dynamic array
	cfg->created_blocks = dynamic_array_alloc();

	//Hold the cfg
	cfg_ref = cfg;

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
	heap_stack_dealloc(deferred_stmts);
	
	//Destroy the temp variable symtab
	variable_symtab_dealloc(temp_vars);
	
	//Add all phi functions for SSA
	insert_phi_functions(cfg, results.variable_symtab);

	//FOR PRINTING
	emit_blocks_bfs(cfg, EMIT_DOMINANCE_FRONTIER);
	//emit_blocks_dfs(cfg);
	//emit_blocks_direct_successor(cfg);
	
	//Give back the reference
	return cfg;
}
