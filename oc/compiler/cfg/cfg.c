/**
 * The implementation file for all CFG related operations
 *
 * The CFG will translate the higher level code into something referred to as 
 * "Ollie Intermediate Representation Language"(OIR). This intermediary form 
 * is a hybrid of abstract machine code and assembly. Some operations, like 
 * jump commands, are able to be deciphered at this stage, and as such we do
 * so in the OIR
 *
 * This module will take an AST, put it into a CFG, put the CFG into SSA form, and pass it along to the optimizer
*/

#include "cfg.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "../queue/heap_queue.h"
#include "../jump_table/jump_table.h"

//For magic number removal
#define TRUE 1
#define FALSE 0

//For loops, we estimate that they'll execute 10 times each
#define LOOP_ESTIMATED_COST 10

//Our atomically incrementing integer
//If at any point a block has an ID of (-1), that means that it is in error and can be dealt with as such
static int32_t current_block_id = 0;
//Keep global references to the number of errors and warnings
u_int32_t* num_errors_ref;
u_int32_t* num_warnings_ref;
//Keep the type symtab up and running
type_symtab_t* type_symtab;
//The CFG that we're working with
cfg_t* cfg_ref;
//Keep a reference to whatever function we are currently in
symtab_function_record_t* current_function;
//The current function exit block. Unlike loops, these can't be nested, so this is totally fine
basic_block_t* function_exit_block = NULL;
//Keep ahold of our stack pointer
symtab_variable_record_t* stack_pointer = NULL;
//Keep a variable for it too
three_addr_var_t* stack_pointer_var = NULL;
//Store this for usage
static generic_type_t* u64 = NULL;
//The current stack offset for any given function
u_int64_t stack_offset = 0;
//For any/all error printing
char error_info[1500];

//Define a package return struct that is used by the binary op expression code
typedef struct{
	//The starting block of what we've made
	basic_block_t* starting_block;
	//The final block we end up with(only used for ternary operators)
	basic_block_t* final_block;
	//What is the final assignee
	three_addr_var_t* assignee;
	//What operator was used, if any
	Token operator;
} expr_ret_package_t;


//A package of values that each visit function uses
typedef struct {
	//The initial node
	generic_ast_node_t* initial_node;
	//For continue statements
	basic_block_t* loop_stmt_start;
	//For break statements
	basic_block_t* loop_stmt_end;
	//For any time we need to do for-loop operations
	basic_block_t* for_loop_update_block;
} values_package_t;


//Are we emitting the dominance frontier or not?
typedef enum{
	EMIT_DOMINANCE_FRONTIER,
	DO_NOT_EMIT_DOMINANCE_FRONTIER
} emit_dominance_frontier_selection_t;


//An enum for temp variable selection
typedef enum{
	USE_TEMP_VAR,
	PRESERVE_ORIG_VAR,
} temp_selection_t;


//An enum for declare and let statements letting us know what kind of variable
//that we have
typedef enum{
	VARIABLE_SCOPE_GLOBAL,
	VARIABLE_SCOPE_LOCAL,
} variable_scope_type_t;


//We predeclare up here to avoid needing any rearrangements
static basic_block_t* visit_declaration_statement(values_package_t* values, variable_scope_type_t scope);
static basic_block_t* visit_compound_statement(values_package_t* values);
static basic_block_t* visit_let_statement(values_package_t* values, variable_scope_type_t scope, u_int8_t is_branch_ending);
static basic_block_t* visit_if_statement(values_package_t* values);
static basic_block_t* visit_while_statement(values_package_t* values);
static basic_block_t* visit_do_while_statement(values_package_t* values);
static basic_block_t* visit_for_statement(values_package_t* values);
static basic_block_t* visit_case_statement(values_package_t* values);
static basic_block_t* visit_default_statement(values_package_t* values);
static basic_block_t* visit_switch_statement(values_package_t* values);

static expr_ret_package_t emit_binary_expression(basic_block_t* basic_block, generic_ast_node_t* logical_or_expr, u_int8_t is_branch_ending);
static expr_ret_package_t emit_ternary_expression(basic_block_t* basic_block, generic_ast_node_t* ternary_operation, u_int8_t is_branch_ending);
static three_addr_var_t* emit_binary_operation_with_constant(basic_block_t* basic_block, three_addr_var_t* assignee, three_addr_var_t* op1, Token op, three_addr_const_t* constant, u_int8_t is_branch_ending);
static three_addr_var_t* emit_function_call(basic_block_t* basic_block, generic_ast_node_t* function_call_node, u_int8_t is_branch_ending);
static three_addr_var_t* emit_unary_expr_code(basic_block_t* basic_block, generic_ast_node_t* unary_expr_parent, temp_selection_t use_temp, side_type_t side, u_int8_t is_branch_ending);
static expr_ret_package_t emit_expr_code(basic_block_t* basic_block, generic_ast_node_t* expr_node, u_int8_t is_branch_ending, u_int8_t check_for_coniditional);
static basic_block_t* basic_block_alloc(u_int32_t estimated_execution_frequency);

/**
 * Let's determine if a value is a positive power of 2.
 * Here's how this will work. In binary, powers of 2 look like:
 * 0010
 * 0100
 * 1000
 * ....
 *
 * In other words, they have exactly 1 on bit that is not in the LSB position
 *
 * Here's an example: 5 = 0101, so 5-1 = 0100
 *
 * 0101 & (0100) = 0100 which is 4, not 0
 *
 * How about 8?
 * 8 is 1000
 * 8 - 1 = 0111
 *
 * 1000 & 0111 = 0, so 8 is a power of 2
 *
 * Therefore, the formula we will use is value & (value - 1) == 0
 */
static u_int8_t is_power_of_2(int64_t value){
	//If it's negative or 0, we're done here
	if(value <= 0){
		return FALSE;
	}

	//Using the bitwise formula described above
	if((value & (value - 1)) == 0){
		return TRUE;
	} else {
		return FALSE;
	}
}


/**
 * This is a very simple helper function that will pack values for us. This is done to avoid repeated code
 */
static values_package_t pack_values(generic_ast_node_t* initial_node, basic_block_t* loop_stmt_start, basic_block_t* loop_stmt_end, basic_block_t* for_loop_update_block){
	//Allocate it
	values_package_t values;

	//Pack with all of our values
	values.initial_node = initial_node;
	values.loop_stmt_start = loop_stmt_start;
	values.loop_stmt_end = loop_stmt_end;
	values.for_loop_update_block = for_loop_update_block;

	//And give the copy back
	return values;
}


/**
 * We'll go through in the regular traversal, pushing each node onto the stack in
 * postorder. 
 */
static void reverse_post_order_traversal_rec(heap_stack_t* stack, basic_block_t* entry, u_int8_t use_reverse_cfg){
	//If we've already seen this then we're done
	if(entry->visited == TRUE){
		return;
	}

	//Mark it as visited
	entry->visited = TRUE;

	//Dependending on what we're doing here, we may be using reverse mode
	if(use_reverse_cfg == TRUE){
		//For every child(predecessor-it's reverse), we visit it as well
		for(u_int16_t _ = 0; entry->predecessors != NULL && _ < entry->predecessors->current_index; _++){
			//Visit each of the blocks
			reverse_post_order_traversal_rec(stack, dynamic_array_get_at(entry->predecessors, _), use_reverse_cfg);
		}
	} else {
		//We'll go in regular order
		//For every child(successor), we visit it as well
		for(u_int16_t _ = 0; entry->successors != NULL && _ < entry->successors->current_index; _++){
			//Visit each of the blocks
			reverse_post_order_traversal_rec(stack, dynamic_array_get_at(entry->successors, _), use_reverse_cfg);
		}
	}


	//Now we can push entry onto the stack
	push(stack, entry);
}


/**
 * Get and return a reverse post order traversal of a function-level CFG
 * 
 * NOTE: for data liveness problems, we have the option to compute this on the reverse cfg. This will
 * treat every successor like a predecessor, and vice versa
 */
dynamic_array_t* compute_reverse_post_order_traversal(basic_block_t* entry, u_int8_t use_reverse_cfg){
	//For our postorder traversal
	heap_stack_t* stack = heap_stack_alloc();
	//We'll need this eventually for postorder
	dynamic_array_t* reverse_post_order_traversal = dynamic_array_alloc();

	//If we are using the reverse tree, we'll need to reformulate entry to be exit
	if(use_reverse_cfg == TRUE){
		//Go all the way to the bottom
		while(entry->block_type != BLOCK_TYPE_FUNC_EXIT){
			entry = entry->direct_successor;
		}
	}

	//Invoke the recursive helper
	reverse_post_order_traversal_rec(stack, entry, use_reverse_cfg);

	//Now we'll pop everything off of the stack, and put it onto the RPO 
	//array in backwards order
	while(heap_stack_is_empty(stack) == HEAP_STACK_NOT_EMPTY){
		dynamic_array_add(reverse_post_order_traversal, pop(stack));
	}

	//And when we're done, get rid of the stack
	heap_stack_dealloc(stack);

	//Give back the reverse post order traversal
	return reverse_post_order_traversal;
}


/**
 * Reset all reverse post order sets
 */
void reset_reverse_post_order_sets(cfg_t* cfg){
	//Run through all of the function blocks
	for(u_int16_t _ = 0; _ < cfg->function_blocks->current_index; _++){
		//Grab the block out
		basic_block_t* function_entry_block = dynamic_array_get_at(cfg->function_blocks, _);

		//Set the RPO to be null
		if(function_entry_block->reverse_post_order != NULL){
			dynamic_array_dealloc(function_entry_block->reverse_post_order);
			function_entry_block->reverse_post_order = NULL;
		}

		//Set the RPO reverse CFG to be null
		if(function_entry_block->reverse_post_order_reverse_cfg != NULL){
			dynamic_array_dealloc(function_entry_block->reverse_post_order_reverse_cfg);
			function_entry_block->reverse_post_order_reverse_cfg = NULL;
		}
	}
}


/**
 * A recursive post order simplifies the code, so it's what we'll use here
 */
void post_order_traversal_rec(dynamic_array_t* post_order_traversal, basic_block_t* entry){
	//If we've visited this one before, skip
	if(entry->visited == TRUE){
		return;
	}

	//Otherwise mark that we've visited
	entry->visited = TRUE;

	//We will visit the children first
	for(u_int16_t _ = 0; entry->successors != NULL && _ < entry->successors->current_index; _++){
		//Recursive call to every child first
		post_order_traversal_rec(post_order_traversal, dynamic_array_get_at(entry->successors, _));
	}
	
	//Now we'll finally visit the node
	dynamic_array_add(post_order_traversal, entry);

	//And we're done
}


/**
 * Get and return the regular postorder traversal for a function-level CFG
 */
dynamic_array_t* compute_post_order_traversal(basic_block_t* entry){
	//Reset the visited status
	reset_visited_status(cfg_ref, FALSE);

	//Create our dynamic array
	dynamic_array_t* post_order_traversal = dynamic_array_alloc();

	//Make the recursive call
	post_order_traversal_rec(post_order_traversal, entry);

	//Give the traversal back
	return post_order_traversal;
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
	fprintf(stdout, "\n[LINE %d: COMPILER %s]: %s\n", line_number, type[parse_message.message], parse_message.info);
}


/**
 * A simple helper function that allows us to add a used variable into the block's
 * header. It is important to note that only actual variables(not temp variables) count
 * as live
 */
static void add_used_variable(basic_block_t* basic_block, three_addr_var_t* var){
	//If this is NULL, we'll need to allocate it
	if(basic_block->used_variables == NULL){
		basic_block->used_variables = dynamic_array_alloc();
	}

	//We need a special kind of comparison here, so we can't use the canned method
	for(u_int16_t i = 0; i < basic_block->used_variables->current_index; i++){
		//If the linked variables are the same, we're out
		if(((three_addr_var_t*)(basic_block->used_variables->internal_array[i]))->linked_var == var->linked_var){
			return;
		}
	}
	
	//we didn't find it, so we will add
	dynamic_array_add(basic_block->used_variables, var); 
}


/**
 * A simple helper function that allows us to add an assigned-to variable into the block's
 * header. It is important to note that only actual variables(not temp variables) count
 * as live
 */
static void add_assigned_variable(basic_block_t* basic_block, three_addr_var_t* var){
	//If the assigned variable dynamic array is NULL, we'll allocate it here
	if(basic_block->assigned_variables == NULL){
		basic_block->assigned_variables = dynamic_array_alloc();
	}

	//We need a special kind of comparison here, so we can't use the canned method
	for(u_int16_t i = 0; i < basic_block->assigned_variables->current_index; i++){
		//If the linked variables are the same, we're out
		if(((three_addr_var_t*)(basic_block->assigned_variables->internal_array[i]))->linked_var == var->linked_var){
			return;
		}
	}

	//We didn't find it, so we'll add it
	dynamic_array_add(basic_block->assigned_variables, var);
}


/**
 * Print a block our for reading
*/
static void print_block_three_addr_code(basic_block_t* block, emit_dominance_frontier_selection_t print_df){
	//If this is some kind of switch block, we first print the jump table
	if(block->block_type == BLOCK_TYPE_SWITCH || block->jump_table.nodes != NULL){
		print_jump_table(stdout, &(block->jump_table));
	}

	//Print the block's ID or the function name
	if(block->block_type == BLOCK_TYPE_FUNC_ENTRY){
		printf("%s", block->function_defined_in->func_name);
	} else {
		printf(".L%d", block->block_id);
	}

	//Now, we will print all of the active variables that this block has
	if(block->used_variables != NULL){
		printf("(");

		//Run through all of the live variables and print them out
		for(u_int16_t i = 0; i < block->used_variables->current_index; i++){
			//Print it out
			print_variable(stdout, block->used_variables->internal_array[i], PRINTING_VAR_BLOCK_HEADER);

			//If it isn't the very last one, we need a comma
			if(i != block->used_variables->current_index - 1){
				printf(", ");
			}
		}

		//Close it out here
		printf(")");
	}

	//We always need the colon and newline
	printf(":\n");

	//First print out the estimate frequency of execution
	printf("Estimated Execution Frequency: %d\n", block->estimated_execution_frequency);

	printf("Predecessors: {");

	for(u_int16_t i = 0; block->predecessors != NULL && i < block->predecessors->current_index; i++){
		basic_block_t* predecessor = block->predecessors->internal_array[i];

		//Print the block's ID or the function name
		if(predecessor->block_type == BLOCK_TYPE_FUNC_ENTRY){
			printf("%s", predecessor->function_defined_in->func_name);
		} else {
			printf(".L%d", predecessor->block_id);
		}

		if(i != block->predecessors->current_index - 1){
			printf(", ");
		}
	}

	printf("}\n");

	printf("Successors: {");

	for(u_int16_t i = 0; block->successors != NULL && i < block->successors->current_index; i++){
		basic_block_t* successor = block->successors->internal_array[i];

		//Print the block's ID or the function name
		if(successor->block_type == BLOCK_TYPE_FUNC_ENTRY){
			printf("%s", successor->function_defined_in->func_name);
		} else {
			printf(".L%d", successor->block_id);
		}

		if(i != block->successors->current_index - 1){
			printf(", ");
		}
	}

	printf("}\n");

	//If we have some assigned variables, we will dislay those for debugging
	if(block->assigned_variables != NULL){
		printf("Assigned: (");

		for(u_int16_t i = 0; i < block->assigned_variables->current_index; i++){
			print_variable(stdout, block->assigned_variables->internal_array[i], PRINTING_VAR_BLOCK_HEADER);

			//If it isn't the very last one, we need a comma
			if(i != block->assigned_variables->current_index - 1){
				printf(", ");
			}
		}
		printf(")\n");
	}

	//Now if we have LIVE_IN variables, we'll print those out
	if(block->live_in != NULL){
		printf("LIVE_IN: (");

		for(u_int16_t i = 0; i < block->live_in->current_index; i++){
			print_variable(stdout, block->live_in->internal_array[i], PRINTING_VAR_BLOCK_HEADER);

			//If it isn't the very last one, print out a comma
			if(i != block->live_in->current_index - 1){
				printf(", ");
			}
		}

		//Close it out
		printf(")\n");
	}

	//Now if we have LIVE_IN variables, we'll print those out
	if(block->live_out != NULL){
		printf("LIVE_OUT: (");

		for(u_int16_t i = 0; i < block->live_out->current_index; i++){
			print_variable(stdout, block->live_out->internal_array[i], PRINTING_VAR_BLOCK_HEADER);

			//If it isn't the very last one, print out a comma
			if(i != block->live_out->current_index - 1){
				printf(", ");
			}
		}

		//Close it out
		printf(")\n");
	}


	//Print out the dominance frontier if we're in DEBUG mode
	if(print_df == EMIT_DOMINANCE_FRONTIER && block->dominance_frontier != NULL){
		printf("Dominance frontier: {");

		//Run through and print them all out
		for(u_int16_t i = 0; i < block->dominance_frontier->current_index; i++){
			basic_block_t* printing_block = block->dominance_frontier->internal_array[i];

			//Print the block's ID or the function name
			if(printing_block->block_type == BLOCK_TYPE_FUNC_ENTRY){
				printf("%s", printing_block->function_defined_in->func_name);
			} else {
				printf(".L%d", printing_block->block_id);
			}

			//If it isn't the very last one, we need a comma
			if(i != block->dominance_frontier->current_index - 1){
				printf(", ");
			}
		}

		//And close it out
		printf("}\n");
	}

	//Print out the reverse dominance frontier if we're in DEBUG mode
	if(print_df == EMIT_DOMINANCE_FRONTIER && block->reverse_dominance_frontier != NULL){
		printf("Reverse Dominance frontier: {");

		//Run through and print them all out
		for(u_int16_t i = 0; i < block->reverse_dominance_frontier->current_index; i++){
			basic_block_t* printing_block = block->reverse_dominance_frontier->internal_array[i];

			//Print the block's ID or the function name
			if(printing_block->block_type == BLOCK_TYPE_FUNC_ENTRY){
				printf("%s", printing_block->function_defined_in->func_name);
			} else {
				printf(".L%d", printing_block->block_id);
			}

			//If it isn't the very last one, we need a comma
			if(i != block->reverse_dominance_frontier->current_index - 1){
				printf(", ");
			}
		}

		//And close it out
		printf("}\n");
	}


	//Only if this is false - global var blocks don't have any of these
	printf("Dominator set: {");

	//Run through and print them all out
	for(u_int16_t i = 0; i < block->dominator_set->current_index; i++){
		basic_block_t* printing_block = block->dominator_set->internal_array[i];

		//Print the block's ID or the function name
		if(printing_block->block_type == BLOCK_TYPE_FUNC_ENTRY){
			printf("%s", printing_block->function_defined_in->func_name);
		} else {
			printf(".L%d", printing_block->block_id);
		}
		//If it isn't the very last one, we need a comma
		if(i != block->dominator_set->current_index - 1){
			printf(", ");
		}
	}

	//And close it out
	printf("}\n");

	printf("Postdominator(reverse dominator) Set: {");

	//Run through and print them all out
	for(u_int16_t i = 0; i < block->postdominator_set->current_index; i++){
		basic_block_t* postdominator = block->postdominator_set->internal_array[i];

		//Print the block's ID or the function name
		if(postdominator->block_type == BLOCK_TYPE_FUNC_ENTRY){
			printf("%s", postdominator->function_defined_in->func_name);
		} else {
			printf(".L%d", postdominator->block_id);
		}
		//If it isn't the very last one, we need a comma
		if(i != block->postdominator_set->current_index - 1){
			printf(", ");
		}
	}

	//And close it out
	printf("}\n");

	//Now print out the dominator children
	printf("Dominator Children: {");

	for(u_int16_t i = 0; block->dominator_children != NULL && i < block->dominator_children->current_index; i++){
			basic_block_t* printing_block = block->dominator_children->internal_array[i];

			//Print the block's ID or the function name
			if(printing_block->block_type == BLOCK_TYPE_FUNC_ENTRY){
				printf("%s", printing_block->function_defined_in->func_name);
			} else {
				printf(".L%d", printing_block->block_id);
			}
			//If it isn't the very last one, we need a comma
			if(i != block->dominator_children->current_index - 1){
				printf(", ");
			}
	}

	printf("}\n");

	//Now grab a cursor and print out every statement that we 
	//have
	instruction_t* cursor = block->leader_statement;

	//So long as it isn't null
	while(cursor != NULL){
		//Hand off to printing method
		print_three_addr_code_stmt(stdout, cursor);
		//Move along to the next one
		cursor = cursor->next_statement;
	}

	//Some spacing
	printf("\n");
}


/**
 * Add a phi statement into the basic block. Phi statements are always added, without exception,
 * to the very front of the block
 *
 * This statement also takes care of the linking that we need to do. When we have a phi-function, we'll
 * need to link it back to whichever variables it refers to
 */
static void add_phi_statement(basic_block_t* target, instruction_t* phi_statement){
	//Generic fail case - this should never happen
	if(target == NULL){
		print_parse_message(PARSE_ERROR, "NULL BASIC BLOCK FOUND", 0);
		exit(1);
	}

	//Special case -- we're adding the head
	if(target->leader_statement == NULL || target->exit_statement == NULL){
		//Assign this to be the head and the tail
		target->leader_statement = phi_statement;
		target->exit_statement = phi_statement;
		//Mark the block that we're in
		phi_statement->block_contained_in = target;
		return;
	}

	//Otherwise we will add this in at the very front
	phi_statement->next_statement = target->leader_statement;
	//Update this reference
	target->leader_statement->previous_statement = phi_statement;
	//And then we can update this one
	target->leader_statement = phi_statement;

	//Mark what block we're in
	phi_statement->block_contained_in = target;
}


/**
 * Add a parameter to a phi statement
 */
static void add_phi_parameter(instruction_t* phi_statement, three_addr_var_t* var){
	//If we've not yet given the dynamic array
	if(phi_statement->phi_function_parameters == NULL){
		//Take care of allocation then
		phi_statement->phi_function_parameters = dynamic_array_alloc();
	}

	//Add this to the phi statement parameters
	dynamic_array_add(phi_statement->phi_function_parameters, var);
}


/**
 * Add a statement to the target block, following all standard linked-list protocol
 */
void add_statement(basic_block_t* target, instruction_t* statement_node){
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
		//Save what block we're in
		statement_node->block_contained_in = target;
		return;
	}

	//Otherwise, we are not dealing with the head. We'll simply tack this on to the tail
	target->exit_statement->next_statement = statement_node;
	//Mark this as the prior statement
	statement_node->previous_statement = target->exit_statement;

	//Update the tail reference
	target->exit_statement = statement_node;

	//Save what block we're in
	statement_node->block_contained_in = target;
}


/**
 * Delete a statement from the CFG - handling any/all edge cases that may arise
 */
void delete_statement(instruction_t* stmt){
	//Grab the block out
	basic_block_t* block = stmt->block_contained_in;

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
}


/**
 * Add a block to the dominance frontier of the first block
 */
static void add_block_to_dominance_frontier(basic_block_t* block, basic_block_t* df_block){
	//If the dominance frontier hasn't been allocated yet, we'll do that here
	if(block->dominance_frontier == NULL){
		block->dominance_frontier = dynamic_array_alloc();
	}

	//Let's just check - is this already in there. If it is, we will not add it
	for(u_int16_t i = 0; i < block->dominance_frontier->current_index; i++){
		//This is not a problem at all, we just won't add it
		if(block->dominance_frontier->internal_array[i] == df_block){
			return;
		}
	}

	//Add this into the dominance frontier
	dynamic_array_add(block->dominance_frontier, df_block);
}


/**
 * Add a block to the reverse dominance frontier of the first block
 */
static void add_block_to_reverse_dominance_frontier(basic_block_t* block, basic_block_t* rdf_block){
	//If the dominance frontier hasn't been allocated yet, we'll do that here
	if(block->reverse_dominance_frontier == NULL){
		block->reverse_dominance_frontier = dynamic_array_alloc();
	}

	//Let's just check - is this already in there. If it is, we will not add it
	for(u_int16_t i = 0; i < block->reverse_dominance_frontier->current_index; i++){
		//This is not a problem at all, we just won't add it
		if(block->reverse_dominance_frontier->internal_array[i] == rdf_block){
			return;
		}
	}

	//Add this into the dominance frontier
	dynamic_array_add(block->reverse_dominance_frontier, rdf_block);
}


/**
 * Does the block assign this variable? We'll do a simple linear scan to find out
 */
static u_int8_t does_block_assign_variable(basic_block_t* block, symtab_variable_record_t* variable){
	//Sanity check - if this is NULL then it's false by default
	if(block->assigned_variables == NULL){
		return FALSE;
	}

	//We'll need to use a special comparison here because we're comparing variables, not plain addresses.
	//We will compare the linked var of each block in the assigned variables dynamic array
	for(u_int16_t i = 0; i < block->assigned_variables->current_index; i++){
		//Grab the variable out
		three_addr_var_t* var = dynamic_array_get_at(block->assigned_variables, i);
		
		//Now we'll compare the linked variable to the record
		if(var->linked_var == variable){
			//If we found it bail out
			return TRUE;
		}
	}

	//If we make it all of the way down here and we didn't find it, fail out
	return FALSE;
}


/**
 * Grab the immediate dominator of the block
 * A IDOM B if A SDOM B and there does not exist a node C 
 * such that C ≠ A, C ≠ B, A dom C, and C dom B
 *
 */
static basic_block_t* immediate_dominator(basic_block_t* B){
	//If we've already found the immediate dominator, why find it again?
	//We'll just give that back
	if(B->immediate_dominator != NULL){
		return B->immediate_dominator;
	}

	//Regular variable declarations
	basic_block_t* A; 
	basic_block_t* C;
	u_int8_t A_is_IDOM;
	
	//For each node in B's Dominance Frontier set(we call this node A)
	//These nodes are our candidates for immediate dominator
	for(u_int16_t i = 0; B->dominator_set != NULL && i < B->dominator_set->current_index; i++){
		//By default we assume A is an IDOM
		A_is_IDOM = TRUE;

		//A is our "candidate" for possibly being an immediate dominator
		A = dynamic_array_get_at(B->dominator_set, i);

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
		for(u_int16_t j = 0; j < B->dominator_set->current_index; j++){
			//Skip this case
			if(i == j){
				continue;
			}

			//If it's aleady B or A, we're skipping
			C = dynamic_array_get_at(B->dominator_set, j);

			//If this is the case, disqualified
			if(C == B || C == A){
				continue;
			}

			//We can now see that C dominates B. The true test now is
			//if C is dominated by A. If that's the case, then we do NOT
			//have an immediate dominator in A.
			//
			//This would look like A -Doms> C -Doms> B, so A is not an immediate dominator
			if(dynamic_array_contains(C->dominator_set, A) != NOT_FOUND){
				//A is disqualified, it's not an IDOM
				A_is_IDOM = FALSE;
				break;
			}
		}

		//If we survived, then we're done here
		if(A_is_IDOM == TRUE){
			//Mark this for any future runs...we won't waste any time doing this
			//calculation over again
			B->immediate_dominator = A;
			return A;
		}
	}

	//Otherwise we didn't find it, so there is no immediate dominator
	return NULL;
}


/**
 * Grab the immediate postdominator dominator of the block
 * A IPDOM B if A SPDOM B and there does not exist a node C 
 * such that C ≠ A, C ≠ B, A pdom C, and C pdom B
 *
 */
static basic_block_t* immediate_postdominator(basic_block_t* B){
	//If we've already found the immediate dominator, why find it again?
	//We'll just give that back
	if(B->immediate_postdominator != NULL){
		return B->immediate_postdominator;
	}

	//Regular variable declarations
	basic_block_t* A; 
	basic_block_t* C;
	u_int8_t A_is_IPDOM;
	
	//For each node in B's PostDominantor set(we call this node A)
	//These nodes are our candidates for immediate postdominator
	for(u_int16_t i = 0; B->postdominator_set != NULL && i < B->postdominator_set->current_index; i++){
		//By default we assume A is a IPDOM
		A_is_IPDOM = TRUE;

		//A is our "candidate" for possibly being an immediate postdominator
		A = dynamic_array_get_at(B->postdominator_set, i);

		//If A == B, that means that A does NOT strictly postdominate(PDOM)
		//B, so it's disqualified
		if(A == B){
			continue;
		}

		//If we get here, we know that A SPDOM B
		//Now we must check, is there any "C" in the way.
		//We can tell if this is the case by checking every other
		//node in the dominance frontier of B, and seeing if that
		//node is also dominated by A
		
		//For everything in B's dominator set that IS NOT A, we need
		//to check if this is an intermediary. As in, does C get in-between
		//A and B in the dominance chain
		for(u_int16_t j = 0; j < B->postdominator_set->current_index; j++){
			//Skip this case
			if(i == j){
				continue;
			}

			//If it's aleady B or A, we're skipping
			C = dynamic_array_get_at(B->postdominator_set, j);

			//If this is the case, disqualified
			if(C == B || C == A){
				continue;
			}

			//We can now see that C dominates B. The true test now is
			//if C is dominated by A. If that's the case, then we do NOT
			//have an immediate dominator in A.
			//
			//This would look like A -PDoms> C -PDoms> B, so A is not an immediate postdominator
			if(dynamic_array_contains(C->postdominator_set, A) != NOT_FOUND){
				//A is disqualified, it's not an IDOM
				A_is_IPDOM = FALSE;
				break;
			}
		}

		//If we survived, then we're done here
		if(A_is_IPDOM == TRUE){
			//Mark this for any future runs...we won't waste any time doing this
			//calculation over again
			B->immediate_postdominator = A;
			return A;
		}
	}

	//Otherwise we didn't find it, so there is no immediate postdominator
	return NULL;
}


/**
 * Calculate the dominance frontiers of every block in the CFG
 *
 * The dominance frontier is every block, in relation to the current block, that:
 * 	Is a successor of a block that IS dominated by the current block
 * 		BUT
 * 	It itself is not dominated by the current block
 *
 * To think of it, it's essentially every block that is "just barely not dominated" by the current block
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
		if(block->predecessors == NULL || block->predecessors->current_index < 2){
			//Hop out here, there is no need to analyze further
			continue;
		}

		//A cursor for traversing our predecessors
		basic_block_t* cursor;

		//Now we run through every predecessor of the block
		for(u_int8_t i = 0; i < block->predecessors->current_index; i++){
			//Grab it out
			cursor = block->predecessors->internal_array[i];

			//While cursor is not the immediate dominator of block
			while(cursor != immediate_dominator(block)){
				//Add block to cursor's dominance frontier set
				add_block_to_dominance_frontier(cursor, block);
				
				//Cursor now becomes it's own immediate dominator, and
				//we crawl our way up the CFG
				cursor = immediate_dominator(cursor);
			}
		}
	}
}


/**
 * Calculate the reverse dominance frontiers of every block in the CFG
 *
 * The reverse dominance frontier is every block, in relation to the current block, that:
 * 	Is a predecessor of a block that IS postdominated by the current block
 * 		BUT
 * 	It itself is not postdominated by the current block
 *
 * To think of it, it's essentially every block that is "just barely not postdominated" by the current block
 *
 * Standard reverse dominance frontier algorithm:
 * 	for all nodes b in the CFG
 * 		if b has less than 2 successors 
 * 			continue
 * 		else
 * 			for all successors p of b
 * 				cursor = p
 * 				while cursor is not IPDOM(b)
 * 					add b to cursor RDF set
 * 					cursor = IPDOM(cursor)
 * 	
 */
static void calculate_reverse_dominance_frontiers(cfg_t* cfg){
	//Our metastructure will come in handy here, we'll be able to run through 
	//node by node and ensure that we've gotten everything
	
	//Run through every block
	for(u_int16_t i = 0; i < cfg->created_blocks->current_index; i++){
		//Grab this from the array
		basic_block_t* block = dynamic_array_get_at(cfg->created_blocks, i);

		//If we have less than 2 successors,the rest of 
		//the search here is useless
		if(block->successors == NULL || block->successors->current_index < 2){
			//Hop out here, there is no need to analyze further
			continue;
		}

		//A cursor for traversing our successors 
		basic_block_t* cursor;

		//Now we run through every successor of the block
		for(u_int8_t i = 0; i < block->successors->current_index; i++){
			//Grab it out
			cursor = block->successors->internal_array[i];

			//While cursor is not the immediate dominator of block
			while(cursor != immediate_postdominator(block)){
				//Add block to cursor's reverse dominance frontier set
				add_block_to_reverse_dominance_frontier(cursor, block);
				
				//Cursor now becomes it's own immediate postdominator, and
				//we crawl our down up the CFG
				cursor = immediate_postdominator(cursor);
			}
		}
	}
}


/**
 * Add a dominated block to the dominator block that we have
 */
static void add_dominated_block(basic_block_t* dominator, basic_block_t* dominated){
	//If this is NULL, then we'll allocate it right now
	if(dominator->dominator_children == NULL){
		dominator->dominator_children = dynamic_array_alloc();
	}

	//If we do not already have this in the dominator children, then we will add it
	if(dynamic_array_contains(dominator->dominator_children, dominated) == NOT_FOUND){
		dynamic_array_add(dominator->dominator_children, dominated);
	}
}


/**
 * Calculate the postdominator sets for each and every node
 *
 * Routing postdominators
 * 	For each basic block
 * 		if block is exit then Pdom <- exit else pdom = all nodes
 *
 * while change = true
 * 	changed = false
 * 	for each Basic Block w/o exit block
 * 		temp = BB + {x | x elem of intersect of pdom(s) | s is a successor to B}
 *
 * 		if temp != pdom
 * 			pdom = temp
 * 			change = true
 *
 *
 * We'll be using a change watcher algorithm for this one. This algorithm will repeat until a stable solution
 * is found
 */
static void calculate_postdominator_sets(cfg_t* cfg){
	//Reset the visited status
	reset_visited_status(cfg, FALSE);
	//The current block
	basic_block_t* current;

	//We'll first initialize everything here
	for(u_int16_t i = 0; i < cfg->created_blocks->current_index; i++){
		//Grab the block out
		current = dynamic_array_get_at(cfg->created_blocks, i);

		//If it's an exit block, then it's postdominator set just has itself
		if(current->block_type == BLOCK_TYPE_FUNC_EXIT){
			//If it's an exit block, then this set just contains itself
			current->postdominator_set = dynamic_array_alloc();
			//Add the block to it's own set
			dynamic_array_add(current->postdominator_set, current);
		} else {
			//If it's not an exit block, then we set this to be the entire body of blocks
			current->postdominator_set = clone_dynamic_array(cfg->created_blocks);
		}
	}

	//Now that we've initialized, we'll perform the same while change algorithm as before
	//For each and every function
	for(u_int16_t i = 0; i < cfg->function_blocks->current_index; i++){
		basic_block_t* current_function_block = dynamic_array_get_at(cfg->function_blocks, i);

		//If we don't have the RPO for this block, we'll make it now
		if(current_function_block->reverse_post_order == NULL){
			//We'll use false because we want the straightforward CFG, not the reverse one
			current_function_block->reverse_post_order = compute_reverse_post_order_traversal(current_function_block, FALSE);
		}

		//Have we seen a change
		u_int8_t changed;
		
		//Now we will go through everything in this blocks reverse post order set
		do {
			//By default, we'll assume there was no change
			changed = FALSE;

			//Now for each basic block in the reverse post order set
			for(u_int16_t _ = 0; _ < current_function_block->reverse_post_order->current_index; _++){
				//Grab the block out
				basic_block_t* current = dynamic_array_get_at(current_function_block->reverse_post_order, _);

				//If it's the exit block, we don't need to bother with it. The exit block is always postdominated
				//by itself
				if(current->block_type == BLOCK_TYPE_FUNC_EXIT){
					//Just go onto the next one
					continue;
				}

				//The temporary array that we will use as a holder for this iteration's postdominator set 
				dynamic_array_t* temp = dynamic_array_alloc();

				//The temp will always have this block in it
				dynamic_array_add(temp, current);

				//The temporary array also has the intersection of all of the successor's of BB's postdominator 
				//sets in it. As such, we'll now compute those

				//If this block has any successors
				if(current->successors != NULL){
					//Let's just grab out the very first successor
					basic_block_t* first_successor = dynamic_array_get_at(current->successors, 0);

					//Now, if a node IS in the set of all successor's postdominator sets, it must be in here. As such, any node that
					//is not in the set of all successors will NOT be in here, and every node that IS in the set
					//of all successors WILL be. So, we can just run through this entire array and see if each node is 
					//everywhere else

					//For each node in the first one's postdominator set
					for(u_int16_t k = 0; first_successor->postdominator_set != NULL && k < first_successor->postdominator_set->current_index; k++){
						//Are we in the intersection of the sets? By default we think so
						u_int8_t in_intersection = TRUE;

						//Grab out the postdominator
						basic_block_t* postdominator = dynamic_array_get_at(first_successor->postdominator_set, k);

						//Now let's see if this postdominator is in every other postdominator set for the remaining successors
						for(u_int16_t l = 1; l < current->successors->current_index; l++){
							//Grab the successor out
							basic_block_t* other_successor = dynamic_array_get_at(current->successors, l);

							//Now we'll check to see - is our given postdominator in this one's dominator set?
							//If it isn't, we'll set the flag and break out. If it is we'll move on to the next one
							if(dynamic_array_contains(other_successor->postdominator_set, postdominator) == NOT_FOUND){
								//We didn't find it, set the flag and get out
								in_intersection = FALSE;
								break;
							}

							//Otherwise we did find it, so we'll keep going
						}

						//By the time we make it here, we'll either have our flag set to true or false. If the postdominator
						//made it to true, it's in the intersection, and will add it to the new set
						if(in_intersection == TRUE){
							dynamic_array_add(temp, postdominator);
						}
					}
				}

				//Let's compare the two dynamic arrays - if they aren't the same, we've found a difference
				if(dynamic_arrays_equal(temp, current->postdominator_set) == FALSE){
					//Set the flag
					changed = TRUE;

					//And we can get rid of the old one
					dynamic_array_dealloc(current->postdominator_set);
					
					//Set temp to be the new postdominator set
					current->postdominator_set = temp;

				//Otherwise they weren't changed, so the new one that we made has to go
				} else {
					dynamic_array_dealloc(temp);
				}

				//And now we're onto the next block
			}
		} while(changed == TRUE);
	}
}


/**
 * Calculate the dominator sets for each and every node
 *
 * For each node in the nodeset:
 * 	dom(N) <- All nodes
 *	
 * Worklist = {StartNode}
 * while worklist is not empty
 * 	Remove any node Y from Worklist
 * 	New = {Y} U {X | X elem Pred(Y)}
 *
 * 	if new != dom 
 * 		Dom(Y) = New
 * 		For each successor X of Y
 * 			add X to the worklist
 *
 * This algorithm repeats indefinitely UNTIL a stable solution
 * is found(this is when new == DOM for every node, hence there's nowhere
 * left to go)
 *
 * NOTE: We repeat this for each and every function in the CFG. If blocks aren't in
 * the same function, then their dominance is completely unrelated
 */
static void calculate_dominator_sets(cfg_t* cfg){
	//Every node in the CFG has a dominator set that is set
	//to be identical to the list of all nodes
	for(u_int16_t i = 0; i < cfg->created_blocks->current_index; i++){
		//Grab this out
		basic_block_t* block = dynamic_array_get_at(cfg->created_blocks, i);

		//We will initialize the block's dominator set to be the entire set of nodes
		block->dominator_set = clone_dynamic_array(cfg->created_blocks);
	}

	//For each and every function that we have, we will perform this operation separately
	for(u_int16_t _ = 0; _ < cfg->function_blocks->current_index; _++){
		//Initialize a "worklist" dynamic array for this particular function
		dynamic_array_t* worklist = dynamic_array_alloc();

		//Add this into the worklist as a seed
		dynamic_array_add(worklist, dynamic_array_get_at(cfg->function_blocks, _));
		
		//The new dominance frontier that we have each time
		dynamic_array_t* new;

		//So long as the worklist is not empty
		while(dynamic_array_is_empty(worklist) == FALSE){
			//Remove a node Y from the worklist(remove from back - most efficient{O(1)})
			basic_block_t* Y = dynamic_array_delete_from_back(worklist);
			
			//Create the new dynamic array that will be used for the next
			//dominator set
			new = dynamic_array_alloc();

			//We will add Y into it's own dominator set
			dynamic_array_add(new, Y);

			//If Y has predecessors, we will find the intersection of
			//their dominator sets
			if(Y->predecessors != NULL){
				//Grab the very first predecessor's dominator set
				dynamic_array_t* pred_dom_set = ((basic_block_t*)(Y->predecessors->internal_array[0]))->dominator_set;

				//Are we in the intersection of the dominator sets?
				u_int8_t in_intersection;

				//We will now search every item in this dominator set
				for(u_int16_t i = 0; i < pred_dom_set->current_index; i++){
					//Grab the dominator out
					basic_block_t* dominator = dynamic_array_get_at(pred_dom_set, i);

					//By default we assume that this given dominator is in the set. If it
					//isn't we'll set it appropriately
					in_intersection = TRUE;

					/**
					 * An item is in the intersection if and only if it is contained 
					 * in all of the dominator sets of the predecessors of Y
					*/
					//We'll start at 1 here - we've already accounted for 0
					for(u_int8_t j = 1; j < Y->predecessors->current_index; j++){
						//Grab our other predecessor
						basic_block_t* other_predecessor = Y->predecessors->internal_array[j];

						//Now we will go over this predecessor's dominator set, and see if "dominator"
						//is also contained within it

						//Let's check for it in here. If we can't find it, we set the flag to false and bail out
						if(dynamic_array_contains(other_predecessor->dominator_set, dominator) == NOT_FOUND){
							in_intersection = FALSE;
							break;
						}
					
						//Otherwise we did find it, so we'll look at the next predecessor, and see if it is also
						//in there. If we get to the end and "in_intersection" is true, then we know that we've
						//found this one dominator in every single set
					}

					//If we get here and it is in the intersection, we can add it in
					//IMPORTANT: we also don't want to add a block in if it's from another
					//function. While other functions may appear on the page as one above
					//the other, there is no guarantee that a function will be called in
					//any particular order, or that it will be called at all. As such,
					//we exclude dominators that have different function records attached to
					//them
					if(in_intersection == TRUE){
						//Add the dominator in
						dynamic_array_add(new, dominator);
					}
				}
			}

			//Now we'll check - are these two dominator sets the same? If not, we'll need
			//to update them
			if(dynamic_arrays_equal(new, Y->dominator_set) == FALSE){
				//Destroy the old one
				dynamic_array_dealloc(Y->dominator_set);

				//And replace it with the new
				Y->dominator_set = new;

				//Now for every successor of Y, add it into the worklist
				for(u_int16_t i = 0; Y->successors != NULL && i < Y->successors->current_index; i++){
					dynamic_array_add(worklist, Y->successors->internal_array[i]);
				}

			//Otherwise they are the same
			} else {
				//Destroy the dominator set that we just made
				dynamic_array_dealloc(new);
			}
		}

		//Destroy the worklist now that we're done with it
		dynamic_array_dealloc(worklist);
	}
}


/**
 * A special helper function that we use for dynamic arrays of variables. Since variables
 * can be duplicated, we need to compare the symtab variable record, not the three address
 * variable itself
 */
static int16_t variable_dynamic_array_contains(dynamic_array_t* variable_array, three_addr_var_t* variable){
	//No question here -- we won't be finding it
	if(variable_array == NULL){
		return NOT_FOUND;
	}

	//We assume that everything in here is a variable and will cast as such
	three_addr_var_t* current_var;

	//Run through every record in here
	for(u_int16_t i = 0; i < variable_array->current_index; i++){
		//Grab a reference
		current_var = variable_array->internal_array[i];

		//If we found it, give back the index
		if(current_var->linked_var == variable->linked_var){
			return i;
		}
	}

	//We couldn't find this one, so give back not found
	return NOT_FOUND;
}


/**
 * A special helper function that we use for dynamic arrays of variables. Since variables
 * can be duplicated, we need to compare the symtab variable record, not the three address
 * variable itself
 */
static int16_t symtab_record_variable_dynamic_array_contains(dynamic_array_t* variable_array, symtab_variable_record_t* variable){
	//No question here -- we won't be finding it
	if(variable_array == NULL){
		return NOT_FOUND;
	}

	//We assume that everything in here is a variable and will cast as such
	three_addr_var_t* current_var;

	//Run through every record in here
	for(u_int16_t i = 0; i < variable_array->current_index; i++){
		//Grab a reference
		current_var = variable_array->internal_array[i];

		//If we found it, give back the index
		if(current_var->linked_var == variable){
			return i;
		}
	}

	//We couldn't find this one, so give back not found
	return NOT_FOUND;
}


/**
 * Are two variable dynamic arrays equal? We again use special rules for these kind of comparisons
 * that are not applicable to regular dynamic arrays
 */
static u_int8_t variable_dynamic_arrays_equal(dynamic_array_t* a, dynamic_array_t* b){
	//If either one is NULL we're out
	if(a == NULL || b == NULL){
		return FALSE;
	}

	//Are they the same size? If not they're not equal
	if(a->current_index != b->current_index){
		return FALSE;
	}

	//Otherwise we'll do a variable by variable comparison. If there's a variable
	//in a that isn't in b, we'll fail immediately
	for(int16_t i = a->current_index - 1; i >= 0; i--){
		//If b does not contain the variable in A, we're done
		if(variable_dynamic_array_contains(b, a->internal_array[i]) == NOT_FOUND){
			return FALSE;
		}
	}
	
	//If we make it down here then we know that they're the exact same
	return TRUE;
}


/**
 * Add a variable into a variable dynamic array. Again this requires special duplication
 * checks
 */
static void variable_dynamic_array_add(dynamic_array_t* array, three_addr_var_t* var){
	//We'll first check to see if it's already in there. If it isn't, we add
	if(variable_dynamic_array_contains(array, var) == NOT_FOUND){
		dynamic_array_add(array, var);
	}
	//Otherwise we don't add it
}


/**
 * Calculate the "live_in" and "live_out" sets for each basic block
 *
 * General algorithm
 *
 * for each block n
 * 	live_out[n] = {}
 * 	live_in[n] = {}
 *
 * for each block n in reverse order
 * 	in'[n] = in[n]
 * 	out'[n] = out[n]
 * 	in[n] = use[n] U (out[n] - def[n])
 * 	out[n] = {}U{x|x is an element of in[S] where S is a successor of n}
 *
 * NOTE: The algorithm converges very fast when the CFG is done in reverse order.
 * As such, we'll go back to front here
 *
 */
static void calculate_liveness_sets(cfg_t* cfg){
	//Reset the visited status
	reset_visited_status(cfg, FALSE);
	//Did we find a difference
	u_int8_t difference_found;

	//The "Prime" blocks are just ways to hold the old dynamic arrays
	dynamic_array_t* in_prime;
	dynamic_array_t* out_prime;

	//A cursor for the current block
	basic_block_t* current;

	do {
		//We'll assume we didn't find a difference each iteration
		difference_found = FALSE;

		//Run through all of the blocks backwards
		for(int16_t i = cfg->function_blocks->current_index - 1; i >= 0; i--){
			//Grab the block out
			basic_block_t* func_entry = dynamic_array_get_at(cfg->function_blocks, i);

			//Calculate the reverse post order in reverse mode for this block, if it doesn't
			//already exist
			if(func_entry->reverse_post_order_reverse_cfg == NULL){
				//True because we want this in reverse mode
				func_entry->reverse_post_order_reverse_cfg = compute_reverse_post_order_traversal(func_entry, TRUE);
			}

			//Now we can go through the entire RPO set
			for(u_int16_t _ = 0; _ < func_entry->reverse_post_order_reverse_cfg->current_index; _++){
				//The current block is whichever we grab
				current = dynamic_array_get_at(func_entry->reverse_post_order_reverse_cfg, _);

				//Transfer the pointers over
				in_prime = current->live_in;
				out_prime = current->live_out;

				//The live in is a combination of the variables used
				//at current and the difference of the LIVE_OUT variables defined
				//ones

				//Since we need all of the used variables, we'll just clone this
				//dynamic array so that we start off with them all
				current->live_in = clone_dynamic_array(current->used_variables);

				//Now we need to add every variable that is in LIVE_OUT but NOT in assigned
				for(u_int16_t j = 0; current->live_out != NULL && j < current->live_out->current_index; j++){
					//Grab a reference for our use
					three_addr_var_t* live_out_var = dynamic_array_get_at(current->live_out, j);

					//Now we need this block to be not in "assigned" also. If it is in assigned we can't
					//add it
					if(variable_dynamic_array_contains(current->assigned_variables, live_out_var) == NOT_FOUND){
						//If this is true we can add
						variable_dynamic_array_add(current->live_in, live_out_var);
					}
				}

				//Now we'll turn our attention to live out. The live out set for any block is the union of the
				//LIVE_IN set for all of it's successors
				
				//Set live out to be a new array
				current->live_out = dynamic_array_alloc();

				//Run through all of the successors
				for(u_int16_t k = 0; current->successors != NULL && k < current->successors->current_index; k++){
					//Grab the successor out
					basic_block_t* successor = dynamic_array_get_at(current->successors, k);

					//Add everything in his live_in set into the live_out set
					for(u_int16_t l = 0; successor->live_in != NULL && l < successor->live_in->current_index; l++){
						//Let's check to make sure we haven't already added this
						three_addr_var_t* successor_live_in_var = dynamic_array_get_at(successor->live_in, l);

						//Let the helper method do it for us
						variable_dynamic_array_add(current->live_out, successor_live_in_var);
					}
				}


				//Now we'll go through and check if the new live in and live out sets are different. If they are different,
				//we'll be doing this whole thing again

				//For efficiency - if there was a difference in one block, it's already done - no use in comparing
				if(difference_found == FALSE){
					//So we haven't found a difference so far - let's see if we can find one now
					if(variable_dynamic_arrays_equal(in_prime, current->live_in) == FALSE 
					  || variable_dynamic_arrays_equal(out_prime, current->live_out) == FALSE){
						//We have in fact found a difference
						difference_found = TRUE;
					}
				}

				//We made it down here, the prime variables are useless. We'll deallocate them
				dynamic_array_dealloc(in_prime);
				dynamic_array_dealloc(out_prime);
			}
		}
	//So long as we continue finding differences
	} while(difference_found == TRUE);
}


/**
 * Build the dominator tree for each function in the CFG
 */
static void build_dominator_trees(cfg_t* cfg, u_int8_t build_fresh){
	//For each node in the CFG, we will use that node's immediate dominators to
	//build a dominator tree
	
	//Hold the current block
	basic_block_t* current;

	//For each block in the CFG
	for(int16_t _ = cfg->created_blocks->current_index - 1; _ >= 0; _--){
		//Grab out whatever block we're on
		current = dynamic_array_get_at(cfg->created_blocks, _);

		//We will find this block's "immediate dominator". Once we have that,
		//we will add this block to the "dominator children" set of said immediate
		//dominator
		basic_block_t* immediate_dom = immediate_dominator(current);

		//Now we'll go to the immediate dominator's list and add the dominated block in. Of course,
		//we'll account for the case where there is no immediate dominator. This is possible in
		//the case of function entry blocks
		if(immediate_dom != NULL){
			add_dominated_block(immediate_dom, current);
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
 * 		For each of these basic blocks that contains the variable as assigned to then 
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
	//A cursor that we can use
	basic_block_t* block_cursor;
	//Once we're done with all of this, we're finally ready to insert phi functions

	//------------------------------------------
	// FIRST STEP: FOR EACH variable we have
	//------------------------------------------
	//Run through all of the sheafs
	for	(u_int16_t i = 0; i < var_symtab->sheafs->current_index; i++){
		//Grab the current sheaf
		sheaf_cursor = dynamic_array_get_at(var_symtab->sheafs, i);

		//Now we'll free all non-null records
		for(u_int16_t j = 0; j < KEYSPACE; j++){
			//Grab the record
			record = sheaf_cursor->records[j];

			//Remember that symtab records can be chained in case
			//of hash collisions, so we need to run through every
			//variable like this
			while(record != NULL){
				//----------------------------------
				// SECOND STEP: For each block that 
				// defines(assigns) said variable
				//----------------------------------
				//We'll need a "worklist" here - just a dynamic array 
				dynamic_array_t* worklist = dynamic_array_alloc();
				//Keep track of those that were ever on the worklist
				dynamic_array_t* ever_on_worklist;

				//We'll also need a dynamic array that contains everything relating
				//to if we did or did not already put a phi function into this block
				dynamic_array_t* already_has_phi_func = dynamic_array_alloc();
				
				//Just run through the entire thing
				for(u_int16_t i = 0; i < cfg->created_blocks->current_index; i++){
					//Grab the block out of here
					block_cursor = dynamic_array_get_at(cfg->created_blocks, i);

					//Does this block define(assign) our variable?
					if(does_block_assign_variable(block_cursor, record) == TRUE){
						//Then we add this block onto the "worklist"
						dynamic_array_add(worklist, block_cursor);
					}
				}

				//Now we can clone the "was ever on worklist" dynamic array
				ever_on_worklist = clone_dynamic_array(worklist);

				//Now we can actually perform our insertions
				//So long as the worklist is not empty
				while(dynamic_array_is_empty(worklist) == FALSE){
					//Remove the node from the back - more efficient
					basic_block_t* node = dynamic_array_delete_from_back(worklist);
					
					//Now we will go through each node in this worklist's dominance frontier
					for(u_int16_t j = 0; node->dominance_frontier != NULL && j < node->dominance_frontier->current_index; j++){
						//Grab this node out
						basic_block_t* df_node = dynamic_array_get_at(node->dominance_frontier, j);

						//If this node already has a phi function, we're not gonna bother with it
						if(dynamic_array_contains(already_has_phi_func, df_node) != NOT_FOUND){
							//We DID find it, so we will NOT add anything, it already has one
							continue;
						}

						//Let's check to see if we really need one here.
						//----------------------------------------
						// CRITERION:
						// If a variable is NOT Live-out at the join node,
						// that means that it is not LIVE-IN at any of
						// the successors of that block. If a variable
						// is not active(used) at the join node either,
						// that means that the phi function is useless.
						//
						// So, we will skip inserting a phi function
						// if the variable is not used and not LIVE_OUT
						// at N
						//----------------------------------------

						//Let's see if we can find it in one of these. We'll record if we can
						if(symtab_record_variable_dynamic_array_contains(df_node->used_variables, record) == NOT_FOUND
							&& symtab_record_variable_dynamic_array_contains(df_node->live_out, record) == NOT_FOUND){
							continue;
						}

						//If we make it here that means that we don't already have one, so we'll add it
						//This function only emits the skeleton of a phi function
						instruction_t* phi_stmt = emit_phi_function(record, record->type_defined_as);

						//Add the phi statement into the block	
						add_phi_statement(df_node, phi_stmt);

						//We'll mark that this block already has one for the future
						dynamic_array_add(already_has_phi_func, df_node); 

						//If this node has not ever been on the worklist, we'll add it
						//to keep the search going
						if(dynamic_array_contains(ever_on_worklist, df_node) == NOT_FOUND){
							//We did NOT find it, so we WILL add it
							dynamic_array_add(worklist, df_node);
							dynamic_array_add(ever_on_worklist, df_node);
						}
					}
				}

				//Now that we're done with these, we'll remove them for
				//the next round
				dynamic_array_dealloc(worklist);
				dynamic_array_dealloc(ever_on_worklist);
				dynamic_array_dealloc(already_has_phi_func);
			
				//Advance to the next record in the chain
				record = record->next;
			}
		}
	}
}


/**
 * Generate a new name for the given three address variable
 */
static void lhs_new_name(three_addr_var_t* var){
	//Grab the linked variable out
	symtab_variable_record_t* linked_var = var->linked_var;

	//Grab the name out of the counter
	u_int16_t generation_level = linked_var->counter;

	//Now we increment the counter for the next go around
	(linked_var->counter)++;

	//We'll also push this generation level onto the stack
	lightstack_push(&(linked_var->counter_stack), generation_level);

	//Store the generation level in here
	var->ssa_generation = generation_level;
}


/**
 * Rename the variable with the top of the stack. This DOES NOT
 * manipulate the stack in any way
 */
static void rhs_new_name(three_addr_var_t* var){
	//Grab the linked var out
	symtab_variable_record_t* linked_var = var->linked_var;

	//Grab the value off of the stack
	u_int16_t generation_level = lightstack_peek(&(linked_var->counter_stack));

	//Store the generation level in here
	var->ssa_generation = generation_level;
}


/**
 * Rename all variables to be in SSA form. This is the final step in our conversion
 *
 * Algorithm:
 *
 * rename(){
 * 	for each block b
 * 		if b previously visited continue
 * 		for each phi-function p in b
 * 			v = LHS(p)
 * 			vn = GenName(v) and replace v with vn
 * 		for each statement s in b
 * 			for each variable v in the RHS of s
 * 				replace V with Top(Stacks[V]);
 * 			for each variable V in the LHS
 * 				vn = GenName(V) and replace v with vn
 * 			for each CFG successor of b
 * 				j <- position in s's phi-functon belonging to b
 * 				for each phi function p in s
 * 					replace the jth operand of RHS(p) with Top(Stacks[V])
 * 			for each s in the dominator children of b
 * 				Rename(s)
 * 			for each phi-function or statement t in b
 * 				for each vi in the LHS(T)
 * 					pop(Stacks[V])
 * }
 */
static void rename_block(basic_block_t* entry){
	//If we've previously visited this block, then return
	if(entry->visited == TRUE){
		return;
	}

	//Otherwise we'll flag it for the future
	entry->visited = TRUE;

	//Grab out our leader statement here. We will iterate over all statements
	//looking for phi functions
	instruction_t* cursor = entry->leader_statement;

	//So long as this isn't null
	while(cursor != NULL){
		//First option - if we encounter a phi function
		if(cursor->CLASS == THREE_ADDR_CODE_PHI_FUNC){
			//We will rewrite the assigneed of the phi function(LHS)
			//with the new name
			lhs_new_name(cursor->assignee);

		//And now if it's anything else that has an assignee, operands, etc,
		//we'll need to rewrite all of those as well
		//We'll exclude direct jump statements, these we don't care about
		} else if(cursor->CLASS != THREE_ADDR_CODE_DIR_JUMP_STMT && cursor->CLASS != THREE_ADDR_CODE_LABEL_STMT){
			//If we get here we know that we don't have a phi function

			//If we have a non-temp variable, rename it
			if(cursor->op1 != NULL && cursor->op1->is_temporary == FALSE){
				rhs_new_name(cursor->op1);
			}

			//If we have a non-temp variable, rename it
			if(cursor->op2 != NULL && cursor->op2->is_temporary == FALSE){
				rhs_new_name(cursor->op2);
			}

			//Same goes for the assignee, except this one is the LHS
			if(cursor->assignee != NULL && cursor->assignee->is_temporary == FALSE){
				lhs_new_name(cursor->assignee);
			}

			//Special case - do we have a function call?
			if(cursor->CLASS == THREE_ADDR_CODE_FUNC_CALL){
				//Grab it out
				dynamic_array_t* func_params = cursor->function_parameters;
				//Run through them all
				for(u_int16_t k = 0; func_params != NULL && k < func_params->current_index; k++){
					//Grab it out
					three_addr_var_t* current_param = dynamic_array_get_at(func_params, k);

					//If it's not temporary, we rename
					if(current_param->is_temporary == FALSE){
						rhs_new_name(current_param);
					}
				}
			}
		}

		//Advance up to the next statement
		cursor = cursor->next_statement;
	}

	//Now for each successor of b, we'll need to add the phi-function parameters according
	for(u_int16_t _ = 0; entry->successors != NULL && _ < entry->successors->current_index; _++){
		//Grab the successor out
		basic_block_t* successor = dynamic_array_get_at(entry->successors, _);

		//Now for each phi-function in this successor that uses something in the defined variables
		//here, we'll want to add that newly renamed defined variable into the phi function parameters
		
		//Yet another cursor
		instruction_t* succ_cursor = successor->leader_statement;

		//So long as it isn't null AND it's a phi function
		while(succ_cursor != NULL && succ_cursor->CLASS == THREE_ADDR_CODE_PHI_FUNC){
			//We have a phi function, so what are we assigning to it?
			symtab_variable_record_t* phi_func_var = succ_cursor->assignee->linked_var;

			//Emit a new variable for this one
			three_addr_var_t* phi_func_param = emit_var(phi_func_var, FALSE);

			//Emit the name for this variable
			rhs_new_name(phi_func_param);

			//Now add it into the phi function
			add_phi_parameter(succ_cursor, phi_func_param);

			//Advance this up
			succ_cursor = succ_cursor->next_statement;
		}
	}

	//Now that we're done with the renaming, we'll go through each dominator child in this node
	//and perform the same operation
	for(u_int16_t _ = 0; entry->dominator_children != NULL && _ < entry->dominator_children->current_index; _++){
		rename_block(dynamic_array_get_at(entry->dominator_children, _));
	}

	//Once we're done, we'll need to unwind our stack here. Anything that involves an assignee, we'll
	//need to pop it's stack so we don't have excessive variable numbers. We'll now iterate over again
	//and perform pops whereever we see a variable being assigned
	
	//Grab the cursor again
	cursor = entry->leader_statement;
	while(cursor != NULL){
		//If we see a statement that has an assignee that is not temporary, we'll unwind(pop) his stack
		if(cursor->CLASS != THREE_ADDR_CODE_DIR_JUMP_STMT && cursor->CLASS != THREE_ADDR_CODE_LABEL_STMT &&
			cursor->assignee != NULL && cursor->assignee->is_temporary == FALSE){
			//Pop it off
			lightstack_pop(&(cursor->assignee->linked_var->counter_stack));
		}

		//Advance to the next one
		cursor = cursor->next_statement;
	}
}


/**
 * Rename all of the variables in the CFG
 */
static void rename_all_variables(cfg_t* cfg){
	//Before we do this - let's reset the entire CFG
	reset_visited_status(cfg, FALSE);

	//We will call the rename block function on the first block
	//for each of our functions. The rename block function is 
	//recursive, so that should in theory take care of everything for us
	
	//For each function block
	for(u_int16_t _ = 0; _ < cfg->function_blocks->current_index; _++){
		//Invoke the rename function on it
		rename_block(dynamic_array_get_at(cfg->function_blocks, _));
	}
}


/**
 * Emit a pointer arithmetic statement that can arise from either a ++ or -- on a pointer
 */
static three_addr_var_t* handle_pointer_arithmetic(basic_block_t* basic_block, Token operator, three_addr_var_t* assignee, u_int8_t is_branch_ending){
	//Emit the constant size
	three_addr_const_t* constant = emit_long_constant_direct(assignee->type->pointer_type->points_to->type_size, type_symtab);

	//We need this temp assignment for bookkeeping reasons
	instruction_t* temp_assignment = emit_assignment_instruction(emit_temp_var(assignee->type), assignee);
	temp_assignment->is_branch_ending = is_branch_ending;

	//Add this to the block
	add_statement(basic_block, temp_assignment);

	//Decide what the op is
	Token op = operator == PLUSPLUS ? PLUS : MINUS;

	//We need to emit a temp assignment for the assignee
	instruction_t* operation = emit_binary_operation_with_const_instruction(emit_temp_var(assignee->type), temp_assignment->assignee, op, constant);
	operation->is_branch_ending = is_branch_ending;

	//Add this to the block
	add_statement(basic_block, operation);

	//We need one final assignment
	instruction_t* final_assignment = emit_assignment_instruction(emit_var_copy(assignee), operation->assignee);
	final_assignment->is_branch_ending = is_branch_ending;

	//And add this one in
	add_statement(basic_block, final_assignment);

	//Give back the assignee
	return assignee;
}


/**
 * Emit a statement that fits the definition of a lea statement. This usually takes the
 * form of address computations
 */
static three_addr_var_t* emit_lea(basic_block_t* basic_block, three_addr_var_t* base_addr, three_addr_var_t* offset, generic_type_t* base_type, u_int8_t is_branch_ending){
	//We need a new temp var for the assignee. We know it's an address always
	three_addr_var_t* assignee = emit_temp_var(base_addr->type);

	//If the base addr is not temporary, this counts as a read
	if(base_addr->is_temporary == FALSE){
		add_used_variable(basic_block, base_addr);
	}

	//Now we leverage the helper to emit this
	instruction_t* stmt = emit_lea_instruction(assignee, base_addr, offset, base_type->type_size);

	//Mark this with whatever was passed through
	stmt->is_branch_ending = is_branch_ending;

	//Now add the statement into the block
	add_statement(basic_block, stmt);

	//And give back the assignee
	return assignee;
}


/**
 * Emit an address calculation that would not work if we used a lea because the base_type is not a power of 2
 */
static three_addr_var_t* emit_address_offset_calc(basic_block_t* basic_block, three_addr_var_t* base_addr, three_addr_var_t* offset, generic_type_t* base_type, u_int8_t is_branch_ending){
	//We'll need the size to multiply by
	three_addr_const_t* type_size = emit_unsigned_int_constant_direct(base_type->type_size, type_symtab);

	//We'll need a temp assignment if this isn't temporary
	if(offset->is_temporary == FALSE){
		//Create the statement
		instruction_t* temp_assignment = emit_assignment_instruction(emit_temp_var(offset->type), offset);

		//Add it to the block
		add_statement(basic_block, temp_assignment);

		//Reassign this
		offset = temp_assignment->assignee;
	}

	//Now we emit the offset multiplication
	three_addr_var_t* total_offset = emit_binary_operation_with_constant(basic_block, offset, offset, STAR, type_size, is_branch_ending);

	//Once we have the total offset, we add it to the base address
	instruction_t* result = emit_binary_operation_instruction(emit_temp_var(u64), base_addr, PLUS, total_offset);
	
	//Add this into the block
	add_statement(basic_block, result);

	//Give back whatever we assigned
	return result->assignee;
}


/**
 * Emit a construct access lea statement
 */
static three_addr_var_t* emit_construct_address_calculation(basic_block_t* basic_block, three_addr_var_t* base_addr, three_addr_const_t* offset, u_int8_t is_branch_ending){
	//We need a new temp var for the assignee. We know it's an address always
	three_addr_var_t* assignee = emit_temp_var(u64);

	//If the base addr is not temporary, this counts as a read
	if(base_addr->is_temporary == FALSE){
		add_used_variable(basic_block, base_addr);
	}

	//Now we leverage the helper to emit this
	instruction_t* stmt = emit_binary_operation_with_const_instruction(assignee, base_addr, PLUS, offset);

	//Mark this with whatever was passed through
	stmt->is_branch_ending = is_branch_ending;

	//Now add the statement into the block
	add_statement(basic_block, stmt);

	//And give back the assignee
	return assignee;
}


/**
 * Emit an indirect jump statement
 */
static three_addr_var_t* emit_indirect_jump_address_calculation(basic_block_t* basic_block, jump_table_t* initial_address, three_addr_var_t* mutliplicand, u_int8_t is_branch_ending){
	//We'll need a new temp var for the assignee
	three_addr_var_t* assignee = emit_temp_var(lookup_type_name_only(type_symtab, "label")->type);

	//If the multiplicand is not temporary we have a new used variable
	if(mutliplicand->is_temporary == FALSE){
		add_used_variable(basic_block, mutliplicand);
	}

	//Use the helper to emit it - type size is 8 because it's an address
	instruction_t* stmt = emit_indir_jump_address_calc_instruction(assignee, initial_address, mutliplicand, 8);

	//Mark it as branch ending
	stmt->is_branch_ending = is_branch_ending;

	//Add it in
	add_statement(basic_block, stmt);

	//Give back the assignee
	return assignee;
}


/**
 * Directly emit the assembly nop instruction
 */
static void emit_idle(basic_block_t* basic_block, u_int8_t is_branch_ending){
	//Use the helper
	instruction_t* idle_stmt = emit_idle_instruction();

	//Mark this with whatever was passed through
	idle_stmt->is_branch_ending = is_branch_ending;
	
	//Add it into the block
	add_statement(basic_block, idle_stmt);

	//And that's all
}


/**
 * Directly emit the assembly code for an inlined statement. Users who write assembly inline
 * want it directly inserted in order, nothing more, nothing less
 */
static void emit_assembly_inline(basic_block_t* basic_block, generic_ast_node_t* asm_inline_node, u_int8_t is_branch_ending){
	//First we allocate the whole thing
	instruction_t* asm_inline_stmt = emit_asm_inline_instruction(asm_inline_node->node); 
	
	//Mark this with whatever was passed through
	asm_inline_stmt->is_branch_ending =is_branch_ending;
	
	//Once done we add it into the block
	add_statement(basic_block, asm_inline_stmt);
	
	//And that's all
}


/**
 * Emit the abstract machine code for a return statement
 */
static void emit_ret(basic_block_t* basic_block, generic_ast_node_t* ret_node, u_int8_t is_branch_ending){
	//For holding our temporary return variable
	expr_ret_package_t package;

	//Is null by default
	package.assignee = NULL;

	//This is what we'll be using to return
	three_addr_var_t* return_variable = NULL;

	//If the ret node's first child is not null, we'll let the expression rule
	//handle it. We'll always do an assignment here because return statements present
	//a special case. We always need our return variable to be in %rax, and that may
	//not happen all the time naturally. As such, we need this assignment here
	if(ret_node->first_child != NULL){
		//Perform the binary operation here
		package = emit_expr_code(basic_block, ret_node->first_child, is_branch_ending, FALSE);

		//Emit the temp assignment
		instruction_t* assn_stmt = emit_assignment_instruction(emit_temp_var(package.assignee->type), package.assignee);
		//Add it into the block
		add_statement(basic_block, assn_stmt);
		//The return variable is now what was assigned
		return_variable	= assn_stmt->assignee;
	}

	//We'll use the ret stmt feature here
	instruction_t* ret_stmt = emit_ret_instruction(return_variable);

	//Mark this with whatever was passed through
	ret_stmt->is_branch_ending = is_branch_ending;

	//Once it's been emitted, we'll add it in as a statement
	add_statement(basic_block, ret_stmt);
}


/**
 * Emit the abstract machine code for a label statement
 */
static void emit_label(basic_block_t* basic_block, generic_ast_node_t* label_node, u_int8_t is_branch_ending){
	//Emit the appropriate variable
	three_addr_var_t* label_var = emit_var(label_node->variable, TRUE);

	//This is a special case here -- these don't really count as variables
	//in the way that most do. As such, we will not add it in as live

	//We'll just use the helper to emit this
	instruction_t* stmt = emit_label_instruction(label_var);

	//Mark with whatever was passed through
	stmt->is_branch_ending = is_branch_ending;

	//Add this statement into the block
	add_statement(basic_block, stmt);
}


/**
 * Emit the abstract machine code for a jump statement
 */
static void emit_direct_jump(basic_block_t* basic_block, generic_ast_node_t* jump_statement, u_int8_t is_branch_ending){
	//Emit the appropriate variable
	three_addr_var_t* label_var = emit_var(jump_statement->variable, TRUE);

	//This is a special case here -- these don't really count as variables
	//in the way that most do. As such, we will not add it in as live
	
	//We'll just use the helper to do this
	instruction_t* stmt = emit_direct_jmp_instruction(label_var);

	//Is this branch ending?
	stmt->is_branch_ending = is_branch_ending;

	//Add this statement into the block
	add_statement(basic_block, stmt);
}


/**
 * Emit a jump statement jumping to the destination block, using the jump type that we
 * provide
 */
void emit_jump(basic_block_t* basic_block, basic_block_t* dest_block, jump_type_t type, u_int8_t is_branch_ending, u_int8_t inverse_jump){
	//Use the helper function to emit the statement
	instruction_t* stmt = emit_jmp_instruction(dest_block, type);

	//Is this branch ending?
	stmt->is_branch_ending = is_branch_ending;

	//Mark where we came from
	stmt->block_contained_in = basic_block;

	//Is this an inverse jump? Important for optimization down the line
	stmt->inverse_jump = inverse_jump;

	//Add this into the first block
	add_statement(basic_block, stmt);
}


/**
 * Emit an indirect jump statement
 *
 * Indirect jumps are written in the form:
 * 	jump *__var__, where var holds the address that we need
 */
void emit_indirect_jump(basic_block_t* basic_block, three_addr_var_t* dest_addr, jump_type_t type, u_int8_t is_branch_ending){
	//Use the helper function to create it
	instruction_t* indirect_jump = emit_indirect_jmp_instruction(dest_addr, type);

	//Is it branch ending?
	indirect_jump->is_branch_ending = is_branch_ending;

	//Now we'll add it into the block
	add_statement(basic_block, indirect_jump);
}


/**
 * Emit the abstract machine code for a constant to variable assignment. 
 */
static three_addr_var_t* emit_constant_assignment(basic_block_t* basic_block, generic_ast_node_t* constant_node, u_int8_t is_branch_ending){
	//We'll use the constant var feature here
	instruction_t* const_var = emit_assignment_with_const_instruction(emit_temp_var(constant_node->inferred_type), emit_constant(constant_node));

	//Mark this with whatever was passed through
	const_var->is_branch_ending = is_branch_ending;
	
	//Add this into the basic block
	add_statement(basic_block, const_var);

	//Now give back the assignee variable
	return const_var->assignee;
}


/**
 * Emit the abstract machine code for a constant to variable assignment. 
 */
static three_addr_var_t* emit_direct_constant_assignment(basic_block_t* basic_block, three_addr_const_t* constant, generic_type_t* inferred_type, u_int8_t is_branch_ending){
	//We'll use the constant var feature here
	instruction_t* const_var = emit_assignment_with_const_instruction(emit_temp_var(inferred_type), constant);

	//Mark this with whatever was passed through
	const_var->is_branch_ending = is_branch_ending;
	
	//Add this into the basic block
	add_statement(basic_block, const_var);

	//Now give back the assignee variable
	return const_var->assignee;
}


/**
 * Emit the identifier machine code. This function is to be used in the instance where we want
 * to move an identifier to some temporary location
 */
static three_addr_var_t* emit_identifier(basic_block_t* basic_block, generic_ast_node_t* ident_node, temp_selection_t use_temp, side_type_t side, u_int8_t is_branch_ending){
	//Just give back the name
	if(use_temp == PRESERVE_ORIG_VAR || side == SIDE_TYPE_RIGHT){
		//If it's an enum constant
		if(ident_node->variable->is_enumeration_member == TRUE){
			return emit_direct_constant_assignment(basic_block, emit_int_constant_direct(ident_node->variable->enum_member_value, type_symtab), ident_node->inferred_type, is_branch_ending);
		}

		//Emit the variable
		three_addr_var_t* var = emit_var(ident_node->variable, FALSE);

		//This variable has been assigned to, so we'll add that too
		if(side == SIDE_TYPE_LEFT){
			//We only do this if it's the LHS
			add_assigned_variable(basic_block, var);
		} else {
			//Add it as a live variable to the block, because we've used it
			add_used_variable(basic_block, var);
		}

		//Give it back
		return var;

	//We will do an on-the-fly conversion to a number
	} else if(ident_node->inferred_type->type_class == TYPE_CLASS_ENUMERATED) {
		//Look up the type
		symtab_type_record_t* type_record = lookup_type_name_only(type_symtab, "u8");
		generic_type_t* type = type_record->type;
		//Just create a constant here with the enum
		return emit_direct_constant_assignment(basic_block, emit_int_constant_direct(ident_node->variable->enum_member_value, type_symtab), type, is_branch_ending);

	} else {
		//First we'll create the non-temp var here
		three_addr_var_t* non_temp_var = emit_var(ident_node->variable, FALSE);

		//Add this in as a used variable
		add_used_variable(basic_block, non_temp_var);

		//Let's first create the assignment statement
		instruction_t* temp_assnment = emit_assignment_instruction(emit_temp_var(ident_node->inferred_type), non_temp_var);

		//Carry this through
		temp_assnment->is_branch_ending = is_branch_ending;

		//Add the statement in
		add_statement(basic_block, temp_assnment);

		//Just give back the temp var here
		return temp_assnment->assignee;
	}
}


/**
 * Emit increment three adress code
 */
static three_addr_var_t* emit_inc_code(basic_block_t* basic_block, three_addr_var_t* incrementee, u_int8_t is_branch_ending){
	//Create the code
	instruction_t* inc_code = emit_inc_instruction(incrementee);

	//This will count as live if we read from it
	if(incrementee->is_temporary == FALSE){
		add_assigned_variable(basic_block, incrementee);
		//This is a rare case were we're assigning to AND using
		add_used_variable(basic_block, incrementee);
	}

	//Mark this with whatever was passed through
	inc_code->is_branch_ending = is_branch_ending;

	//Add it into the block
	add_statement(basic_block, inc_code);

	//Return the incrementee
	return incrementee;
}


/**
 * Emit decrement three address code
 */
static three_addr_var_t* emit_dec_code(basic_block_t* basic_block, three_addr_var_t* decrementee, u_int8_t is_branch_ending){
	//Create the code
	instruction_t* dec_code = emit_dec_instruction(decrementee);

	//This will count as live if we read from it
	if(decrementee->is_temporary == FALSE){
		add_assigned_variable(basic_block, decrementee);
		//This is a rare case were we're assigning to AND using
		add_used_variable(basic_block, decrementee);
	}

	//Mark this with whatever was passed through
	dec_code->is_branch_ending = is_branch_ending;

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
		add_used_variable(basic_block, indirect_var);
	}

	//Increment the indirection
	indirect_var->indirection_level++;
	//Temp or not same deal
	indirect_var->is_temporary = assignee->is_temporary;

	//And get out
	return indirect_var;
}


/**
 * Emit a pointer indirection statement. The parser has already done the dereferencing for us, so we'll just
 * be able to store the dereferenced type in here
 */
static three_addr_var_t* emit_pointer_indirection(basic_block_t* basic_block, three_addr_var_t* assignee, generic_type_t* dereferenced_type){
	//No actual code here, we are just accessing this guy's memory
	//Create a new variable with an indirection level
	three_addr_var_t* indirect_var = emit_var_copy(assignee);

	//This will count as live if we read from it
	if(indirect_var->is_temporary == FALSE){
		add_used_variable(basic_block, indirect_var);
	}

	//Increment the indirection
	indirect_var->indirection_level++;
	//Temp or not same deal
	indirect_var->is_temporary = assignee->is_temporary;

	//Store the dereferenced type
	indirect_var->type = dereferenced_type;

	//And get out
	return indirect_var;
}


/**
 * Emit a bitwise not statement 
 */
static three_addr_var_t* emit_bitwise_not_expr_code(basic_block_t* basic_block, three_addr_var_t* var, temp_selection_t use_temp, u_int8_t is_branch_ending){
	//First we'll create it here
	instruction_t* not_stmt = emit_not_instruction(var);

	//This is also a case where the variable is read from, so it counts as live. It's also
	//assigned to, so it counts as assigned
	if(var->is_temporary == FALSE){
		add_assigned_variable(basic_block, var);
		add_used_variable(basic_block, var);
	}

	//Mark this with its branch end status
	not_stmt->is_branch_ending = is_branch_ending;

	//Add this into the block
	add_statement(basic_block, not_stmt);

	//Give back the assignee
	return not_stmt->assignee;
}


/**
 * Emit a binary operation statement with a constant built in
 */
static three_addr_var_t* emit_binary_operation_with_constant(basic_block_t* basic_block, three_addr_var_t* assignee, three_addr_var_t* op1, Token op, three_addr_const_t* constant, u_int8_t is_branch_ending){
	//If these variables are not temporary, then we have read from them
	if(assignee->is_temporary == FALSE){
		add_used_variable(basic_block, assignee);
	}

	//Add this one in too
	if(op1->is_temporary == FALSE){
		add_used_variable(basic_block, assignee);
	}

	//First let's create it
	instruction_t* stmt = emit_binary_operation_with_const_instruction(assignee, op1, op, constant);

	//Is this branch ending?
	stmt->is_branch_ending = is_branch_ending;

	//Then we'll add it into the block
	add_statement(basic_block, stmt);

	//Finally we'll return it
	return assignee;
}


/**
 * Emit a negation statement
 */
static three_addr_var_t* emit_neg_stmt_code(basic_block_t* basic_block, three_addr_var_t* negated, temp_selection_t use_temp, u_int8_t is_branch_ending){
	three_addr_var_t* var;

	//We make our temp selection based on this
	if(use_temp == USE_TEMP_VAR){
		var = emit_temp_var(negated->type);
	} else {
		var = negated;
	}

	//If this isn't a temp var, we'll add it in as live
	if(negated->is_temporary == FALSE){
		add_used_variable(basic_block, negated);
	}

	//Now let's create it
	instruction_t* stmt = emit_neg_instruction(var, negated);

	//Mark with it's branch ending status
	stmt->is_branch_ending = is_branch_ending;
	
	//Add it into the block
	add_statement(basic_block, stmt);

	//We always return the assignee
	return var;
}


/**
 * Emit a logical negation statement
 */
static three_addr_var_t* emit_logical_neg_stmt_code(basic_block_t* basic_block, three_addr_var_t* negated, u_int8_t is_branch_ending){
	//We need to emit a temp assignment for the negation
	instruction_t* temp_assingment = emit_assignment_instruction(emit_temp_var(negated->type), negated);

	//Add this into the block
	add_statement(basic_block, temp_assingment);

	//This will always overwrite the other value
	instruction_t* stmt = emit_logical_not_instruction(temp_assingment->assignee, temp_assingment->assignee);
	
	//If negated isn't temp, it also counts as a read
	if(negated->is_temporary == FALSE){
		add_used_variable(basic_block, negated);
	}

	//Mark this with its branch ending status
	stmt->is_branch_ending = is_branch_ending;

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
static three_addr_var_t* emit_primary_expr_code(basic_block_t* basic_block, generic_ast_node_t* primary_parent, temp_selection_t use_temp, side_type_t side, u_int8_t is_branch_ending){
	//Switch based on what kind of expression we have. This mainly just calls the appropriate rules
	switch(primary_parent->CLASS){
		case AST_NODE_CLASS_IDENTIFIER:
		 	return emit_identifier(basic_block, primary_parent, use_temp, side, is_branch_ending);
		case AST_NODE_CLASS_CONSTANT:
			return emit_constant_assignment(basic_block, primary_parent, is_branch_ending);
		case AST_NODE_CLASS_TERNARY_EXPRESSION:
			//FOR NOW
			return emit_ternary_expression(basic_block, primary_parent, is_branch_ending).assignee;
		case AST_NODE_CLASS_BINARY_EXPR:
			return emit_binary_expression(basic_block, primary_parent, is_branch_ending).assignee;
		case AST_NODE_CLASS_FUNCTION_CALL:
			return emit_function_call(basic_block, primary_parent, is_branch_ending);
		//Something went wrong here if we're hitting the default rule
		default:
			//Throw some error here, really this should never occur
			print_parse_message(PARSE_ERROR, "Did not find identifier, constant, expression or function call in primary expression", primary_parent->line_number);
			(*num_errors_ref)++;
			exit(0);
	}
}


/**
 * Handle a postincrement/postdecrement operation
 */
static three_addr_var_t* emit_postoperation_code(basic_block_t* basic_block, three_addr_var_t* current_var, Token unary_operator, side_type_t side, u_int8_t is_branch_ending){
	//This is either a postincrement or postdecrement. Regardless, we emit
	//a temp var for this because we assign before we mutate

	//Emit the temp var with the current type
	three_addr_var_t* temp_var = emit_temp_var(current_var->type);

	//Now we'll need to emit the assignment operation
	instruction_t* assignment = emit_assignment_instruction(temp_var, current_var);

	//Mark this for later
	assignment->is_branch_ending = is_branch_ending;

	//Ensure that we add this into the block
	add_statement(basic_block, assignment);

	//We'll now perform the actual operation
	if(unary_operator == PLUSPLUS){
		//If we have a pointer, use the helper
		if(current_var->type->type_class == TYPE_CLASS_POINTER){
			handle_pointer_arithmetic(basic_block, PLUS, current_var, is_branch_ending);
		} else {
			//Use the helper for this
			emit_inc_code(basic_block, current_var, is_branch_ending);
		}

	//Otherwise we know that it has to be minusminus
	} else {
		//If we have a pointer, use the helper
		if(current_var->type->type_class == TYPE_CLASS_POINTER){
			handle_pointer_arithmetic(basic_block, MINUS, current_var, is_branch_ending);
		} else {
			//Use the helper here as well
			emit_dec_code(basic_block, current_var, is_branch_ending);
		}
	}

	//This is the variable that we'll use if we make use of this after
	//the fact
	return temp_var;
}


/**
 * Emit the abstract machine code for various different kinds of postfix expressions
 * that we could see. The two that we'll need to be concerned about are construct
 * and array access
 */
static three_addr_var_t* emit_postfix_expr_code(basic_block_t* basic_block, generic_ast_node_t* postfix_parent, temp_selection_t use_temp, side_type_t side, u_int8_t is_branch_ending){
	//We'll first want a cursor
	generic_ast_node_t* cursor = postfix_parent->first_child;

	//We should always have a primary expression first. We'll first call the primary expression
	three_addr_var_t* current_var = emit_primary_expr_code(basic_block, cursor, use_temp, side, is_branch_ending);

	//Move the cursor along
	cursor = cursor->next_sibling;

	//This will happen a lot - we'll have nowhere else to go. When that happens,
	//all we need to do is give back the first variable
	if(cursor == NULL){
		return current_var;
	
	//We could also go right into a terminal expression like a unary operator. If so
	//handle it and then bail
	} else if(cursor->CLASS == AST_NODE_CLASS_UNARY_OPERATOR){
		//Let the helper do it
		return emit_postoperation_code(basic_block, current_var, cursor->unary_operator, side, is_branch_ending);
	}

	//If we get here we know that we have at least one construct/array access statement

	//The currently calculated address
	three_addr_var_t* current_address = NULL;

	//What is the type of our current variable?
	//The current type is intended to represent what we'll get out
	//when we dereference
	generic_type_t* current_type = current_var->type;

	//Keep track of the array/construct variable here
	symtab_variable_record_t* array_or_construct_var = current_var->linked_var;

	//So long as we're hitting arrays or constructs, we need to be memory conscious
	while(cursor != NULL &&
		(cursor->CLASS == AST_NODE_CLASS_CONSTRUCT_ACCESSOR || cursor->CLASS == AST_NODE_CLASS_ARRAY_ACCESSOR)){
		//First of two potentialities is the array accessor
		if(cursor->CLASS == AST_NODE_CLASS_ARRAY_ACCESSOR){
			//The first thing we'll see is the value in the brackets([value]). We'll let the helper emit this
			three_addr_var_t* offset = emit_binary_expression(basic_block, cursor->first_child, is_branch_ending).assignee;
			
			//What is the internal type that we're pointing to? This will determine our scale
			if(current_type->type_class == TYPE_CLASS_ARRAY){
				//We'll dereference the current type
				current_type = current_type->array_type->member_type;
			} else {
				//We'll dereference the current type
				current_type = current_type->pointer_type->points_to;
			}

			/**
			 * The formula for array subscript is: base_address + type_size * subscript
			 * 
			 * However, if we're on our second or third round, the current var may be an address
			 *
			 * This can be done using a lea instruction, so we will emit that directly
			 */
			three_addr_var_t* address;
			//Calculate the address using the current var or current address
			if(current_address == NULL){
				//Remember, we can only use lea if we have a power of 2 
				if(is_power_of_2(current_type->type_size) == TRUE){
					address = emit_lea(basic_block, current_var, offset, current_type, is_branch_ending);
				} else {
					address = emit_address_offset_calc(basic_block, current_var, offset, current_type, is_branch_ending);
				}
			} else {
				//Remember, we can only use lea if we have a power of 2 
				if(is_power_of_2(current_type->type_size) == TRUE){
					address = emit_lea(basic_block, current_address, offset, current_type, is_branch_ending);
				} else {
					address = emit_address_offset_calc(basic_block, current_address, offset, current_type, is_branch_ending);
				}
			}

			//Now this is the current address
			current_address = address;

			//If we see that the next sibling is NULL or it's not an array accessor(i.e. construct accessor),
			//we're done here. We'll emit our memory code and leave this part of the loop
			if(cursor->next_sibling == NULL){
				//We're using indirection, address is being wiped out
				current_address = NULL;

				//If we're on the left hand side, we're trying to write to this variable. NO deref statement here
				if(side == SIDE_TYPE_LEFT){
					//Emit the indirection for this one
					current_var = emit_mem_code(basic_block, address);
					//It's a write
					current_var->access_type = MEMORY_ACCESS_WRITE;

					//This is related to a write of a var. We'll need to set this flag for later processing
					//by the optimizer
					current_var->related_write_var = array_or_construct_var;

				//Otherwise we're dealing with a read
				} else {
					//Still emit the memory code
					current_var = emit_mem_code(basic_block, address);
					//It's a read
					current_var->access_type = MEMORY_ACCESS_READ;

					//We will perform the deref here, as we can't do it in the lea 
					instruction_t* deref_stmt = emit_assignment_instruction(emit_temp_var(current_type), current_var);
					//Is this branch ending?
					deref_stmt->is_branch_ending = is_branch_ending;
					//And add it in
					add_statement(basic_block, deref_stmt);

					//Update the current bar too
					current_var = deref_stmt->assignee;

					//Mark this too for later mapping
					current_var->related_write_var = array_or_construct_var;
				}

			} else {
				//Otherwise, the current var is the address
				current_var = address;
			}

		//This has to be a construct accessor, we've no other choice
		} else {
			//What we'll do first is grab the associated fields that we need out
			symtab_variable_record_t* var = cursor->variable;

			//Remember - when we get here, current var will hold the base address of the construct
			//If current var is a pointer, then we need to dereference it to get the actual construct type	
			if(current_type->type_class == TYPE_CLASS_POINTER){
				//We need to first dereference this
				three_addr_var_t* dereferenced = emit_pointer_indirection(basic_block, current_var, current_type->pointer_type->points_to);

				//Assign temp to be the current address
				instruction_t* assnment = emit_assignment_instruction(emit_temp_var(dereferenced->type), dereferenced);
				add_statement(basic_block, assnment);

				//Reassign what current address really is
				current_address = assnment->assignee;

				//Dereference the current type
				current_type = current_type->pointer_type->points_to;
			}

			//Now we'll grab the associated construct record
			constructed_type_field_t* field = get_construct_member(current_type->construct_type, var->var_name);

			//The field we have
			symtab_variable_record_t* member = field->variable;

			//The constant that represents the offset
			three_addr_const_t* offset = emit_int_constant_direct(field->offset, type_symtab);

			//This is now the member's type
			current_type = member->type_defined_as;

			//Let's hold onto the address
			three_addr_var_t* address;
			//If the current address is NULL, we'll use the current var. Otherwise, we use the address
			//we've already gotten
			if(current_address == NULL){
				address = emit_construct_address_calculation(basic_block, current_var, offset, is_branch_ending);
			} else {
				address = emit_construct_address_calculation(basic_block, current_address, offset, is_branch_ending);
			}

			//Do we need to do more memory work? We can tell if the array accessor node is next
			if(cursor->next_sibling == NULL){
				//We're using indirection, address is being wiped out
				current_address = NULL;

				//If we're on the left hand side, we're trying to write to this variable. NO deref statement here
				if(side == SIDE_TYPE_LEFT){
					//Emit the indirection for this one
					current_var = emit_mem_code(basic_block, address);
					//It's a write
					current_var->access_type = MEMORY_ACCESS_WRITE;

					//Record where these variables came from
					address->related_write_var = member;
					current_var->related_write_var = member;
				
				//Otherwise we're dealing with a read
				} else {
					//Still emit the memory code
					current_var = emit_mem_code(basic_block, address);
					//It's a read
					current_var->access_type = MEMORY_ACCESS_READ;

					//We will perform the deref here, as we can't do it in the lea 
					instruction_t* deref_stmt = emit_assignment_instruction(emit_temp_var(current_type), current_var);
					//Is this branch ending?
					deref_stmt->is_branch_ending = is_branch_ending;
					//And add it in
					add_statement(basic_block, deref_stmt);

					//Update the current bar too
					current_var = deref_stmt->assignee;

					//Mark this too
					current_var->related_write_var = member;
				}
			//Otherwise, our current var is this address
			} else {
				current_var = address;
			}
		}

		//Advance to the next sibling 
		cursor = cursor->next_sibling;
	}

	//We could have a post inc/dec afterwards, so we'll let the helper hand if we do
	if(cursor != NULL && cursor->CLASS == AST_NODE_CLASS_UNARY_OPERATOR){
		//Let the helper do it
		return emit_postoperation_code(basic_block, current_var, cursor->unary_operator, side, is_branch_ending);
	} else {
		return current_var;
	}
}


/**
 * Handle a unary operator, in whatever form it may be
 */
static three_addr_var_t* emit_unary_operation_code(basic_block_t* basic_block, generic_ast_node_t* unary_expr_parent, temp_selection_t use_temp, side_type_t side, u_int8_t is_branch_ending){
	//Top level declarations to avoid using them in the switch statement
	three_addr_var_t* dereferenced;
	instruction_t* assignment;

	//Extract the first child from the unary expression parent node
	generic_ast_node_t* first_child = unary_expr_parent->first_child;

	//The very first thing that we'll do is emit the assignee that comes after the unary expression
	three_addr_var_t* assignee = emit_unary_expr_code(basic_block, first_child->next_sibling, use_temp, side,  is_branch_ending);

	//Now that we've emitted the assignee, we can handle the specific unary operators
	switch(first_child->unary_operator){
		//Handle the case of a preincrement
		case PLUSPLUS:
			//If the assignee is not a pointer, we'll handle the normal case
			if(assignee->type->type_class == TYPE_CLASS_BASIC){
				//We really just have an "inc" instruction here
				return emit_inc_code(basic_block, assignee, is_branch_ending);
			//If we actually do have a pointer, we need the helper to deal with this
			} else {
				//Let the helper deal with this
				return handle_pointer_arithmetic(basic_block, first_child->unary_operator, assignee, is_branch_ending);
			}

		//Handle the case of a predecrement
		case MINUSMINUS:
			//If we have a basic type, we can use the regular process
			if(assignee->type->type_class == TYPE_CLASS_BASIC){
				//We really just have an "inc" instruction here
				return emit_dec_code(basic_block, assignee, is_branch_ending);
			//If we actually have a pointer, we'll let the helper deal with it
			} else {
				//Let the helper deal with this
				return handle_pointer_arithmetic(basic_block, first_child->unary_operator, assignee, is_branch_ending);
			}

		//Handle a dereference
		case STAR:
			//Get the dereferenced variable
			dereferenced = emit_pointer_indirection(basic_block, assignee, unary_expr_parent->inferred_type);

			//If we're on the right hand side, we need to have a temp assignment
			if(side == SIDE_TYPE_RIGHT){
				//Emit the temp assignment
				instruction_t* temp_assignment = emit_assignment_instruction(emit_temp_var(dereferenced->type), dereferenced);

				//Add it in
				add_statement(basic_block, temp_assignment);

				//Return the assignee of this
				return temp_assignment->assignee;

			//Otherwise just give back what we had
			} else {
				return dereferenced;
			}
	
		//Bitwise not operator
		case B_NOT:
			//Let the helper handle it
			return emit_bitwise_not_expr_code(basic_block, assignee, use_temp, is_branch_ending);

		//Logical not operator
		case L_NOT:
			//Let the helper deal with it
			return emit_logical_neg_stmt_code(basic_block, assignee, is_branch_ending);

		/**
		 * Negation operator
		 * x = -a;
		 * t <- a;
		 * negl t;
		 * x <- t;
		 *
		 * Uses strategy of: negl rdx
		 */
		case MINUS:
			//We'll need to assign to a temp here, these are
			//only ever on the RHS
			assignment = emit_assignment_instruction(emit_temp_var(assignee->type), assignee);

			//Add this into the block
			add_statement(basic_block, assignment);

			//We will emit the negation code here
			return emit_neg_stmt_code(basic_block, assignment->assignee, use_temp, is_branch_ending);

		//Handle the case of the address operator
		case SINGLE_AND:
			//We'll need to assign to a temp here, these are
			//only ever on the RHS
			assignment = emit_memory_address_assignment(emit_temp_var(unary_expr_parent->inferred_type), assignee);
			assignment->is_branch_ending = is_branch_ending;

			//We now need to flag that the assignee here absolutely must be spilled by the register allocator
			assignee->linked_var->must_be_spilled = TRUE;

			//Add this into the block
			add_statement(basic_block, assignment);

			//Give back the assignee
			return assignment->assignee;

		//In reality we should never actually hit this, it's just there for the C compiler to be happy
		default:
			return assignee;
	}
}


/**
 * Emit the abstract machine code for a unary expression
 * Unary expressions come in the following forms:
 * 	
 * 	<postfix-expression> | <unary-operator> <cast-expression> | typesize(<type-specifier>) | sizeof(<logical-or-expression>) 
 */
static three_addr_var_t* emit_unary_expr_code(basic_block_t* basic_block, generic_ast_node_t* unary_expr_parent, temp_selection_t use_temp, side_type_t side, u_int8_t is_branch_ending){
	//The last two instances return a constant node. If that's the case, we'll just emit a constant node here
	if(unary_expr_parent->CLASS == AST_NODE_CLASS_CONSTANT){
		//Let the helper deal with it
		return emit_constant_assignment(basic_block, unary_expr_parent, is_branch_ending);
	}

	//If it isn't a constant, then this node should have children
	generic_ast_node_t* first_child = unary_expr_parent->first_child;

	//Since we know that this node has childen, we'll now go through
	//and decide what to do with it based on its class
	switch(first_child->CLASS){
		//Leverage the helper for this method
		case AST_NODE_CLASS_POSTFIX_EXPR:
			return emit_postfix_expr_code(basic_block, first_child, use_temp, side, is_branch_ending);

		//We can also see a unary operator here. In this case, we'll let the unary operator helper function take care of it
		case AST_NODE_CLASS_UNARY_OPERATOR:
			return emit_unary_operation_code(basic_block, unary_expr_parent, use_temp, side, is_branch_ending);
				
		//By defualt, emit a primary expression
		default:
			return emit_primary_expr_code(basic_block, first_child, use_temp, side, is_branch_ending);
	}
}


/**
 * Emit the abstract machine code for a ternary operation. A ternary operation is really a conditional
 * movement operation by a different name
 *
 * x == 0 ? a : b becomes
 *
 * declare final_var;
 * if(x == 0) then {
 * 		final_var =	a
 * } else {
 * 		final_var = b
 * }
 *
 * Which in reality would be something like this:
 * 	cmpl $0, x
 * 	cmove a, result
 * 	cmovne b, result
 */
static expr_ret_package_t emit_ternary_expression(basic_block_t* origin_block, generic_ast_node_t* ternary_operation, u_int8_t is_branch_ending){
	//Expression return package that we need
	expr_ret_package_t return_package;
	//Mark that we had a ternary here
	return_package.operator = QUESTION;

	//The ending block for the whole thing
	basic_block_t* end_block = basic_block_alloc(1);
	//The if area block
	basic_block_t* if_block = basic_block_alloc(1);
	//And the else area block
	basic_block_t* else_block = basic_block_alloc(1);

	//Let's first create the final result variable here
	three_addr_var_t* result = emit_temp_var(ternary_operation->inferred_type);

	//Grab a cursor to the first child
	generic_ast_node_t* cursor = ternary_operation->first_child;

	//Let's first process the conditional
	expr_ret_package_t package = emit_binary_expression(origin_block, cursor, is_branch_ending);

	//The package's assignee is what we base all conditional moves on
	u_int8_t is_signed = is_type_signed(package.assignee->type); 

	//Select the jump type for our conditional
	jump_type_t jump = select_appropriate_jump_stmt(package.operator, JUMP_CATEGORY_NORMAL, is_signed);
	
	//Now we'll emit a jump to the if block and else block
	emit_jump(origin_block, if_block, jump, is_branch_ending, FALSE);
	emit_jump(origin_block, else_block, JUMP_TYPE_JMP, is_branch_ending, FALSE);

	//These are both now successors to the if block
	add_successor(origin_block, if_block);
	add_successor(origin_block, else_block);

	//Now we'll go through and process the two children
	cursor = cursor->next_sibling;

	//Emit this in our new if block
	expr_ret_package_t if_branch = emit_expr_code(if_block, cursor, is_branch_ending, TRUE);

	//We'll now create a conditional move for the if branch into the result
	instruction_t* if_assignment = emit_assignment_instruction(result, if_branch.assignee);

	//Add this into the if block
	add_statement(if_block, if_assignment);

	//Now add a direct jump to the end
	emit_jump(if_block, end_block, JUMP_TYPE_JMP, is_branch_ending, FALSE);

	//Process the else branch
	cursor = cursor->next_sibling;

	//Emit this in our else block
	expr_ret_package_t else_branch = emit_expr_code(else_block, cursor, is_branch_ending, TRUE);

	//We'll now create a conditional move for the else branch into the result
	instruction_t* else_assignment = emit_assignment_instruction(result, else_branch.assignee);

	//Add this into the else block
	add_statement(else_block, else_assignment);

	//Now add a direct jump to the end
	emit_jump(else_block, end_block, JUMP_TYPE_JMP, is_branch_ending, FALSE);

	//The end block is a successor to both the if and else blocks
	add_successor(if_block, end_block);
	add_successor(else_block, end_block);

	return_package.final_block = end_block;
	return_package.assignee = result;

	//Give back the result
	return return_package;
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
static expr_ret_package_t emit_binary_expression(basic_block_t* basic_block, generic_ast_node_t* logical_or_expr, u_int8_t is_branch_ending){
	//The return package here
	expr_ret_package_t package;
	//Operator is blank by default
	package.operator = BLANK;

	//Store the left and right hand types
	generic_type_t* left_hand_type;
	generic_type_t* right_hand_type;
	//Temporary holders for our operands
	three_addr_var_t* op1;
	three_addr_var_t* op2;
	//Our assignee - this can change dynamically based on the kind of operator
	three_addr_var_t* assignee;

	//Is the cursor here a unary expression or a constant? If so just emit that
	switch(logical_or_expr->CLASS){
		//Process a unary expression(these are usually idents)
		case AST_NODE_CLASS_UNARY_EXPR:
			//Return the temporary character from here
			package.assignee = emit_unary_expr_code(basic_block, logical_or_expr, USE_TEMP_VAR, SIDE_TYPE_RIGHT, is_branch_ending);
			return package;

		//Process a constant, we may have these in here directly
		case AST_NODE_CLASS_CONSTANT:
			package.assignee = emit_constant_assignment(basic_block, logical_or_expr, is_branch_ending);
			return package;

		//We could also have a ternary operation here
		case AST_NODE_CLASS_TERNARY_EXPRESSION:
			package.assignee = emit_ternary_expression(basic_block, logical_or_expr, is_branch_ending).assignee;
			return package;
		
		//Break out by default
		default:
			break;
	}

	//Otherwise we actually have a binary operation of some kind
	//Grab a cursor
	generic_ast_node_t* cursor = logical_or_expr->first_child;

	//Store the left hand type for our type comparison later
	left_hand_type = cursor->inferred_type;
	
	//Emit the binary expression on the left first
	expr_ret_package_t left_hand_temp = emit_binary_expression(basic_block, cursor, is_branch_ending);

	//If this is temporary *or* a type conversion is needed, we'll do some reassigning here
	if(left_hand_temp.assignee->is_temporary == FALSE){
		//emit the temp assignment
		instruction_t* temp_assnment = emit_assignment_instruction(emit_temp_var(left_hand_type), left_hand_temp.assignee);
		//Add it into here
		add_statement(basic_block, temp_assnment);
		
		//We can mark that op1 was used
		add_used_variable(basic_block, left_hand_temp.assignee);
		
		//Grab the assignee out
		op1 = temp_assnment->assignee;

	//Otherwise the left hand temp assignee is just fine for us
	} else {
		op1 = left_hand_temp.assignee;
	}

	//Advance up here
	cursor = cursor->next_sibling;
	right_hand_type = cursor->inferred_type;

	//Then grab the right hand temp
	expr_ret_package_t right_hand_temp = emit_binary_expression(basic_block, cursor, is_branch_ending);

	//Let's see what binary operator that we have
	Token binary_operator = logical_or_expr->binary_operator;
	//Store this binary operator
	package.operator = binary_operator;
	//Grab this out for convenience
	op2 = right_hand_temp.assignee;

	//Switch based on whatever operator that we have
	switch(binary_operator){
		case L_THAN:
		case G_THAN:
		case G_THAN_OR_EQ:
		case L_THAN_OR_EQ:
		case NOT_EQUALS:
		case DOUBLE_EQUALS:
		case DOUBLE_OR:
		case DOUBLE_AND:
			//Emit an assignee based on the inferred type
			assignee = emit_temp_var(logical_or_expr->inferred_type);
			break;
		//We use the default strategy - op1 is also the assignee
		default:
			assignee = op1;
			break;
	}
	
	//Emit the binary operator expression using our helper
	instruction_t* stmt = emit_binary_operation_instruction(assignee, op1, binary_operator, op2);
	package.assignee = assignee;

	//Mark this with what we have
	stmt->is_branch_ending = is_branch_ending;

	//Add this statement to the block
	add_statement(basic_block, stmt);

	//If these are not temporary, they also count as live
	if(left_hand_temp.assignee->is_temporary == FALSE){
		add_used_variable(basic_block, left_hand_temp.assignee);
	}

	if(right_hand_temp.assignee->is_temporary == FALSE){
		add_used_variable(basic_block, right_hand_temp.assignee);
	}

	//Return the temp variable that we assigned to
	return package;
}


/**
 * Emit abstract machine code for an expression. This is a top level statement.
 * These statements almost always involve some kind of assignment "<-" and generate temporary
 * variables
 */
static expr_ret_package_t emit_expr_code(basic_block_t* basic_block, generic_ast_node_t* expr_node, u_int8_t is_branch_ending, u_int8_t check_for_coniditional){
	//A cursor for tree traversal
	generic_ast_node_t* cursor;
	symtab_variable_record_t* assigned_var;
	//The return package
	expr_ret_package_t ret_package;
	//By default, last seen op is blank
	ret_package.operator = BLANK;

	//If we have a declare statement,
	if(expr_node->CLASS == AST_NODE_CLASS_DECL_STMT){
		//Extract the type info out of here
		generic_type_t* type = expr_node->variable->type_defined_as;

		//If we have an array, we'll need to decrement the stack
		if(type->type_class == TYPE_CLASS_ARRAY || type->type_class == TYPE_CLASS_CONSTRUCT){
			//Now we emit the variable for the array base address
			three_addr_var_t* base_addr = emit_var(expr_node->variable, FALSE);

			//Add this variable into the current function's stack. This is what we'll use
			//to store the address
			add_variable_to_stack(&(current_function->data_area), base_addr);

			//We'll now emit the actual address calculation using the offset
			emit_binary_operation_with_constant(basic_block, base_addr, stack_pointer_var, PLUS, emit_int_constant_direct(base_addr->stack_offset, type_symtab), is_branch_ending);

			//NO need to worry about signedness here, we're just declaring
		}

	//Convert our let statement into abstract machine code 
	} else if(expr_node->CLASS == AST_NODE_CLASS_LET_STMT){
		//Let's grab the associated variable record here
		symtab_variable_record_t* var =  expr_node->variable;

		//Create the variable associated with this
	 	three_addr_var_t* left_hand_var = emit_var(var, FALSE);

		//This has been assigned to
		add_assigned_variable(basic_block, left_hand_var);

		//Now emit whatever binary expression code that we have
		expr_ret_package_t package = emit_expr_code(basic_block, expr_node->first_child, is_branch_ending, FALSE);

		//The actual statement is the assignment of right to left
		instruction_t* assn_stmt = emit_assignment_instruction(left_hand_var, package.assignee);

		//Finally we'll add this into the overall block
		add_statement(basic_block, assn_stmt);
		
		//Also no need to worry about signedness here either
	
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
		three_addr_var_t* left_hand_var = emit_unary_expr_code(basic_block, cursor, PRESERVE_ORIG_VAR, SIDE_TYPE_LEFT, is_branch_ending);

		//Advance the cursor up
		cursor = cursor->next_sibling;

		//Now emit the right hand expression
		expr_ret_package_t package = emit_expr_code(basic_block, cursor, is_branch_ending, FALSE);

		//Finally we'll construct the whole thing
		instruction_t* stmt = emit_assignment_instruction(left_hand_var, package.assignee);
		
		//Mark this with what was passed through
		stmt->is_branch_ending = is_branch_ending;

		//Now add this statement in here
		add_statement(basic_block, stmt);

		//Now pack the return value here
		ret_package.operator = BLANK;
		ret_package.assignee = left_hand_var;
		
		//Return what we had
		return ret_package;

	} else if(expr_node->CLASS == AST_NODE_CLASS_BINARY_EXPR){
		//Emit the binary expression node
		return emit_binary_expression(basic_block, expr_node, is_branch_ending);
	} else if(expr_node->CLASS == AST_NODE_CLASS_FUNCTION_CALL){
		//Emit the function call statement
		ret_package.assignee = emit_function_call(basic_block, expr_node, is_branch_ending);
		return ret_package;

	//If we make it here, we have found a standalone ternary expression
	} else if(expr_node->CLASS == AST_NODE_CLASS_TERNARY_EXPRESSION){
		//Emit the ternary expression
		ret_package.assignee = emit_ternary_expression(basic_block, expr_node, is_branch_ending).assignee;
		return ret_package;

	} else if(expr_node->CLASS == AST_NODE_CLASS_UNARY_EXPR){
		/**
	 	* This is a very special check where we look for any if(x) kind of statements that just require a
	 	* left hand temp assignment
	 	*/
		if(check_for_coniditional == TRUE && expr_node->first_child->CLASS == AST_NODE_CLASS_IDENTIFIER){
			//If this is the case, then we need to just emit the temporary value and be done with it
			ret_package.assignee =  emit_identifier(basic_block, expr_node->first_child, USE_TEMP_VAR, SIDE_TYPE_LEFT, TRUE);
			//Signedness is irrelevant here because any jumps would just be "je/jne"
			return ret_package;
		} else {
			//Let this rule handle it
			ret_package.assignee = emit_unary_expr_code(basic_block, expr_node, PRESERVE_ORIG_VAR, SIDE_TYPE_RIGHT, is_branch_ending);
			//Again signedness is irrelevant here because any jumps would just be "je/jne"
			return ret_package;
		}

	} else {
		return ret_package;

	}

	return ret_package;
}


/**
 * Emit a function call node. In this iteration of a function call, we will still be parameterized, so the actual 
 * node will record what needs to be passed into the function
 */
static three_addr_var_t* emit_function_call(basic_block_t* basic_block, generic_ast_node_t* function_call_node, u_int8_t is_branch_ending){
	//Grab this out first
	symtab_function_record_t* func_record = function_call_node->func_record;

	//The function's assignee
	three_addr_var_t* assignee = NULL;

	instruction_t* func_call_stmt;

	//May be NULL or not based on what we have as the return type
	if(func_record->return_type->type_class == TYPE_CLASS_BASIC && func_record->return_type->basic_type->basic_type == VOID){
		//We'll have a dummy one here
		three_addr_var_t* temp_var = emit_temp_var(lookup_type_name_only(type_symtab, "u64")->type);
		func_call_stmt = emit_function_call_instruction(func_record, temp_var);
	} else {
		//Otherwise we have one like this
		assignee = emit_temp_var(func_record->return_type);
		func_call_stmt = emit_function_call_instruction(func_record, assignee);
	}

	//Mark this with whatever we have
	func_call_stmt->is_branch_ending = is_branch_ending;

	//Let's grab a param cursor for ourselves
	generic_ast_node_t* param_cursor = function_call_node->first_child;

	//If this isn't NULL, we have parameters
	if(param_cursor != NULL){
		//Create this
		func_call_stmt->function_parameters = dynamic_array_alloc();
	}

	//The current param of the indext9 <- call parameter_pass2(t10, t11, t12, t14, t16, t18)
	u_int8_t current_func_param_idx = 1;

	//So long as this isn't NULL
	while(param_cursor != NULL){
		//Emit whatever we have here into the basic block
		expr_ret_package_t package = emit_expr_code(basic_block, param_cursor, is_branch_ending, FALSE);

		//We'll also need to emit a temp assignment here. This is because we need to move everything into given
		//registers before a function call
		instruction_t* assignment = emit_assignment_instruction(emit_temp_var(package.assignee->type), package.assignee);

		//Add this to the block
		add_statement(basic_block, assignment);

		//Mark this
		assignment->assignee->parameter_number = current_func_param_idx;
		
		//Add the parameter in
		dynamic_array_add(func_call_stmt->function_parameters, assignment->assignee);

		//And move up
		param_cursor = param_cursor->next_sibling;

		//Increment this
		current_func_param_idx++;
	}

	//Once we make it here, we should have all of the params stored in temp vars
	//We can now add the function call statement in
	add_statement(basic_block, func_call_stmt);

	//We'll always have an assignment instruction
	instruction_t* assignment;

	//Emit an assignment instruction. This will become very important way down the line in register
	//allocation to avoid interference
	if(assignee != NULL){
		//Emit it
		assignment = emit_assignment_instruction(emit_temp_var(assignee->type), assignee);
				
		//Reassign this value
		assignee = assignment->assignee;

		//This cannot be coalesced
		assignment->cannot_be_combined = TRUE;

		//Add it in
		add_statement(basic_block, assignment);
	} 

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
static basic_block_t* basic_block_alloc(u_int32_t estimated_execution_frequency){
	//Allocate the block
	basic_block_t* created = calloc(1, sizeof(basic_block_t));

	//Put the block ID in
	created->block_id = increment_and_get();

	//Our sane defaults here - normal termination and normal type
	created->block_terminal_type = BLOCK_TERM_TYPE_NORMAL;
	//By default we're normal here
	created->block_type = BLOCK_TYPE_NORMAL;

	//What is the estimated execution cost of this block?
	created->estimated_execution_frequency = estimated_execution_frequency;

	//Let's add in what function this block came from
	created->function_defined_in = current_function;

	//Add this into the dynamic array
	dynamic_array_add(cfg_ref->created_blocks, created);

	return created;
}


/**
 * Print out the whole program in order. Done using an iterative
 * bfs
 */
static void emit_blocks_bfs(cfg_t* cfg, emit_dominance_frontier_selection_t print_df){
	//First, we'll reset every single block here
	reset_visited_status(cfg, FALSE);

	//For holding our blocks
	basic_block_t* block;

	//Now we'll print out each and every function inside of the function_blocks
	//array. Each function will be printed using the BFS strategy
	for(u_int16_t i = 0; i < cfg->function_blocks->current_index; i++){
		//We'll need a queue for our BFS
		heap_queue_t* queue = heap_queue_alloc();

		//Grab this out for convenience
		basic_block_t* function_entry_block = dynamic_array_get_at(cfg->function_blocks, i);

		//We'll want to see what the stack looks like
		print_stack_data_area(&(function_entry_block->function_defined_in->data_area));

		//Seed the search by adding the funciton block into the queue
		enqueue(queue, dynamic_array_get_at(cfg->function_blocks, i));

		//So long as the queue isn't empty
		while(queue_is_empty(queue) == HEAP_QUEUE_NOT_EMPTY){
			//Pop off of the queue
			block = dequeue(queue);

			//If this wasn't visited, we'll print
			if(block->visited == FALSE){
				print_block_three_addr_code(block, print_df);	
			}

			//Now we'll mark this as visited
			block->visited = TRUE;

			//And finally we'll add all of these onto the queue
			for(u_int16_t j = 0; block->successors != NULL && j < block->successors->current_index; j++){
				//Add the successor into the queue, if it has not yet been visited
				basic_block_t* successor = block->successors->internal_array[j];

				if(successor->visited == FALSE){
					enqueue(queue, successor);
				}
			}
		}

		//Destroy the heap queue when done
		heap_queue_dealloc(queue);
	}
}


/**
 * Destroy all old control relations in anticipation of new ones coming in
 */
void cleanup_all_control_relations(cfg_t* cfg){
	//For each block in the CFG
	for(u_int16_t _ = 0; _ < cfg->created_blocks->current_index; _++){
		//Grab the block out
		basic_block_t* block = dynamic_array_get_at(cfg->created_blocks, _);

		//Run through and destroy all of these old control flow constructs
		//Deallocate the postdominator set
		if(block->postdominator_set != NULL){
			dynamic_array_dealloc(block->postdominator_set);
			block->postdominator_set = NULL;
		}

		//Deallocate the dominator set
		if(block->dominator_set != NULL){
			dynamic_array_dealloc(block->dominator_set);
			block->dominator_set = NULL;
		}

		//Deallocate the dominator children
		if(block->dominator_children != NULL){
			dynamic_array_dealloc(block->dominator_children);
			block->dominator_children = NULL;
		}

		//Deallocate the domninance frontier
		if(block->dominance_frontier != NULL){
			dynamic_array_dealloc(block->dominance_frontier);
			block->dominance_frontier = NULL;
		}

		//Deallocate the reverse dominance frontier
		if(block->reverse_dominance_frontier != NULL){
			dynamic_array_dealloc(block->reverse_dominance_frontier);
			block->reverse_dominance_frontier = NULL;
		}

		//Deallocate the reverse post order set
		if(block->reverse_post_order_reverse_cfg != NULL){
			dynamic_array_dealloc(block->reverse_post_order_reverse_cfg);
			block->reverse_post_order_reverse_cfg = NULL;
		}

		//Deallocate the reverse post order set
		if(block->reverse_post_order != NULL){
			dynamic_array_dealloc(block->reverse_post_order);
			block->reverse_post_order = NULL;
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

	//Deallocate the live variable array
	if(block->used_variables != NULL){
		dynamic_array_dealloc(block->used_variables);
	}

	//Deallocate the assigned variable array
	if(block->assigned_variables != NULL){
		dynamic_array_dealloc(block->assigned_variables);
	}

	//Deallocate the postdominator set
	if(block->postdominator_set != NULL){
		dynamic_array_dealloc(block->postdominator_set);
	}

	//Deallocate the dominator set
	if(block->dominator_set != NULL){
		dynamic_array_dealloc(block->dominator_set);
	}

	//Deallocate the dominator children
	if(block->dominator_children != NULL){
		dynamic_array_dealloc(block->dominator_children);
	}

	//Deallocate the domninance frontier
	if(block->dominance_frontier != NULL){
		dynamic_array_dealloc(block->dominance_frontier);
	}

	//Deallocate the reverse dominance frontier
	if(block->reverse_dominance_frontier != NULL){
		dynamic_array_dealloc(block->reverse_dominance_frontier);
	}

	//Deallocate the reverse post order set
	if(block->reverse_post_order_reverse_cfg != NULL){
		dynamic_array_dealloc(block->reverse_post_order_reverse_cfg);
	}

	//Deallocate the reverse post order set
	if(block->reverse_post_order != NULL){
		dynamic_array_dealloc(block->reverse_post_order);
	}

	//Deallocate the liveness sets
	if(block->live_out != NULL){
		dynamic_array_dealloc(block->live_out);
	}

	if(block->live_in != NULL){
		dynamic_array_dealloc(block->live_in);
	}

	//Deallocate the successors
	if(block->successors != NULL){
		dynamic_array_dealloc(block->successors);
	}

	//Deallocate the predecessors
	if(block->predecessors != NULL){
		dynamic_array_dealloc(block->predecessors);
	}

	//If this is a switch statement entry block, then it will have a jump table
	if(block->block_type == BLOCK_TYPE_SWITCH){
		jump_table_dealloc(&(block->jump_table));
	}

	//Grab a statement cursor here
	instruction_t* cursor = block->leader_statement;
	//We'll need a temp block too
	instruction_t* temp = cursor;

	//So long as the cursor is not NULL
	while(cursor != NULL){
		temp = cursor;
		cursor = cursor->next_statement;
		//Destroy temp
		instruction_dealloc(temp);
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

	//Destroy the dynamic arrays too
	dynamic_array_dealloc(cfg->created_blocks);
	dynamic_array_dealloc(cfg->function_blocks);

	//At the very end, be sure to destroy this too
	free(cfg);
}


/**
 * Helper for returning error blocks. Error blocks always have an ID of -1
 */
static basic_block_t* create_and_return_err(){
	//Create the error
	basic_block_t* err_block = basic_block_alloc(1);
	//Set the ID to -1
	err_block->block_id = -1;

	return err_block;
}


/**
 * Exclusively add a successor to target. The predecessors of successor will not be touched
 */
void add_successor_only(basic_block_t* target, basic_block_t* successor){
	//If we ever find this - don't add it
	if(target == successor){
		return;
	}

	//If this is null, we'll perform the initial allocation
	if(target->successors == NULL){
		target->successors = dynamic_array_alloc();
	}

	//Does this block already contain the successor? If so we'll leave
	if(dynamic_array_contains(target->successors, successor) != NOT_FOUND){
		//We DID find it, so we won't add
		return;
	}

	//TODO DEPRECATE
	if(target->successors->current_index == 0){
		target->direct_successor = successor;
	}

	//Otherwise we're set to add it in
	dynamic_array_add(target->successors, successor);
}


/**
 * Exclusively add a predecessor to target. Nothing with successors
 * will be touched
 */
void add_predecessor_only(basic_block_t* target, basic_block_t* predecessor){
	//If we ever find this - don't add it
	if(target == predecessor){
		return;
	}

	//If this is NULL, we'll allocate here
	if(target->predecessors == NULL){
		target->predecessors = dynamic_array_alloc();
	}

	//Does this contain the predecessor block already? If so we won't add
	if(dynamic_array_contains(target->predecessors, predecessor) != NOT_FOUND){
		//We DID find it, so we won't add
		return;
	}

	//Now we can add
	dynamic_array_add(target->predecessors, predecessor);
}


/**
 * Add a successor to the target block. This method is entirely comprehensive.
 * Since "successor" comes after "target", "target" will be added as a predecessor
 * of "successor". If you wish to ONLY add a successor or predecessor(very rare),
 * then use the "only" methods
 */
 void add_successor(basic_block_t* target, basic_block_t* successor){
	//First we'll add successor as a successor of target
	add_successor_only(target, successor);

	//And then for completeness, we'll add target as a predecessor of successor
	add_predecessor_only(successor, target);
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
	if(b == NULL || b->leader_statement == NULL){
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
		//Connect backwards too
		b->leader_statement->previous_statement = a->exit_statement;
		//Now once they're connected we'll set a's exit to be b's exit
		a->exit_statement = b->exit_statement;
	}

	//If we're gonna merge two blocks, then they'll share all the same successors and predecessors
	//Let's merge predecessors first
	for(u_int16_t i = 0; b->predecessors != NULL && i < b->predecessors->current_index; i++){
		//Add b's predecessor as one to a
		add_predecessor_only(a, b->predecessors->internal_array[i]);
	}

	//Now merge successors
	for(u_int16_t i = 0; b->successors != NULL && i < b->successors->current_index; i++){
		//Add b's successors to be a's successors
		add_successor_only(a, b->successors->internal_array[i]);
	}

	//FOR EACH Successor of B, it will have a reference to B as a predecessor.
	//This is now wrong though. So, for each successor of B, it will need
	//to have A as predecessor
	for(u_int8_t i = 0; b->successors != NULL && i < b->successors->current_index; i++){
		//Grab the block first
		basic_block_t* successor_block = b->successors->internal_array[i];

		//Now for each of the predecessors that equals b, it needs to now point to A
		for(u_int8_t i = 0; successor_block->predecessors != NULL && i < successor_block->predecessors->current_index; i++){
			//If it's pointing to b, it needs to be updated
			if(successor_block->predecessors->internal_array[i] == b){
				//Update it to now be correct
				successor_block->predecessors->internal_array[i] = a;
			}
		}
	}

	//Also make note of any direct succession
	a->direct_successor = b->direct_successor;
	//Copy over the block type and terminal type
	if(a->block_type != BLOCK_TYPE_FUNC_ENTRY){
		a->block_type = b->block_type;
	}

	a->block_terminal_type = b->block_terminal_type;

	//If b executes more than A and it's now a part of A, we'll need to bump up A appropriately
	if(a->estimated_execution_frequency < b->estimated_execution_frequency){
		a->estimated_execution_frequency = b->estimated_execution_frequency;
	}

	//For each statement in b, all of it's old statements are now "defined" in a
	instruction_t* b_stmt = b->leader_statement;

	while(b_stmt != NULL){
		b_stmt->block_contained_in = a;

		//Push it up
		b_stmt = b_stmt->next_statement;
	}
	
	//IMPORTANT--wipe b's statements out
	b->leader_statement = NULL;
	b->exit_statement = NULL;

	//We'll now need to ensure that all of the used variables in B are also in A
	for(u_int16_t i = 0; b->used_variables != NULL && i < b->used_variables->current_index; i++){
		//Add these in one by one to A
		add_used_variable(a, b->used_variables->internal_array[i]);
	}

	//Copy over all of the assigned variables too
	for(u_int16_t i = 0; b->assigned_variables != NULL && i < b->assigned_variables->current_index; i++){
		//Add these in one by one
		add_assigned_variable(a, b->assigned_variables->internal_array[i]);
	}


	//We'll remove this from the list of created blocks
	dynamic_array_delete(cfg_ref->created_blocks, b);

	//And finally we'll deallocate b
	basic_block_dealloc(b);

	//Give back the pointer to a
	return a;
}


/**
 * A for-statement is another kind of control flow construct. As always the direct successor is the path that reliably
 * leads us down and out
 */
static basic_block_t* visit_for_statement(values_package_t* values){
	//Create our entry block. The entry block also only executes once
	basic_block_t* for_stmt_entry_block = basic_block_alloc(1);
	//Create our exit block. We assume that the exit only happens once
	basic_block_t* for_stmt_exit_block = basic_block_alloc(1);
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
		emit_expr_code(for_stmt_entry_block, ast_cursor->first_child, TRUE, FALSE);
	}

	//We'll now need to create our repeating node. This is the node that will actually repeat from the for loop.
	//The second and third condition in the for loop are the ones that execute continously. The third condition
	//always executes at the end of each iteration
	basic_block_t* condition_block = basic_block_alloc(LOOP_ESTIMATED_COST);

	//The condition block is always a successor to the entry block
	add_successor(for_stmt_entry_block, condition_block);

	//We will now emit a jump from the entry block, to the condition block
	emit_jump(for_stmt_entry_block, condition_block, JUMP_TYPE_JMP ,TRUE, FALSE);

	//Move along to the next node
	ast_cursor = ast_cursor->next_sibling;

	//The condition block values package
	expr_ret_package_t condition_block_vals;
	//By default, make this blank
	condition_block_vals.operator = BLANK;

	//If the second one is not blank
	if(ast_cursor->first_child != NULL){
		//This is always the first part of the repeating block
		condition_block_vals = emit_expr_code(condition_block, ast_cursor->first_child, TRUE, TRUE);
	//It is impossible for the second one to be blank
	} else {
		print_parse_message(PARSE_ERROR, "Fatal internal compiler error. Should not have gotten here if blank", for_stmt_node->line_number);
		exit(0);
	}

	//We'll use our inverse jumping("jump out") strategy here. We'll need this jump for later
	jump_type_t jump_type = select_appropriate_jump_stmt(condition_block_vals.operator, JUMP_CATEGORY_INVERSE, is_type_signed(condition_block_vals.assignee->type));

	//Now move it along to the third condition
	ast_cursor = ast_cursor->next_sibling;

	//Create the update block
	basic_block_t* for_stmt_update_block = basic_block_alloc(LOOP_ESTIMATED_COST);
	for_stmt_update_block->block_type = BLOCK_TYPE_FOR_STMT_UPDATE;

	//If the third one is not blank
	if(ast_cursor->first_child != NULL){
		//Emit the update expression
		emit_expr_code(for_stmt_update_block, ast_cursor->first_child, FALSE, FALSE);
	}
	
	//Unconditional jump to condition block
	emit_jump(for_stmt_update_block, condition_block, JUMP_TYPE_JMP, TRUE, FALSE);

	//This node will always jump right back to the start
	add_successor(for_stmt_update_block, condition_block);
	
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
													 	condition_block, //Loop statement start -- for loops start at their condition
													 	for_stmt_exit_block, //Exit block of loop
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

		//And also make sure that the condition block can point to the
		//exit
		add_successor(condition_block, for_stmt_exit_block);

		//Make the condition block jump to the exit. This is an inverse jump
		emit_jump(condition_block, for_stmt_exit_block, jump_type, TRUE, TRUE);

		//And we're done
		return for_stmt_entry_block;
	}

	//This will always be a successor to the conditional statement
	add_successor(condition_block, compound_stmt_start);

	//We must also remember that the condition block can point to the ending block, because
	//if the condition fails, we will be jumping here
	add_successor(condition_block, for_stmt_exit_block);

	//Make the condition block jump to the exit. This is an inverse jump
	emit_jump(condition_block, for_stmt_exit_block, jump_type, TRUE, TRUE);

	//Emit a direct jump from the condition block to the compound stmt start
	emit_jump(condition_block, compound_stmt_start, JUMP_TYPE_JMP, TRUE, FALSE);

	//This is a loop ending block
	condition_block->block_terminal_type = BLOCK_TERM_TYPE_LOOP_END;

	//However if it isn't NULL, we'll need to find the end of this compound statement
	basic_block_t* compound_stmt_end = compound_stmt_start;

	//So long as we don't see the end or a return
	while(compound_stmt_end->direct_successor != NULL && compound_stmt_end->block_terminal_type == BLOCK_TERM_TYPE_NORMAL){
		compound_stmt_end = compound_stmt_end->direct_successor;
	}

	//If it ends in a return statement, there is no point in continuing this
	if(compound_stmt_end->block_terminal_type != BLOCK_TERM_TYPE_RET){
		//We also need an uncoditional jump right to the update block
		emit_jump(compound_stmt_end, for_stmt_update_block, JUMP_TYPE_JMP, TRUE, FALSE);
	}

	//We'll add the successor either way for control flow reasons
	add_successor(compound_stmt_end, for_stmt_update_block);

	//The direct successor to the entry block is the exit block, for efficiency reasons
	for_stmt_entry_block->direct_successor = for_stmt_exit_block;

	//Give back the entry block
	return for_stmt_entry_block;
}


/**
 * A do-while statement is a simple control flow construct. As always, the direct successor path is the path that reliably
 * leads us down and out
 */
static basic_block_t* visit_do_while_statement(values_package_t* values){
	//Create our entry block. This in reality will be the compound statement
	basic_block_t* do_while_stmt_entry_block = basic_block_alloc(LOOP_ESTIMATED_COST);
	//The true ending block. We assume that the exit only happens once
	basic_block_t* do_while_stmt_exit_block = basic_block_alloc(1);
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
													 	do_while_stmt_entry_block, //Loop statement start
													 	do_while_stmt_exit_block, //Exit block of loop
													 	NULL); //For loop update block

	//We go right into the compound statement here
	basic_block_t* do_while_compound_stmt_entry = visit_compound_statement(&compound_stmt_values);

	//If this is NULL, it means that we really don't have a compound statement there
	if(do_while_compound_stmt_entry == NULL){
		print_parse_message(PARSE_ERROR, "Do-while statement has empty clause, statement has no effect", do_while_stmt_node->line_number);
		(*num_warnings_ref)++;
	}

	//No matter what, this will get merged into the top statement
	add_successor(do_while_stmt_entry_block, do_while_compound_stmt_entry);
	//Now we'll jump to it
	emit_jump(do_while_stmt_entry_block, do_while_compound_stmt_entry, JUMP_TYPE_JMP, TRUE, FALSE);

	//We will drill to the bottom of the compound statement
	basic_block_t* compound_stmt_end = do_while_stmt_entry_block;

	//So long as we don't see NULL or return
	while(compound_stmt_end->direct_successor != NULL && compound_stmt_end->block_terminal_type == BLOCK_TERM_TYPE_NORMAL){
		compound_stmt_end = compound_stmt_end->direct_successor;
	}

	//If we get this, we can't go forward. Just give it back
	if(compound_stmt_end->block_terminal_type == BLOCK_TERM_TYPE_RET){
		return do_while_stmt_entry_block;
	}

	//Add the conditional check into the end here
	expr_ret_package_t package = emit_expr_code(compound_stmt_end, ast_cursor->next_sibling, TRUE, TRUE);

	//Now we'll make do our necessary connnections. The direct successor of this end block is the true
	//exit block
	add_successor(compound_stmt_end, do_while_stmt_entry_block);
	//It's other successor though is the loop entry. This is for flow analysis
	add_successor(compound_stmt_end, do_while_stmt_exit_block);

	//Make sure it's the direct successor
	compound_stmt_end->direct_successor = do_while_stmt_exit_block;

	//We'll set the entry block's direct successor to be the exit block for efficiency
	do_while_stmt_entry_block->direct_successor = do_while_stmt_exit_block;

	//Discern the jump type here--This is a direct jump
	jump_type_t jump_type = select_appropriate_jump_stmt(package.operator, JUMP_CATEGORY_NORMAL, is_type_signed(package.assignee->type));
		
	//We'll need a jump statement here to the entrance block
	emit_jump(compound_stmt_end, do_while_stmt_entry_block, jump_type, TRUE, FALSE);
	//Also emit a jump statement to the ending block
	emit_jump(compound_stmt_end, do_while_stmt_exit_block, JUMP_TYPE_JMP, TRUE, FALSE);
	//This is our condition block here, so we'll add the estimated cost
	compound_stmt_end->estimated_execution_frequency = LOOP_ESTIMATED_COST;

	//Set the termination type of this block
	if(compound_stmt_end->block_terminal_type == BLOCK_TERM_TYPE_NORMAL){
		compound_stmt_end->block_terminal_type = BLOCK_TERM_TYPE_LOOP_END;
	}

	//Always return the entry block
	return do_while_stmt_entry_block;
}


/**
 * A while statement is a very simple control flow construct. As always, the "direct successor" path is the path
 * that reliably leads us down and out
 */
static basic_block_t* visit_while_statement(values_package_t* values){
	//Create our entry block
	basic_block_t* while_statement_entry_block = basic_block_alloc(LOOP_ESTIMATED_COST);
	//And create our exit block. We assume that this executes once
	basic_block_t* while_statement_end_block = basic_block_alloc(1);
	//We will specifically mark the end block here as an ending block
	while_statement_end_block->block_type = BLOCK_TYPE_WHILE_END;

	//For drilling efficiency reasons, we'll want the entry block's direct successor to be the end block
	while_statement_entry_block->direct_successor = while_statement_end_block;

	//Grab this for convenience
	generic_ast_node_t* while_stmt_node = values->initial_node;

	//Grab a cursor to the while statement node
	generic_ast_node_t* ast_cursor = while_stmt_node->first_child;

	//The entry block contains our expression statement
	expr_ret_package_t package = emit_expr_code(while_statement_entry_block, ast_cursor, TRUE, TRUE);

	//The very next node is a compound statement
	ast_cursor = ast_cursor->next_sibling;

	//If it isn't, we'll error out. This is really only for dev use
	if(ast_cursor->CLASS != AST_NODE_CLASS_COMPOUND_STMT){
		print_cfg_message(PARSE_ERROR, "Found node that is not a compound statement in while-loop subtree", while_stmt_node->line_number);
		exit(0);
	}

	//Create a copy of our values here
	values_package_t compound_stmt_values = pack_values(ast_cursor, //Initial Node
													 	while_statement_entry_block, //Loop statement start
													 	while_statement_end_block, //Exit block of loop
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

		//We'll just return now
		return while_statement_entry_block;
	}


	//We'll now determine what kind of jump statement that we have here. We want to jump to the exit if
	//we're bad, so we'll do an inverse jump
	jump_type_t jump_type = select_appropriate_jump_stmt(package.operator, JUMP_CATEGORY_INVERSE, is_type_signed(package.assignee->type));
	//"Jump over" the body if it's bad
	emit_jump(while_statement_entry_block, while_statement_end_block, jump_type, TRUE, TRUE);

	//Otherwise it isn't null, so we can add it as a successor
	add_successor(while_statement_entry_block, compound_stmt_start);

	//We want to have a direct jump to the body too
	emit_jump(while_statement_entry_block, compound_stmt_start, JUMP_TYPE_JMP, TRUE, FALSE);

	//The exit block is also a successor to the entry block
	add_successor(while_statement_entry_block, while_statement_end_block);

	//Let's now find the end of the compound statement
	basic_block_t* compound_stmt_end = compound_stmt_start;

	//So long as it isn't null or return
	while (compound_stmt_end->direct_successor != NULL && compound_stmt_end->block_terminal_type == BLOCK_TERM_TYPE_NORMAL){
		compound_stmt_end = compound_stmt_end->direct_successor;
	}

	//If it's not a return statement, we can add all of these in
	if(compound_stmt_end->block_terminal_type != BLOCK_TERM_TYPE_RET){
		//A successor to the end block is the block at the top of the loop
		add_successor(compound_stmt_end, while_statement_entry_block);
		//His direct successor is the end block
		compound_stmt_end->direct_successor = while_statement_end_block;
		//The compound statement end will jump right back up to the entry block
		emit_jump(compound_stmt_end, while_statement_entry_block, JUMP_TYPE_JMP, TRUE, FALSE);
	}

	//Set this to make sure
	compound_stmt_end->direct_successor = while_statement_end_block;

	//Set the termination type of this block
	if(compound_stmt_end->block_terminal_type == BLOCK_TERM_TYPE_NORMAL){
		compound_stmt_end->block_terminal_type = BLOCK_TERM_TYPE_LOOP_END;
	}

	//Now we're done, so
	return while_statement_entry_block;
}


/**
 * Process the if-statement subtree into CFG form
 */
static basic_block_t* visit_if_statement(values_package_t* values){
	//We always have an entry block and an exit block. We assume initially that
	//these both happen once
	basic_block_t* entry_block = basic_block_alloc(1);
	basic_block_t* exit_block = basic_block_alloc(1);
	exit_block->block_type = BLOCK_TYPE_IF_STMT_END;

	//Grab the cursor
	generic_ast_node_t* cursor = values->initial_node->first_child;

	//Add whatever our conditional is into the starting block
	expr_ret_package_t package = emit_expr_code(entry_block, cursor, TRUE, TRUE);

	//No we'll move one step beyond, the next node must be a compound statement
	cursor = cursor->next_sibling;

	//Create a copy of our values here
	values_package_t if_compound_stmt_values = pack_values(cursor, //Initial Node
													 	values->loop_stmt_start, //Loop statement start
													 	values->loop_stmt_end, //Exit block of loop
													 	values->for_loop_update_block); //For loop update block

	//Visit the compound statement that we're required to have here
	basic_block_t* if_compound_stmt_entry = visit_compound_statement(&if_compound_stmt_values);
	basic_block_t* if_compound_stmt_end;

	if(if_compound_stmt_entry != NULL){
		//Add the if statement node in as a direct successor
		add_successor(entry_block, if_compound_stmt_entry);
		//We will perform a normal jump to this one
		jump_type_t jump_to_if = select_appropriate_jump_stmt(package.operator, JUMP_CATEGORY_NORMAL, is_type_signed(package.assignee->type));
		emit_jump(entry_block, if_compound_stmt_entry, jump_to_if, TRUE, FALSE);

		//Now we'll find the end of this statement
		if_compound_stmt_end = if_compound_stmt_entry;

		//Once we've visited, we'll need to drill to the end of this compound statement
		while(if_compound_stmt_end->direct_successor != NULL && if_compound_stmt_end->block_terminal_type == BLOCK_TERM_TYPE_NORMAL){
			if_compound_stmt_end = if_compound_stmt_end->direct_successor;
		}

		//If this is not a return block, we will add these
		if(if_compound_stmt_end->block_terminal_type != BLOCK_TERM_TYPE_RET){
			//The successor to the if-stmt end path is the if statement end block
			emit_jump(if_compound_stmt_end, exit_block, JUMP_TYPE_JMP, TRUE, FALSE);
			//If this is the case, the end block is a successor of the if_stmt end
			add_successor(if_compound_stmt_end, exit_block);
		} else {
			//If this is the case, the end block is a successor of the if_stmt end
			add_successor(if_compound_stmt_end, exit_block);
		}

	//If this is null, it's fine, but we should throw a warning
	} else {
		print_cfg_message(WARNING, "Empty if clause in if-statement", cursor->line_number);
		(*num_warnings_ref)++;

		//We'll just set this to jump out of here
		//We will perform a normal jump to this one
		jump_type_t jump_to_if = select_appropriate_jump_stmt(package.operator, JUMP_CATEGORY_NORMAL, is_type_signed(package.assignee->type));
		emit_jump(entry_block, exit_block, jump_to_if, TRUE, FALSE);
		add_successor(entry_block, exit_block);
	}

	//Advance the cursor up to it's next sibling
	cursor = cursor->next_sibling;

	//We'll need to keep track of the current entry block
	basic_block_t* current_entry_block = entry_block;
	//And we'll have a temp for when we switch over
	basic_block_t* temp;

	//For traversing the else-if tree
	generic_ast_node_t* else_if_cursor;

	//So long as we keep seeing else-if clauses
	while(cursor != NULL && cursor->CLASS == AST_NODE_CLASS_ELSE_IF_STMT){
		//This will be the expression
		else_if_cursor = cursor->first_child;

		//Since we're already in the else-if region, we know that we'll
		//need a new temp block. This means that we'll need to jump from the old
		//entry block to a new one
		
		//Save the old one
		temp = current_entry_block;
		//Make a new one
		current_entry_block = basic_block_alloc(1);
		//The new one is a successor of the old one
		add_successor(temp, current_entry_block);
		//And we'll emit a direct jump from the old one to the new one
		emit_jump(temp, current_entry_block, JUMP_TYPE_JMP, TRUE, FALSE);

		//So we've seen the else-if clause. Let's grab the expression first
		package = emit_expr_code(current_entry_block, else_if_cursor, TRUE, TRUE);

		//Advance it up -- we should now have a compound statement
		else_if_cursor = else_if_cursor->next_sibling;

		//For compound statement handling
		values_package_t else_if_compound_stmt_values = pack_values(else_if_cursor, //Initial Node
													 	values->loop_stmt_start, //Loop statement start
													 	values->loop_stmt_end, //Exit block of loop
													 	values->for_loop_update_block); //For loop update block

		//Let this handle the compound statement
		basic_block_t* else_if_compound_stmt_entry = visit_compound_statement(&else_if_compound_stmt_values);
		basic_block_t* else_if_compound_stmt_exit;

		//If it's not null, we'll process fully
		if(else_if_compound_stmt_entry != NULL){
			//Add the if statement node in as a direct successor
			add_successor(current_entry_block, else_if_compound_stmt_entry);
			//We will perform a normal jump to this one
			jump_type_t jump_to_if = select_appropriate_jump_stmt(package.operator, JUMP_CATEGORY_NORMAL, is_type_signed(package.assignee->type));
			emit_jump(current_entry_block, else_if_compound_stmt_entry, jump_to_if, TRUE, FALSE);

			//Now we'll find the end of this statement
			else_if_compound_stmt_exit = else_if_compound_stmt_entry;

			//Once we've visited, we'll need to drill to the end of this compound statement
			while(else_if_compound_stmt_exit->direct_successor != NULL && else_if_compound_stmt_exit->block_terminal_type == BLOCK_TERM_TYPE_NORMAL){
				else_if_compound_stmt_exit = else_if_compound_stmt_exit->direct_successor;
			}

			//If this is not a return block, we will add these
			if(else_if_compound_stmt_exit->block_terminal_type != BLOCK_TERM_TYPE_RET){
				//The successor to the if-stmt end path is the if statement end block
				emit_jump(else_if_compound_stmt_exit, exit_block, JUMP_TYPE_JMP, TRUE, FALSE);
				//If this is the case, the end block is a successor of the if_stmt end
				add_successor(else_if_compound_stmt_exit, exit_block);
			}	else {
				add_successor(else_if_compound_stmt_exit, exit_block);
			}

		//If this is NULL, it's fine, but we should warn
		} else {
			print_cfg_message(WARNING, "Empty else-if clause in else-if-statement", cursor->line_number);
			(*num_warnings_ref)++;

			//We'll just set this to jump out of here
			//We will perform a normal jump to this one
			jump_type_t jump_to_else_if = select_appropriate_jump_stmt(package.operator, JUMP_CATEGORY_NORMAL, is_type_signed(package.assignee->type));
			emit_jump(current_entry_block, exit_block, jump_to_else_if, TRUE, FALSE);
			add_successor(current_entry_block, exit_block);
		}

		//Advance this up to the next one
		cursor = cursor->next_sibling;
	}

	//Now that we're out of here - we may have an else statement on our hands
	if(cursor != NULL && cursor->CLASS == AST_NODE_CLASS_COMPOUND_STMT){
		//Let's handle the compound statement
		
		//For compound statement handling
		values_package_t else_compound_stmt_values = pack_values(cursor, //Initial Node
													 	values->loop_stmt_start, //Loop statement start
													 	values->loop_stmt_end, //Exit block of loop
													 	values->for_loop_update_block); //For loop update block

		//Grab the compound statement
		basic_block_t* else_compound_stmt_entry = visit_compound_statement(&else_compound_stmt_values);
		basic_block_t* else_compound_stmt_exit;

		//If it's NULL, that's fine, we'll just throw a warning
		if(else_compound_stmt_entry == NULL){
			print_cfg_message(WARNING, "Empty else clause in else-statement", cursor->line_number);
			(*num_warnings_ref)++;
			//We'll jump to the end here
			add_successor(current_entry_block, exit_block);
			//Emit a direct jump here
			emit_jump(current_entry_block, exit_block, JUMP_TYPE_JMP, TRUE, FALSE);
		} else {
			//Add the if statement node in as a direct successor
			add_successor(current_entry_block, else_compound_stmt_entry);
			//We will perform a normal jump to this one
			emit_jump(current_entry_block, else_compound_stmt_entry, JUMP_TYPE_JMP, TRUE, FALSE);

			//Now we'll find the end of this statement
			else_compound_stmt_exit = else_compound_stmt_entry;

			//Once we've visited, we'll need to drill to the end of this compound statement
			while(else_compound_stmt_exit->direct_successor != NULL && else_compound_stmt_exit->block_terminal_type == BLOCK_TERM_TYPE_NORMAL){
				else_compound_stmt_exit = else_compound_stmt_exit->direct_successor;
			}

			//If this is not a return block, we will add these
			if(else_compound_stmt_exit->block_terminal_type != BLOCK_TERM_TYPE_RET){
				//The successor to the if-stmt end path is the if statement end block
				emit_jump(else_compound_stmt_exit, exit_block, JUMP_TYPE_JMP, TRUE, FALSE);
				//If this is the case, the end block is a successor of the if_stmt end
				add_successor(else_compound_stmt_exit, exit_block);
			} else {
				add_successor(else_compound_stmt_exit, exit_block);
			}
		}

	//Otherwise the if statement will need to jump directly to the end
	} else {
		//We'll jump to the end here
		add_successor(current_entry_block, exit_block);
		//Emit a direct jump here
		emit_jump(current_entry_block, exit_block, JUMP_TYPE_JMP, TRUE, FALSE);
	}

	//For our convenience - this makes drilling way faster
	entry_block->direct_successor = exit_block;

	//give the entry block back
	return entry_block;
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
	//Create it. We assume that this happens once
	basic_block_t* default_stmt = basic_block_alloc(1);
	//Treated as case statements
	default_stmt->block_type = BLOCK_TYPE_CASE;

	//Now that we've actually packed up the value of the case statement here, we'll use the helper method to go through
	//any/all statements that are below it
	values_package_t statement_values = *values;
	//Only difference here is the starting place
	statement_values.initial_node = default_stmt_cursor->first_child;

	//Let this take care of it
	if(statement_values.initial_node != NULL){
		basic_block_t* compound_statement_start = visit_compound_statement(&statement_values);
	
		//If this is the case, just allocate a dummy
		if(compound_statement_start == NULL){
			compound_statement_start = basic_block_alloc(1);
		}	

		//If we have an error
		if(compound_statement_start->block_id == -1){
			return compound_statement_start;
		}

		//Once we get this back, we'll add it in to the main block
		merge_blocks(default_stmt, compound_statement_start);
	}
	

	//Give the block back
	return default_stmt;
}


/**
 * Visit a case statement. It is very important that case statements know
 * where the end of the switch statement is, in case break statements are used
 */
static basic_block_t* visit_case_statement(values_package_t* values){
	//We need to make the block first
	basic_block_t* case_stmt = basic_block_alloc(1);
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

	//If this isn't Null, we'll run the analysis. If it is NULL, we have an empty case block
	if(statement_values.initial_node != NULL){
		//Let this take care of it
		basic_block_t* compound_statement_start = visit_compound_statement(&statement_values);

		//If this is the case, just allocate a dummy
		if(compound_statement_start == NULL){
			compound_statement_start = basic_block_alloc(1);
		}

		//If we have an error
		if(compound_statement_start->block_id == -1){
			return compound_statement_start;
		}

		//Once we get this back, we'll add it in to the main block
		merge_blocks(case_stmt, compound_statement_start);
	}

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
	basic_block_t* starting_block = basic_block_alloc(1);
	//Mark that this is a switch statement
	starting_block->block_type = BLOCK_TYPE_SWITCH;

	//We also need to know the ending block here -- Knowing
	//this is important for break statements
	basic_block_t* ending_block = basic_block_alloc(1);

	//We need a quick reference to the starting block ID
	u_int16_t starting_block_id = starting_block->block_id;

	//If this is empty, serious issue. The initial node already is
	//a switch statement. Its first child is the expression inside of it
	if(values->initial_node->first_child == NULL){
		//Print this message
		print_cfg_message(WARNING, "Empty switch statement detected", values->initial_node->line_number);
		(*num_warnings_ref)++;
		//It's just going to be empty
		return starting_block;
	}

	//Let's also allocate our jump table. We know how large the jump table needs to be from
	//data passed in by the parser
	starting_block->jump_table = jump_table_alloc(values->initial_node->upper_bound - values->initial_node->lower_bound + 1);

	//We'll also have some adjustment amount, since we always want the lowest value in the jump table to be 0. This
	//adjustment will be subtracted from every value at the top to "knock it down" to be within the jump table
	u_int32_t offset = values->initial_node->lower_bound - 0;

	//Grab a cursor to the case statements
	generic_ast_node_t* case_stmt_cursor = values->initial_node->first_child;
	
	//Save the expression node for now, we won't use this until later on
	generic_ast_node_t* expression_node = case_stmt_cursor;

	//The values package that we have
	values_package_t passing_values = *values;

	//Keep a reference to whatever the current switch statement block is
	basic_block_t* current_block = starting_block;
	
	//The current block(case or default) that we're on
	basic_block_t* case_block;

	//We'll need to save the default block for our purposes later
	basic_block_t* default_block;

	//Get to the next statement. This is the first actual case 
	//statement
	case_stmt_cursor = case_stmt_cursor->next_sibling;

	//So long as this isn't null
	while(case_stmt_cursor != NULL){
		//Switch based on the class
		switch(case_stmt_cursor->CLASS){
			//Handle a case statement
			case AST_NODE_CLASS_CASE_STMT:
				//Update this
				passing_values.initial_node = case_stmt_cursor;
				//Visit our case stmt here
				case_block = visit_case_statement(&passing_values);

				//We'll now need to add this into the jump table. We always subtract the adjustment to ensure
				//that we start down at 0 as the lowest value
				add_jump_table_entry(&(starting_block->jump_table), case_block->case_stmt_val - offset, case_block);
				break;

			//Handle a default statement
			case AST_NODE_CLASS_DEFAULT_STMT:
				//Update this
				passing_values.initial_node = case_stmt_cursor;
				//Visit the default statement
				case_block = visit_default_statement(&passing_values);

				//This is the default block, so save for now
				default_block = case_block;

				break;

			//Otherwise we have some weird error, so we'll fail out
			default:
				print_cfg_message(PARSE_ERROR, "Switch statements are only allowed \"case\" and \"default\" statements", case_stmt_cursor->line_number);
				exit(0);
		}

		//Now we'll add this one into the overall structure
		add_successor(starting_block, case_block);

		//Now we'll drill down to the bottom to prime the next pass
		current_block = case_block;
		while(current_block->direct_successor != NULL && current_block->block_terminal_type == BLOCK_TERM_TYPE_NORMAL){
			current_block = current_block->direct_successor;
		}

		//Since there is no concept of falling through in Ollie, these case statements all branch right to the end
		add_successor(current_block, ending_block);

		//We will always emit a direct jump from this block to the ending block
		emit_jump(current_block, ending_block, JUMP_TYPE_JMP, TRUE, FALSE);

		//Move the cursor up
		case_stmt_cursor = case_stmt_cursor->next_sibling;
	}

	//Now at the ever end, we'll need to fill the remaining jump table blocks that are empty
	//with the default value
	for(u_int16_t _ = 0; _ < starting_block->jump_table.num_nodes; _++){
		//If it's null, we'll make it the default
		if(starting_block->jump_table.nodes[_] == NULL){
			starting_block->jump_table.nodes[_] = default_block;
		}
	}

	//Now that everything has been situated, we can start emitting the values in the initial node

	//We'll need both of these as constants for our computation
	three_addr_const_t* lower_bound = emit_int_constant_direct(values->initial_node->lower_bound, type_symtab);
	three_addr_const_t* upper_bound = emit_int_constant_direct(values->initial_node->upper_bound, type_symtab);

	//Now that we have our expression, we'll want to speed things up by seeing if our value is either below the lower
	//range or above the upper range. If it is, we jump to the very end
	
	//The very first thing should be an expression telling us what to switch on
	//There should be some kind of expression here
	expr_ret_package_t package1 = emit_expr_code(starting_block, expression_node, TRUE, TRUE);

	//Unsigned by default
	u_int8_t is_signed = is_type_signed(package1.assignee->type);

	//First step -> if we're below the minimum, we jump to default 
	 emit_binary_operation_with_constant(starting_block, package1.assignee, package1.assignee, L_THAN, lower_bound, TRUE);
	//If we are lower than this(regular jump), we will go to the default block
	jump_type_t jump_lower_than = select_appropriate_jump_stmt(L_THAN, JUMP_CATEGORY_NORMAL, is_signed);
	//Now we'll emit our jump
	emit_jump(starting_block, default_block, jump_lower_than, TRUE, FALSE);

	//Due to the way temp assignment works, we actually need to re-emit this whole thing
	expr_ret_package_t package2 = emit_expr_code(starting_block, expression_node, TRUE, TRUE);

	//Next step -> if we're above the maximum, jump to default
	emit_binary_operation_with_constant(starting_block, package2.assignee, package2.assignee, G_THAN, upper_bound, TRUE);
	//If we are lower than this(regular jump), we will go to the default block
	jump_type_t jump_greater_than = select_appropriate_jump_stmt(G_THAN, JUMP_CATEGORY_NORMAL, is_signed);
	//Now we'll emit our jump
	emit_jump(starting_block, default_block, jump_greater_than, TRUE, FALSE);

	//Due to the way temp assignment works, we actually need to re-emit this whole thing
	expr_ret_package_t package3 = emit_expr_code(starting_block, expression_node, TRUE, TRUE);

	//Now that all this is done, we can use our jump table for the rest
	//We'll now need to cut the value down by whatever our offset was	
	three_addr_var_t* input = emit_binary_operation_with_constant(starting_block, package3.assignee, package3.assignee, MINUS, emit_int_constant_direct(offset, type_symtab), TRUE);

	/**
	 * Now that we've subtracted, we'll need to do the address calculation. The address calculation is as follows:
	 * 	base address(.JT1) + input * 8 
	 *
	 * We have a special kind of statement for doing this
	 * 	
	 *
	 * 	TODO an idea: we could replace this is a right shift by 3(just a thought)
	 */
	//Emit the address first
	three_addr_var_t* address = emit_indirect_jump_address_calculation(starting_block, &(starting_block->jump_table), input, TRUE);

	//Now we'll emit the indirect jump to the address
	emit_indirect_jump(starting_block, address, JUMP_TYPE_JMP, TRUE);

	//Ensure that the starting block's direct successor is the end block, for convenience
	starting_block->direct_successor = ending_block;

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
			basic_block_t* decl_block = visit_declaration_statement(&values, VARIABLE_SCOPE_LOCAL);

			//If the start block is null, then this is the start block. Otherwise, we merge it in
			if(starting_block == NULL){
				starting_block = decl_block;
				current_block = decl_block;
			//Just merge with current
			} else {
				current_block = merge_blocks(current_block, decl_block); 
			}

		//We've found a let statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_LET_STMT){
			values_package_t values;
			values.initial_node = ast_cursor;

			//We'll visit the block here
			basic_block_t* let_block = visit_let_statement(&values, VARIABLE_SCOPE_LOCAL, FALSE);

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
				//We assume that this only happens once
				starting_block = basic_block_alloc(1);
				current_block = starting_block;
			}

			//Emit the return statement, let the sub rule handle
			emit_ret(current_block, ast_cursor, FALSE);

			//Destroy any/all successors of the current block. Once you have a return statement in a block, there
			//can be no other successors
			if(current_block->successors != NULL){
				dynamic_array_dealloc(current_block->successors);
				current_block->successors = NULL;
			}

			//A successor to this block is the exit block
			add_successor(current_block, function_exit_block);

			//The current block will now be marked as a return statement
			current_block->block_terminal_type = BLOCK_TERM_TYPE_RET;

			//If there is anything after this statement, it is UNREACHABLE
			if(ast_cursor->next_sibling != NULL){
				print_cfg_message(WARNING, "Unreachable code detected after return statement", ast_cursor->next_sibling->line_number);
				(*num_warnings_ref)++;
			}

			//We're completely done here
			return starting_block;

		//We've found an if-statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_IF_STMT){
			//Create the values package
			values_package_t if_stmt_values;
			if_stmt_values.initial_node = ast_cursor;
			if_stmt_values.for_loop_update_block = values->for_loop_update_block;
			if_stmt_values.loop_stmt_start = values->loop_stmt_start;
			if_stmt_values.loop_stmt_end = values->loop_stmt_end;

			//We'll now enter the if statement
			basic_block_t* if_stmt_start = visit_if_statement(&if_stmt_values);
			
			//Once we have the if statement start, we'll add it in as a successor
			if(starting_block == NULL){
				starting_block = if_stmt_start;
				current_block = if_stmt_start;
			} else {
				//Add a successor to the current block
				add_successor(current_block, if_stmt_start);
				//Emit a jump from current to the start
				emit_jump(current_block, if_stmt_start, JUMP_TYPE_JMP, TRUE, FALSE);
				current_block = if_stmt_start;
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
				//Emit a direct jump to it
				emit_jump(current_block, while_stmt_entry_block, JUMP_TYPE_JMP, TRUE, FALSE);
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
				emit_jump(current_block, do_while_stmt_entry_block, JUMP_TYPE_JMP, TRUE, FALSE);
			}

			//Now we'll need to reach the end-point of this statement
			current_block = do_while_stmt_entry_block;

			//As long as we aren't at the very end
			while(current_block->direct_successor != NULL && current_block->block_type != BLOCK_TYPE_DO_WHILE_END){
				current_block = current_block->direct_successor;
			}

			//Otherwise, we're all set to go to the next iteration

		//Handle a for statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_FOR_STMT){
			//Create the values package
			values_package_t for_stmt_values;
			for_stmt_values.initial_node = ast_cursor;
			for_stmt_values.for_loop_update_block = values->for_loop_update_block;
			for_stmt_values.loop_stmt_start = NULL;
			for_stmt_values.loop_stmt_end = NULL;

			//First visit the statement
			basic_block_t* for_stmt_entry_block = visit_for_statement(&for_stmt_values);

			//Now we'll add it in
			if(starting_block == NULL){
				starting_block = for_stmt_entry_block;
				current_block = starting_block;
			//We don't merge, we'll add successors
			} else {
				add_successor(current_block, for_stmt_entry_block);
				emit_jump(current_block, for_stmt_entry_block, JUMP_TYPE_JMP, TRUE, FALSE);
				current_block = for_stmt_entry_block;
			}
			
			//Once we're here the start is in current
			while(current_block->block_type != BLOCK_TYPE_FOR_STMT_END){
				current_block = current_block->direct_successor;
			}

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
				//We'll assume that this only happens once
				starting_block = basic_block_alloc(1);
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
					emit_jump(current_block, values->loop_stmt_start, JUMP_TYPE_JMP, TRUE, FALSE);

				//We are in a for loop
				} else {
					//Otherwise we are in a for loop, so we just need to point to the for loop update block
					add_successor(current_block, values->for_loop_update_block);
					//Emit a direct unconditional jump statement to it
					emit_jump(current_block, values->for_loop_update_block, JUMP_TYPE_JMP, TRUE, FALSE);
				}

				//We're done here, so return the starting block. There is no 
				//point in going on
				return starting_block;

			//Otherwise, we have a conditional continue here
			} else {
				//Emit the expression code into the current statement
				expr_ret_package_t package = emit_expr_code(current_block, ast_cursor->first_child, TRUE, TRUE);
				//Decide the appropriate jump statement -- direct path here
				jump_type_t jump_type = select_appropriate_jump_stmt(package.operator, JUMP_CATEGORY_NORMAL, is_type_signed(package.assignee->type));

				//We'll need a new block here - this will count as a branch
				basic_block_t* new_block = basic_block_alloc(1);
				
				//Two divergent paths here -- whether or not we have a for loop
				//Not a for loop
				if(values->for_loop_update_block == NULL){
					//Otherwise we are in a loop, so this means that we need to point the continue statement to
					//the loop entry block
					//Add the successor in
					add_successor(current_block, values->loop_stmt_start);
					//Add this new block in as a successor
					add_successor(current_block, new_block);
					//Restore the direct successor
					current_block->direct_successor = new_block;
					//We always jump to the start of the loop statement unconditionally
					emit_jump(current_block, values->loop_stmt_start, jump_type, TRUE, FALSE);
					//The other end of the conditional continue will be jumping to this new block
					emit_jump(current_block, new_block, JUMP_TYPE_JMP, TRUE, FALSE);
				//We are in a for loop
				} else {
					//Otherwise we are in a for loop, so we just need to point to the for loop update block
					//Add the successor in
					add_successor(current_block, values->for_loop_update_block);
					//Add this new block in as a successor
					add_successor(current_block, new_block);
					//Restore the direct successor
					current_block->direct_successor = new_block;
					//Emit a direct unconditional jump statement to it
					emit_jump(current_block, values->for_loop_update_block, jump_type, TRUE, FALSE);
					//The other end of the conditional continue will be jumping to this new block
					emit_jump(current_block, new_block, JUMP_TYPE_JMP, TRUE, FALSE);
				}

				//And as we go forward, this new block will be the current block
				current_block = new_block;
			}

		//Hand le a break out statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_BREAK_STMT){
			//Let's first see if we're in a loop or switch statement or not
			if(values->loop_stmt_start == NULL){
				print_cfg_message(PARSE_ERROR, "Break statement was not found in a loop", ast_cursor->line_number);
				(*num_errors_ref)++;
				return create_and_return_err();
			}

			//This could happen where we have nothing here
			if(starting_block == NULL){
				starting_block = basic_block_alloc(1);
				current_block = starting_block;
			}

			//There are two options here: We could have a conditional break
			//or a normal break. If there is no child node, we have a normal break
			if(ast_cursor->first_child == NULL){
				//Mark this for later
				current_block->block_terminal_type = BLOCK_TERM_TYPE_BREAK;

				//We'll need to break out of the loop
				add_successor(current_block, values->loop_stmt_end);
				//We will jump to it -- this is always an uncoditional jump
				emit_jump(current_block, values->loop_stmt_end, JUMP_TYPE_JMP, TRUE, FALSE);

				//For a regular break statement, this is it, so we just get out
				//Give back the starting block
				return starting_block;

			//Otherwise, we have a conditional break, which will generate a conditional jump instruction
			} else {
				//We'll also need a new block to jump to, since this is a conditional break
				basic_block_t* new_block = basic_block_alloc(1);

				//First let's emit the conditional code
				expr_ret_package_t ret_package = emit_expr_code(current_block, ast_cursor->first_child, TRUE, TRUE);

				//Now based on whatever we have in here, we'll emit the appropriate jump type(direct jump)
				jump_type_t jump_type = select_appropriate_jump_stmt(ret_package.operator, JUMP_CATEGORY_NORMAL, is_type_signed(ret_package.assignee->type));

				//Add a successor to the end
				add_successor(current_block, values->loop_stmt_end);
				//Add the new block as a successor as well
				add_successor(current_block, new_block);

				//Make sure we mark this properly
				current_block->direct_successor = new_block;

				//We will jump to it -- this jump is decided above
				emit_jump(current_block, values->loop_stmt_end, jump_type, TRUE, FALSE);
				//Emit a jump to the new block
				emit_jump(current_block, new_block, JUMP_TYPE_JMP, TRUE, FALSE);

				//Once we're out here, the current block is now the new one
				current_block = new_block;
			}

		//Handle a defer statement. Remember that a defer statment is one monolithic
		//node with a bunch of sub-nodes underneath that are all handleable by "expr"
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_DEFER_STMT){
			//Grab a cursor here
			generic_ast_node_t* defer_stmt_cursor = ast_cursor->first_child;

			//So long as this cursor is not null, we'll keep processing and adding
			//compound statements
			while(defer_stmt_cursor != NULL){
				//Package the values
				values_package_t values = pack_values(defer_stmt_cursor, NULL, NULL, NULL);

				//Let the helper process this
				basic_block_t* compound_stmt_block = visit_compound_statement(&values);

				//The successor to the current block is this block
				//If it's null then this is this block
				if(starting_block == NULL){
					starting_block = compound_stmt_block;
				} else {
					//Otherwise it's a successor
					add_successor(current_block, compound_stmt_block);
					//Jump to it - important for optimizer
					emit_jump(current_block, compound_stmt_block, JUMP_TYPE_JMP, TRUE, FALSE);
				}

				//Regardless, the current block now is the compound statement
				current_block = compound_stmt_block;

				//Once we're here the start is in current, we'll need to drill to the end
				while(current_block->direct_successor != NULL && current_block->block_terminal_type != BLOCK_TERM_TYPE_RET){
					current_block = current_block->direct_successor;
				}

				//Advance this to the next one
				defer_stmt_cursor = defer_stmt_cursor->next_sibling;
			}


		//Handle a labeled statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_LABEL_STMT){
			//This really shouldn't happen, but it can't hurt
			if(starting_block == NULL){
				starting_block = basic_block_alloc(1);
				current_block = starting_block;
			}
			
			//We rely on the helper to do it for us
			emit_label(current_block, ast_cursor, FALSE);

		//Handle a jump statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_JUMP_STMT){
			//This really shouldn't happen, but it can't hurt
			if(starting_block == NULL){
				starting_block = basic_block_alloc(1);
				current_block = starting_block;
			}

			//We rely on the helper to do it for us
			emit_direct_jump(current_block, ast_cursor, TRUE);

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
			} else {
				//Otherwise this is a direct successor
				add_successor(current_block, switch_stmt_entry);
				//We will also emit a jump from the current block to the entry
				emit_jump(current_block, switch_stmt_entry, JUMP_TYPE_JMP, TRUE, FALSE);
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

		//These are 100% user generated,
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_ASM_INLINE_STMT){
			//If we find an assembly inline statement, the actuality of it is
			//incredibly easy. All that we need to do is literally take the 
			//user's statement and insert it into the code

			//We'll need a new block here regardless
			if(starting_block == NULL){
				starting_block = basic_block_alloc(1);
				current_block = starting_block;
			}

			//Let the helper handle
			emit_assembly_inline(current_block, ast_cursor, FALSE);
		//Handle a nop statement
		} else if(ast_cursor->CLASS == AST_NODE_CLASS_IDLE_STMT){
			//Do we need a new block?
			if(starting_block == NULL){
				starting_block = basic_block_alloc(1);
				current_block = starting_block;
			}

			//Let the helper handle -- doesn't even need the cursor
			emit_idle(current_block, FALSE);
	
		//This means that we have some kind of expression statement
		} else {
			//This could happen where we have nothing here
			if(starting_block == NULL){
				starting_block = basic_block_alloc(1);
				current_block = starting_block;
			}
			
			//Also emit the simplified machine code
			emit_expr_code(current_block, ast_cursor, FALSE, FALSE);
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
 * Go through a function end block and determine/insert the ret statements that we need
 */
static void determine_and_insert_return_statements(basic_block_t* function_entry_block, basic_block_t* function_exit_block){
	//For convenience
	symtab_function_record_t* function_defined_in = function_exit_block->function_defined_in;

	//Run through all of the predecessors
	for(u_int16_t i = 0; i < function_exit_block->predecessors->current_index; i++){
		//Grab the predecessor out
		basic_block_t* block = dynamic_array_get_at(function_exit_block->predecessors, i);

		//No point in looking at this if it's null and not the function entry block
		if(block->exit_statement == NULL && block != function_entry_block){
			continue;
		}

		//If the exit statement is not a return statement, we need to know what's happening here
		if(block->exit_statement == NULL || block->exit_statement->CLASS != THREE_ADDR_CODE_RET_STMT){
			//If this isn't void, then we need to throw a warning
			if(function_defined_in->return_type->type_class != TYPE_CLASS_BASIC
				|| function_defined_in->return_type->basic_type->basic_type != VOID){
				print_parse_message(WARNING, "Non-void function does not return in all control paths", 0);
			}
			
			//We'll now manually insert the ret statement here
			instruction_t* instruction = emit_ret_instruction(NULL);
			
			//We'll now add this at the very end of the block
			add_statement(block, instruction);
		}
	}
}


/**
 * A function definition will always be considered a leader statement. As such, it
 * will always have it's own separate block
 */
static basic_block_t* visit_function_definition(generic_ast_node_t* function_node){
	//Grab the function record
	symtab_function_record_t* func_record = function_node->func_record;
	//We will now store this as the current function
	current_function = func_record;
	//We also need to zero out the current stack offset value
	stack_offset = 0;

	//Reset the three address code accordingly
	set_new_function(func_record);

	//The starting block
	basic_block_t* function_starting_block = basic_block_alloc(1);
	//The function exit block
	function_exit_block = basic_block_alloc(1);
	//Mark that this is a starting block
	function_starting_block->block_type = BLOCK_TYPE_FUNC_ENTRY;
	//Mark that this is an exit block
	function_exit_block->block_type = BLOCK_TYPE_FUNC_EXIT;
	//Store this in the entry block
	function_starting_block->function_defined_in = func_record;

	//We don't care about anything until we reach the compound statement
	generic_ast_node_t* func_cursor = function_node->first_child;

	//It could be null, though it usually is not
	if(func_cursor != NULL){
		//Package the values up
		values_package_t compound_stmt_values = pack_values(func_cursor, //Initial Node
														NULL, //Loop statement start
														NULL, //Exit block of loop
														NULL); //For loop update block

		//Once we get here, we know that func cursor is the compound statement that we want
		basic_block_t* compound_stmt_block = visit_compound_statement(&compound_stmt_values);

		//Once we're done with the compound statement, we will merge it into the function
		merge_blocks(function_starting_block, compound_stmt_block);

		//Let's see if we actually made it all the way through and found a return
		basic_block_t* compound_stmt_cursor = function_starting_block;

		//Until we hit the end
		while(compound_stmt_cursor->direct_successor != NULL){
			compound_stmt_cursor = compound_stmt_cursor->direct_successor;
		}

		//We will mark that this end here has a direct successor in the function exit block
		add_successor(compound_stmt_cursor, function_exit_block);
		//Ensure that it's the direct successor
		compound_stmt_cursor->direct_successor = function_exit_block;
	
	//Otherwise we'll just connect the exit and entry
	} else {
		add_successor(function_starting_block, function_exit_block);
		function_starting_block->direct_successor = function_exit_block;
	}

	//Determine and insert any needed ret statements
	determine_and_insert_return_statements(function_starting_block, function_exit_block);

	//Now that we're done, we will clear this current function parameter
	current_function = NULL;

	//Mark this as NULL for the next go around
	function_exit_block = NULL;

	//We always return the start block
	return function_starting_block;
}


/**
 * Visit a declaration statement
 */
static basic_block_t* visit_declaration_statement(values_package_t* values, variable_scope_type_t scope){
	//What block are we emitting into?
	basic_block_t* emitted_block = basic_block_alloc(1);

	//Emit the expression code
	emit_expr_code(emitted_block, values->initial_node, FALSE, FALSE);

	//Give the block back
	return emitted_block;
}


/**
 * Visit a let statement
 */
static basic_block_t* visit_let_statement(values_package_t* values, variable_scope_type_t scope, u_int8_t is_branch_ending){
	//What block are we emitting to?
	basic_block_t* emittance_block = basic_block_alloc(1);

	//Add the expresssion into the node
	emit_expr_code(emittance_block, values->initial_node, is_branch_ending, FALSE);

	//Give the block back
	return emittance_block;
}


/**
 * Visit the prog node for our CFG. This rule will simply multiplex to all other rules
 * between functions, let statements and declaration statements
 */
static u_int8_t visit_prog_node(cfg_t* cfg, generic_ast_node_t* prog_node){
	//A prog node can decay into a function definition, a let statement or otherwise
	generic_ast_node_t* ast_cursor = prog_node->first_child;
	//Generic block holder
	basic_block_t* block;
	//The values that we have to pack
	values_package_t values;

	//So long as the AST cursor is not null
	while(ast_cursor != NULL){
		//Switch based on the class of cursor that we have here
		switch(ast_cursor->CLASS){
			//We can see a function definition. In this case, we'll
			//allow the helper to do it
			case AST_NODE_CLASS_FUNC_DEF:
				//Visit the function definition
				block = visit_function_definition(ast_cursor);
			
				//If this failed, we're out
				if(block->block_id == -1){
					return FALSE;
				}

				//Otherwise we'll add him to the functions dynamic array
				dynamic_array_add(cfg->function_blocks, block);

				//All good to move along
				break;

			//We can also see a let statement
			case AST_NODE_CLASS_LET_STMT:
				//Package the values up
				values = pack_values(ast_cursor, //Initial Node
													NULL, //Loop statement start
													NULL, //Exit block of loop
													NULL); //For loop update block

				//We'll visit the block here
				basic_block_t* let_block = visit_let_statement(&values, VARIABLE_SCOPE_GLOBAL, FALSE);
				
				//And we'll move along here
				break;
		
			//Finally, we could see a declaration
			case AST_NODE_CLASS_DECL_STMT:
				//Package the values up
				values = pack_values(ast_cursor, //Initial Node
													NULL, //Loop statement start
													NULL, //Exit block of loop
													NULL); //For loop update block

				//We'll visit the block here
				basic_block_t* decl_block = visit_declaration_statement(&values, VARIABLE_SCOPE_GLOBAL);
				
				//And we're done here
				break;

			//Some very weird error if we hit here
			default:
				print_parse_message(PARSE_ERROR, "Unrecognizable node found as child to prog node", ast_cursor->line_number);
				(*num_errors_ref)++;
				//Return this because we failed
				return FALSE;
		}


		//We now advance to the next sibling
		ast_cursor = ast_cursor->next_sibling;
	}

	//Return true because it worked
	return TRUE;
}


/**
 * For DEBUGGING purposes - we will print all of the blocks in the control
 * flow graph. This is meant to be invoked by the programmer, and as such is exposed
 * via the header file
 */
void print_all_cfg_blocks(cfg_t* cfg){
	//We will emit the DF
	emit_blocks_bfs(cfg, EMIT_DOMINANCE_FRONTIER);
}


/**
 * Reset the visited status of the CFG
 */
void reset_visited_status(cfg_t* cfg, u_int8_t reset_direct_successor){
	//For each block in the CFG
	for(u_int16_t _ = 0; _ < cfg->created_blocks->current_index; _++){
		//Grab the block out
		basic_block_t* block = dynamic_array_get_at(cfg->created_blocks, _);

		//Set it's visited status to 0
		block->visited = FALSE;

		//If we want to reset this, we'll null it out
		if(reset_direct_successor == TRUE){
			block->direct_successor = NULL;
		}
	}
}


/**
 * We will calculate:
 *  1.) Dominator Sets
 *  2.) Dominator Trees
 *  3.) Dominance Frontiers
 *  4.) Postdominator sets
 *  5.) Reverse Dominance frontiers
 *  6.) Reverse post order traversals
 *
 * For every block in the CFG
 */
void calculate_all_control_relations(cfg_t* cfg, u_int8_t build_fresh, u_int8_t recalculate_rpo){
	//We first need to calculate the dominator sets of every single node
	calculate_dominator_sets(cfg);
	
	//Now we'll build the dominator tree up
	build_dominator_trees(cfg, build_fresh);

	//We need to calculate the dominance frontier of every single block before
	//we go any further
	calculate_dominance_frontiers(cfg);

	//Calculate the postdominator sets for later analysis in the optimizer
	calculate_postdominator_sets(cfg);

	//We'll also now calculate the reverse dominance frontier that will be used
	//in later analysis by the optimizer
	calculate_reverse_dominance_frontiers(cfg);

	if(recalculate_rpo == TRUE){
		//Clear all of these old values out
		reset_reverse_post_order_sets(cfg);

		//For each function entry block, recompute all reverse post order
		//CFG work
		for(u_int16_t i = 0; i < cfg->function_blocks->current_index; i++){
			//Grab the block out
			basic_block_t* block = dynamic_array_get_at(cfg->function_blocks, i);

			//Recompute the reverse post order cfg
			block->reverse_post_order_reverse_cfg = compute_reverse_post_order_traversal(block, TRUE);

			for(u_int16_t a = 0; a < block->reverse_post_order_reverse_cfg->current_index; a++){
				basic_block_t* internal_block = dynamic_array_get_at(block->reverse_post_order_reverse_cfg, a);
				printf(".L%d\n", internal_block->block_id);
			}
		}
	}
}


/**
 * Build a cfg from the ground up
*/
cfg_t* build_cfg(front_end_results_package_t* results, u_int32_t* num_errors, u_int32_t* num_warnings){
	//Store our references here
	num_errors_ref = num_errors;
	num_warnings_ref = num_warnings;

	//Add this in
	type_symtab = results->type_symtab;

	//Keep this on hand
	u64 = lookup_type_name_only(type_symtab, "u64")->type;

	//We'll first create the fresh CFG here
	cfg_t* cfg = calloc(1, sizeof(cfg_t));

	//Store this along with it
	cfg->type_symtab = type_symtab;

	//Create the dynamic arrays that we need
	cfg->created_blocks = dynamic_array_alloc();
	cfg->function_blocks = dynamic_array_alloc();

	//Hold the cfg
	cfg_ref = cfg;

	//Set this to NULL initially
	current_function = NULL;

	//Create the stack pointer
	stack_pointer = initialize_stack_pointer(results->variable_symtab, results->type_symtab);
	//Initialize the variable to
	stack_pointer_var = emit_var(stack_pointer, FALSE);
	//Mark it
	stack_pointer_var->is_stack_pointer = TRUE;

	//Store the stack pointer
	cfg->stack_pointer = stack_pointer_var;

	// -1 block ID, this means that the whole thing failed
	if(visit_prog_node(cfg, results->root) == FALSE){
		print_parse_message(PARSE_ERROR, "CFG was unable to be constructed", 0);
		(*num_errors_ref)++;
	}

	//Let the helper deal with this
	calculate_all_control_relations(cfg, FALSE, FALSE);

	//now we calculate the liveness sets
	calculate_liveness_sets(cfg);

	//Add all phi functions for SSA
	insert_phi_functions(cfg, results->variable_symtab);

	//Rename all variables after we're done with the phi functions
	rename_all_variables(cfg);

	//Give back the reference
	return cfg;
}
