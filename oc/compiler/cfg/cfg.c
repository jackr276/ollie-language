/**
 * Author: Jack Robbins
 * 
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
#include "../utils/queue/heap_queue.h"
#include "../jump_table/jump_table.h"
#include "../utils/stack/nesting_stack.h"
#include "../utils/constants.h"

//Our atomically incrementing integer
//If at any point a block has an ID of (-1), that means that it is in error and can be dealt with as such
static int32_t current_block_id = 0;
//Keep global references to the number of errors and warnings
u_int32_t* num_errors_ref;
u_int32_t* num_warnings_ref;
//Keep the type symtab up and running
type_symtab_t* type_symtab;
//The CFG that we're working with
static cfg_t* cfg = NULL;
//Keep a reference to whatever function we are currently in
static symtab_function_record_t* current_function;
//The current function exit block. Unlike loops, these can't be nested, so this is totally fine
static basic_block_t* function_exit_block = NULL;
//Keep a varaible/record for the instruction pointer(rip)
static three_addr_var_t* instruction_pointer_var = NULL;
//Keep a record for the variable symtab
static variable_symtab_t* variable_symtab;
//Store for use
static generic_type_t* char_type = NULL;
static generic_type_t* u8 = NULL;
static generic_type_t* i8 = NULL;
static generic_type_t* u16 = NULL;
static generic_type_t* i16 = NULL;
static generic_type_t* i32 = NULL;
static generic_type_t* u32 = NULL;
static generic_type_t* u64 = NULL;
static generic_type_t* i64 = NULL;
//The break and continue stack will
//hold values that we can break & continue
//to. This is done here to avoid the need
//to send value packages at each rule
static heap_stack_t break_stack;
static heap_stack_t continue_stack;
//The overall nesting stack will tell us what level of nesting we're at(if, switch/case, loop)
static nesting_stack_t nesting_stack;
//Keep a list of all lable statements in the function(block jumps are internal only)
static dynamic_array_t current_function_labeled_blocks;
//Also keep a list of all custom jumps in the function
static dynamic_array_t current_function_user_defined_jump_statements;
//The current stack offset for any given function
static u_int64_t stack_offset = 0;
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
	ollie_token_t operator;
} cfg_result_package_t;

/**
 * Determine whether or not a given three address variable is eligible for SSA
 */
#define IS_SSA_VARIABLE_TYPE(variable) ((variable->variable_type == VARIABLE_TYPE_NON_TEMP || variable->variable_type == VARIABLE_TYPE_MEMORY_ADDRESS) ? TRUE : FALSE)

//Are we emitting the dominance frontier or not?
typedef enum{
	EMIT_DOMINANCE_FRONTIER,
	DO_NOT_EMIT_DOMINANCE_FRONTIER
} emit_dominance_frontier_selection_t;


//An enum for declare and let statements letting us know what kind of variable
//that we have
typedef enum{
	VARIABLE_SCOPE_GLOBAL,
	VARIABLE_SCOPE_LOCAL,
} variable_scope_type_t;


//We predeclare up here to avoid needing any rearrangements
static void visit_declaration_statement(generic_ast_node_t* node);
static cfg_result_package_t visit_compound_statement(generic_ast_node_t* root_node);
static cfg_result_package_t visit_let_statement(generic_ast_node_t* node, u_int8_t is_branch_ending);
static cfg_result_package_t visit_if_statement(generic_ast_node_t* root_node);
static cfg_result_package_t visit_while_statement(generic_ast_node_t* root_node);
static cfg_result_package_t visit_do_while_statement(generic_ast_node_t* root_node);
static cfg_result_package_t visit_for_statement(generic_ast_node_t* root_node);
static cfg_result_package_t visit_case_statement(generic_ast_node_t* root_node);
static cfg_result_package_t visit_default_statement(generic_ast_node_t* root_node);
static cfg_result_package_t visit_switch_statement(generic_ast_node_t* root_node);
static cfg_result_package_t visit_statement_chain(generic_ast_node_t* first_node);

static cfg_result_package_t emit_binary_expression(basic_block_t* basic_block, generic_ast_node_t* logical_or_expr, u_int8_t is_branch_ending);
static cfg_result_package_t emit_ternary_expression(basic_block_t* basic_block, generic_ast_node_t* ternary_operation, u_int8_t is_branch_ending);
static three_addr_var_t* emit_binary_operation_with_constant(basic_block_t* basic_block, three_addr_var_t* assignee, three_addr_var_t* op1, ollie_token_t op, three_addr_const_t* constant, u_int8_t is_branch_ending);
static cfg_result_package_t emit_function_call(basic_block_t* basic_block, generic_ast_node_t* function_call_node, u_int8_t is_branch_ending);
static cfg_result_package_t emit_indirect_function_call(basic_block_t* basic_block, generic_ast_node_t* indirect_function_call_node, u_int8_t is_branch_ending);
static cfg_result_package_t emit_unary_expression(basic_block_t* basic_block, generic_ast_node_t* unary_expression, u_int8_t is_branch_ending);
static cfg_result_package_t emit_expression(basic_block_t* basic_block, generic_ast_node_t* expr_node, u_int8_t is_branch_ending, u_int8_t is_condition);
static cfg_result_package_t emit_string_initializer(basic_block_t* current_block, three_addr_var_t* base_address, u_int32_t offset, generic_ast_node_t* string_initializer, u_int8_t is_branch_ending);
static cfg_result_package_t emit_struct_initializer(basic_block_t* current_block, three_addr_var_t* base_address, u_int32_t offset, generic_ast_node_t* struct_initializer, u_int8_t is_branch_ending);

/**
 * Lea statements may only have: 1, 2, 4, or 8 as their scales
 * due to internal hardware constraints. This operation will find
 * if a given value is compatible
 */
static u_int8_t is_lea_compatible_power_of_2(int64_t value){
	switch(value){
		case 1:
		case 2:
		case 4:
		case 8:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Reset the used, live in, live out, and assigned arrays in a block
 */
void reset_block_variable_tracking(basic_block_t* block){
	//Let's first wipe everything regarding this block's used and assigned variables. If they don't exist,
	//we'll allocate them fresh
	if(block->assigned_variables.internal_array == NULL){
		block->assigned_variables = dynamic_array_alloc();
	} else {
		reset_dynamic_array(&(block->assigned_variables));
	}

	//Do the same with the used variables
	if(block->used_variables.internal_array == NULL){
		block->used_variables = dynamic_array_alloc();
	} else {
		reset_dynamic_array(&(block->used_variables));
	}

	//Reset live in completely
	if(block->live_in.internal_array != NULL){
		dynamic_array_dealloc(&(block->live_in));
	}

	//Reset live out completely
	if(block->live_out.internal_array != NULL){
		dynamic_array_dealloc(&(block->live_out));
	}
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
 * Create a basic block explicitly using the estimate
 * that comes from the nesting stack that we maintain.
 * 
 * This automates the process of estimating execution
 * frequencies, so long as we are keeping the nesting stack
 * up-to-date
 */
static basic_block_t* basic_block_alloc_and_estimate(){
	//Allocate the block
	basic_block_t* created = calloc(1, sizeof(basic_block_t));

	//Put the block ID in
	created->block_id = increment_and_get();

	//By default we're normal here
	created->block_type = BLOCK_TYPE_NORMAL;

	//What is the estimated execution cost of this block? We will
	//rely entirely on the nesting stack to do this for us
	created->estimated_execution_frequency = get_estimated_execution_frequency_from_nesting_stack(&nesting_stack);

	//Let's add in what function this block came from
	created->function_defined_in = current_function;

	//Add this into the dynamic array
	dynamic_array_add(&(cfg->created_blocks), created);

	//Give it back
	return created;
}


/**
 * The standard basic_block_alloc function will take in the estimated
 * execution frequency. If you are in the CFG, *you should not be
 * using this function*
 */
basic_block_t* basic_block_alloc(u_int32_t estimated_execution_frequency){
	//Allocate the block
	basic_block_t* created = calloc(1, sizeof(basic_block_t));

	//Put the block ID in
	created->block_id = increment_and_get();

	//By default we're normal here
	created->block_type = BLOCK_TYPE_NORMAL;

	//What is the estimated execution cost of this block?
	created->estimated_execution_frequency = estimated_execution_frequency;

	//Let's add in what function this block came from
	created->function_defined_in = current_function;

	//Add this into the dynamic array
	dynamic_array_add(&(cfg->created_blocks), created);

	return created;
}


/**
 * Allocate a basic block that comes from a user-defined label statement
*/
static basic_block_t* labeled_block_alloc(symtab_variable_record_t* label){
	//Allocate the block
	basic_block_t* created = calloc(1, sizeof(basic_block_t));

	//Put the block ID in even though it is a labeled block
	created->block_id = increment_and_get();

	//This block's name will draw from the label
	created->label = label;

	//Put the block ID in
	created->block_id = increment_and_get();

	//We'll mark this to indicate that this is a labeled block
	created->block_type = BLOCK_TYPE_LABEL;

	//What is the estimated execution cost of this block? Rely on the nesting stack
	//to do this
	created->estimated_execution_frequency = get_estimated_execution_frequency_from_nesting_stack(&nesting_stack);

	//Let's add in what function this block came from
	created->function_defined_in = current_function;

	//Add this into the dynamic array
	dynamic_array_add(&(cfg->created_blocks), created);

	return created;
}


/**
 * For certain variables in conditionals, we want to emit a temp assignment of said variable for optimization
 * reasons. This function will take a variable in and:
 *   If it's a temp, just give it back
 *   If it's not a temp, emit a temp assignment, add that to the block, then give the temp var back
 */
static three_addr_var_t* handle_conditional_identifier_copy_if_needed(basic_block_t* block, three_addr_var_t* variable, u_int8_t is_branch_ending){
	//Nothing more to see here
	if(variable->variable_type == VARIABLE_TYPE_TEMP){
		return variable;
	}

	//Otherwise we must copy
	instruction_t* copy = emit_assignment_instruction(emit_temp_var(variable->type), variable);
	copy->is_branch_ending = is_branch_ending;

	//Add it into the block
	add_statement(block, copy);

	//Add it in as used
	add_used_variable(block, variable);

	//Give back the copy var
	return copy->assignee;
}


/**
 * We'll go through in the regular traversal, pushing each node onto the stack in
 * postorder. 
 */
static void reverse_post_order_traversal_reverse_cfg_rec(heap_stack_t* stack, basic_block_t* entry){
	//If we've already seen this then we're done
	if(entry->visited == TRUE){
		return;
	}

	//Mark it as visited
	entry->visited = TRUE;

	//For every child(predecessor-it's reverse), we visit it as well
	for(u_int16_t _ = 0; _ < entry->predecessors.current_index; _++){
		//Visit each of the blocks
		reverse_post_order_traversal_reverse_cfg_rec(stack, dynamic_array_get_at(&(entry->predecessors), _));
	}

	//Now we can push entry onto the stack
	push(stack, entry);
}


/**
 * Get and return a reverse post order traversal of a function-level CFG where
 * we are going in reverse order. This is used mainly for data flow(liveness)
 */
dynamic_array_t compute_reverse_post_order_traversal_reverse_cfg(basic_block_t* entry){
	//For our postorder traversal
	heap_stack_t stack = heap_stack_alloc();
	//We'll need this eventually for postorder
	dynamic_array_t reverse_post_order_traversal = dynamic_array_alloc();

	//Go all the way to the bottom
	while(entry->block_type != BLOCK_TYPE_FUNC_EXIT){
		entry = entry->direct_successor;
	}

	//Invoke the recursive helper
	reverse_post_order_traversal_reverse_cfg_rec(&stack, entry);

	//Now we'll pop everything off of the stack, and put it onto the RPO 
	//array in backwards order
	while(heap_stack_is_empty(&stack) == FALSE){
		dynamic_array_add(&reverse_post_order_traversal, pop(&stack));
	}

	//And when we're done, get rid of the stack
	heap_stack_dealloc(&stack);

	//Give back the reverse post order traversal
	return reverse_post_order_traversal;
}


/**
 * We'll go through in the regular traversal, pushing each node onto the stack in
 * postorder. 
 */
static void reverse_post_order_traversal_rec(heap_stack_t* stack, basic_block_t* entry){
	//If we've already seen this then we're done
	if(entry->visited == TRUE){
		return;
	}

	//Mark it as visited
	entry->visited = TRUE;

	//For every child(successor), we visit it as well
	for(u_int16_t _ = 0; _ < entry->successors.current_index; _++){
		//Visit each of the blocks
		reverse_post_order_traversal_rec(stack, dynamic_array_get_at(&(entry->successors), _));
	}

	//Now we can push entry onto the stack
	push(stack, entry);
}


/**
 * Get and return a reverse-post order traversal for a function level CFG
 */
dynamic_array_t compute_reverse_post_order_traversal(basic_block_t* entry){
	//For our postorder traversal
	heap_stack_t stack = heap_stack_alloc();
	//We'll need this eventually for postorder
	dynamic_array_t reverse_post_order_traversal = dynamic_array_alloc();

	//Invoke the recursive helper
	reverse_post_order_traversal_rec(&stack, entry);

	//Now we'll pop everything off of the stack, and put it onto the RPO 
	//array in backwards order
	while(heap_stack_is_empty(&stack) == FALSE){
		dynamic_array_add(&reverse_post_order_traversal, pop(&stack));
	}

	//And when we're done, get rid of the stack
	heap_stack_dealloc(&stack);

	//Give back the reverse post order traversal
	return reverse_post_order_traversal;
}


/**
 * Reset all reverse post order sets
 */
void reset_reverse_post_order_sets(cfg_t* cfg){
	//Run through all of the function blocks
	for(u_int16_t _ = 0; _ < cfg->function_entry_blocks.current_index; _++){
		//Grab the block out
		basic_block_t* function_entry_block = dynamic_array_get_at(&(cfg->function_entry_blocks), _);

		//Set the RPO to be null
		if(function_entry_block->reverse_post_order.internal_array != NULL){
			dynamic_array_dealloc(&(function_entry_block->reverse_post_order));
		}

		//Set the RPO reverse CFG to be null
		if(function_entry_block->reverse_post_order_reverse_cfg.internal_array != NULL){
			dynamic_array_dealloc(&(function_entry_block->reverse_post_order_reverse_cfg));
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

	//Run through every successor
	for(u_int16_t _ = 0; _ < entry->successors.current_index; _++){
		//Recursive call to every child first
		post_order_traversal_rec(post_order_traversal, dynamic_array_get_at(&(entry->successors), _));
	}
	
	//Now we'll finally visit the node
	dynamic_array_add(post_order_traversal, entry);

	//And we're done
}


/**
 * Get and return the regular postorder traversal for a function-level CFG
 */
dynamic_array_t compute_post_order_traversal(basic_block_t* entry){
	//Reset the visited status
	reset_visited_status(cfg, FALSE);

	//Create our dynamic array
	dynamic_array_t post_order_traversal = dynamic_array_alloc();

	//Make the recursive call
	post_order_traversal_rec(&post_order_traversal, entry);

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
void add_used_variable(basic_block_t* basic_block, three_addr_var_t* var){
	//This can happen, so we'll check here to avoid complexity elsewhere
	if(var == NULL){
		return;
	}

	//Increment the use count of this variable, regardless of what it is
	var->use_count++;

	//If this is a temporary var, then we're done here, we'll simply bail out
	if(var->variable_type == VARIABLE_TYPE_TEMP){
		return;
	}

	//If this is NULL, we'll need to allocate it
	if(basic_block->used_variables.internal_array == NULL){
		basic_block->used_variables = dynamic_array_alloc();
	}

	//We need a special kind of comparison here, so we can't use the canned method
	for(u_int16_t i = 0; i < basic_block->used_variables.current_index; i++){
		//If the linked variables are the same, we're out
		if(((three_addr_var_t*)(basic_block->used_variables.internal_array[i]))->linked_var == var->linked_var){
			return;
		}
	}
	
	//we didn't find it, so we will add
	dynamic_array_add(&(basic_block->used_variables), var); 
}


/**
 * A simple helper function that allows us to add an assigned-to variable into the block's
 * header. It is important to note that only actual variables(not temp variables) count
 * as live
 */
void add_assigned_variable(basic_block_t* basic_block, three_addr_var_t* var){
	//If it's temp just don't add it
	if(var->variable_type == VARIABLE_TYPE_TEMP){
		return;
	}

	//If the assigned variable dynamic array is NULL, we'll allocate it here
	if(basic_block->assigned_variables.internal_array == NULL){
		basic_block->assigned_variables = dynamic_array_alloc();
	}

	//We need a special kind of comparison here, so we can't use the canned method
	for(u_int16_t i = 0; i < basic_block->assigned_variables.current_index; i++){
		//If the linked variables are the same, we're out
		if(((three_addr_var_t*)(basic_block->assigned_variables.internal_array[i]))->linked_var == var->linked_var){
			return;
		}
	}

	//We didn't find it, so we'll add it
	dynamic_array_add(&(basic_block->assigned_variables), var);
}


/**
 * Print a block our for reading
*/
static void print_block_three_addr_code(basic_block_t* block, emit_dominance_frontier_selection_t print_df){
	//If this is some kind of switch block, we first print the jump table
	if(block->jump_table != NULL){
		print_jump_table(stdout, block->jump_table);
	}

	//Different blocks have different printing rules
	switch(block->block_type){
		case BLOCK_TYPE_FUNC_ENTRY:
			//Print out any/all local constants
			print_local_constants(stdout, block->function_defined_in);

			//Now the block name
			printf("%s", block->function_defined_in->func_name.string);
			break;

		//By default it's just the .L printing
		default:
			printf(".L%d", block->block_id);
	
	}


	//Now, we will print all of the active variables that this block has
	if(block->used_variables.internal_array != NULL){
		printf("(");

		//Run through all of the live variables and print them out
		for(u_int16_t i = 0; i < block->used_variables.current_index; i++){
			//Print it out
			print_variable(stdout, block->used_variables.internal_array[i], PRINTING_VAR_BLOCK_HEADER);

			//If it isn't the very last one, we need a comma
			if(i != block->used_variables.current_index - 1){
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

	//If we have predecessors
	if(block->predecessors.internal_array != NULL){
		for(u_int16_t i = 0; i < block->predecessors.current_index; i++){
			basic_block_t* predecessor = block->predecessors.internal_array[i];

			//Print the block's ID or the function name
			if(predecessor->block_type == BLOCK_TYPE_FUNC_ENTRY){
				printf("%s", predecessor->function_defined_in->func_name.string);
			} else {
				printf(".L%d", predecessor->block_id);
			}

			if(i != block->predecessors.current_index - 1){
				printf(", ");
			}
		}
	}

	printf("}\n");

	printf("Successors: {");
	//If we have successor
	if(block->successors.internal_array != NULL){
		for(u_int16_t i = 0; i < block->successors.current_index; i++){
			basic_block_t* successor = block->successors.internal_array[i];

			//Print the block's ID or the function name
			if(successor->block_type == BLOCK_TYPE_FUNC_ENTRY){
				printf("%s", successor->function_defined_in->func_name.string);
			} else {
				printf(".L%d", successor->block_id);
			}

			if(i != block->successors.current_index - 1){
				printf(", ");
			}
		}
	}

	printf("}\n");

	//If we have some assigned variables, we will dislay those for debugging
	if(block->assigned_variables.internal_array != NULL){
		printf("Assigned: (");

		for(u_int16_t i = 0; i < block->assigned_variables.current_index; i++){
			print_variable(stdout, block->assigned_variables.internal_array[i], PRINTING_VAR_BLOCK_HEADER);

			//If it isn't the very last one, we need a comma
			if(i != block->assigned_variables.current_index - 1){
				printf(", ");
			}
		}
		printf(")\n");
	}

	//Now if we have LIVE_IN variables, we'll print those out
	if(block->live_in.internal_array != NULL){
		printf("LIVE_IN: (");

		for(u_int16_t i = 0; i < block->live_in.current_index; i++){
			print_variable(stdout, block->live_in.internal_array[i], PRINTING_VAR_BLOCK_HEADER);

			//If it isn't the very last one, print out a comma
			if(i != block->live_in.current_index - 1){
				printf(", ");
			}
		}

		//Close it out
		printf(")\n");
	}

	//Now if we have LIVE_IN variables, we'll print those out
	if(block->live_out.internal_array != NULL){
		printf("LIVE_OUT: (");

		for(u_int16_t i = 0; i < block->live_out.current_index; i++){
			print_variable(stdout, block->live_out.internal_array[i], PRINTING_VAR_BLOCK_HEADER);

			//If it isn't the very last one, print out a comma
			if(i != block->live_out.current_index - 1){
				printf(", ");
			}
		}

		//Close it out
		printf(")\n");
	}


	//Print out the dominance frontier if we're in DEBUG mode
	if(print_df == EMIT_DOMINANCE_FRONTIER && block->dominance_frontier.internal_array != NULL){
		printf("Dominance frontier: {");

		//Run through and print them all out
		for(u_int16_t i = 0; i < block->dominance_frontier.current_index; i++){
			basic_block_t* printing_block = block->dominance_frontier.internal_array[i];

			//Print the block's ID or the function name
			if(printing_block->block_type == BLOCK_TYPE_FUNC_ENTRY){
				printf("%s", printing_block->function_defined_in->func_name.string);
			} else {
				printf(".L%d", printing_block->block_id);
			}

			//If it isn't the very last one, we need a comma
			if(i != block->dominance_frontier.current_index - 1){
				printf(", ");
			}
		}

		//And close it out
		printf("}\n");
	}

	//Print out the reverse dominance frontier if we're in DEBUG mode
	if(print_df == EMIT_DOMINANCE_FRONTIER && block->reverse_dominance_frontier.internal_array != NULL){
		printf("Reverse Dominance frontier: {");

		//Run through and print them all out
		for(u_int16_t i = 0; i < block->reverse_dominance_frontier.current_index; i++){
			basic_block_t* printing_block = block->reverse_dominance_frontier.internal_array[i];

			//Print the block's ID or the function name
			if(printing_block->block_type == BLOCK_TYPE_FUNC_ENTRY){
				printf("%s", printing_block->function_defined_in->func_name.string);
			} else {
				printf(".L%d", printing_block->block_id);
			}

			//If it isn't the very last one, we need a comma
			if(i != block->reverse_dominance_frontier.current_index - 1){
				printf(", ");
			}
		}

		//And close it out
		printf("}\n");
	}


	//Only if this is false - global var blocks don't have any of these
	printf("Dominator set: {");
	if(block->dominator_set.internal_array != NULL){
		//Run through and print them all out
		for(u_int16_t i = 0; i < block->dominator_set.current_index; i++){
			basic_block_t* printing_block = block->dominator_set.internal_array[i];

			//Print the block's ID or the function name
			if(printing_block->block_type == BLOCK_TYPE_FUNC_ENTRY){
				printf("%s", printing_block->function_defined_in->func_name.string);
			} else {
				printf(".L%d", printing_block->block_id);
			}
			//If it isn't the very last one, we need a comma
			if(i != block->dominator_set.current_index - 1){
				printf(", ");
			}
		}

	}
	//And close it out
	printf("}\n");

	printf("Postdominator(reverse dominator) Set: {");
	if(block->postdominator_set.internal_array != NULL){
		//Run through and print them all out
		for(u_int16_t i = 0; i < block->postdominator_set.current_index; i++){
			basic_block_t* postdominator = block->postdominator_set.internal_array[i];

			//Print the block's ID or the function name
			if(postdominator->block_type == BLOCK_TYPE_FUNC_ENTRY){
				printf("%s", postdominator->function_defined_in->func_name.string);
			} else {
				printf(".L%d", postdominator->block_id);
			}
			//If it isn't the very last one, we need a comma
			if(i != block->postdominator_set.current_index - 1){
				printf(", ");
			}
		}
	}
	//And close it out
	printf("}\n");

	//Now print out the dominator children
	printf("Dominator Children: {");
	//If we have dominator children
	if(block->dominator_children.internal_array != NULL){
		for(u_int16_t i = 0; i < block->dominator_children.current_index; i++){
			basic_block_t* printing_block = block->dominator_children.internal_array[i];

			//Print the block's ID or the function name
			if(printing_block->block_type == BLOCK_TYPE_FUNC_ENTRY){
				printf("%s", printing_block->function_defined_in->func_name.string);
			} else {
				printf(".L%d", printing_block->block_id);
			}
			//If it isn't the very last one, we need a comma
			if(i != block->dominator_children.current_index - 1){
				printf(", ");
			}
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

	//This needs to match up for later processing
	phi_statement->function = target->function_defined_in;

	//Special case -- we're adding the head
	if(target->leader_statement == NULL || target->exit_statement == NULL){
		//Assign this to be the head and the tail
		target->leader_statement = phi_statement;
		target->exit_statement = phi_statement;
		//Mark the block that we're in
		phi_statement->block_contained_in = target;
		return;
	}

	//Counts as an instruction
	target->number_of_instructions++;

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
	if(phi_statement->parameters.internal_array == NULL){
		//Take care of allocation then
		phi_statement->parameters = dynamic_array_alloc();
	}

	//Add this to the phi statement parameters
	dynamic_array_add(&(phi_statement->parameters), var);
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

	//No matter what - we are adding a statement to this block
	target->number_of_instructions++;

	//Store the function as well
	statement_node->function = target->function_defined_in;

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

	//If we have a string constant and we're doing this, we'll need to decrement the reference
	//count by 1 because we are losing a reference to it
	if(stmt->op2 != NULL && stmt->op2->variable_type == VARIABLE_TYPE_LOCAL_CONSTANT){
		//Knock one off of the reference count
		stmt->op2->associated_memory_region.local_constant->reference_count--;
	}

	//No matter what, we are reducing the number of statements in this block
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

	//Now we need to do all maintenance when it comes to used variables for these statements. All variables
	//in here that were used now have one less "use" instance, and we'll need to update accordingly
	if(stmt->op1 != NULL){
		stmt->op1->use_count--;
	}

	//One less use count here as well
	if(stmt->op2 != NULL){
		stmt->op2->use_count--;
	}
}


/**
 * Add a block to the dominance frontier of the first block
 */
static void add_block_to_dominance_frontier(basic_block_t* block, basic_block_t* df_block){
	//If the dominance frontier hasn't been allocated yet, we'll do that here
	if(block->dominance_frontier.internal_array == NULL){
		block->dominance_frontier = dynamic_array_alloc();
	}

	//Let's just check - is this already in there. If it is, we will not add it
	for(u_int16_t i = 0; i < block->dominance_frontier.current_index; i++){
		//This is not a problem at all, we just won't add it
		if(block->dominance_frontier.internal_array[i] == df_block){
			return;
		}
	}

	//Add this into the dominance frontier
	dynamic_array_add(&(block->dominance_frontier), df_block);
}


/**
 * Add a block to the reverse dominance frontier of the first block
 */
static void add_block_to_reverse_dominance_frontier(basic_block_t* block, basic_block_t* rdf_block){
	//If the dominance frontier hasn't been allocated yet, we'll do that here
	if(block->reverse_dominance_frontier.internal_array == NULL){
		block->reverse_dominance_frontier = dynamic_array_alloc();
	}

	//Let's just check - is this already in there. If it is, we will not add it
	for(u_int16_t i = 0; i < block->reverse_dominance_frontier.current_index; i++){
		//This is not a problem at all, we just won't add it
		if(block->reverse_dominance_frontier.internal_array[i] == rdf_block){
			return;
		}
	}

	//Add this into the dominance frontier
	dynamic_array_add(&(block->reverse_dominance_frontier), rdf_block);
}


/**
 * Does the block assign this variable? We'll do a simple linear scan to find out
 */
static u_int8_t does_block_assign_variable(basic_block_t* block, symtab_variable_record_t* variable){
	//Sanity check - if this is NULL then it's false by default
	if(block->assigned_variables.internal_array == NULL){
		return FALSE;
	}

	//We'll need to use a special comparison here because we're comparing variables, not plain addresses.
	//We will compare the linked var of each block in the assigned variables dynamic array
	for(u_int16_t i = 0; i < block->assigned_variables.current_index; i++){
		//Grab the variable out
		three_addr_var_t* var = dynamic_array_get_at(&(block->assigned_variables), i);
		
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
	//If B even has a dominator ser
	for(u_int16_t i = 0; i < B->dominator_set.current_index; i++){
		//By default we assume A is an IDOM
		A_is_IDOM = TRUE;

		//A is our "candidate" for possibly being an immediate dominator
		A = dynamic_array_get_at(&(B->dominator_set), i);

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
		for(u_int16_t j = 0; j < B->dominator_set.current_index; j++){
			//Skip this case
			if(i == j){
				continue;
			}

			//If it's aleady B or A, we're skipping
			C = dynamic_array_get_at(&(B->dominator_set), j);

			//If this is the case, disqualified
			if(C == B || C == A){
				continue;
			}

			//We can now see that C dominates B. The true test now is
			//if C is dominated by A. If that's the case, then we do NOT
			//have an immediate dominator in A.
			//
			//This would look like A -Doms> C -Doms> B, so A is not an immediate dominator
			if(dynamic_array_contains(&(C->dominator_set), A) != NOT_FOUND){
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
 * The immediate postdominator is the first breadth-first 
 * successor that post dominates a node
 */
static basic_block_t* immediate_postdominator(basic_block_t* B){
	//If we've already found the immediate dominator, why find it again?
	//We'll just give that back
	if(B->immediate_postdominator != NULL){
		return B->immediate_postdominator;
	}

	//Create the queue
	heap_queue_t queue = heap_queue_alloc();

	//The visited array
	dynamic_array_t visited = dynamic_array_alloc();

	//Save this for when we find it
	basic_block_t* ipdom = NULL;

	//Extract the postdominator set
	dynamic_array_t postdominator_set = B->postdominator_set;

	//Seed the search with B
	enqueue(&queue, B);

	//So long as the queue isn't empty
	while(queue_is_empty(&queue) == FALSE){
		//Pop off of the queue
		basic_block_t* current = dequeue(&queue);

		/**
		 * If we have found the first breadth-first successor that postdominates B,
		 * we are done
		 */
		if(current != B && dynamic_array_contains(&postdominator_set, current) != NOT_FOUND){
			ipdom = current;
			break;
		}

		//Add to the visited set
		dynamic_array_add(&visited, current);

		//Run through all successors
		for(u_int16_t j = 0; j < current->successors.current_index; j++){
			//Add the successor into the queue, if it has not yet been visited
			basic_block_t* successor = current->successors.internal_array[j];

			if(dynamic_array_contains(&visited, successor) == NOT_FOUND){
				enqueue(&queue, successor);
			}
		}
	}

	//Destroy visited
	dynamic_array_dealloc(&visited);

	//Destroy the queue
	heap_queue_dealloc(&queue);

	//Give it back
	return ipdom;
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
	for(u_int16_t i = 0; i < cfg->created_blocks.current_index; i++){
		//Grab this from the array
		basic_block_t* block = dynamic_array_get_at(&(cfg->created_blocks), i);

		//If we have less than 2 successors,the rest of 
		//the search here is useless
		if(block->predecessors.internal_array == NULL || block->predecessors.current_index < 2){
			//Hop out here, there is no need to analyze further
			continue;
		}

		//A cursor for traversing our predecessors
		basic_block_t* cursor;

		//Now we run through every predecessor of the block
		for(u_int8_t i = 0; i < block->predecessors.current_index; i++){
			//Grab it out
			cursor = block->predecessors.internal_array[i];

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
	for(u_int16_t i = 0; i < cfg->created_blocks.current_index; i++){
		//Grab this from the array
		basic_block_t* block = dynamic_array_get_at(&(cfg->created_blocks), i);

		//If we have less than 2 successors,the rest of 
		//the search here is useless
		if(block->successors.internal_array == NULL || block->successors.current_index < 2){
			//Hop out here, there is no need to analyze further
			continue;
		}

		//A cursor for traversing our successors 
		basic_block_t* cursor;

		//Now we run through every successor of the block
		for(u_int8_t i = 0; i < block->successors.current_index; i++){
			//Grab it out
			cursor = block->successors.internal_array[i];

			//While cursor is not the immediate postdominator of block
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
	if(dominator->dominator_children.internal_array == NULL){
		dominator->dominator_children = dynamic_array_alloc();
	}

	//If we do not already have this in the dominator children, then we will add it
	if(dynamic_array_contains(&(dominator->dominator_children), dominated) == NOT_FOUND){
		dynamic_array_add(&(dominator->dominator_children), dominated);
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
	for(u_int16_t i = 0; i < cfg->created_blocks.current_index; i++){
		//Grab the block out
		current = dynamic_array_get_at(&(cfg->created_blocks), i);

		//If it's an exit block, then it's postdominator set just has itself
		if(current->block_type == BLOCK_TYPE_FUNC_EXIT){
			//If it's an exit block, then this set just contains itself
			current->postdominator_set = dynamic_array_alloc();
			//Add the block to it's own set
			dynamic_array_add(&(current->postdominator_set), current);
		} else {
			//If it's not an exit block, then we set this to be the entire body of blocks
			current->postdominator_set = clone_dynamic_array(&(cfg->created_blocks));
		}
	}

	//Now that we've initialized, we'll perform the same while change algorithm as before
	//For each and every function
	for(u_int16_t i = 0; i < cfg->function_entry_blocks.current_index; i++){
		basic_block_t* current_function_block = dynamic_array_get_at(&(cfg->function_entry_blocks), i);

		//Have we seen a change
		u_int8_t changed;
		
		//Now we will go through everything in this blocks reverse post order set
		do {
			//By default, we'll assume there was no change
			changed = FALSE;

			//Now for each basic block in the reverse post order set
			for(u_int16_t _ = 0; _ < current_function_block->reverse_post_order.current_index; _++){
				//Grab the block out
				basic_block_t* current = dynamic_array_get_at(&(current_function_block->reverse_post_order), _);

				//If it's the exit block, we don't need to bother with it. The exit block is always postdominated
				//by itself
				if(current->block_type == BLOCK_TYPE_FUNC_EXIT){
					//Just go onto the next one
					continue;
				}

				//The temporary array that we will use as a holder for this iteration's postdominator set 
				dynamic_array_t temp = dynamic_array_alloc();

				//The temp will always have this block in it
				dynamic_array_add(&temp, current);

				//The temporary array also has the intersection of all of the successor's of BB's postdominator 
				//sets in it. As such, we'll now compute those

				//If this block has any successors
				if(current->successors.internal_array != NULL){
					//Let's just grab out the very first successor
					basic_block_t* first_successor = dynamic_array_get_at(&(current->successors), 0);

					//Now, if a node IS in the set of all successor's postdominator sets, it must be in here. As such, any node that
					//is not in the set of all successors will NOT be in here, and every node that IS in the set
					//of all successors WILL be. So, we can just run through this entire array and see if each node is 
					//everywhere else

					//If we have any postdominators
					if(first_successor->postdominator_set.internal_array != NULL){
						//For each node in the first one's postdominator set
						for(u_int16_t k = 0; k < first_successor->postdominator_set.current_index; k++){
							//Are we in the intersection of the sets? By default we think so
							u_int8_t in_intersection = TRUE;

							//Grab out the postdominator
							basic_block_t* postdominator = dynamic_array_get_at(&(first_successor->postdominator_set), k);

							//Now let's see if this postdominator is in every other postdominator set for the remaining successors
							for(u_int16_t l = 1; l < current->successors.current_index; l++){
								//Grab the successor out
								basic_block_t* other_successor = dynamic_array_get_at(&(current->successors), l);

								//Now we'll check to see - is our given postdominator in this one's dominator set?
								//If it isn't, we'll set the flag and break out. If it is we'll move on to the next one
								if(dynamic_array_contains(&(other_successor->postdominator_set), postdominator) == NOT_FOUND){
									//We didn't find it, set the flag and get out
									in_intersection = FALSE;
									break;
								}

								//Otherwise we did find it, so we'll keep going
							}

							//By the time we make it here, we'll either have our flag set to true or false. If the postdominator
							//made it to true, it's in the intersection, and will add it to the new set
							if(in_intersection == TRUE){
								dynamic_array_add(&temp, postdominator);
							}
						}
					}
				}

				//Let's compare the two dynamic arrays - if they aren't the same, we've found a difference
				if(dynamic_arrays_equal(&temp, &(current->postdominator_set)) == FALSE){
					//Set the flag
					changed = TRUE;

					//And we can get rid of the old one
					dynamic_array_dealloc(&(current->postdominator_set));
					
					//Set temp to be the new postdominator set
					current->postdominator_set = temp;

				//Otherwise they weren't changed, so the new one that we made has to go
				} else {
					dynamic_array_dealloc(&temp);
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
	for(u_int16_t i = 0; i < cfg->created_blocks.current_index; i++){
		//Grab this out
		basic_block_t* block = dynamic_array_get_at(&(cfg->created_blocks), i);

		//We will initialize the block's dominator set to be the entire set of nodes
		block->dominator_set = clone_dynamic_array(&(cfg->created_blocks));
	}

	//For each and every function that we have, we will perform this operation separately
	for(u_int16_t _ = 0; _ < cfg->function_entry_blocks.current_index; _++){
		//Initialize a "worklist" dynamic array for this particular function
		dynamic_array_t worklist = dynamic_array_alloc();

		//Add this into the worklist as a seed
		dynamic_array_add(&worklist, dynamic_array_get_at(&(cfg->function_entry_blocks), _));
		
		//The new dominance frontier that we have each time
		dynamic_array_t new;

		//So long as the worklist is not empty
		while(dynamic_array_is_empty(&worklist) == FALSE){
			//Remove a node Y from the worklist(remove from back - most efficient{O(1)})
			basic_block_t* Y = dynamic_array_delete_from_back(&worklist);
			
			//Create the new dynamic array that will be used for the next
			//dominator set
			new = dynamic_array_alloc();

			//We will add Y into it's own dominator set
			dynamic_array_add(&new, Y);

			//If Y has predecessors, we will find the intersection of
			//their dominator sets
			if(Y->predecessors.internal_array != NULL){
				//Grab the very first predecessor's dominator set
				dynamic_array_t pred_dom_set = ((basic_block_t*)(Y->predecessors.internal_array[0]))->dominator_set;

				//Are we in the intersection of the dominator sets?
				u_int8_t in_intersection;

				//We will now search every item in this dominator set
				for(u_int16_t i = 0; i < pred_dom_set.current_index; i++){
					//Grab the dominator out
					basic_block_t* dominator = dynamic_array_get_at(&pred_dom_set, i);

					//By default we assume that this given dominator is in the set. If it
					//isn't we'll set it appropriately
					in_intersection = TRUE;

					/**
					 * An item is in the intersection if and only if it is contained 
					 * in all of the dominator sets of the predecessors of Y
					*/
					//We'll start at 1 here - we've already accounted for 0
					for(u_int8_t j = 1; j < Y->predecessors.current_index; j++){
						//Grab our other predecessor
						basic_block_t* other_predecessor = Y->predecessors.internal_array[j];

						//Now we will go over this predecessor's dominator set, and see if "dominator"
						//is also contained within it

						//Let's check for it in here. If we can't find it, we set the flag to false and bail out
						if(dynamic_array_contains(&(other_predecessor->dominator_set), dominator) == NOT_FOUND){
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
						dynamic_array_add(&new, dominator);
					}
				}
			}

			//Now we'll check - are these two dominator sets the same? If not, we'll need
			//to update them
			if(dynamic_arrays_equal(&new, &(Y->dominator_set)) == FALSE){
				//Destroy the old one
				dynamic_array_dealloc(&(Y->dominator_set));

				//And replace it with the new
				Y->dominator_set = new;

				//Now for every successor of Y, add it into the worklist
				for(u_int16_t i = 0; i < Y->successors.current_index; i++){
					dynamic_array_add(&worklist, Y->successors.internal_array[i]);
				}

			//Otherwise they are the same
			} else {
				//Destroy the dominator set that we just made
				dynamic_array_dealloc(&new);
			}
		}

		//Destroy the worklist now that we're done with it
		dynamic_array_dealloc(&worklist);
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
 * 	out[n] = {}U{x|x is an element of in[S] where S is a successor of n}
 * 	in[n] = use[n] U (out[n] - def[n])
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
	dynamic_array_t in_prime;
	dynamic_array_t out_prime;

	//A cursor for the current block
	basic_block_t* current;

	/**
	 * We will run the algorithm for every single function. Since *all* functions
	 * are *separate*, we can run the do-while algorithm on each function independently. This
	 * avoids us needing to recompute the entire CFG every time the disjoint-union-find does not work
	 */
	for(u_int16_t i = 0; i < cfg->function_entry_blocks.current_index; i++){
		//Extract it
		basic_block_t* function_entry = dynamic_array_get_at(&(cfg->function_entry_blocks), i);

		//Run the algorithm until we have no difference found
		do{
			//We'll assume we didn't find a difference each iteration
			difference_found = FALSE;

			//Now we can go through the entire RPO set
			for(u_int16_t _ = 0; _ < function_entry->reverse_post_order_reverse_cfg.current_index; _++){
				//The current block is whichever we grab
				current = dynamic_array_get_at(&(function_entry->reverse_post_order_reverse_cfg), _);

				//Transfer the pointers over
				in_prime = current->live_in;
				out_prime = current->live_out;

				//Set live out to be a new array
				current->live_out = dynamic_array_alloc();
				
				//If we have any successors
				//Run through all of the successors
				for(u_int16_t k = 0; k < current->successors.current_index; k++){
					//Grab the successor out
					basic_block_t* successor = dynamic_array_get_at(&(current->successors), k);

					//If it has a live in set
					if(successor->live_in.internal_array != NULL){
						//Add everything in his live_in set into the live_out set
						for(u_int16_t l = 0; l < successor->live_in.current_index; l++){
							//Let's check to make sure we haven't already added this
							three_addr_var_t* successor_live_in_var = dynamic_array_get_at(&(successor->live_in), l);

							//Let the helper method do it for us
							variable_dynamic_array_add(&(current->live_out), successor_live_in_var);
						}
					}
				}

				//The live in is a combination of the variables used
				//at current and the difference of the LIVE_OUT variables defined
				//ones

				//Since we need all of the used variables, we'll just clone this
				//dynamic array so that we start off with them all
				current->live_in = clone_dynamic_array(&(current->used_variables));

				//Now we need to add every variable that is in LIVE_OUT but NOT in assigned
				//If we have any live out vars
				//Run through them all
				for(u_int16_t j = 0; j < current->live_out.current_index; j++){
					//Grab a reference for our use
					three_addr_var_t* live_out_var = dynamic_array_get_at(&(current->live_out), j);

					//Now we need this block to be not in "assigned" also. If it is in assigned we can't
					//add it
					if(variable_dynamic_array_contains(&(current->assigned_variables), live_out_var) == NOT_FOUND){
						//If this is true we can add
						variable_dynamic_array_add(&(current->live_in), live_out_var);
					}
				}
			
				//Now we'll go through and check if the new live in and live out sets are different. If they are different,
				//we'll be doing this whole thing again

				//For efficiency - if there was a difference in one block, it's already done - no use in comparing
				if(difference_found == FALSE){
					//So we haven't found a difference so far - let's see if we can find one now
					if(variable_dynamic_arrays_equal(&in_prime, &(current->live_in)) == FALSE 
					  || variable_dynamic_arrays_equal(&out_prime, &(current->live_out)) == FALSE){
						//We have in fact found a difference
						difference_found = TRUE;
					}
				}

				//We made it down here, the prime variables are useless. We'll deallocate them
				dynamic_array_dealloc(&in_prime);
				dynamic_array_dealloc(&out_prime);
			}
		
		//So long as this holds we repeat
		} while(difference_found == TRUE);
	}
}


/**
 * Build the dominator tree for each function in the CFG
 */
static void build_dominator_trees(cfg_t* cfg){
	//For each node in the CFG, we will use that node's immediate dominators to
	//build a dominator tree
	
	//Hold the current block
	basic_block_t* current;

	//For each block in the CFG
	for(int16_t _ = cfg->created_blocks.current_index - 1; _ >= 0; _--){
		//Grab out whatever block we're on
		current = dynamic_array_get_at(&(cfg->created_blocks), _);

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
	for	(u_int16_t i = 0; i < var_symtab->sheafs.current_index; i++){
		//Grab the current sheaf
		sheaf_cursor = dynamic_array_get_at(&(var_symtab->sheafs), i);

		//Now we'll free all non-null records
		for(u_int16_t j = 0; j < VARIABLE_KEYSPACE; j++){
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
				dynamic_array_t worklist = dynamic_array_alloc();
				//Keep track of those that were ever on the worklist
				dynamic_array_t ever_on_worklist;

				//We'll also need a dynamic array that contains everything relating
				//to if we did or did not already put a phi function into this block
				dynamic_array_t already_has_phi_func = dynamic_array_alloc();
				
				//Just run through the entire thing
				for(u_int16_t i = 0; i < cfg->created_blocks.current_index; i++){
					//Grab the block out of here
					block_cursor = dynamic_array_get_at(&(cfg->created_blocks), i);

					//Does this block define(assign) our variable?
					if(does_block_assign_variable(block_cursor, record) == TRUE){
						//Then we add this block onto the "worklist"
						dynamic_array_add(&worklist, block_cursor);
					}
				}

				//Now we can clone the "was ever on worklist" dynamic array
				ever_on_worklist = clone_dynamic_array(&worklist);

				//Now we can actually perform our insertions
				//So long as the worklist is not empty
				while(dynamic_array_is_empty(&worklist) == FALSE){
					//Remove the node from the back - more efficient
					basic_block_t* node = dynamic_array_delete_from_back(&worklist);

					//Now we will go through each node in this worklist's dominance frontier
					for(u_int16_t j = 0; j < node->dominance_frontier.current_index; j++){
						//Grab this node out
						basic_block_t* df_node = dynamic_array_get_at(&(node->dominance_frontier), j);

						//If this node already has a phi function, we're not gonna bother with it
						if(dynamic_array_contains(&already_has_phi_func, df_node) != NOT_FOUND){
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
						if(symtab_record_variable_dynamic_array_contains(&(df_node->used_variables), record) == NOT_FOUND
							&& symtab_record_variable_dynamic_array_contains(&(df_node->live_out), record) == NOT_FOUND){
							continue;
						}

						//If we make it here that means that we don't already have one, so we'll add it
						//This function only emits the skeleton of a phi function
						instruction_t* phi_stmt = emit_phi_function(record);

						//Add the phi statement into the block	
						add_phi_statement(df_node, phi_stmt);

						//We'll mark that this block already has one for the future
						dynamic_array_add(&already_has_phi_func, df_node); 

						//If this node has not ever been on the worklist, we'll add it
						//to keep the search going
						if(dynamic_array_contains(&ever_on_worklist, df_node) == NOT_FOUND){
							//We did NOT find it, so we WILL add it
							dynamic_array_add(&worklist, df_node);
							dynamic_array_add(&ever_on_worklist, df_node);
						}
					}
				}

				//Now that we're done with these, we'll remove them for
				//the next round
				dynamic_array_dealloc(&worklist);
				dynamic_array_dealloc(&ever_on_worklist);
				dynamic_array_dealloc(&already_has_phi_func);
			
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
 * Directly increment the counter without need
 * for a three_addr_var_t that's holding it. This
 * is used exclusively for function parameters that in 
 * all technicality have already been assignedby virtue of 
 * existing
 */
static void lhs_new_name_direct(symtab_variable_record_t* variable){
	//Store the old generation level
	u_int16_t generation_level = variable->counter;

	//Increment the counter
	(variable->counter)++;

	//Push the old generation level onto here
	lightstack_push(&(variable->counter_stack), generation_level);

	//And that should be all
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

	//If this is a function entry block, then all of it's
	//parameters have technically already been "assigned"
	if(entry->block_type == BLOCK_TYPE_FUNC_ENTRY){
		//Grab the record out
		symtab_function_record_t* function_defined_in = entry->function_defined_in;
		
		//We'll run through the parameters and mark them as assigned
		for(u_int16_t i = 0; i < function_defined_in->number_of_params; i++){
			//make the new name here
			lhs_new_name_direct(function_defined_in->func_params[i]);
		}
	}

	//Otherwise we'll flag it for the future
	entry->visited = TRUE;

	//Grab out our leader statement here. We will iterate over all statements
	//looking for phi functions
	instruction_t* cursor = entry->leader_statement;

	//So long as this isn't null
	while(cursor != NULL){
		switch(cursor->statement_type){
			//First option - if we encounter a phi function
			case THREE_ADDR_CODE_PHI_FUNC:
				//We will rewrite the assigneed of the phi function(LHS)
				//with the new name
				lhs_new_name(cursor->assignee);
				break;
				
			case THREE_ADDR_CODE_FUNC_CALL:
			case THREE_ADDR_CODE_INDIRECT_FUNC_CALL:
				//If we have a non-temp variable, rename it
				if(cursor->op1 != NULL && IS_SSA_VARIABLE_TYPE(cursor->op1) == TRUE){
					rhs_new_name(cursor->op1);
				}

				//Same goes for the assignee, except this one is the LHS
				if(cursor->assignee != NULL && IS_SSA_VARIABLE_TYPE(cursor->assignee) == TRUE){
					lhs_new_name(cursor->assignee);
				}
				
				//Special case - do we have a function call?
				//Grab it out
				dynamic_array_t func_params = cursor->parameters;

				//If we have any
				//Run through them all
				for(u_int16_t k = 0; k < func_params.current_index; k++){
					//Grab it out
					three_addr_var_t* current_param = dynamic_array_get_at(&func_params, k);

					//If it's not temporary, we rename
					if(IS_SSA_VARIABLE_TYPE(current_param) == TRUE){
						rhs_new_name(current_param);
					}
				}

				break;

			/**
			 * These statements are interesting because the "assignee" is not really
			 * being assigned to. It holds a memory address that is being dereferenced
			 * and then assigned to. As such, these values should never count as an lhs new name
			 */
			case THREE_ADDR_CODE_STORE_STATEMENT:
			case THREE_ADDR_CODE_STORE_WITH_CONSTANT_OFFSET:
			case THREE_ADDR_CODE_STORE_WITH_VARIABLE_OFFSET:
				//If we have a non-temp variable, rename it
				if(cursor->op1 != NULL && IS_SSA_VARIABLE_TYPE(cursor->op1) == TRUE){
					rhs_new_name(cursor->op1);
				}

				//If we have a non-temp variable, rename it
				if(cursor->op2 != NULL && IS_SSA_VARIABLE_TYPE(cursor->op2) == TRUE){
					rhs_new_name(cursor->op2);
				}

				//UNIQUE CASE - rhs also gets a new name here
				if(cursor->assignee != NULL && IS_SSA_VARIABLE_TYPE(cursor->assignee) == TRUE){
					rhs_new_name(cursor->assignee);
				}

				break;

			//And now if it's anything else that has an assignee, operands, etc,
			//we'll need to rewrite all of those as well
			//We'll exclude direct jump statements, these we don't care about
			default:
				//If we have a non-temp variable, rename it
				if(cursor->op1 != NULL && IS_SSA_VARIABLE_TYPE(cursor->op1) == TRUE){
					rhs_new_name(cursor->op1);
				}

				//If we have a non-temp variable, rename it
				if(cursor->op2 != NULL && IS_SSA_VARIABLE_TYPE(cursor->op2) == TRUE){
					rhs_new_name(cursor->op2);
				}

				//Same goes for the assignee, except this one is the LHS
				if(cursor->assignee != NULL && IS_SSA_VARIABLE_TYPE(cursor->assignee) == TRUE){
					lhs_new_name(cursor->assignee);
				}

				break;
		}

		//Advance up to the next statement
		cursor = cursor->next_statement;
	}

	//Now for each successor of b, we'll need to add the phi-function parameters according
	for(u_int16_t _ = 0; _ < entry->successors.current_index; _++){
		//Grab the successor out
		basic_block_t* successor = dynamic_array_get_at(&(entry->successors), _);

		//Now for each phi-function in this successor that uses something in the defined variables
		//here, we'll want to add that newly renamed defined variable into the phi function parameters
		
		//Yet another cursor
		instruction_t* succ_cursor = successor->leader_statement;

		//So long as it isn't null AND it's a phi function
		while(succ_cursor != NULL && succ_cursor->statement_type == THREE_ADDR_CODE_PHI_FUNC){
			//We have a phi function, so what are we assigning to it?
			symtab_variable_record_t* phi_func_var = succ_cursor->assignee->linked_var;

			//Emit a new variable for this one
			three_addr_var_t* phi_func_param = emit_var(phi_func_var);

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
	for(u_int16_t _ = 0; _ < entry->dominator_children.current_index; _++){
		rename_block(dynamic_array_get_at(&(entry->dominator_children), _));
	}

	//Again if this is a function entry block, then we need to unwind the stack
	//so that we avoid excessive variable numbers here as well
	if(entry->block_type == BLOCK_TYPE_FUNC_ENTRY){
		//Grab the record out
		symtab_function_record_t* function_defined_in = entry->function_defined_in;
		
		//We need to pop these all only once so that we have parity with what we
		//did up top
		for(u_int16_t i = 0; i < function_defined_in->number_of_params; i++){
			//Pop it off here
			lightstack_pop(&(function_defined_in->func_params[i]->counter_stack));
		}
	}

	//Once we're done, we'll need to unwind our stack here. Anything that involves an assignee, we'll
	//need to pop it's stack so we don't have excessive variable numbers. We'll now iterate over again
	//and perform pops whereever we see a variable being assigned
	
	//Grab the cursor again
	cursor = entry->leader_statement;
	while(cursor != NULL){
		//We have some special exceptions here...
		switch(cursor->statement_type){
			//These ones have assignees in name only - they do not count
			case THREE_ADDR_CODE_STORE_STATEMENT:
			case THREE_ADDR_CODE_STORE_WITH_CONSTANT_OFFSET:
			case THREE_ADDR_CODE_STORE_WITH_VARIABLE_OFFSET:
				break;

			//Otherwise this does count
			default:
				//If we see a statement that has an assignee that is not temporary, we'll unwind(pop) his stack
				if(cursor->assignee != NULL && IS_SSA_VARIABLE_TYPE(cursor->assignee) == TRUE){
					//Pop it off
					lightstack_pop(&(cursor->assignee->linked_var->counter_stack));
				}

				break;
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

	//All global variables have themselves been assigned. As such, we'll
	//need to mark that by giving them a left hand rename
	for(u_int16_t i = 0; i < cfg->global_variables.current_index; i++){
		global_variable_t* variable = dynamic_array_get_at(&(cfg->global_variables), i);
		lhs_new_name_direct(variable->variable);
	}

	//We will call the rename block function on the first block
	//for each of our functions. The rename block function is 
	//recursive, so that should in theory take care of everything for us
	
	//For each function block
	for(u_int16_t _ = 0; _ < cfg->function_entry_blocks.current_index; _++){
		//Invoke the rename function on it
		rename_block(dynamic_array_get_at(&(cfg->function_entry_blocks), _));
	}
}


/**
 * Emit a pointer arithmetic statement that can arise from either a ++ or -- on a pointer
 */
static three_addr_var_t* handle_pointer_arithmetic(basic_block_t* basic_block, ollie_token_t operator, three_addr_var_t* assignee, u_int8_t is_branch_ending){
	//Emit the constant size
	three_addr_const_t* constant = emit_direct_integer_or_char_constant(assignee->type->internal_types.points_to->type_size, u64);

	//We need this temp assignment for bookkeeping reasons
	instruction_t* temp_assignment = emit_assignment_instruction(emit_temp_var(assignee->type), assignee);
	temp_assignment->is_branch_ending = is_branch_ending;

	//If the assignee is not temporary, it counts as used
	add_used_variable(basic_block, assignee);

	//Add this to the block
	add_statement(basic_block, temp_assignment);

	//Decide what the op is
	ollie_token_t op = operator == PLUSPLUS ? PLUS : MINUS;

	//We need to emit a temp assignment for the assignee
	instruction_t* operation = emit_binary_operation_with_const_instruction(emit_temp_var(assignee->type), temp_assignment->assignee, op, constant);
	operation->is_branch_ending = is_branch_ending;

	//This now counts as used
	add_used_variable(basic_block, temp_assignment->assignee);

	//Add this to the block
	add_statement(basic_block, operation);

	//We need one final assignment
	instruction_t* final_assignment = emit_assignment_instruction(emit_var_copy(assignee), operation->assignee);
	final_assignment->is_branch_ending = is_branch_ending;

	//This now counts as used
	add_used_variable(basic_block, operation->assignee);

	//This variable was assigned, so we must mark it
	add_assigned_variable(basic_block, final_assignment->assignee); 

	//And add this one in
	add_statement(basic_block, final_assignment);

	//Give back the assignee
	return final_assignment->assignee;
}


/**
 * Emit the appropriate address calculation for a given array member, based on what is given in the parameters. This will
 * result in either a lea or a binary operation and then a lea
 */
static three_addr_var_t* emit_array_address_calculation(basic_block_t* basic_block, three_addr_var_t* base_addr, three_addr_var_t* offset, generic_type_t* member_type, u_int8_t is_branch_ending){
	//We need a new temp var for the assignee. We know it's an address always
	three_addr_var_t* assignee = emit_temp_var(i64);

	//Is this a lea compatible power of 2? If so we will use the lea shortcut
	if(is_lea_compatible_power_of_2(member_type->type_size) == TRUE){
		//Let the helper emit the lea
		instruction_t* address_calculation = emit_lea_multiplier_and_operands(assignee, base_addr, offset, member_type->type_size);
		address_calculation->is_branch_ending = is_branch_ending;

		//Do our used variable tracking as needed
		add_used_variable(basic_block, base_addr);
		add_used_variable(basic_block, offset);

		//Get this into the block
		add_statement(basic_block, address_calculation);

	//Otherwise, we can't fully do a lea here so we'll need to instead
	//use a binary operation to multiply followed by a different kind of lea
	} else {
		//We'll need the size to multiply by
		three_addr_const_t* type_size = emit_direct_integer_or_char_constant(member_type->type_size, i64);

		//Let the helper emit the entire thing. We'll store into a temp var there
		three_addr_var_t* final_offset = emit_binary_operation_with_constant(basic_block, emit_temp_var(i64), offset, STAR, type_size, is_branch_ending);

		//The offset has been used here
		add_used_variable(basic_block, offset);

		//And now that we have the incompatible multiplication over with, we can use a lea to add
		instruction_t* lea_statement = emit_lea_operands_only(assignee, base_addr, final_offset);
		lea_statement->is_branch_ending = is_branch_ending;

		//This counts as a use
		add_used_variable(basic_block, base_addr);
		add_used_variable(basic_block, final_offset);

		//Insert into the block
		add_statement(basic_block, lea_statement);
	}

	//Whatever happened return the assignee
	return assignee;
}


/**
 * Emit a struct access lea statement
 */
static three_addr_var_t* emit_struct_address_calculation(basic_block_t* basic_block, generic_type_t* struct_type, three_addr_var_t* current_offset, three_addr_const_t* offset, u_int8_t is_branch_ending){
	//We need a new temp var for the assignee. We know it's an address always
	three_addr_var_t* assignee = emit_temp_var(struct_type);

	//Use the lea helper to emit this
	instruction_t* stmt = emit_lea_offset_only(assignee, current_offset, offset);
	stmt->is_branch_ending = is_branch_ending;

	//The true base address was used here
	add_used_variable(basic_block, current_offset);

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
	three_addr_var_t* assignee = emit_temp_var(u64);

	//If the multiplicand is not temporary we have a new used variable
	add_used_variable(basic_block, mutliplicand);

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
	instruction_t* asm_inline_stmt = emit_asm_inline_instruction(asm_inline_node); 
	
	//Mark this with whatever was passed through
	asm_inline_stmt->is_branch_ending =is_branch_ending;
	
	//Once done we add it into the block
	add_statement(basic_block, asm_inline_stmt);
	
	//And that's all
}


/**
 * Emit the abstract machine code for a return statement
 */
static cfg_result_package_t emit_return(basic_block_t* basic_block, generic_ast_node_t* ret_node, u_int8_t is_branch_ending){
	//For holding our temporary return variable
	cfg_result_package_t return_package = {basic_block, basic_block, NULL, BLANK};

	//Keep track of a current block here for our purposes
	basic_block_t* current = basic_block;

	//This is what we'll be using to return
	three_addr_var_t* return_variable = NULL;

	//If the ret node's first child is not null, we'll let the expression rule
	//handle it. We'll always do an assignment here because return statements present
	//a special case. We always need our return variable to be in %rax, and that may
	//not happen all the time naturally. As such, we need this assignment here
	if(ret_node->first_child != NULL){
		//Most common case so it's the if for branch-pred. If we're not returning a
		//reference that *is* an identifier, we go through all of the common steps
		if(ret_node->inferred_type->type_class != TYPE_CLASS_REFERENCE || ret_node->first_child->ast_node_type != AST_NODE_TYPE_IDENTIFIER){
			//Perform the binary operation here
			cfg_result_package_t expression_package = emit_expression(current, ret_node->first_child, is_branch_ending, FALSE);

			//If we hit a ternary here, we'll need to reassign what our current block is
			if(expression_package.final_block != current){
				//Assign current to be the new end
				current = expression_package.final_block;

				//The final block of the overall return chunk will be this
				return_package.final_block = current;
			}

			//Grab this out to look at
			return_variable = expression_package.assignee;

		//If we get here, we know for a fact that we have the special case of returning a reference that is
		//an identifier. Since this is the case, we need to hijack the normal identifier pipeline and emit
		//this as-is
		} else {
			//Just emit the var like this - no dereference
			return_variable = emit_var(ret_node->first_child->variable);
		}

		/**
		 * Special case here - we have a reference that is the result of our expression, but we do *not* want to return a reference
		 * As such, we will perform an automatic dereference here to get the actual value that we're after
		 */
		if(return_variable->type->type_class == TYPE_CLASS_REFERENCE && ret_node->inferred_type->type_class != TYPE_CLASS_REFERENCE){
			//Dereference the type
			generic_type_t* dereferenced_type = dereference_type(return_variable->type);

			//Dereference the return variable into this
			instruction_t* load_instruction = emit_load_ir_code(emit_temp_var(ret_node->inferred_type), return_variable, dereferenced_type);
			
			//Counts as a use
			add_used_variable(current, load_instruction->op1);

			//Get it into the block
			add_statement(current, load_instruction);

			//This is the new return variable
			return_variable = load_instruction->assignee;
		}

		/**
		 * The type of this final assignee will *always* be the inferred type of the node. We need to ensure that
		 * the function is returning the type as promised, and not what is done through type coercion
		 */
		instruction_t* assignment = emit_assignment_instruction(emit_temp_var(ret_node->inferred_type), return_variable);

		//Add this in as a used variable - make sure we're using the "current" block
		add_used_variable(current, return_variable);

		//Add it into the block
		add_statement(current, assignment);

		//The return variable is now what was assigned
		return_variable	= assignment->assignee;
	}

	//We'll use the ret stmt feature here
	instruction_t* ret_stmt = emit_ret_instruction(return_variable);

	//This variable is now used
	add_used_variable(current, return_variable);

	//Mark this with whatever was passed through
	ret_stmt->is_branch_ending = is_branch_ending;

	//Once it's been emitted, we'll add it in as a statement
	add_statement(current, ret_stmt);

	//Give back the results
	return return_package;
}


/**
 * Emit a jump statement jumping to the destination block, using the jump type that we
 * provide
 *
 * Returns the jump that is emitted
 */
instruction_t* emit_jump(basic_block_t* basic_block, basic_block_t* destination_block){
	//Use the helper function to emit the statement
	instruction_t* stmt = emit_jmp_instruction(destination_block);

	//Mark where we came from
	stmt->block_contained_in = basic_block;

	//Add this into the first block
	add_statement(basic_block, stmt);

	//The destination block is by definition a successor here
	add_successor(basic_block, destination_block);

	//And we give it back
	return stmt;
}


/**
 * Emit a branch statement with an if destination, else destination, conditional result and branch type
 *
 * This rule also handles all successor management required for the rule
 */
void emit_branch(basic_block_t* basic_block, basic_block_t* if_destination, basic_block_t* else_destination, branch_type_t branch_type, three_addr_var_t* conditional_result, branch_category_t branch_category){
	//Emit the actual instruction here
	instruction_t* branch_instruction = emit_branch_statement(if_destination, else_destination, conditional_result, branch_type);

	//By definitiona, this is true
	branch_instruction->is_branch_ending = TRUE;

	//Mark this as the op1 so that we can track in the optimizer
	branch_instruction->op1 = conditional_result;

	//This counts as a use for the result variable
	add_used_variable(basic_block, conditional_result);

	//Add the statement into the block
	add_statement(basic_block, branch_instruction);

	/**
	 * Based on what the category is, we can add the successors in a different
	 * order just so that the IRs look somewhat nicer. This has no effect on
	 * functionality, purely visual
	 */
	if(branch_category == BRANCH_CATEGORY_NORMAL){
		//The if and else destinations are now both successors
		add_successor(basic_block, if_destination);
		add_successor(basic_block, else_destination);
		//Not an inverse branch
		branch_instruction->inverse_branch = FALSE;
	} else {
		//The if and else destinations are now both successors
		add_successor(basic_block, else_destination);
		add_successor(basic_block, if_destination);
		//An inverse branch
		branch_instruction->inverse_branch = TRUE;
	}
}


/**
 * Emit a user defined jump statement that points to a label, not to a block
 *
 * We'll leave out all of the successor logic here as well, until we reach the end
 */
static void emit_user_defined_branch(basic_block_t* basic_block, symtab_variable_record_t* if_destination, basic_block_t* else_destination, three_addr_var_t* conditional_decider, branch_type_t branch_type){
	//Emit the branch, purposefully leaving the if area NULL
	instruction_t* branch = emit_branch_statement(NULL, else_destination, conditional_decider, branch_type);

	//We'll need to store the label in here for later on down the line
	branch->var_record = if_destination;

	//Mark where we came from
	branch->block_contained_in = basic_block;

	//Add this to the array of user defined jumps
	dynamic_array_add(&current_function_user_defined_jump_statements, branch);

	//Add this into the first block
	add_statement(basic_block, branch);
}


/**
 * Emit an indirect jump statement
 *
 * Indirect jumps are written in the form:
 * 	jump *__var__, where var holds the address that we need
 */
void emit_indirect_jump(basic_block_t* basic_block, three_addr_var_t* dest_addr, u_int8_t is_branch_ending){
	//Use the helper function to create it
	instruction_t* indirect_jump = emit_indirect_jmp_instruction(dest_addr);

	//Is it branch ending?
	indirect_jump->is_branch_ending = is_branch_ending;

	//Now we'll add it into the block
	add_statement(basic_block, indirect_jump);
}


/**
 * Emit the abstract machine code for a constant to variable assignment. 
 */
static three_addr_var_t* emit_constant_assignment(basic_block_t* basic_block, generic_ast_node_t* constant_node, u_int8_t is_branch_ending){
	//Placeholders for constant/var values
	three_addr_const_t* const_val;
	three_addr_var_t* local_constant_val;
	three_addr_var_t* function_pointer_variable;
	//Holder for the constant assignment
	instruction_t* const_assignment;

	/**
	 * Constants that are: strings, f32, f64, and function pointers require
	 * special attention here since they use rip-relative addressing/local constants
	 * to work. All other constants do not require this special treatment and are
	 * handled in the catch-all default bucket
	 */
	switch(constant_node->constant_type){
		case STR_CONST:
			//Here's our constant value
			local_constant_val = emit_string_local_constant(current_function, constant_node);

			//We'll emit an instruction that adds this constant value to the %rip to accurately calculate an address to jump to
			const_assignment = emit_lea_rip_relative_constant(emit_temp_var(constant_node->inferred_type), local_constant_val, instruction_pointer_var);
			break;

		//For float constants, we need to emit the local constant equivalent via the helper
		case FLOAT_CONST:
			//Here's our constant value
			local_constant_val = emit_f32_local_constant(current_function, constant_node);

			//We'll emit an instruction that adds this constant value to the %rip to accurately calculate an address to jump to
			const_assignment = emit_lea_rip_relative_constant(emit_temp_var(constant_node->inferred_type), local_constant_val, instruction_pointer_var);
			break;

		//For double constants, we need to emit the local constant equivalent via the helper
		case DOUBLE_CONST:
			//Here's our constant value
			local_constant_val = emit_f64_local_constant(current_function, constant_node);

			//We'll emit an instruction that adds this constant value to the %rip to accurately calculate an address to jump to
			const_assignment = emit_lea_rip_relative_constant(emit_temp_var(constant_node->inferred_type), local_constant_val, instruction_pointer_var);
			break;

		//Special case here - we need to emit a variable for the function pointer itself
		case FUNC_CONST:
			//Emit the variable first
			function_pointer_variable = emit_function_pointer_temp_var(constant_node->func_record);

			//Now emit the rip-relative assignment used to load the address
			const_assignment = emit_lea_rip_relative_constant(emit_temp_var(constant_node->inferred_type), function_pointer_variable, instruction_pointer_var);
			break;
			
		//The most commmon case
		default:
			//Emit the constant value
			const_val = emit_constant(constant_node);

			//For later on
			three_addr_var_t* assignee;

			//These are all basic types
			generic_type_t* type = constant_node->inferred_type;

			//Emit the temp var here
			assignee = emit_temp_var(type);

			//We'll use the constant var feature here
			const_assignment = emit_assignment_with_const_instruction(assignee, const_val);
			break;
	}

	//Mark this with whatever was passed through
	const_assignment->is_branch_ending = is_branch_ending;
	
	//Add this into the basic block
	add_statement(basic_block, const_assignment);

	//Now give back the assignee variable
	return const_assignment->assignee;
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
static three_addr_var_t* emit_identifier(basic_block_t* basic_block, generic_ast_node_t* ident_node, u_int8_t is_branch_ending){
	//Handle an enumerated type right here
	if(ident_node->variable->membership == ENUM_MEMBER) {
		//Just create a constant here with the enum
		return emit_direct_constant_assignment(basic_block, emit_direct_integer_or_char_constant(ident_node->variable->enum_member_value, ident_node->variable->type_defined_as), ident_node->variable->type_defined_as, is_branch_ending);
	}

	/**
	 * If we're emitting a variable that represents a memory region(array, struct, union), then we're really
	 * asking for the stack address of said variable. As such, the emit_identifier rule will intelligently
	 * realize this and instead of just emitting the var itself, will emit a "memory address of" statement.
	 */
	if(is_memory_region(ident_node->variable->type_defined_as) == TRUE && ident_node->variable->membership != FUNCTION_PARAMETER){
		//Emit a special variable that denotes that we are seeking the memory address of this variable,
		//not anything else with it
		return emit_memory_address_var(ident_node->variable);
	}

	/**
	 * If we're on the right side of the equation and this is a stack variable, when we want to use 
	 * the address we have to load
	 */
	if(ident_node->side == SIDE_TYPE_RIGHT && 
		(ident_node->variable->stack_variable == TRUE || ident_node->variable->membership == GLOBAL_VARIABLE)){
		//Extract the "true type" here in case we are dealing with a reference type
		generic_type_t* type = ident_node->variable->type_defined_as;
		//"True" type for dereferencing
		generic_type_t* true_type = ident_node->variable->type_defined_as;

		//If it is a reference, we need to grab what it references
		if(true_type->type_class == TYPE_CLASS_REFERENCE){
			true_type = true_type->internal_types.references;
		}

		//Store for later on
		three_addr_var_t* memory_address;

		//There are 2 cases here - if we have a local stack variable, we'll need to load the memory
		//address up itself. However, if we have a function parameter that's a reference variable,
		//we know that it already is a memory address, so we can just adjust the base address and
		//not load a memory address at all
		if(type->type_class != TYPE_CLASS_REFERENCE || ident_node->variable->membership != FUNCTION_PARAMETER){
			//Emit the memory address variable
			memory_address = emit_memory_address_var(ident_node->variable);

		//Otherwise this is our special case with a reference function parameter - so all we'll do
		//is rememdiate the type
		} else {
			memory_address = emit_var(ident_node->variable);
		}

		//Emit the load instruction. We need to be sure to use the "true type" here in case we are dealing with 
		//a reference
		instruction_t* load_instruction = emit_load_ir_code(emit_temp_var(true_type), memory_address, true_type);
		load_instruction->is_branch_ending = is_branch_ending;

		//This counts as a use
		add_used_variable(basic_block, load_instruction->op1);

		//Add it to the block
		add_statement(basic_block, load_instruction);

		//Just give back the temp var here
		return load_instruction->assignee;
	}

	//Create our variable - the most basic case
	three_addr_var_t* returned_variable = emit_var(ident_node->variable);

	//Give our variable back
	return returned_variable;
}


/**
 * Emit increment three adress code
 */
static three_addr_var_t* emit_inc_code(basic_block_t* basic_block, three_addr_var_t* incrementee, u_int8_t is_branch_ending){
	//Create the code
	instruction_t* inc_code = emit_inc_instruction(incrementee);

	//This will count as live if we read from it
	add_assigned_variable(basic_block, inc_code->assignee);

	//This is a rare case were we're assigning to AND using
	add_used_variable(basic_block, incrementee);

	//Mark this with whatever was passed through
	inc_code->is_branch_ending = is_branch_ending;

	//Add it into the block
	add_statement(basic_block, inc_code);

	//Return the incrementee
	return inc_code->assignee;
}


/**
 * Emit decrement three address code
 */
static three_addr_var_t* emit_dec_code(basic_block_t* basic_block, three_addr_var_t* decrementee, u_int8_t is_branch_ending){
	//Create the code
	instruction_t* dec_code = emit_dec_instruction(decrementee);

	//This will count as live if we read from it
	add_assigned_variable(basic_block, dec_code->assignee);

	//This is a rare case were we're assigning to AND using
	add_used_variable(basic_block, decrementee);

	//Mark this with whatever was passed through
	dec_code->is_branch_ending = is_branch_ending;

	//Add it into the block
	add_statement(basic_block, dec_code);

	//Return the incrementee
	return dec_code->assignee;
}


/**
 * Emit a test instruction
 */
static three_addr_var_t* emit_test_code(basic_block_t* basic_block, three_addr_var_t* op1, three_addr_var_t* op2, u_int8_t is_branch_ending){
	//Emit the test statement based on the type
	instruction_t* test_statement = emit_test_statement(emit_temp_var(op1->type), op1, op2);

	//This counts as a use for op1
	add_used_variable(basic_block, op1);

	//This counts as a use for op2
	add_used_variable(basic_block, op2);

	//Mark if it is branch ending
	test_statement->is_branch_ending = is_branch_ending;

	//Now we'll add it into the block
	add_statement(basic_block, test_statement);

	//Give back the final assignee
	return test_statement->assignee;
}


/**
 * Emit a bitwise not statement 
 */
static three_addr_var_t* emit_bitwise_not_expr_code(basic_block_t* basic_block, three_addr_var_t* var, u_int8_t is_branch_ending){
	//Emit a copy so that we are distinct
	three_addr_var_t* assignee = emit_var_copy(var);

	//First we'll create it here
	instruction_t* not_stmt = emit_not_instruction(assignee);

	//We will still save op1 here, for tracking reasons
	not_stmt->op1 = var;

	//The assignee was assigned
	add_assigned_variable(basic_block, assignee);

	//Regardless this is still used here
	add_used_variable(basic_block, var);

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
static three_addr_var_t* emit_binary_operation_with_constant(basic_block_t* basic_block, three_addr_var_t* assignee, three_addr_var_t* op1, ollie_token_t op, three_addr_const_t* constant, u_int8_t is_branch_ending){
	//Assigned variables need to be non-constant
	add_assigned_variable(basic_block, assignee);

	//Add op1 as a used variable
	add_used_variable(basic_block, op1);

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
static three_addr_var_t* emit_neg_stmt_code(basic_block_t* basic_block, three_addr_var_t* negated, u_int8_t is_branch_ending){
	//We make our temp selection based on this
	three_addr_var_t* var = emit_temp_var(negated->type);

	//This counts as used
	add_used_variable(basic_block, negated);

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
 *
 * It is important to note that logical note statements always return a type of u8 in the end
 */
static three_addr_var_t* emit_logical_neg_stmt_code(basic_block_t* basic_block, three_addr_var_t* negated, u_int8_t is_branch_ending){
	//This will always overwrite the other value
	instruction_t* stmt = emit_logical_not_instruction(emit_temp_var(u8), negated);

	//This counts as a use
	add_used_variable(basic_block, negated);

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
static cfg_result_package_t emit_primary_expr_code(basic_block_t* basic_block, generic_ast_node_t* primary_parent, u_int8_t is_branch_ending){
	//Initialize these results at first
	cfg_result_package_t result_package = {basic_block, basic_block, NULL, BLANK};

	//Switch based on what kind of expression we have. This mainly just calls the appropriate rules
	switch(primary_parent->ast_node_type){
		//In this case we'll only worry about the assignee
		case AST_NODE_TYPE_IDENTIFIER:
		 	result_package.assignee = emit_identifier(basic_block, primary_parent, is_branch_ending);
			return result_package;

		//Same in this case - just an assignee in basic block
		case AST_NODE_TYPE_CONSTANT:
			result_package.assignee = emit_constant_assignment(basic_block, primary_parent, is_branch_ending);
			return result_package;

		//This could potentially have ternaries - so we'll just return whatever is in here
		case AST_NODE_TYPE_FUNCTION_CALL:
			return emit_function_call(basic_block, primary_parent, is_branch_ending);

		//Emit an indirect function call here
		case AST_NODE_TYPE_INDIRECT_FUNCTION_CALL:
			return emit_indirect_function_call(basic_block, primary_parent, is_branch_ending);

		//By default, we're emitting some kind of expression here
		default:
			return emit_expression(basic_block, primary_parent, is_branch_ending, FALSE);
	}
}


/**
 * Emit the code needed to perform an array access
 *
 * This rule returns *the offset* of the value that we want. It has no idea what the array's
 * base address even is
 */
static cfg_result_package_t emit_array_offset_calculation(basic_block_t* block, generic_ast_node_t* array_accessor, three_addr_var_t** current_offset, u_int8_t is_branch_ending){
	//Keep track of whatever the current block is
	basic_block_t* current_block = block;

	//The first thing we'll see is the value in the brackets([value]). We'll let the helper emit this
	cfg_result_package_t expression_package = emit_expression(current_block, array_accessor->first_child, is_branch_ending, FALSE);

	//Set this to be at the end
	current_block = expression_package.final_block;

	//This is whatever was emitted by the expression
	three_addr_var_t* array_offset = expression_package.assignee;

	//The current type will always be what was inferred here
	generic_type_t* member_type = array_accessor->inferred_type;

	/**
	 * If this is not null, we'll be adding on top of it
	 * with this rule and eventually reassigning what the current offset
	 * actually is
	 */
	if(*current_offset != NULL){
		/**
		 * The formula for array subscript is: base_address + type_size * subscript
		 * 
		 * However, if we're on our second or third round, the current var may be an address
		 *
		 * This can be done using a lea instruction, so we will emit that directly
		 */
		three_addr_var_t* address = emit_array_address_calculation(current_block, *current_offset, array_offset, member_type, is_branch_ending);

		//And finally - our current offset is no longer the actual offset
		*current_offset = address;

	/**
	 * If this is NULL, then we can just make the current offset be
	 * the result + the array offset * member type
	 */
	} else {
		//Emit the variable directly here
		*current_offset = emit_temp_var(u64);

		//Emit the binary operation directly with this. The current offset remains unchanged
		emit_binary_operation_with_constant(current_block, *current_offset, array_offset, STAR, emit_direct_integer_or_char_constant(member_type->type_size, u64), is_branch_ending);
	}

	//And the final block is this as well
	expression_package.final_block = current_block;

	//And finally we give this back
	return expression_package;
}


/**
 * Emit the code needed to perform a struct access
 *
 * This rule returns *the offset* of the address that we're after. It has no idea
 * what the base address even is
 */
static cfg_result_package_t emit_struct_offset_calculation(basic_block_t* block, generic_type_t* struct_type, generic_ast_node_t* struct_accessor, three_addr_var_t** current_offset, u_int8_t is_branch_ending){
	//Grab the variable that we need
	symtab_variable_record_t* struct_variable = struct_accessor->variable;

	//Now we'll grab the associated struct record
	symtab_variable_record_t* struct_record = get_struct_member(struct_type, struct_variable->var_name.string);

	//The constant that represents the offset
	three_addr_const_t* struct_offset = emit_direct_integer_or_char_constant(struct_record->struct_offset, u64);

	/**
	 * If the current offset is not null, we're just building on top of something
	 */
	if(*current_offset != NULL){
		//Now we'll emit the address using the helper
		three_addr_var_t* offset_calculation_result = emit_struct_address_calculation(block, struct_type, *current_offset, struct_offset, is_branch_ending);

		//The current offset now is the struct address itself
		*current_offset = offset_calculation_result;

	/**
	 * Otherwise, we'll need to emit the current offset here
	 */
	} else {
		//Emit it here
		*current_offset = emit_temp_var(u64);

		//Emit the const assignment here
		instruction_t* assignment_instruction = emit_assignment_with_const_instruction(*current_offset, struct_offset);
		assignment_instruction->is_branch_ending = is_branch_ending;

		//Add it into the block
		add_statement(block, assignment_instruction);
	}

	//Package & return the results
	cfg_result_package_t results = {block, block, *current_offset, BLANK};
	return results;
}


/**
 * Emit the code needed to perform a struct pointer access
 *
 * This rule returns *the offset* of the value that we want. It has
 * no idea what the base address of the memory region it's in is
 */
static cfg_result_package_t emit_struct_pointer_accessor_expression(basic_block_t* block, generic_type_t* struct_pointer_type, generic_ast_node_t* struct_accessor, three_addr_var_t** base_address, three_addr_var_t** current_offset, u_int8_t is_branch_ending){
	//Get what the raw struct type is
	generic_type_t* raw_struct_type = struct_pointer_type->internal_types.points_to;

	//Now we need to emit the load by doing our offset calculation to get out of the pointer
	//space and into memory
	instruction_t* load_instruction;

	//The current offset is not null, we need to emit some calculation here
	if(*current_offset != NULL){
		//Emit the load
		load_instruction = emit_load_with_variable_offset_ir_code(emit_temp_var(u64), *base_address, *current_offset, (*base_address)->type);
		load_instruction->is_branch_ending = is_branch_ending;

		//This counts as a use for both
		add_used_variable(block, *base_address);
		add_used_variable(block, *current_offset);

		//Add it into the block
		add_statement(block, load_instruction);

		//The new base address now is the load instruction's assignee
		*base_address = load_instruction->assignee;

		//And the offset is now nothing
		*current_offset = NULL;

	//If we get here, we have an empty offset so we just need a regular load
	} else {
		//Regular load here
		load_instruction = emit_load_ir_code(emit_temp_var(u64), *base_address, (*base_address)->type);
		load_instruction->is_branch_ending = is_branch_ending;

		//This counts as a use
		add_used_variable(block, *base_address);
		
		//Get it into the block
		add_statement(block, load_instruction);

		//Again this now is the base address
		*base_address = load_instruction->assignee;
	}

	//Once we get down here, we've emitted everything that we would like to regarding
	//the pointer. Now we need to handle the struct part

	//Extract the var first
	symtab_variable_record_t* struct_variable = struct_accessor->variable;

	//Now we'll grab the associated struct record
	symtab_variable_record_t* struct_record = get_struct_member(raw_struct_type, struct_variable->var_name.string);
	
	//Let's create our offset here
	three_addr_const_t* offset = emit_direct_integer_or_char_constant(struct_record->struct_offset, u64);

	//Now we'll have one final assignment here
	instruction_t* final_assignment =  emit_assignment_with_const_instruction(emit_temp_var(u64), offset);

	//Add it into the block
	add_statement(block, final_assignment);

	//The current offset now is this
	*current_offset = final_assignment->assignee;

	//And we're done here, we can package and return what we have
	cfg_result_package_t results = {block, block, *base_address, BLANK};
	return results;
}


/**
 * Emit the code needed to perform a regular union access
 *
 * This rule returns *the address* of the value that we've asked for
 */
static cfg_result_package_t emit_union_accessor_expression(basic_block_t* block, three_addr_var_t* base_address){
	//Very simple rule, we just have this for consistency
	cfg_result_package_t accessor = {block, block, base_address, BLANK};

	//Give it back
	return accessor;
}


/**
 * Emit the code needed to perform a union pointer access
 *
 * This rule returns *the address* of the value that we've asked for
 */
static cfg_result_package_t emit_union_pointer_accessor_expression(basic_block_t* block, generic_type_t* union_pointer_type, three_addr_var_t** base_address, three_addr_var_t** current_offset, u_int8_t is_branch_ending){
	//Get the current type
	generic_type_t* raw_union_type = union_pointer_type->internal_types.points_to;

	//Store the current block
	basic_block_t* current_block = block;

	//We need to emit a load instruction here to load the union into a variable
	//The current offset could be NULL
	if(*current_offset != NULL){
		//Emit our load statement here
		instruction_t* load_statement = emit_load_with_variable_offset_ir_code(emit_temp_var(raw_union_type), *base_address, *current_offset, (*base_address)->type);
		load_statement->is_branch_ending = is_branch_ending;

		//These count as uses
		add_used_variable(block, *base_address);
		add_used_variable(block, *current_offset);

		//Now add this statement to the block
		add_statement(block, load_statement);

		//The current offset is now NULL because we've used what we stored up so far
		*current_offset = NULL;

		//And the base address is now what we have here
		*base_address = load_statement->assignee;

	//Otherwise the current offset is already NULL, so we just have a regular load statement
	//here
	} else {
		//Emit our load statement here
		instruction_t* load_statement = emit_load_ir_code(emit_temp_var(raw_union_type), *base_address, (*base_address)->type);
		load_statement->is_branch_ending = is_branch_ending;

		//These count as uses
		add_used_variable(block, *base_address);

		//Now add this statement to the block
		add_statement(block, load_statement);

		//And the base address is now what we have here
		*base_address = load_statement->assignee;
	}

	//By the time we get out here, we have performed a dereference and loaded whatever our offset
	//math was before into the new base address variable. The current offset will be NULL again
	//because we need to start over if we have any more offsets
	cfg_result_package_t return_package = {current_block, current_block, *base_address, BLANK};
	return return_package;
}


/**
 * The helper will process the postfix expression for us in a recursive way. We will pass along the "base_address" variable which
 * will eventually be populated by the root level expression
 */
static cfg_result_package_t emit_postfix_expression_rec(basic_block_t* basic_block, generic_ast_node_t* root, three_addr_var_t** base_address, three_addr_var_t** current_offset, u_int8_t is_branch_ending){
	//A tracker for what the current block actually is(this can change)
	basic_block_t* current = basic_block;

	/**
	 * If we make it here, this is actually our base address emittal. We will use the
	 * results from here to hang onto our base address
	 */
	if(root->ast_node_type != AST_NODE_TYPE_POSTFIX_EXPR){
		//Run the primary results function
		cfg_result_package_t primary_results = emit_primary_expr_code(basic_block, root, is_branch_ending);

		//The base address is whatever this assignee is
		*base_address = primary_results.assignee;

		//And give these back
		return primary_results;
	}

	//Once we make it down here, we know that we don't have a primary expression so we need to do postfix processing
	//The left child *always* decays into another postfix expression
	generic_ast_node_t* left_child = root->first_child;
	//And this will *always* be our postoperation code
	generic_ast_node_t* right_child = left_child->next_sibling;
	
	//The type of the memory region we're accessing is all we need here. This is always
	//the left child's type
	generic_type_t* memory_region_type = left_child->inferred_type;

	//We need to first recursively emit the left child's postfix expression
	cfg_result_package_t left_child_results = emit_postfix_expression_rec(basic_block, left_child, base_address, current_offset, is_branch_ending);

	//Update whatever the last block may be
	current = left_child_results.final_block;

	//The postfix results package
	cfg_result_package_t postfix_results;

	//NOTE: by the time we get down here, base address will have been populated with an actual value(usually "memory address of")

	//Now we need to go through and calculate the offset
	switch(right_child->ast_node_type){
		//Handle an array accessor
		case AST_NODE_TYPE_ARRAY_ACCESSOR:
			postfix_results = emit_array_offset_calculation(current, right_child, current_offset, is_branch_ending);
			break;

		//Handle a regular struct accessor(: access)
		case AST_NODE_TYPE_STRUCT_ACCESSOR:
			postfix_results = emit_struct_offset_calculation(current, memory_region_type, right_child, current_offset, is_branch_ending);
			break;

		//Handle a struct pointer access
		case AST_NODE_TYPE_STRUCT_POINTER_ACCESSOR:
			postfix_results = emit_struct_pointer_accessor_expression(current, memory_region_type, right_child, base_address, current_offset, is_branch_ending);
			break;

		//Handle a regular union access(. access)
		case AST_NODE_TYPE_UNION_ACCESSOR:
			postfix_results = emit_union_accessor_expression(current, *base_address);
			break;

		//Handle a union pointer access (-> access)
		case AST_NODE_TYPE_UNION_POINTER_ACCESSOR:
			postfix_results = emit_union_pointer_accessor_expression(current, memory_region_type, base_address, current_offset, is_branch_ending);
			break;
			
		//We should never actually hit this, it's just so the compiler is happy
		default:
			break;
	}

	//Give back our final results(assignee is not needed here)
	cfg_result_package_t final_results = {current, postfix_results.final_block, NULL, BLANK};
	return final_results;
}


/**
 * Emit a postfix expression
 *
 * Postfix expressions have their left subtree most developed. The root of the subtree
 * is the very last postfix expression. This is because we need to "execute" the first 
 * the deepest(first) part first and the highest(root) part last
 */
static cfg_result_package_t emit_postfix_expression(basic_block_t* basic_block, generic_ast_node_t* root, u_int8_t is_branch_ending){
	//This is our "base case". If it's not a postfix expression,
	//just move out
	if(root->ast_node_type != AST_NODE_TYPE_POSTFIX_EXPR){
		return emit_primary_expr_code(basic_block, root, is_branch_ending);
	}

	//Hold onto what our current block is, it may change
	basic_block_t* current_block = basic_block;

	//A variable for our base address(it starts off as null, the recursive rule will modify it)
	three_addr_var_t* base_address = NULL;

	//Another variable for our current offset(again it starts as NULL, the rule will populate if need be)
	three_addr_var_t* current_offset = NULL;
	
	//Let the recursive rule do all the work
	cfg_result_package_t postfix_results = emit_postfix_expression_rec(basic_block, root, &base_address, &current_offset, is_branch_ending);

	//Grab htese out for later
	generic_ast_node_t* left_child = root->first_child;
	generic_ast_node_t* right_child = left_child->next_sibling;

	//For this rule, we care about the parent node's type(after cast/coercion) and
	//the original memory access type(before cast/coercion). We will use these 2 to 
	//determine if a converting operation is needed
	generic_type_t* parent_node_type = root->inferred_type;
	generic_type_t* original_memory_access_type = right_child->inferred_type;

	//This is whatever the final block is
	current_block = postfix_results.final_block;

	//In case we need them - load and store
	instruction_t* load_instruction;
	instruction_t* store_instruction;

	//Do we need a dereference(load or store) here?
	if(root->dereference_needed == TRUE){
		//Based on what we have here - we emit the appropriate statement
		switch(root->side){
			//Left side = store statement
			case SIDE_TYPE_LEFT:
				//This could not be null in the case of structs & arrays
				if(current_offset != NULL){
					//Intentionally leave the storee null, it will be populated down the line
					store_instruction = emit_store_with_variable_offset_ir_code(base_address, current_offset, NULL, original_memory_access_type);

					//Counts as uses for both
					add_used_variable(current_block, base_address);
					add_used_variable(current_block, current_offset);

					//Add it into the block
					add_statement(current_block, store_instruction);

					//Give back the base address as the assignee(even though it's not really)
					postfix_results.assignee = base_address;

				//Otherwise, this means that the current offset is null
				} else {
					//Emit the store here - remember we leave the op1 NULL so that
					//a later rule can fill it in
					store_instruction = emit_store_ir_code(base_address, NULL, original_memory_access_type);

					//Counts as a use
					add_used_variable(current_block, base_address);

					//Add it into our block
					add_statement(current_block, store_instruction);

					//Give back the base address as the assignee(even though it's not really)
					postfix_results.assignee = base_address;
				}

				break;

			//Right side = load statement
			case SIDE_TYPE_RIGHT:
				//This will not be null in the case of structs & arrays
				if(current_offset != NULL){
					//Calculate our load here
					load_instruction = emit_load_with_variable_offset_ir_code(emit_temp_var(parent_node_type), base_address, current_offset, original_memory_access_type);

					//Counts as uses for both
					add_used_variable(current_block, base_address);
					add_used_variable(current_block, current_offset);

					//Add it into the block
					add_statement(current_block, load_instruction);

					//Now the final assignee here is important - it's what we give it here
					postfix_results.assignee = load_instruction->assignee;

				//Otherwise we have a null current offset, so we're just
				//relying on the base address
				} else {
					//Emit the load instruction between the base address and the parent node type
					load_instruction = emit_load_ir_code(emit_temp_var(parent_node_type), base_address, original_memory_access_type);

					//Counts as a use
					add_used_variable(current_block, base_address);

					//Add it into the block
					add_statement(current_block, load_instruction);

					//This is our final assignee
					postfix_results.assignee = load_instruction->assignee;
				}

				break;
		}

	//Otherwise it's just a memory address call, just emit the 
	//base address plus the offset
	} else {
		//If the current offset is not NULL, we'll need to do some calculations
		//here
		if(current_offset != NULL){
			//Just do base address + offset
			instruction_t* address_calculation = emit_binary_operation_instruction(emit_temp_var(base_address->type), base_address, PLUS, current_offset);

			//These count as uses
			add_used_variable(current_block, base_address);
			add_used_variable(current_block, current_offset);

			//Add the instruction in
			add_statement(current_block, address_calculation);

			//This is what we're returning
			postfix_results.assignee = address_calculation->assignee;

		//Otherwise it is null, so we can just use the base address
		} else {
			postfix_results.assignee = base_address;
		}
	}

	//Give back these results
	return postfix_results;
}


/**
 * Emit a postoperation(postincrement or postdecrement) expression
 *
 * These are of the form:
 * 		<postincrement-expression>
 * 				  | 
 * 		  <primary-expression>
 *
 * It is up to use in the CFG to appropriately clone/reconstruct the way that this works
 */
static cfg_result_package_t emit_postoperation_code(basic_block_t* basic_block, generic_ast_node_t* node, u_int8_t is_branch_ending){
	//Store the current block
	basic_block_t* current_block = basic_block;

	//The postfix node is always the first child
	generic_ast_node_t* postfix_node = node->first_child;

	//We will first emit the postfix expression code that comes from this
	cfg_result_package_t postfix_expression_results = emit_postfix_expression(current_block, postfix_node, is_branch_ending);

	//If this is now different, which it could be, we'll change what current is
	if(postfix_expression_results.final_block != current_block){
		current_block = postfix_expression_results.final_block;
	}

	//This is the value that we will be modifying
	three_addr_var_t* assignee = postfix_expression_results.assignee;

	/**
	 * Remember that for a postoperation, we save the value that we get before
	 * we apply the operation. The "postoperation" does not happen until after the value is
	 * used. To facilitate this, we will perform a temp assignment here. The result of
	 * this temp assignment is actually what the user will be using
	 */

	//Emit the assignment
	instruction_t* temp_assignment = emit_assignment_instruction(emit_temp_var(assignee->type), assignee);
	temp_assignment->is_branch_ending = is_branch_ending;

	//This counts as a use for the assignee
	add_used_variable(current_block, assignee);

	//Add this statement in
	add_statement(current_block, temp_assignment);

	//Initialize this off the bat
	cfg_result_package_t postoperation_package = {basic_block, current_block, temp_assignment->assignee, BLANK};

	//If the assignee is not a pointer, we'll handle the normal case
	switch(assignee->type->type_class){
		//If we have basic or reference types, we emit the
		//inc codes
		case TYPE_CLASS_BASIC:
			switch(node->unary_operator){
				case PLUSPLUS:
					//We really just have an "inc" instruction here
					assignee = emit_inc_code(current_block, assignee, is_branch_ending);
					break;
					
				case MINUSMINUS:
					//We really just have an "dec" instruction here
					assignee = emit_dec_code(current_block, assignee, is_branch_ending);
					break;

				//We shouldn't ever hit here
				default:
					break;
			}

			break;

		//A pointer type is a special case
		case TYPE_CLASS_POINTER:
			//Let the helper deal with this
			assignee = handle_pointer_arithmetic(current_block, node->unary_operator, assignee, is_branch_ending);
			break;

		//Everything else should be impossible
		default:
			printf("Fatal internal compiler error: Unreachable path hit for postinc in the CFG\n");
			exit(1);
	}

	//Now that we've handled all of the emitting, we need to check and see if we have any special cases here where 
	//we need to do additional work to reassign this
	/**
	 * Logic here: if this is not some simple identifier(it could be array access, struct access, etc.), then
	 * we'll need to perform this duplication. If it is just an identifier, then we're able to leave this be
	 */
	if(postfix_node->ast_node_type != AST_NODE_TYPE_IDENTIFIER){
		//Duplicate the subtree here for us to use
		generic_ast_node_t* copy = duplicate_subtree(postfix_node, SIDE_TYPE_LEFT);

		//Now we emit the copied package
		cfg_result_package_t copied_package = emit_postfix_expression(current_block, copy, is_branch_ending);

		//This is now the final block
		current_block = copied_package.final_block;

		//Is this a store operation? If so, we just need to fill in the op1 here
		if(is_store_operation(current_block->exit_statement) == TRUE){
			//This is our store statement
			instruction_t* store_statement = current_block->exit_statement;

			/**
			 * Different store statement types have different areas where the operands go
			 */
			switch(store_statement->statement_type){
				//Store statements have the storee in op1
				case THREE_ADDR_CODE_STORE_STATEMENT:
					//This is now our op1
					current_block->exit_statement->op1 = assignee;
					break;

				//When we have offsets, the storee goes into op2
				case THREE_ADDR_CODE_STORE_WITH_CONSTANT_OFFSET:
				case THREE_ADDR_CODE_STORE_WITH_VARIABLE_OFFSET:
					current_block->exit_statement->op2 = assignee;
					break;

				//This is unreachable, just so the compiler is happy
				default:
					break;
			}

			//No matter what happened, we used this
			add_used_variable(current_block, assignee);

		//Otherwise we just have a regular assignment
		} else {
			//And finally, we'll emit the save instruction that stores the value that we've incremented into the location we got it from
			instruction_t* assignment_instruction = emit_assignment_instruction(copied_package.assignee, assignee);
			assignment_instruction->is_branch_ending = is_branch_ending;

			//This counts as a use
			add_used_variable(current_block, assignee);

			//Add this into the block
			add_statement(current_block, assignment_instruction);
		}

		//This is always the new final block
		postoperation_package.final_block = current_block;

	//Otherwise - it is possible that we have a stack variable or reference here. In that case, we'll need to emit a
	//store to get the variable back to where it needs to be
	} else if (postfix_node->variable->stack_variable == TRUE){
		//Get the "true type". If we have a reference, this goes through an implicit dereference
		generic_type_t* true_type = postfix_node->variable->type_defined_as; 

		//The real type in this case is whatever is one layer beneath here
		if(true_type->type_class == TYPE_CLASS_REFERENCE){
			true_type = true_type->internal_types.references;
		}

		//Get the version that represents our memory indirection. Be sure to use the "true type" here
		//just in case we were dealing with a reference
		three_addr_var_t* memory_address_var = emit_memory_address_var(postfix_node->variable);

		//Now we need to add the final store
		instruction_t* store_instruction = emit_store_ir_code(memory_address_var, assignee, true_type);

		//Counts as a use
		add_used_variable(current_block, assignee);

		//Get this in there
		add_statement(current_block, store_instruction);
		
		//This is always the new final block
		postoperation_package.final_block = current_block;
	}

	//Give back the final unary package
	return postoperation_package;
}


/**
 * Handle a unary operator, in whatever form it may be
 */
static cfg_result_package_t emit_unary_operation(basic_block_t* basic_block, generic_ast_node_t* unary_expression_parent, u_int8_t is_branch_ending){
	//Top level declarations to avoid using them in the switch statement
	instruction_t* assignment;
	three_addr_var_t* assignee;
	//The unary expression package
	cfg_result_package_t unary_package = {NULL, NULL, NULL, BLANK};

	//We'll keep track of what the current block here is
	basic_block_t* current_block = basic_block;

	//Extract the first child from the unary expression parent node
	generic_ast_node_t* unary_operator_node = unary_expression_parent->first_child;
	// For use later on
	generic_ast_node_t* unary_expression_child = unary_operator_node->next_sibling;

	//Now that we've emitted the assignee, we can handle the specific unary operators
	switch(unary_operator_node->unary_operator){
		/**
		 * Prefix operations involve the actual increment/decrement and the saving operation. The value
		 * that is returned to be used by the user is the incremented/decremented value unlike in a
		 * postfix expression
		 *
		 * We'll need to apply desugaring here
		 * ++x is really temp = x + 1
		 * 				 x = temp
		 * 				 use temp going forward
		 */
		case PLUSPLUS:
		case MINUSMINUS:
			//The very first thing that we'll do is emit the assignee that comes after the unary expression
			unary_package = emit_unary_expression(current_block, unary_expression_child, is_branch_ending);

			//If this is now different, which it could be, we'll change what current is
			if(unary_package.final_block != current_block){
				current_block = unary_package.final_block;
			}

			//The assignee comes from our package. This is what we are ultimately using in the final result
			assignee = unary_package.assignee;
		
			//Go based on what we have here
			switch(assignee->type->type_class){
				case TYPE_CLASS_BASIC:
					//If we have a temporary variable, then we need to perform
					//a reassignment here for analysis purposes
					if(unary_package.assignee->variable_type == VARIABLE_TYPE_TEMP){
						//Emit the assignment
						instruction_t* temp_assignment = emit_assignment_instruction(emit_temp_var(assignee->type), assignee);
						temp_assignment->is_branch_ending = is_branch_ending;

						//This now counts as a use
						add_used_variable(current_block, assignee);

						//Throw it in the block
						add_statement(current_block, temp_assignment);

						//Now the new assignee equals this new temp that we have
						assignee = temp_assignment->assignee;
					}
					
					//Go based on the op here
					switch(unary_operator_node->unary_operator){
						case PLUSPLUS:
							//We really just have an "inc" instruction here
							assignee = emit_inc_code(current_block, assignee, is_branch_ending);
							break;
							
						case MINUSMINUS:
							//We really just have an "dec" instruction here
							assignee = emit_dec_code(current_block, assignee, is_branch_ending);
							break;

						//We shouldn't ever hit here
						default:	
							break;
					}

					break;

				//The pointer type is a special case
				case TYPE_CLASS_POINTER:
					//Let the helper deal with this
					assignee = handle_pointer_arithmetic(current_block, unary_operator_node->unary_operator, assignee, is_branch_ending);

					break;
				
				//This should never occur
				default:
					printf("Fatal internal compiler error: unreachable type for postincrement found\n");
					exit(1);
			}

			/**
			 * Logic here: if this is not some simple identifier(it could be array access, struct access, etc.), then
			 * we'll need to perform this duplication. If it is just an identifier, then we're able to leave this be
			 */
			if(unary_expression_child->ast_node_type != AST_NODE_TYPE_IDENTIFIER){
				//Duplicate the subtree here for us to use
				generic_ast_node_t* copy = duplicate_subtree(unary_expression_child, SIDE_TYPE_LEFT);

				//Now we emit the copied package
				cfg_result_package_t copied_package = emit_unary_expression(current_block, copy, is_branch_ending);

				//This is now the final block
				current_block = unary_package.final_block;

				//Is this a store operation? If so, we just need to fill in the op1 here
				if(is_store_operation(current_block->exit_statement) == TRUE){
					//This is our store statement
					instruction_t* store_statement = current_block->exit_statement;

					/**
					 * Different store statement types have different areas where the operands go
					 */
					switch(store_statement->statement_type){
						//Store statements have the storee in op1
						case THREE_ADDR_CODE_STORE_STATEMENT:
							//This is now our op1
							current_block->exit_statement->op1 = assignee;
							break;

						//When we have offsets, the storee goes into op2
						case THREE_ADDR_CODE_STORE_WITH_CONSTANT_OFFSET:
						case THREE_ADDR_CODE_STORE_WITH_VARIABLE_OFFSET:
							current_block->exit_statement->op2 = assignee;
							break;

						//This is unreachable, just so the compiler is happy
						default:
							break;
					}

					//No matter what happened, we used this
					add_used_variable(current_block, assignee);

				//Otherwise we just have a regular assignment
				} else {
					//And finally, we'll emit the save instruction that stores the value that we've incremented into the location we got it from
					instruction_t* assignment_instruction = emit_assignment_instruction(copied_package.assignee, assignee);
					assignment_instruction->is_branch_ending = is_branch_ending;

					//This counts as a use
					add_used_variable(current_block, assignee);

					//Add this into the block
					add_statement(current_block, assignment_instruction);
				}

			//Otherwise - it is possible that we have a stack variable or reference here. In that case, we'll need to emit a
			//store to get the variable back to where it needs to be
			} else if (unary_expression_child->variable->stack_variable == TRUE){
				//Get the "true type". If we have a reference, this goes through an implicit dereference
				generic_type_t* true_type = unary_expression_child->variable->type_defined_as; 

				//The real type in this case is whatever is one layer beneath here
				if(true_type->type_class == TYPE_CLASS_REFERENCE){
					true_type = true_type->internal_types.references;
				}

				//Get the version that represents our memory indirection. Be sure to use the "true type" here
				//just in case we were dealing with a reference
				three_addr_var_t* memory_address_var = emit_memory_address_var(unary_expression_child->variable);

				//Now we need to add the final store
				instruction_t* store_instruction = emit_store_ir_code(memory_address_var, assignee, true_type);

				//Counts as a use
				add_used_variable(current_block, assignee);

				//Get this in there
				add_statement(current_block, store_instruction);
				
				//This is always the new final block
				unary_package.final_block = current_block;
			}

			//Store the assignee as this
			unary_package.assignee = assignee;
			//Update the final block if it has change
			unary_package.final_block = current_block;
		
			//Give back the final unary package
			return unary_package;

		//Handle a dereference
		case STAR:
			//The very first thing that we'll do is emit the assignee that comes after the unary expression
			unary_package = emit_unary_expression(current_block, unary_expression_child, is_branch_ending);
			//The assignee comes from the package
			assignee = unary_package.assignee;

			//Update the block
			current_block = unary_package.final_block;

			//The pointer will be on the unary expression child
			generic_type_t* pointer_type = unary_expression_child->inferred_type;
			//And the final type comes from when we dereference it
			generic_type_t* dereferenced_type = dereference_type(pointer_type);

			/**
			 * If we make it here, we will return an *incomplete* store
			 * instruction with the knowledge that whomever called use
			 * will fill it in
			 *
			 * Conditions here: If we are on the left hand side *and* our next sibling is on the right hand side,
			 * that means that we need to be doing an assignment here. As such, we emit a store. In any other
			 * area, we emit a load
			 */
			if(unary_expression_parent->side == SIDE_TYPE_LEFT &&
				(unary_expression_parent->next_sibling != NULL && unary_expression_parent->next_sibling->side == SIDE_TYPE_RIGHT)){
				//We will intentionally leave op1 blank so that it can be filled in down the line
				instruction_t* store_instruction = emit_store_ir_code(assignee, NULL, dereferenced_type);

				//This counts as a use
				add_used_variable(current_block, assignee);

				//Now let's get this into the block
				add_statement(current_block, store_instruction);

				//Add the assignee in here
				unary_package.assignee = assignee;

			/**
			 * If we get here, we know that we either have a right hand operation or we're on the left hand
			 * side but we're not entirely done with the unary operations yet. Either way, we'll need a load
			 */
			} else {
				//If the side type here is right, we'll need a load instruction
				instruction_t* load_instruction = emit_load_ir_code(emit_temp_var(unary_expression_parent->inferred_type), assignee, dereferenced_type);

				//The dereferenced variable has been used
				add_used_variable(current_block, assignee);

				//Add it in
				add_statement(current_block, load_instruction);

				//This one's assignee is our overall assignee
				unary_package.assignee = load_instruction->assignee;
			}

			//Give back the final unary package
			return unary_package;
	
		//Bitwise not operator
		case B_NOT:
			//The very first thing that we'll do is emit the assignee that comes after the unary expression
			unary_package = emit_unary_expression(current_block, unary_expression_child, is_branch_ending);
			//The assignee comes from the package
			assignee = unary_package.assignee;

			//If this is now different, which it could be, we'll change what current is
			if(unary_package.final_block != current_block){
				current_block = unary_package.final_block;
			}

			//The new assignee will come from this helper
			unary_package.assignee = emit_bitwise_not_expr_code(current_block, assignee, is_branch_ending);

			//Give the package back
			return unary_package;

		//Logical not operator
		case L_NOT:
			//The very first thing that we'll do is emit the assignee that comes after the unary expression
			unary_package = emit_unary_expression(current_block, unary_expression_child, is_branch_ending);
			//The assignee comes from the package
			assignee = unary_package.assignee;

			//If this is now different, which it could be, we'll change what current is
			if(unary_package.final_block != current_block){
				current_block = unary_package.final_block;
			}

			//The new assignee will come from this helper
			unary_package.assignee = emit_logical_neg_stmt_code(current_block, assignee, is_branch_ending);

			//Give the package back
			return unary_package;

		/**
		 * Arithmetic negation operator
		 * x = -a;
		 * t <- a;
		 * negl t;
		 * x <- t;
		 *
		 * Uses strategy of: negl rdx
		 */
		case MINUS:
			//The very first thing that we'll do is emit the assignee that comes after the unary expression
			unary_package = emit_unary_expression(current_block, unary_expression_child, is_branch_ending);
			//The assignee comes from the package
			assignee = unary_package.assignee;

			//If this is now different, which it could be, we'll change what current is
			if(unary_package.final_block != current_block){
				current_block = unary_package.final_block;
			}

			//We'll need to assign to a temp here, these are
			//only ever on the RHS
			assignment = emit_assignment_instruction(emit_temp_var(assignee->type), assignee);

			//The assignee here counts as used
			add_used_variable(current_block, assignee);

			//Add this into the block
			add_statement(current_block, assignment);

			//We will emit the negation code here
			unary_package.assignee =  emit_neg_stmt_code(basic_block, assignment->assignee, is_branch_ending);

			//And give back the final value
			return unary_package;

		//Handle the case of the address operator - this is a very unique case, we will not call the unary expression
		//helper here, because we know that this must always be an identifier node
		case SINGLE_AND:
			//Go based on the type here
			switch(unary_expression_child->ast_node_type){
				case AST_NODE_TYPE_IDENTIFIER:
					/**
					 * KEY DETAIL HERE: the variable may already be in the stack. If we're requesting
					 * the address of a struct for example, we don't need to add said struct to the
					 * stack - it is already there. We need to account for these nuances when
					 * we do this
					 *
					 * We do not do this if it's a global variable, because global variables have their own unique storage
					 * mechanism that is not stack related
					 *
					 *
					 * Another nuance, if we have an array, say and int[], and we take the address, the user will
					 * receive a type of int[]*(pointer to an array). In order to achieve this, we will need to create
					 * a whole new stack variable to save the array
					 */
					if(unary_expression_child->variable->membership != GLOBAL_VARIABLE
						//Is it not on the stack already?
						&& unary_expression_child->variable->stack_region == NULL) {
						//Create the stack region and store it in the variable
						unary_expression_child->variable->stack_region = create_stack_region_for_type(&(current_function->data_area), unary_expression_child->variable->type_defined_as);
					} 

					//If it's *not* an array type, we can proceed doing this as we normally do
					if(unary_expression_child->variable->type_defined_as->type_class != TYPE_CLASS_ARRAY){
						//The memory address itself
						three_addr_var_t* memory_address_var = emit_memory_address_var(unary_expression_child->variable);

						//Emit a custom assignment instruction for this
						instruction_t* address_assignment = emit_assignment_instruction(emit_temp_var(u64), memory_address_var);
						address_assignment->is_branch_ending = is_branch_ending;

						//Add this into the block
						add_statement(current_block, address_assignment);

						//And package the value up as what we want here
						unary_package.assignee = address_assignment->assignee;

					/**
					 * For an array type, we'll need to create a pointer to this in memory before we are able to load
					 * up the address. Since the array type is already in memory, we'll create a pointer and load it
					 * with the address of the base of the array.
					 *
					 * There are two scenarios here:
					 *  1.) This is the first time we're taking the address of the array. We'll need to go through,
					 *  make the pointer, load it's address and then return it
					 *
					 *  2.) We've already taken the address of this array and there already exists a pointer in memory to it. In
					 *  this case, we will reuse that same region
					 */
					} else {
						//Let's see whether or not we already have it
						stack_region_t* existing_region = does_stack_contain_pointer_to_variable(&(current_function->data_area), unary_expression_child->variable);

						//We don't have it, so we need to go through our whole procedure
						if(existing_region == NULL){
							//We need to load a reference to this into memory
							stack_region_t* region = create_stack_region_for_type(&(current_function->data_area), unary_expression_parent->inferred_type);

							//This is what is being referenced
							region->variable_referenced = unary_expression_child->variable;

							//Emit the purpose made memory address var
							three_addr_var_t* memory_address = emit_memory_address_var(unary_expression_child->variable);

							//Emit a custom assignment instruction for this
							instruction_t* address_assignment = emit_assignment_instruction(emit_temp_var(u64), memory_address);
							address_assignment->is_branch_ending = is_branch_ending;

							//Add this into the block
							add_statement(current_block, address_assignment);

							//Create the memory address var through the symtab to avoid compatibility issues
							symtab_variable_record_t* memory_address_temp_var = create_temp_memory_address_variable(u64, variable_symtab, region, increment_and_get_temp_id());


							//Emit the temp memory address var here
							three_addr_var_t* stored_memory_address = emit_memory_address_var(memory_address_temp_var);

							//We now store the memory address of the array into the stack itself. This is how we create a pointer to a pointer effectively
							instruction_t* store = emit_store_ir_code(stored_memory_address, address_assignment->assignee, u64);
							store->is_branch_ending = is_branch_ending;

							//This comes afterwards
							add_statement(current_block, store);

							//The final instruction will be us grabbing the memory address of the value that we just put in memory. We can do this with
							//a simple binary operation instruction
							instruction_t* final_assignment = emit_assignment_instruction(emit_temp_var(unary_expression_parent->inferred_type), stored_memory_address);
							final_assignment->is_branch_ending = is_branch_ending;

							//Add it into the block
							add_statement(current_block, final_assignment);

							//The final assignee is this offset here
							unary_package.assignee = final_assignment->assignee;

						//Otherwise, we do have it, so all we need to do is reuse the pointer that we already have
						} else {
							//Create the memory address var through the symtab to avoid compatibility issues
							symtab_variable_record_t* memory_address_temp_var = create_temp_memory_address_variable(u64, variable_symtab, existing_region, increment_and_get_temp_id());

							//Emit the temp memory address var here
							three_addr_var_t* stored_memory_address = emit_memory_address_var(memory_address_temp_var);

							//The final instruction will be us grabbing the memory address of the value that we just put in memory. We can do this with
							//a simple binary operation instruction
							instruction_t* final_assignment = emit_assignment_instruction(emit_temp_var(unary_expression_parent->inferred_type), stored_memory_address);
							final_assignment->is_branch_ending = is_branch_ending;

							//Add it into the block
							add_statement(current_block, final_assignment); 

							//The final assignee is this offset here
							unary_package.assignee = final_assignment->assignee;
						}
					}

					break;

				//The other case here
				case AST_NODE_TYPE_POSTFIX_EXPR:
					//Set the deref flag to false so we don't deref
					unary_expression_child->dereference_needed = FALSE;
					//Emit the whole thing
					cfg_result_package_t postfix_results = emit_postfix_expression(current_block, unary_expression_child, is_branch_ending);

					//Set if need be
					if(postfix_results.final_block != current_block){
						current_block = postfix_results.final_block;
					}

					//And package the value up as what we want here
					unary_package.assignee = postfix_results.assignee;
					break;

				//This should never occur
				default:
					print_parse_message(PARSE_ERROR, "Fatal internal compiler error. Unrecognized node type for address operation", unary_expression_child->line_number);
					exit(0);
			}

			//Set the final block here
			unary_package.final_block = current_block;

			//Give back the unary package
			return unary_package;

		//In reality we should never actually hit this, it's just there for the C compiler to be happy
		default:
			return unary_package;
	}
}


/**
 * Emit the abstract machine code for a unary expression
 * Unary expressions come in the following forms:
 * 	
 * 	<postfix-expression> | <unary-operator> <cast-expression> | typesize(<type-specifier>) | sizeof(<logical-or-expression>) 
 */
static cfg_result_package_t emit_unary_expression(basic_block_t* basic_block, generic_ast_node_t* unary_expression, u_int8_t is_branch_ending){
	//Switch based on what class this node actually is
	switch(unary_expression->ast_node_type){
		//If it's actually a unary expression, we can do some processing
		//If we see the actual node here, we know that we are actually doing a unary operation
		case AST_NODE_TYPE_UNARY_EXPR:	
			return emit_unary_operation(basic_block, unary_expression, is_branch_ending);
		case AST_NODE_TYPE_POSTOPERATION:
			return emit_postoperation_code(basic_block, unary_expression, is_branch_ending);
		//Otherwise if we don't see this node, we instead know that this is really a postfix expression of some kind
		default:
			return emit_postfix_expression(basic_block, unary_expression, is_branch_ending);
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
static cfg_result_package_t emit_ternary_expression(basic_block_t* starting_block, generic_ast_node_t* ternary_operation, u_int8_t is_branch_ending){
	//Expression return package that we need
	cfg_result_package_t return_package;

	//The if area block
	basic_block_t* if_block = basic_block_alloc_and_estimate();
	//And the else area block
	basic_block_t* else_block = basic_block_alloc_and_estimate();
	//The ending block for the whole thing
	basic_block_t* end_block = basic_block_alloc_and_estimate();

	//This block could change, so we'll need to keep track of it in a current block variable
	basic_block_t* current_block = starting_block;

	//Create the ternary variable here
	symtab_variable_record_t* ternary_variable = create_ternary_variable(ternary_operation->inferred_type, variable_symtab, increment_and_get_temp_id());

	//Let's first create the final result variable here
	three_addr_var_t* if_result = emit_var(ternary_variable);
	three_addr_var_t* else_result = emit_var(ternary_variable);
	three_addr_var_t* final_result = emit_var(ternary_variable);

	//Grab a cursor to the first child
	generic_ast_node_t* cursor = ternary_operation->first_child;

	//Let's first process the conditional
	cfg_result_package_t expression_package = emit_binary_expression(current_block, cursor, is_branch_ending);

	//Let's see if we need to reassign
	if(expression_package.final_block != current_block){
		//Reassign this to be at the true end
		current_block = expression_package.final_block;
	}

	//Store for later
	three_addr_var_t* conditional_decider = expression_package.assignee;

	//If this is blank, we need a test instruction
	if(expression_package.operator == BLANK){
		conditional_decider = emit_test_code(current_block, expression_package.assignee, expression_package.assignee, TRUE);
	}

	//Select the jump type for our conditional. This is a normal branch, we aren't doing any inverting
	branch_type_t branch_type = select_appropriate_branch_statement(expression_package.operator, BRANCH_CATEGORY_NORMAL, is_type_signed(conditional_decider->type));

	//emit the branch statement
	emit_branch(current_block, if_block, else_block,  branch_type, conditional_decider, BRANCH_CATEGORY_NORMAL);
	
	//Now we'll go through and process the two children
	cursor = cursor->next_sibling;

	//Emit this in our new if block
	cfg_result_package_t if_branch = emit_expression(if_block, cursor, is_branch_ending, TRUE);

	//Again here we could have multiple blocks, so we'll need to account for this and reassign if necessary
	if(if_branch.final_block != if_block){
		if_block = if_branch.final_block;
	}

	//We'll now create a conditional move for the if branch into the result
	instruction_t* if_assignment = emit_assignment_instruction(if_result, if_branch.assignee);

	//Add this into the if block
	add_statement(if_block, if_assignment);

	//This counts as the result being assigned in the if block
	add_assigned_variable(if_block, if_result);

	//This counts as a use
	add_used_variable(if_block, if_branch.assignee);

	//Now add a direct jump to the end
	emit_jump(if_block, end_block);

	//Process the else branch
	cursor = cursor->next_sibling;

	//Emit this in our else block
	cfg_result_package_t else_branch = emit_expression(else_block, cursor, is_branch_ending, TRUE);

	//Again here we could have multiple blocks, so we'll need to account for this and reassign if necessary
	if(else_branch.final_block != else_block){
		else_block = else_branch.final_block;
	}

	//We'll now create a conditional move for the else branch into the result
	instruction_t* else_assignment = emit_assignment_instruction(else_result, else_branch.assignee);

	//Add this into the else block
	add_statement(else_block, else_assignment);

	//This counts as an assignment in the else block
	add_assigned_variable(else_block, else_result);

	//This counts as a use
	add_used_variable(else_block, else_branch.assignee);

	//Now add a direct jump to the end
	emit_jump(else_block, end_block);

	//Add the final things in here
	return_package.starting_block = starting_block;
	return_package.final_block = end_block;
	//The final assignee is the temp var that we assigned to
	return_package.assignee =  final_result;
	//Mark that we had a ternary here
	return_package.operator = QUESTION;

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
static cfg_result_package_t emit_binary_expression(basic_block_t* basic_block, generic_ast_node_t* logical_or_expr, u_int8_t is_branch_ending){
	//The return package here
	cfg_result_package_t package = {basic_block, basic_block, NULL, BLANK};

	//Current block may change as time goes on, so we'll use the term current block up here to refer to it
	basic_block_t* current_block = basic_block;
	
	//Store the left and right hand types
	generic_type_t* left_hand_type;
	//Temporary holders for our operands
	three_addr_var_t* op1;
	three_addr_var_t* op2;
	//Our assignee - this can change dynamically based on the kind of operator
	three_addr_var_t* assignee;

	//Have we hit the so-called "root level" here? If we have, then we're just going to pass this
	//down to another rule
	if(logical_or_expr->ast_node_type != AST_NODE_TYPE_BINARY_EXPR){
		return emit_unary_expression(current_block, logical_or_expr, is_branch_ending);
	}

	//Otherwise, when we get here, we know that we have a binary expression of some kind

	//Otherwise we actually have a binary operation of some kind
	//Grab a cursor
	generic_ast_node_t* cursor = logical_or_expr->first_child;

	//Store the left hand type for our type comparison later
	left_hand_type = cursor->inferred_type;
	
	//Emit the binary expression on the left first
	cfg_result_package_t left_side = emit_binary_expression(current_block, cursor, is_branch_ending);

	//If these are different, then we'll need to reassign current
	if(left_side.final_block != current_block){
		//Reassign current
		current_block = left_side.final_block;

		//This is also the new final block for the overall statement
		package.final_block = current_block;
	}

	//Advance up here
	cursor = cursor->next_sibling;

	//Then grab the right hand temp
	cfg_result_package_t right_side = emit_binary_expression(current_block, cursor, is_branch_ending);

	//If these are different, then we'll need to reassign current
	if(right_side.final_block != current_block){
		//Reassign current
		current_block = right_side.final_block;

		//This is also the new final block for the overall statement
		package.final_block = current_block;
	}

	//If this is temporary *or* a type conversion is needed, we'll do some reassigning here
	if(left_side.assignee->variable_type != VARIABLE_TYPE_TEMP){
		//emit the temp assignment
		instruction_t* left_side_temp_assignment = emit_assignment_instruction(emit_temp_var(left_hand_type), left_side.assignee);

		//Add it into here
		add_statement(current_block, left_side_temp_assignment);
		
		//We can mark that op1 was used
		add_used_variable(current_block, left_side.assignee);

		//Grab the assignee out
		op1 = left_side_temp_assignment->assignee;

	//Otherwise the left hand temp assignee is just fine for us
	} else {
		op1 = left_side.assignee;
	}

	//Grab this out for convenience
	op2 = right_side.assignee;

	//Let's see what binary operator that we have
	ollie_token_t binary_operator = logical_or_expr->binary_operator;
	//Store this binary operator
	package.operator = binary_operator;

	//Switch based on whatever operator that we have
	switch(binary_operator){
		//Because of the way that logical operator short circuiting works, we
		//will require a temp assignment for op2 if we don't have one
		case DOUBLE_OR:
		case DOUBLE_AND:
			//If this is not temporary, emit the temp assignment
			if(op2->variable_type != VARIABLE_TYPE_TEMP){
				instruction_t* right_side_assignment = emit_assignment_instruction(emit_temp_var(op2->type), op2);

				//Add to the block
				add_statement(current_block, right_side_assignment);

				//This counts as a use
				add_used_variable(current_block, op2);

				//Be sure to reassign what op2 is
				op2 = right_side_assignment->assignee;
			}

			//Emit an assignee based on the inferred type
			assignee = emit_temp_var(logical_or_expr->inferred_type);
			break;

		case L_THAN:
		case G_THAN:
		case G_THAN_OR_EQ:
		case L_THAN_OR_EQ:
		case NOT_EQUALS:
		case DOUBLE_EQUALS:
			//Emit an assignee based on the inferred type
			assignee = emit_temp_var(logical_or_expr->inferred_type);
			break;
		//We use the default strategy - op1 is also the assignee
		default:
			assignee = op1;
			break;
	}
	
	//Add the assignee here
	package.assignee = assignee;
	
	//Emit the binary operator expression using our helper
	instruction_t* binary_operation = emit_binary_operation_instruction(assignee, op1, binary_operator, op2);

	//If this isn't temporary, it's being assigned
	add_assigned_variable(current_block, assignee);

	//If these are not temporary, they're being used
	add_used_variable(current_block, op1);

	//Same deal with this one
	add_used_variable(current_block, op2);

	//Mark this with what we have
	binary_operation->is_branch_ending = is_branch_ending;

	//Add this statement to the block
	add_statement(current_block, binary_operation);

	//Return the temp variable that we assigned to
	return package;
}


/**
 * Handle an assignment expression and all of the required bookkeeping that comes 
 * with it
 */
static cfg_result_package_t emit_assignment_expression(basic_block_t* basic_block, generic_ast_node_t* parent_node, u_int8_t is_branch_ending){
	//Final return package here - this will be updated as we go
	cfg_result_package_t result_package = {basic_block, basic_block, NULL, BLANK};

	/**
	 * We will start by emitting the right hand side first. This will allow
	 * us to emit the left side second, leaving the door open for us to 
	 * fill in any incomplete store operations that could be generated by
	 * the left side
	 */

	//Current block always starts off as the basic_block
	basic_block_t* current_block = basic_block;
		
	//This should always be a unary expression
	generic_ast_node_t* left_child = parent_node->first_child;
	generic_ast_node_t* right_child = left_child->next_sibling;

	//Now emit the right hand expression
	cfg_result_package_t right_hand_package = emit_expression(current_block, right_child, is_branch_ending, FALSE);

	//Hold onto this for later
	instruction_t* last_instruction = current_block->exit_statement;

	//Reassign current to be at the end
	current_block = right_hand_package.final_block;

	//The final first operand will be the expression package's assignee for now
	three_addr_var_t* final_op1 = right_hand_package.assignee;

	//If the final op1 is a memory address, we need to emit a temp assignment
	//to make it not one. This is because later on down in the instruction selector,
	//we will need to translate a memory address into an actual result and we can't
	//do that inside of a store
	if(final_op1->variable_type == VARIABLE_TYPE_MEMORY_ADDRESS){
		//Emit the assignment
		instruction_t* assignment = emit_assignment_instruction(emit_temp_var(u64), final_op1);
		assignment->is_branch_ending = is_branch_ending;

		//Counts as a use
		add_used_variable(current_block, final_op1);

		//Insert this into the block before the store
		insert_instruction_after_given(assignment, last_instruction);

		//This is now the last instruction
		last_instruction = assignment;

		//And the final op1 is this one's assignee
		final_op1 = last_instruction->assignee;
	}

	//Emit the left hand unary expression
	cfg_result_package_t unary_package = emit_unary_expression(current_block, left_child, is_branch_ending);

	//Reassign current to be at the end
	current_block = unary_package.final_block;

	//The left hand var is the final assignee of the unary statement
	three_addr_var_t* left_hand_var = unary_package.assignee;

	/**
	 * Do we have a pre-loaded up store statement ready for us to go? If so, then
	 * we'll need to handle this appropriately
	 */
	if(is_store_operation(current_block->exit_statement) == TRUE){
		//This is our store statement
		instruction_t* store_statement = current_block->exit_statement;

		/**
		 * Different store statement types have different areas where the operands go
		 */
		switch(store_statement->statement_type){
			//Store statements have the storee in op1
			case THREE_ADDR_CODE_STORE_STATEMENT:
				//If the last instruction is *not* a constant assignment, we can go ahead like this
				if(last_instruction == NULL
					|| last_instruction->statement_type != THREE_ADDR_CODE_ASSN_CONST_STMT){

					//This is now our op1
					current_block->exit_statement->op1 = final_op1;

					//No matter what happened, we used this
					add_used_variable(current_block, final_op1);

				//Otherwise, we can do a small optimization here by scrapping the 
				//constant assignment and just putting the constant in directly
				} else {
					//Extract it
					three_addr_const_t* constant_assignee = last_instruction->op1_const;

					//This is now useless
					delete_statement(last_instruction);

					//Set the store statement's op1_const to be this
					current_block->exit_statement->op1_const = constant_assignee;
				}

				break;

			//When we have offsets, the storee goes into op2
			case THREE_ADDR_CODE_STORE_WITH_CONSTANT_OFFSET:
			case THREE_ADDR_CODE_STORE_WITH_VARIABLE_OFFSET:
				//If the last instruction is *not* a constant assignment, we can go ahead like this
				if(last_instruction == NULL
					|| last_instruction->statement_type != THREE_ADDR_CODE_ASSN_CONST_STMT){
					//This is now our op1
					current_block->exit_statement->op2 = final_op1;

					//No matter what happened, we used this
					add_used_variable(current_block, final_op1);

				//Otherwise, we can do a small optimization here by scrapping the 
				//constant assignment and just putting the constant in directly
				} else {
					//Extract it
					three_addr_const_t* constant_assignee = last_instruction->op1_const;

					//This is now useless
					delete_statement(last_instruction);

					//Set the store statement's op1_const to be this
					current_block->exit_statement->op1_const = constant_assignee;
				}

				break;

			//This is unreachable, just so the compiler is happy
			default:
				break;
		}


	/**
	 * Is the left hand variable a regular variable or is it a stack address variable? If it's a
	 * variable that is on the stack, then a regular assignment just won't do. We'll need to
	 * emit a store operation
	 */
	} else if(left_hand_var->linked_var == NULL 
		|| (left_hand_var->linked_var->stack_variable == FALSE
		&& left_hand_var->linked_var->membership != GLOBAL_VARIABLE)){
		//Finally we'll struct the whole thing
		instruction_t* final_assignment = emit_assignment_instruction(left_hand_var, final_op1);

		//Copy this over if there is one
		left_hand_var->associated_memory_region.stack_region = final_op1->associated_memory_region.stack_region;

		//If this is not a temp var, then we can flag it as being assigned
		add_assigned_variable(current_block, left_hand_var);

		//This counts as a use
		add_used_variable(current_block, final_op1);
		
		//Mark this with what was passed through
		final_assignment->is_branch_ending = is_branch_ending;

		//Now add thi statement in here
		add_statement(current_block, final_assignment);

	/**
	 * Otherwise, we'll need to emit a store operation here
	 */
	} else {
		/**
		 * Two possibilities here - we may have a memory address assignment if our variable is *not* a function
		 * parameter. If it is a function parameter, then all we need to do here is emit the variable name itself
		 */
		three_addr_var_t* memory_address;

		//If we actually have a stack region to deal with. Things like references as params will have no such thing. We will check to make
		//sure we are *not* a reference function param. If we aren't then this is just fine for the memory address assignment
		if(left_hand_var->membership != FUNCTION_PARAMETER || left_hand_var->linked_var->type_defined_as->type_class != TYPE_CLASS_REFERENCE){
			//This is the memory address
			memory_address = emit_memory_address_var(left_hand_var->linked_var);

		//Otherwise, it's just the variable itself. This usually happens when we have function parameters
		//that are references
		} else {
			//It's just the left hand var
			memory_address = left_hand_var;
		}

		//We need to extract the "true type". For references, we implicitly dereference,
		//so the true type is whatever it points to
		generic_type_t* true_type = left_hand_var->type;

		//If this is a reference type, we need to represent our implicit
		//dereference here
		if(true_type->type_class == TYPE_CLASS_REFERENCE){
			true_type = true_type->internal_types.references;
		}

		//Now for the final store code
		instruction_t* final_assignment = emit_store_ir_code(memory_address, NULL, true_type);
		final_assignment->is_branch_ending = is_branch_ending;

		//If the last instruction is *not* a constant assignment, we can go ahead like this
		if(last_instruction == NULL
			|| last_instruction->statement_type != THREE_ADDR_CODE_ASSN_CONST_STMT){
			//This is now our op1
			final_assignment->op1 = final_op1;

			//No matter what happened, we used this
			add_used_variable(current_block, final_op1);

		//Otherwise, we can do a small optimization here by scrapping the 
		//constant assignment and just putting the constant in directly
		} else {
			//Extract it
			three_addr_const_t* constant_assignee = last_instruction->op1_const;

			//This is now useless
			delete_statement(last_instruction);

			//Set the store statement's op1_const to be this
			final_assignment->op1_const = constant_assignee;
		}

		//If this is not a temp var, then we can flag it as being assigned
		add_assigned_variable(current_block, memory_address);
		
		//Now add thi statement in here
		add_statement(current_block, final_assignment);
	}

	//Now pack the return value here
	result_package.assignee = left_hand_var;
	//This is whatever the current block is
	result_package.final_block = current_block;

	//And give the results back
	return result_package;
}


/**
 * Emit abstract machine code for an expression. This is a top level statement.
 * These statements almost always involve some kind of assignment "<-" and generate temporary
 * variables
 */
static cfg_result_package_t emit_expression(basic_block_t* basic_block, generic_ast_node_t* expr_node, u_int8_t is_branch_ending, u_int8_t is_conditional){
	//Declare and initialize the results
	cfg_result_package_t result_package;

	//We'll process based on the class of our expression node
	switch(expr_node->ast_node_type){
		//Handle an assignment expression
		case AST_NODE_TYPE_ASNMNT_EXPR:
			//Let the helper deal with it
			result_package = emit_assignment_expression(basic_block, expr_node, is_branch_ending);
			break;
	
		case AST_NODE_TYPE_BINARY_EXPR:
			//Emit the binary expression node
			result_package = emit_binary_expression(basic_block, expr_node, is_branch_ending);
			break;

		case AST_NODE_TYPE_FUNCTION_CALL:
			//Emit the function call statement
			result_package = emit_function_call(basic_block, expr_node, is_branch_ending);
			break;

		//Hanlde an indirect function call
		case AST_NODE_TYPE_INDIRECT_FUNCTION_CALL:
			//Let the helper rule deal with it
			result_package = emit_indirect_function_call(basic_block, expr_node, is_branch_ending);
			break;

		case AST_NODE_TYPE_TERNARY_EXPRESSION:
			//Emit the ternary expression
			result_package = emit_ternary_expression(basic_block, expr_node, is_branch_ending);
			break;
			 
		//Default is a unary expression
		default:
			//Let this rule handle it
			result_package = emit_unary_expression(basic_block, expr_node, is_branch_ending);
			break;
	}

	//If this is a conditional, we can let the helper handle it
	if(is_conditional == TRUE){
		result_package.assignee = handle_conditional_identifier_copy_if_needed(result_package.final_block, result_package.assignee, is_branch_ending);
	}

	return result_package;
}


/**
 * Emit an indirect function call like such
 *
 * call *<function_name>
 *
 * Unlike in a regular call, we don't have the function record on hand to inspect. We'll instead need to rely entirely on the function signature
 */
static cfg_result_package_t emit_indirect_function_call(basic_block_t* basic_block, generic_ast_node_t* indirect_function_call_node, u_int8_t is_branch_ending){
	//Initially we'll emit this, though it may change
 	cfg_result_package_t result_package = {basic_block, basic_block, NULL, BLANK};

	//Grab the function's signature type too
	function_type_t* signature = indirect_function_call_node->variable->type_defined_as->internal_types.function_type;

	//We'll assign the first basic block to be "current" - this could change if we hit ternary operations
	basic_block_t* current = basic_block;

	//The function's assignee
	three_addr_var_t* assignee = NULL;

	//May be NULL or not based on what we have as the return type
	if(signature->returns_void == FALSE){
		//Otherwise we have one like this
		assignee = emit_temp_var(signature->return_type);
	}

	//We first need to emit the function pointer variable
	three_addr_var_t* function_pointer_var = emit_var(indirect_function_call_node->variable);

	//Emit the final call here
	instruction_t* func_call_stmt = emit_indirect_function_call_instruction(function_pointer_var, assignee);

	//Mark this with whatever we have
	func_call_stmt->is_branch_ending = is_branch_ending;

	//Let's grab a param cursor for ourselves
	generic_ast_node_t* param_cursor = indirect_function_call_node->first_child;

	//If this isn't NULL, we have parameters
	if(param_cursor != NULL){
		//Create this
		func_call_stmt->parameters = dynamic_array_alloc();
	}

	//Create a temporary storage array for all of our function parameter results
	dynamic_array_t function_parameter_results = dynamic_array_alloc();

	//The current param of the indext9 <- call parameter_pass2(t10, t11, t12, t14, t16, t18)
	u_int8_t current_func_param_idx = 1;

	//So long as this isn't NULL
	while(param_cursor != NULL){
		//Extract the parameter type here
		generic_type_t* parameter_type = signature->parameters[current_func_param_idx - 1];

		//Assignee for us to use
		three_addr_var_t* param_assignee;

		//Most common case - we aren't dealing with reference parameters here - we follow the
		//normal pattern
		if(parameter_type->type_class != TYPE_CLASS_REFERENCE || param_cursor->ast_node_type != AST_NODE_TYPE_IDENTIFIER){
			//Emit whatever we have here into the basic block
			cfg_result_package_t package = emit_expression(current, param_cursor, is_branch_ending, FALSE);

			//If we did hit a ternary at some point here, we'd see current as different than the final block, so we'll need
			//to reassign
			if(package.final_block != current){
				//We've seen a ternary, reassign current
				current = package.final_block;

				//Reassign this as well, so that we stay current
				result_package.final_block = current;
			}

			//For later down the road
			param_assignee = package.assignee;

		//Otherwise we have the unique case of a reference parameter. We need to bypass the
		//entire identifier system and simply get the memory address of said identifier here
		} else {
			//The assignee is just this one's assignee
			param_assignee = emit_memory_address_var(param_cursor->variable);
		}

		//Add this final result into our parameter results list
		dynamic_array_add(&function_parameter_results, param_assignee);

		//And move up
		param_cursor = param_cursor->next_sibling;

		//Increment this
		current_func_param_idx++;
	}

	//Now that we have all of this, we need to go through and emit our final assignments for the function calls
	//themselves
	for(u_int16_t i = 1; i < current_func_param_idx; i++){
		//Get the result
		three_addr_var_t* result = dynamic_array_get_at(&function_parameter_results, i - 1);

		//Extract the parameter type here
		generic_type_t* paramter_type = signature->parameters[i - 1];

		//We need one more assignment here
		instruction_t* assignment = emit_assignment_instruction(emit_temp_var(paramter_type), result);

		//Counts as a use
		add_used_variable(basic_block, result);

		//Mark this parameter order
		assignment->assignee->parameter_number = current_func_param_idx;

		//Add this into the block
		add_statement(basic_block, assignment);

		//Add the parameter in
		dynamic_array_add(&(func_call_stmt->parameters), assignment->assignee);

		//The assignment here is used implicitly by the function call
		add_used_variable(current, assignment->assignee);
	}

	//Once we make it here, we should have all of the params stored in temp vars
	//We can now add the function call statement in
	add_statement(current, func_call_stmt);

	//If this is not a void return type, we'll need to emit this temp assignment
	if(signature->returns_void == FALSE){
		//Emit an assignment instruction. This will become very important way down the line in register
		//allocation to avoid interference
		instruction_t* assignment = emit_assignment_instruction(emit_temp_var(assignee->type), assignee);

		//The assignee here is used
		add_used_variable(current, assignee);
				
		//Reassign this value
		assignee = assignment->assignee;

		//Add it in
		add_statement(current, assignment);
	}

	//This is always the assignee we gave above. Note that this is nullable,
	//we do 
	result_package.assignee = assignee;

	//Destroy the function parameter results here
	dynamic_array_dealloc(&function_parameter_results);

	//Give back what we assigned to
	return result_package;
}


/**
 * Emit a function call node. In this iteration of a function call, we will still be parameterized, so the actual 
 * node will record what needs to be passed into the function
 */
static cfg_result_package_t emit_function_call(basic_block_t* basic_block, generic_ast_node_t* function_call_node, u_int8_t is_branch_ending){
	//Initially we'll emit this, though it may change
 	cfg_result_package_t result_package = {basic_block, basic_block, NULL, BLANK};

	//Grab this out first
	symtab_function_record_t* func_record = function_call_node->func_record;
	//Grab the function's signature type too
	function_type_t* signature = func_record->signature->internal_types.function_type;

	//We'll assign the first basic block to be "current" - this could change if we hit ternary operations
	basic_block_t* current = basic_block;

	//The function's assignee
	three_addr_var_t* assignee = NULL;

	//May be NULL or not based on what we have as the return type
	if(signature->returns_void == FALSE){
		//Otherwise we have one like this
		assignee = emit_temp_var(signature->return_type);
	}

	//Emit the final call here
	instruction_t* func_call_stmt = emit_function_call_instruction(func_record, assignee);

	//Mark this with whatever we have
	func_call_stmt->is_branch_ending = is_branch_ending;

	//Let's grab a param cursor for ourselves
	generic_ast_node_t* param_cursor = function_call_node->first_child;

	//If this isn't NULL, we have parameters
	if(param_cursor != NULL){
		//Create this
		func_call_stmt->parameters = dynamic_array_alloc();
	}

	//The current param of the indext9 <- call parameter_pass2(t10, t11, t12, t14, t16, t18)
	u_int8_t current_func_param_idx = 1;

	//Keep an array of all of our final results for the function call
	dynamic_array_t function_parameter_results = dynamic_array_alloc();

	//So long as this isn't NULL
	while(param_cursor != NULL){
		//Extract the parameter type here
		generic_type_t* parameter_type = signature->parameters[current_func_param_idx - 1];

		//Assignee for us to use
		three_addr_var_t* param_assignee;

		//Most common case - we aren't dealing with reference parameters here - we follow the
		//normal pattern
		if(parameter_type->type_class != TYPE_CLASS_REFERENCE || param_cursor->ast_node_type != AST_NODE_TYPE_IDENTIFIER){
			//Emit whatever we have here into the basic block
			cfg_result_package_t package = emit_expression(current, param_cursor, is_branch_ending, FALSE);

			//If we did hit a ternary at some point here, we'd see current as different than the final block, so we'll need
			//to reassign
			if(package.final_block != current){
				//We've seen a ternary, reassign current
				current = package.final_block;

				//Reassign this as well, so that we stay current
				result_package.final_block = current;
			}

			//For later down the road
			param_assignee = package.assignee;

		//Otherwise we have the unique case of a reference parameter. We need to bypass the
		//entire identifier system and simply get the memory address of said identifier here
		} else {
			//The assignee is just this one's assignee
			param_assignee = emit_memory_address_var(param_cursor->variable);
		}

		//Add this final result into our parameter results list
		dynamic_array_add(&function_parameter_results, param_assignee);

		//And move up
		param_cursor = param_cursor->next_sibling;

		//Increment this
		current_func_param_idx++;
	}

	//Now that we have all of this, we need to go through and emit our final assignments for the function calls
	//themselves
	for(u_int16_t i = 1; i < current_func_param_idx; i++){
		//Get the result
		three_addr_var_t* result = dynamic_array_get_at(&function_parameter_results, i - 1);
		
		//Extract the parameter type here
		generic_type_t* parameter_type = signature->parameters[i - 1];

		//We need one more assignment here
		instruction_t* assignment = emit_assignment_instruction(emit_temp_var(parameter_type), result);

		//Counts as a use
		add_used_variable(basic_block, result);

		//Mark this parameter order
		assignment->assignee->parameter_number = current_func_param_idx;

		//Add this into the block
		add_statement(basic_block, assignment);

		//Add the parameter in
		dynamic_array_add(&func_call_stmt->parameters, assignment->assignee);

		//The assignment here is used implicitly by the function call
		add_used_variable(current, assignment->assignee);
	}

	//Once we make it here, we should have all of the params stored in temp vars
	//We can now add the function call statement in
	add_statement(current, func_call_stmt);

	//If this is not a void return type, we'll need to emit this temp assignment
	if(signature->returns_void == FALSE){
		//Emit an assignment instruction. This will become very important way down the line in register
		//allocation to avoid interference
		instruction_t* assignment = emit_assignment_instruction(emit_temp_var(assignee->type), assignee);
				
		//Reassign this value
		assignee = assignment->assignee;

		//Add it in
		add_statement(current, assignment);
	}

	//This is always the assignee we gave above. It is important to note
	//that this is nullable, not all functions return something
	result_package.assignee = assignee;

	//Destroy the dynamic array now that we're done
	dynamic_array_dealloc(&function_parameter_results);

	//Give back what we assigned to
	return result_package;
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

	//Now we'll print out each and every function inside of the function_entry_blocks
	//array. Each function will be printed using the BFS strategy
	for(u_int16_t i = 0; i < cfg->function_entry_blocks.current_index; i++){
		//We'll need a queue for our BFS
		heap_queue_t queue = heap_queue_alloc();

		//Grab this out for convenience
		basic_block_t* function_entry_block = dynamic_array_get_at(&(cfg->function_entry_blocks), i);

		//We'll want to see what the stack looks like
		print_stack_data_area(&(function_entry_block->function_defined_in->data_area));

		//Seed the search by adding the funciton block into the queue
		enqueue(&queue, function_entry_block);

		//So long as the queue isn't empty
		while(queue_is_empty(&queue) == FALSE){
			//Pop off of the queue
			block = dequeue(&queue);

			//If this wasn't visited, we'll print
			if(block->visited == FALSE){
				print_block_three_addr_code(block, print_df);	
			}

			//Now we'll mark this as visited
			block->visited = TRUE;

			//And finally we'll add all of these onto the queue
			for(u_int16_t j = 0; j < block->successors.current_index; j++){
				//Add the successor into the queue, if it has not yet been visited
				basic_block_t* successor = block->successors.internal_array[j];

				if(successor->visited == FALSE){
					enqueue(&queue, successor);
				}
			}
		}

		//Destroy the heap queue when done
		heap_queue_dealloc(&queue);
	}
}


/**
 * Destroy all old control relations in anticipation of new ones coming in
 */
void cleanup_all_control_relations(cfg_t* cfg){
	//For each block in the CFG
	for(u_int16_t _ = 0; _ < cfg->created_blocks.current_index; _++){
		//Grab the block out
		basic_block_t* block = dynamic_array_get_at(&(cfg->created_blocks), _);

		//Run through and destroy all of these old control flow constructs
		//Deallocate the postdominator set
		if(block->postdominator_set.internal_array != NULL){
			dynamic_array_dealloc(&(block->postdominator_set));
		}

		//Deallocate the dominator set
		if(block->dominator_set.internal_array != NULL){
			dynamic_array_dealloc(&(block->dominator_set));
		}

		//Deallocate the dominator children
		if(block->dominator_children.internal_array != NULL){
			dynamic_array_dealloc(&(block->dominator_children));
		}

		//Deallocate the domninance frontier
		if(block->dominance_frontier.internal_array != NULL){
			dynamic_array_dealloc(&(block->dominance_frontier));
		}

		//Deallocate the reverse dominance frontier
		if(block->reverse_dominance_frontier.internal_array != NULL){
			dynamic_array_dealloc(&(block->reverse_dominance_frontier));
		}

		//Deallocate the reverse post order set
		if(block->reverse_post_order_reverse_cfg.internal_array != NULL){
			dynamic_array_dealloc(&(block->reverse_post_order_reverse_cfg));
		}

		//Deallocate the reverse post order set
		if(block->reverse_post_order.internal_array != NULL){
			dynamic_array_dealloc(&(block->reverse_post_order));
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
	if(block->used_variables.internal_array != NULL){
		dynamic_array_dealloc(&(block->used_variables));
	}

	//Deallocate the assigned variable array
	if(block->assigned_variables.internal_array != NULL){
		dynamic_array_dealloc(&(block->assigned_variables));
	}

	//Deallocate the postdominator set
	if(block->postdominator_set.internal_array != NULL){
		dynamic_array_dealloc(&(block->postdominator_set));
	}

	//Deallocate the dominator set
	if(block->dominator_set.internal_array != NULL){
		dynamic_array_dealloc(&(block->dominator_set));
	}

	//Deallocate the dominator children
	if(block->dominator_children.internal_array != NULL){
		dynamic_array_dealloc(&(block->dominator_children));
	}

	//Deallocate the domninance frontier
	if(block->dominance_frontier.internal_array != NULL){
		dynamic_array_dealloc(&(block->dominance_frontier));
	}

	//Deallocate the reverse dominance frontier
	if(block->reverse_dominance_frontier.internal_array != NULL){
		dynamic_array_dealloc(&(block->reverse_dominance_frontier));
	}

	//Deallocate the reverse post order set
	if(block->reverse_post_order_reverse_cfg.internal_array != NULL){
		dynamic_array_dealloc(&(block->reverse_post_order_reverse_cfg));
	}

	//Deallocate the reverse post order set
	if(block->reverse_post_order.internal_array != NULL){
		dynamic_array_dealloc(&(block->reverse_post_order));
	}

	//Deallocate the liveness sets
	if(block->live_out.internal_array != NULL){
		dynamic_array_dealloc(&(block->live_out));
	}

	if(block->live_in.internal_array != NULL){
		dynamic_array_dealloc(&(block->live_in));
	}

	//Deallocate the successors
	if(block->successors.internal_array != NULL){
		dynamic_array_dealloc(&(block->successors));
	}

	//Deallocate the predecessors
	if(block->predecessors.internal_array != NULL){
		dynamic_array_dealloc(&(block->predecessors));
	}

	//If this is a switch statement entry block, then it will have a jump table
	if(block->jump_table != NULL){
		jump_table_dealloc(block->jump_table);
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
	for(u_int16_t i = 0; i < cfg->created_blocks.current_index; i++){
		//Use this to deallocate
		basic_block_dealloc(dynamic_array_get_at(&(cfg->created_blocks), i));
	}

	//Destroy all variables
	deallocate_all_vars();
	//Destroy all constants
	deallocate_all_consts();

	//Destroy the dynamic arrays too
	dynamic_array_dealloc(&(cfg->created_blocks));
	dynamic_array_dealloc(&(cfg->function_entry_blocks));

	//At the very end, be sure to destroy this too
	free(cfg);
}


/**
 * Exclusively add a successor to target. The predecessors of successor will not be touched
 */
void add_successor_only(basic_block_t* target, basic_block_t* successor){
	//If this is null, we'll perform the initial allocation
	if(target->successors.internal_array == NULL){
		target->successors = dynamic_array_alloc();
	}

	//Does this block already contain the successor? If so we'll leave
	if(dynamic_array_contains(&(target->successors), successor) != NOT_FOUND){
		//We DID find it, so we won't add
		return;
	}

	//Otherwise we're set to add it in
	dynamic_array_add(&(target->successors), successor);
}


/**
 * Exclusively add a predecessor to target. Nothing with successors
 * will be touched
 */
void add_predecessor_only(basic_block_t* target, basic_block_t* predecessor){
	//If this is NULL, we'll allocate here
	if(target->predecessors.internal_array == NULL){
		target->predecessors = dynamic_array_alloc();
	}

	//Does this contain the predecessor block already? If so we won't add
	if(dynamic_array_contains(&(target->predecessors), predecessor) != NOT_FOUND){
		//We DID find it, so we won't add
		return;
	}

	//Now we can add
	dynamic_array_add(&(target->predecessors), predecessor);
}


/**
 * Add a successor to the target block. This method is entirely comprehensive.
 * Since "successor" comes after "target", "target" will be added as a predecessor
 * of "successor". If you wish to ONLY add a successor or predecessor(very rare),
 * then use the "only" methods
 */
 void add_successor(basic_block_t* target, basic_block_t* successor){
	//This is possible, and if it happens we just leave
	if(successor == NULL){
		return;
	}

	//First we'll add successor as a successor of target
	add_successor_only(target, successor);

	//And then for completeness, we'll add target as a predecessor of successor
	add_predecessor_only(successor, target);
}


/**
 * Delete a successor from a block
 *
 * This is mainly just a wrapper for a dynamic_array_delete, for readability
 *
 * Target: the block that has the successor
 * Successor: the successor itself
 */
void delete_successor_only(basic_block_t* target, basic_block_t* successor){
	dynamic_array_delete(&(target->successors), successor);
}


/**
 * Delete a predecessor from a block
 *
 * This is mainly just a wrapper for a dynamic_array_delete, for readability
 *
 * Target: the block that has the predecessor
 * Predecessor: the predecessor itself
 */
void delete_predecessor_only(basic_block_t* target, basic_block_t* predecessor){
	dynamic_array_delete(&(target->predecessors), predecessor);
}


/**
 * Delete a successor from the target block's list. This function is
 * entirely comprehensive. Since target used to be a predecessor
 * of the deleted successor, that will also be deleted
 */
void delete_successor(basic_block_t* target, basic_block_t* deleted_successor){
	//Bail out if this happens
	if(deleted_successor == NULL){
		return;
	}

	//The target is no longer a predecessor of said successor
	delete_predecessor_only(deleted_successor, target);

	//The said successor is no longer a successor of the target
	delete_successor_only(target, deleted_successor);
}


/**
 * Merge two basic blocks. We always return a pointer to a, b will be deallocated
 *
 * IMPORTANT NOTE: ONCE BLOCKS ARE MERGED, BLOCK B IS GONE
 */
static basic_block_t* merge_blocks(basic_block_t* a, basic_block_t* b){
	//Just to double check
	if(a == NULL){
		printf("Fatal error. Attempting to merge null block");
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
	//Let's merge predecessors first if we have any
	for(u_int16_t i = 0; i < b->predecessors.current_index; i++){
		//Add b's predecessor as one to a
		add_predecessor_only(a, b->predecessors.internal_array[i]);
	}

	//Now merge successors if we have any
	for(u_int16_t i = 0; i < b->successors.current_index; i++){
		//Add b's successors to be a's successors
		add_successor_only(a, b->successors.internal_array[i]);
	}

	//FOR EACH Successor of B, it will have a reference to B as a predecessor.
	//This is now wrong though. So, for each successor of B, it will need
	//to have A as predecessor
	for(u_int16_t i = 0; i < b->successors.current_index; i++){
		//Grab the block first
		basic_block_t* successor_block = b->successors.internal_array[i];

		//If the successor block has predecessors
		if(successor_block->predecessors.internal_array != NULL){
			//Now for each of the predecessors that equals b, it needs to now point to A
			for(u_int8_t i = 0; i < successor_block->predecessors.current_index; i++){
				//If it's pointing to b, it needs to be updated
				if(successor_block->predecessors.internal_array[i] == b){
					//Update it to now be correct
					successor_block->predecessors.internal_array[i] = a;
				}
			}
		}
	}

	//Copy over the block type and terminal type
	if(a->block_type != BLOCK_TYPE_FUNC_ENTRY){
		a->block_type = b->block_type;
	}

	a->block_terminal_type = b->block_terminal_type;

	//If b has a jump table, we'll need to add this in as well
	a->jump_table = b->jump_table;
	b->jump_table = NULL;

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
	for(u_int16_t i = 0; i < b->used_variables.current_index; i++){
		//Add these in one by one to A
		add_used_variable(a, b->used_variables.internal_array[i]);
	}

	//Copy over all of the assigned variables too
	for(u_int16_t i = 0; i < b->assigned_variables.current_index; i++){
		//Add these in one by one
		add_assigned_variable(a, b->assigned_variables.internal_array[i]);
	}

	//Update the instruction counts
	a->number_of_instructions += b->number_of_instructions;

	//We'll remove this from the list of created blocks
	dynamic_array_delete(&(cfg->created_blocks), b);

	//And finally we'll deallocate b
	basic_block_dealloc(b);

	//Give back the pointer to a
	return a;
}


/**
 * A for-statement is another kind of control flow construct
 */
static cfg_result_package_t visit_for_statement(generic_ast_node_t* root_node){
	//Initialize the return package
	cfg_result_package_t result_package = {NULL, NULL, NULL, BLANK};

	//Create our entry block. The entry block also only executes once
	basic_block_t* for_stmt_entry_block = basic_block_alloc_and_estimate();
	//Create our exit block. We assume that the exit only happens once
	basic_block_t* for_stmt_exit_block = basic_block_alloc_and_estimate();
	//We will explicitly declare that this is an exit here
	for_stmt_exit_block->block_type = BLOCK_TYPE_LOOP_EXIT;

	//All breaks will go to the exit block
	//Hold off on the continue block for now
	push(&break_stack, for_stmt_exit_block);

	//Once we get here, we already know what the start and exit are for this statement
	result_package.starting_block = for_stmt_entry_block;
	result_package.final_block = for_stmt_exit_block;
	
	//Grab the reference to the for statement node
	generic_ast_node_t* for_stmt_node = root_node;

	//Grab a cursor for walking the sub-tree
	generic_ast_node_t* ast_cursor = for_stmt_node->first_child;

	//If the very first one is not blank
	if(ast_cursor->first_child != NULL){
		//Create this for our results here
		cfg_result_package_t first_child_result_package = {NULL, NULL, NULL, BLANK};

		switch(ast_cursor->first_child->ast_node_type){
			//We could have a let statement
			case AST_NODE_TYPE_LET_STMT:
				//Let the subrule handle this
				first_child_result_package = visit_let_statement(ast_cursor->first_child, FALSE);
				//We'll need to merge the entry block here due to the way that let statements work
				for_stmt_entry_block = merge_blocks(for_stmt_entry_block, first_child_result_package.starting_block);

				//If these aren't equal, that means that we saw a ternary of some kind, and need to reassign
				//This is a special way of working things due to how let statements work
				if(first_child_result_package.starting_block != first_child_result_package.final_block){
				//Make this the new end
					for_stmt_entry_block = first_child_result_package.final_block;
				}
		
				break;
			default:
				//Let the subrule handle this
				first_child_result_package = emit_expression(for_stmt_entry_block, ast_cursor->first_child, TRUE, FALSE);

				//If these aren't equal, that means that we saw a ternary of some kind, and need to reassign
				if(first_child_result_package.final_block != for_stmt_entry_block){
				//Make this the new end
					for_stmt_entry_block = first_child_result_package.final_block;
				}

				break;
			}
		}

	//Once we reach here, we are officially in the loop. Everything beyond this point
	//is going to happen repeatedly
	push_nesting_level(&nesting_stack, NESTING_LOOP_STATEMENT);

	//We'll now need to create our repeating node. This is the node that will actually repeat from the for loop.
	//The second and third condition in the for loop are the ones that execute continously. The third condition
	//always executes at the end of each iteration
	basic_block_t* condition_block = basic_block_alloc_and_estimate();
	//Flag that this is a loop start
	condition_block->block_type = BLOCK_TYPE_LOOP_ENTRY;

	//We will now emit a jump from the entry block, to the condition block
	emit_jump(for_stmt_entry_block, condition_block);

	//Move along to the next node
	ast_cursor = ast_cursor->next_sibling;

	//The condition block values package
	cfg_result_package_t condition_block_vals = emit_expression(condition_block, ast_cursor->first_child, TRUE, TRUE);

	//Store this for later
	three_addr_var_t* conditional_decider = condition_block_vals.assignee;

	//If this is blank, we need to change this
	if(condition_block_vals.operator == BLANK){
		conditional_decider = emit_test_code(condition_block, condition_block_vals.assignee, condition_block_vals.assignee, TRUE);
	}

	//Now move it along to the third condition
	ast_cursor = ast_cursor->next_sibling;

	//Create the update block
	basic_block_t* for_stmt_update_block = basic_block_alloc_and_estimate();

	//If the third one is not blank
	if(ast_cursor->first_child != NULL){
		//Emit the update expression
		emit_expression(for_stmt_update_block, ast_cursor->first_child, FALSE, FALSE);
	}
	
	//Unconditional jump to condition block
	emit_jump(for_stmt_update_block, condition_block);

	//All continues will go to the update block
	push(&continue_stack, for_stmt_update_block);
	
	//Advance to the next sibling
	ast_cursor = ast_cursor->next_sibling;
	
	//Otherwise, we will allow the subsidiary to handle that. The loop statement here is the condition block,
	//because that is what repeats on continue
	cfg_result_package_t compound_statement_results = visit_compound_statement(ast_cursor);

	//Once we're done with the compound statement, we are no longer in the loop
	pop_nesting_level(&nesting_stack);

	//If we have an empty interior just emit a dummy block. It will be optimized away 
	//regardless
	if(compound_statement_results.starting_block == NULL){
		compound_statement_results.starting_block = basic_block_alloc_and_estimate();
		compound_statement_results.final_block = compound_statement_results.starting_block;
	}

	//Determine the kind of branch that we'll need here
	branch_type_t branch_type = select_appropriate_branch_statement(condition_block_vals.operator, BRANCH_CATEGORY_INVERSE, is_type_signed(conditional_decider->type));

	/**
	 * Inverse jumping logic so
	 *
	 * if not condition 
	 * 	goto exit
	 * else
	 * 	goto update
	 */
	emit_branch(condition_block, for_stmt_exit_block, compound_statement_results.starting_block, branch_type, conditional_decider, BRANCH_CATEGORY_INVERSE);

	//However if it isn't NULL, we'll need to find the end of this compound statement
	basic_block_t* compound_stmt_end = compound_statement_results.final_block;

	//If it ends in a return statement, there is no point in continuing this
	if(compound_stmt_end->exit_statement->statement_type == THREE_ADDR_CODE_RET_STMT){
		//We also need an uncoditional jump right to the update block
		emit_jump(compound_stmt_end, for_stmt_update_block);
	}

	//Now that we're done, we'll need to remove these both from the stack
	pop(&continue_stack);
	pop(&break_stack);

	//Give back the result package here
	return result_package;
}


/**
 * A do-while statement is a simple control flow construct. As always, the direct successor path is the path that reliably
 * leads us down and out
 */
static cfg_result_package_t visit_do_while_statement(generic_ast_node_t* root_node){
	//First we'll allocate the result block
	cfg_result_package_t result_package = {NULL, NULL, NULL, BLANK};

	//The true ending block. We assume that the exit only happens once
	basic_block_t* do_while_stmt_exit_block = basic_block_alloc_and_estimate();
	//We will explicitly mark that this is an exit block
	do_while_stmt_exit_block->block_type = BLOCK_TYPE_LOOP_EXIT;

	//After we allocate the exit block, we will push on the 
	//nesting level as the entry block is in the loop
	push_nesting_level(&nesting_stack, NESTING_LOOP_STATEMENT);

	//Create our entry block. This in reality will be the compound statement
	basic_block_t* do_while_stmt_entry_block = basic_block_alloc_and_estimate();
	//This is an entry block
	do_while_stmt_entry_block->block_type = BLOCK_TYPE_LOOP_ENTRY;

	//We'll push the entry block onto the continue stack, because continues will go there.
	push(&continue_stack, do_while_stmt_entry_block);
	//And we'll push the end block onto the break stack, because all breaks go there
	push(&break_stack, do_while_stmt_exit_block);

	//We can add these into the result package already
	result_package.starting_block = do_while_stmt_entry_block;
	result_package.final_block = do_while_stmt_exit_block;

	//Grab the initial node
	generic_ast_node_t* do_while_stmt_node = root_node;

	//Grab a cursor for walking the subtree
	generic_ast_node_t* ast_cursor = do_while_stmt_node->first_child;

	//We go right into the compound statement here
	cfg_result_package_t compound_statement_results = visit_compound_statement(ast_cursor);

	//Once we're done pop the nesting level
	pop_nesting_level(&nesting_stack);

	//It being NULL is ok, we'll just insert a dummy
	if(compound_statement_results.starting_block == NULL){
		compound_statement_results.starting_block = basic_block_alloc_and_estimate();
		compound_statement_results.final_block = compound_statement_results.starting_block;
	}

	//Now we'll jump to it
	emit_jump(do_while_stmt_entry_block, compound_statement_results.starting_block);

	//We will drill to the bottom of the compound statement
	basic_block_t* compound_stmt_end = compound_statement_results.final_block;

	//If we get this, we can't go forward. Just give it back
	if(compound_stmt_end->exit_statement->statement_type == THREE_ADDR_CODE_RET_STMT){
		//Since we have a return block here, we know that everything else is unreachable
		result_package.final_block = compound_stmt_end;
		//And give it back
		return result_package;
	}

	//Add the conditional check into the end here
	cfg_result_package_t package = emit_expression(compound_stmt_end, ast_cursor->next_sibling, TRUE, TRUE);

	//Store for later
	three_addr_var_t* conditional_decider = package.assignee;

	//If this is blank, we'll need to emit the test code here
	if(package.operator == BLANK){
		conditional_decider = emit_test_code(compound_stmt_end, package.assignee, package.assignee, TRUE);
	}

	//Select the appropriate branch type
	branch_type_t branch_type = select_appropriate_branch_statement(package.operator, BRANCH_CATEGORY_NORMAL, is_type_signed(conditional_decider->type));
		
	/**
	 * Branch works in a regular way
	 *
	 * If condition 
	 * 	goto entry
	 * else
	 * 	exit
	 */
	emit_branch(compound_stmt_end, do_while_stmt_entry_block, do_while_stmt_exit_block, branch_type, conditional_decider, BRANCH_CATEGORY_NORMAL);

	//Now that we're done here, pop the break/continue stacks to remove these blocks
	pop(&continue_stack);
	pop(&break_stack);

	//Always return the entry block
	return result_package;
}


/**
 * A while statement is a very simple control flow construct. As always, the "direct successor" path is the path
 * that reliably leads us down and out
 */
static cfg_result_package_t visit_while_statement(generic_ast_node_t* root_node){
	//Initialize the result package
	cfg_result_package_t result_package = {NULL, NULL, NULL, BLANK};

	//Create our exit block. We assume that this executes once
	basic_block_t* while_statement_end_block = basic_block_alloc_and_estimate();
	//We will specifically mark the end block here as an ending block
	while_statement_end_block->block_type = BLOCK_TYPE_LOOP_EXIT;

	//We will not push to the nesting stack for the end block, but we will for the
	//entry block because the entry block does execute in the loop
	push_nesting_level(&nesting_stack, NESTING_LOOP_STATEMENT);

	//Create our entry block
	basic_block_t* while_statement_entry_block = basic_block_alloc_and_estimate();
	//This is an entry block
	while_statement_entry_block->block_type = BLOCK_TYPE_LOOP_ENTRY;

	//We'll push the entry block onto the continue stack, because continues will go there.
	push(&continue_stack, while_statement_entry_block);
	//And we'll push the end block onto the break stack, because all breaks go there
	push(&break_stack, while_statement_end_block);

	//We already know what to populate our result package with here
	result_package.starting_block = while_statement_entry_block;
	result_package.final_block = while_statement_end_block;

	//Grab this for convenience
	generic_ast_node_t* while_stmt_node = root_node;

	//Grab a cursor to the while statement node
	generic_ast_node_t* ast_cursor = while_stmt_node->first_child;

	//The entry block contains our expression statement
	cfg_result_package_t package = emit_expression(while_statement_entry_block, ast_cursor, TRUE, TRUE);

	//The very next node is a compound statement
	ast_cursor = ast_cursor->next_sibling;

	//Now that we know it's a compound statement, we'll let the subsidiary handle it
	cfg_result_package_t compound_statement_results = visit_compound_statement(ast_cursor);

	//We're out of the compound statement - pop the nesting level
	pop_nesting_level(&nesting_stack);

	//If it's null, that means that we were given an empty while loop here.
	//We'll just allocate our own and use that
	if(compound_statement_results.starting_block == NULL){
		//Just give a dummy here
		compound_statement_results.starting_block = basic_block_alloc_and_estimate();
		compound_statement_results.final_block = compound_statement_results.starting_block;
	}

	//What does the conditional jump rely on?
	three_addr_var_t* conditional_decider = package.assignee;

	//If the operator is blank, we need to emit a test instruction
	if(package.operator == BLANK){
		//Emit the testing instruction
	 	conditional_decider = emit_test_code(while_statement_entry_block, package.assignee, package.assignee, TRUE);
	}

	//The branch type here will be an inverse branch
	branch_type_t branch_type = select_appropriate_branch_statement(package.operator, BRANCH_CATEGORY_INVERSE, is_type_signed(conditional_decider->type));

	/**
	 * Inverse jump out of the while loop to the end if bad
	 *
	 * If destination -> end of loop
	 * Else destination -> loop body
	 */
	emit_branch(while_statement_entry_block, while_statement_end_block, compound_statement_results.starting_block, branch_type, conditional_decider, BRANCH_CATEGORY_INVERSE);

	//Let's now find the end of the compound statement
	basic_block_t* compound_stmt_end = compound_statement_results.final_block;

	//If it's not a return statement, we can add all of these in
	if(compound_stmt_end->exit_statement->statement_type != THREE_ADDR_CODE_RET_STMT){
		//The compound statement end will jump right back up to the entry block
		emit_jump(compound_stmt_end, while_statement_entry_block);
	}

	//Now that we're done, pop these both off their respective stacks
	pop(&break_stack);
	pop(&continue_stack);

	//Now we're done, so
	return result_package;
}


/**
 * Process the if-statement subtree into CFG form
 */
static cfg_result_package_t visit_if_statement(generic_ast_node_t* root_node){
	//The statement result package for our if statement
	cfg_result_package_t result_package;

	//We always have an entry block and an exit block
	basic_block_t* entry_block = basic_block_alloc_and_estimate();
	entry_block->block_type = BLOCK_TYPE_IF_ENTRY;
	basic_block_t* exit_block = basic_block_alloc_and_estimate();
	exit_block->block_type = BLOCK_TYPE_IF_EXIT;

	//Note the starting and final blocks here
	result_package.starting_block = entry_block;
	result_package.final_block = exit_block;

	//An if statement has no assignee, and no operator
	result_package.assignee = NULL;
	result_package.operator = BLANK;

	//Grab the cursor
	generic_ast_node_t* cursor = root_node->first_child;

	//Add whatever our conditional is into the starting block
	cfg_result_package_t package = emit_expression(entry_block, cursor, TRUE, TRUE);

	//Variable for down the road
	three_addr_var_t* conditional_decider = package.assignee;

	//If the operator is blank, we need to emit a test instruction
	if(package.operator == BLANK){
		//Emit the testing instruction
		conditional_decider = emit_test_code(entry_block, package.assignee, package.assignee, TRUE);
	}

	//No we'll move one step beyond, the next node must be a compound statement
	cursor = cursor->next_sibling;

	//Push that we're in an if statement for the compound statement
	push_nesting_level(&nesting_stack, NESTING_IF_STATEMENT);

	//Visit the compound statement that we're required to have here
	cfg_result_package_t if_compound_statement_results = visit_compound_statement(cursor);

	//And then pop it off
	pop_nesting_level(&nesting_stack);

	//If the starting block is null, create a dummy one
	if(if_compound_statement_results.starting_block == NULL){
		if_compound_statement_results.starting_block = basic_block_alloc_and_estimate();
		if_compound_statement_results.final_block = if_compound_statement_results.starting_block;
	}

	//Extract this for convenience
	basic_block_t* if_compound_stmt_end = if_compound_statement_results.final_block;

	//If this is not a return block, we will add these
	if(if_compound_stmt_end->exit_statement->statement_type != THREE_ADDR_CODE_RET_STMT){
		//The successor to the if-stmt end path is the if statement end block
		emit_jump(if_compound_stmt_end, exit_block);
	}

	//Select an appropriate branch for the entry block
	branch_type_t entry_block_branch_type = select_appropriate_branch_statement(package.operator, BRANCH_CATEGORY_NORMAL, is_type_signed(conditional_decider->type));

	//Emit the branch from the entry block out to the starting block. We will *intentionally* leave the else case NULL
	//because we may have else-if cases that we need to add down the road
	emit_branch(entry_block, if_compound_statement_results.starting_block, NULL, entry_block_branch_type, conditional_decider, BRANCH_CATEGORY_NORMAL);

	//From our perspective, the previous entry block
	//is now the one we've just made
	basic_block_t* previous_entry_block = entry_block;

	//Advance the cursor up to it's next sibling
	cursor = cursor->next_sibling;

	//So long as we keep seeing else-if clauses
	while(cursor != NULL && cursor->ast_node_type == AST_NODE_TYPE_ELSE_IF_STMT){
		//Grab a cursor to traverse the else-if block
		generic_ast_node_t* else_if_cursor = cursor->first_child;

		//Make a new one
		basic_block_t* new_entry_block = basic_block_alloc_and_estimate();

		//Extract the old branch statement from the previous entry block
		instruction_t* branch_statement = previous_entry_block->exit_statement;

		/**
		 * For our bookeeping, we'll need to force the old branch statement to
		 * point here to the current entry block. We'll also need
		 * to add a successor
		 */
		branch_statement->else_block = new_entry_block;
		//The current entry block is the else branch for the conditional
		//branch in the previous one
		add_successor(previous_entry_block, new_entry_block);

		//So we've seen the else-if clause. Let's grab the expression first
		package = emit_expression(new_entry_block, else_if_cursor, TRUE, TRUE);

		//Advance it up -- we should now have a compound statement
		else_if_cursor = else_if_cursor->next_sibling;

		//Push that we're in an if statement
		push_nesting_level(&nesting_stack, NESTING_IF_STATEMENT);

		//Let this handle the compound statement
		cfg_result_package_t else_if_compound_statement_results = visit_compound_statement(else_if_cursor);

		//And now pop it off
		pop_nesting_level(&nesting_stack);

		//If this is NULL, then we need to emit dummy blocks
		if(else_if_compound_statement_results.starting_block == NULL){
			else_if_compound_statement_results.starting_block = basic_block_alloc_and_estimate();
			else_if_compound_statement_results.final_block = else_if_compound_statement_results.starting_block;
		}

		//This is the package's assignee
		conditional_decider = package.assignee;

		//If the operator is blank, we need to emit a test instruction
		if(package.operator == BLANK){
			//Emit the testing instruction
			conditional_decider = emit_test_code(new_entry_block, package.assignee, package.assignee, TRUE);
		}

		//Select the branch here as well
		branch_type_t else_if_branch = select_appropriate_branch_statement(package.operator, BRANCH_CATEGORY_NORMAL, is_type_signed(conditional_decider->type));

		//Now we'll emit the branch statement into the current entry block. Again we intentionally
		//leave the else area null for later use
		emit_branch(new_entry_block, else_if_compound_statement_results.starting_block, NULL, else_if_branch, conditional_decider, BRANCH_CATEGORY_NORMAL);

		//Now we'll find the end of this statement
		basic_block_t* else_if_compound_stmt_exit = else_if_compound_statement_results.final_block;

		//If this is not a return block, we will add these
		if(else_if_compound_stmt_exit->exit_statement->statement_type != THREE_ADDR_CODE_RET_STMT){
			//The successor to the if-stmt end path is the if statement end block
			emit_jump(else_if_compound_stmt_exit, exit_block);
		}

		//Now for our bookkeeping, the current entry block here now also counts as the previous
		//entry block
		previous_entry_block = new_entry_block;

		//Advance this up to the next one
		cursor = cursor->next_sibling;
	}

	//Now that we're out of here - we may have an else statement on our hands
	if(cursor != NULL && cursor->ast_node_type == AST_NODE_TYPE_COMPOUND_STMT){
		//Push the nesting level now that we're in a compound statement
		push_nesting_level(&nesting_stack, NESTING_IF_STATEMENT);

		//Grab the compound statement
		cfg_result_package_t else_compound_statement_values = visit_compound_statement(cursor);

		//Pop it off
		pop_nesting_level(&nesting_stack);

		//Extract for convenience
		instruction_t* branch_statement = previous_entry_block->exit_statement;

		//This very well could be NULL, in which case we can just go to the end
		if(else_compound_statement_values.starting_block != NULL){
			//The else block here now points to the else's start
			branch_statement->else_block = else_compound_statement_values.starting_block;

			//It is also now a successor as well
			add_successor(previous_entry_block, else_compound_statement_values.starting_block);

			//More bookeeping based on the exit type
			basic_block_t* else_compound_statement_exit = else_compound_statement_values.final_block;

			//If this is not a return block, we will add these
			if(else_compound_statement_exit->block_terminal_type != BLOCK_TERM_TYPE_RET){
				//The successor to the if-stmt end path is the if statement end block
				emit_jump(else_compound_statement_exit, exit_block);
			}

		} else {
			//The else block here is just the exit block
			branch_statement->else_block = exit_block;

			//And it's a successor as well
			add_successor(previous_entry_block, exit_block);
		}

	//Otherwise the if statement will need to jump directly to the end
	} else {
		//Extract the branch for convenience
		instruction_t* branch_statement = previous_entry_block->exit_statement;

		//The else scenario here is just the exit block
		branch_statement->else_block = exit_block;

		//The exit block is now a successor as well
		add_successor(previous_entry_block, exit_block);
	}

	//If we have an exit block that has no predecessors, that means that we return through every
	//control path. In this instance, we need to set the result package's final block to be the
	//exit block
	if(exit_block->predecessors.internal_array == NULL || exit_block->predecessors.current_index == 0){
		result_package.final_block = function_exit_block;
	}

	//Give back the result package
	return result_package;
}


/**
 * Visit a default statement.  These statements are also handled like individual blocks that can 
 * be jumped to
 */
static cfg_result_package_t visit_default_statement(generic_ast_node_t* root_node){
	//Declare and prepack our results
	cfg_result_package_t results = {NULL, NULL, NULL, BLANK};

	//This is nesting inside of a case statement
	push_nesting_level(&nesting_stack, NESTING_CASE_STATEMENT);

	//For a default statement, it performs very similarly to a case statement. 
	//It will be handled slightly differently in the jump table, but we'll get to that 
	//later on

	//Grab a cursor to our default statement
	generic_ast_node_t* default_stmt_cursor = root_node;

	//Grab the compound statement out of here
	cfg_result_package_t default_compound_statement_results = visit_compound_statement(default_stmt_cursor->first_child);

	//Let this take care of it if we have an actual compound statement here
	if(default_compound_statement_results.starting_block != NULL){
		//Otherwise, we'll just copy over the starting and ending block into our results
		results.starting_block = default_compound_statement_results.starting_block;
		results.final_block = default_compound_statement_results.final_block;

	} else {
		//Create it. We assume that this happens once
		basic_block_t* default_stmt = basic_block_alloc_and_estimate();

		//Prepackage these now
		results.starting_block = default_stmt;
		results.final_block = default_stmt;
	}

	//Nesting here is no longer in a case statement
	pop_nesting_level(&nesting_stack);

	//Give the block back
	return results;
}


/**
 * Visit a case statement. It is very important that case statements know
 * where the end of the switch statement is, in case break statements are used
 */
static cfg_result_package_t visit_case_statement(generic_ast_node_t* root_node){
	//Declare and prepack our results
	cfg_result_package_t results = {NULL, NULL, NULL, BLANK};

	//This is nesting inside of a case statement
	push_nesting_level(&nesting_stack, NESTING_CASE_STATEMENT);

	//The case statement should have some kind of constant value here, whether
	//it's an enum value or regular const. All validation should have been
	//done by the parser, so we're guaranteed to see something
	//correct here
	
	generic_ast_node_t* case_stmt_cursor = root_node;

	//Let this take care of it
	cfg_result_package_t case_compound_statement_results = visit_compound_statement(case_stmt_cursor->first_child);

	//If this isn't Null, we'll run the analysis. If it is NULL, we have an empty case block
	if(case_compound_statement_results.starting_block != NULL){
		//Once we get this back, we'll add it in to the main block
		results.starting_block = case_compound_statement_results.starting_block;

		//Add this in as the final block
		results.final_block = case_compound_statement_results.final_block;

		//Be sure that we copy over the case statement value as well
		results.starting_block->case_stmt_val = case_stmt_cursor->constant_value.signed_int_value;

	} else {
		//We need to make the block first
		basic_block_t* case_stmt = basic_block_alloc_and_estimate();

		//Grab the value -- this should've already been done by the parser
		case_stmt->case_stmt_val = case_stmt_cursor->constant_value.signed_int_value;

		//We'll set the front and end block to both be this
		results.starting_block = case_stmt;
		results.final_block = case_stmt;
	}

	//Nesting here is no longer in a case statement
	pop_nesting_level(&nesting_stack);

	//Give the block back
	return results;
}


/**
 * Visit a C-style case statement. These statements do allow the possibility breaks being issued,
 * and they don't inherently use compound statements. We'll need to account for both possibilities
 * in this rule
 *
 * NOTE: There is no new lexical scope for a C-style case statement. Every root level statement
 * maintains the same lexical scope as the switch
 */
static cfg_result_package_t visit_c_style_case_statement(generic_ast_node_t* root_node){
	//Declare and initialize off the bat
	cfg_result_package_t result_package = {NULL, NULL, NULL, BLANK};

	//This is nested in a C-style case statement
	push_nesting_level(&nesting_stack, NESTING_C_STYLE_CASE_STATEMENT);

	//Since a C-style case statement is just a collection of 
	//statements, we'll use the statement sequence to process it here
	cfg_result_package_t statement_results = visit_statement_chain(root_node->first_child); 

	//This would occur whenever we don't have an empty case
	if(statement_results.starting_block != NULL){
		//These become our starting and final blocks
		result_package.starting_block = statement_results.starting_block;
		result_package.final_block = statement_results.final_block;

	} else {
		//If it is NULL, we're going to need to create our own block here
		basic_block_t* case_block = basic_block_alloc_and_estimate();

		//This is the starting and final block
		result_package.starting_block = case_block;
		result_package.final_block = case_block;
	}

	//Remove the nesting now
	pop_nesting_level(&nesting_stack);

	//Give back the final results
	return result_package;
}


/**
 * Visit a C-style default statement. These statements do allow the possibility breaks being issued,
 * and they don't inherently use compound statements. We'll need to account for both possibilities
 * in this rule
 *
 * NOTE: There is no new lexical scope for a C-style default statement. Every root level statement
 * maintains the same lexical scope as the switch
 */
static cfg_result_package_t visit_c_style_default_statement(generic_ast_node_t* root_node){
	//Declare and initialize off the bat
	cfg_result_package_t result_package = {NULL, NULL, NULL, BLANK};

	//This is nested in a C-style case statement
	push_nesting_level(&nesting_stack, NESTING_CASE_STATEMENT);

	//Since a C-style case statement is just a collection of 
	//statements, we'll use the statement sequence to process it here
	cfg_result_package_t statement_results = visit_statement_chain(root_node->first_child); 

	//This would occur whenever we don't have an empty case
	if(statement_results.starting_block != NULL){
		//These become our starting and final blocks
		result_package.starting_block = statement_results.starting_block;
		result_package.final_block = statement_results.final_block;

	} else {
		//If it is NULL, we're going to need to create our own block here
		basic_block_t* case_block = basic_block_alloc_and_estimate();

		//This is the starting and final block
		result_package.starting_block = case_block;
		result_package.final_block = case_block;

	}

	//Remove the nesting now
	pop_nesting_level(&nesting_stack);

	//Give back the final results
	return result_package;
}


/**
 * Visit a C-style switch statement. Ollie supports a new version of switch statements(with no fallthrough),
 * and the older C-version as well that allows break through. To keep the order true, ollie 
 * This rule is specifically for the c-style switch statements
 */
static cfg_result_package_t visit_c_style_switch_statement(generic_ast_node_t* root_node){
	//Declare and initialize off the bat
	cfg_result_package_t result_package = {NULL, NULL, NULL, BLANK};

	//Th starting and ending blocks for the switch statements
	basic_block_t* root_level_block = basic_block_alloc_and_estimate();
	//The upper bound check block
	basic_block_t* upper_bound_check_block = basic_block_alloc_and_estimate();
	//The jump calculation block
	basic_block_t* jump_calculation_block = basic_block_alloc_and_estimate();
	//Since C-style switches support break statements, we'll need
	//this as well
	basic_block_t* ending_block = basic_block_alloc_and_estimate();

	//The ending block now goes onto the breaking stack
	push(&break_stack, ending_block);

	//We already know what these will be, so populate them
	result_package.starting_block = root_level_block;
	result_package.final_block = ending_block;

	//We'll grab a cursor to the first child and begin crawling through
	generic_ast_node_t* cursor = root_node->first_child;

	//We'll first need to emit the expression node
	cfg_result_package_t input_results = emit_expression(root_level_block, cursor, TRUE, TRUE);

	//Check for ternary expansion
	if(input_results.final_block != root_level_block){
		root_level_block = input_results.final_block;
	}

	//This is a switch type block
	jump_calculation_block->block_type = BLOCK_TYPE_SWITCH;

	//We'll now allocate this one's jump table
	jump_calculation_block->jump_table = jump_table_alloc(root_node->upper_bound - root_node->lower_bound + 1);

	//The offset(amount that we'll need to knock down any case values by) is always the 
	//case statement's value subtracted by the lower bound. We'll call it offset here
	//for consistency
	int32_t offset = root_node->lower_bound;

	//A generic result package for all of our case/default statements
	cfg_result_package_t case_default_results = {NULL, NULL, NULL, BLANK};

	//We will eventually need to know what the default block is,
	//so reserve a variable here
	basic_block_t* default_block = NULL;

	//We'll also need a current block variable for chaining things together
	basic_block_t* current_block = NULL;
	//Keep track of what the previous block was(for fall through)
	basic_block_t* previous_block = NULL;

	//Now we advance to the first real case statement
	cursor = cursor->next_sibling;

	//So long as we haven't hit the end
	while(cursor != NULL){
		switch(cursor->ast_node_type){
			//C-style case statement, we'll let the appropriate rule handle
			case AST_NODE_TYPE_C_STYLE_CASE_STMT:
				//Let the helper rule handle it
				case_default_results = visit_c_style_case_statement(cursor);

				//Add this in as an entry to the jump table
				add_jump_table_entry(jump_calculation_block->jump_table, cursor->constant_value.signed_int_value - offset, case_default_results.starting_block);

				break;

			//C-style default, also let the appropriate rule handle
			case AST_NODE_TYPE_C_STYLE_DEFAULT_STMT:
				//Let the helper rule handle it
				case_default_results = visit_c_style_default_statement(cursor);

				//This is the default block. We'll save this for later when
				//we need to fill in the rest of the jump table
				default_block= case_default_results.starting_block;

				break;

			//Some weird error, this should never happen
			default:
				exit(0);
		}

		//This block counts as a successor to the root level block
		add_successor(jump_calculation_block, case_default_results.starting_block);

		//Reassign current block
		current_block = case_default_results.final_block;

		//If we have a previous block and this one has a non-jump ex
		if(previous_block != NULL) {
			//If the previous block isn't totally empty, we'll check to see if it has
			//an exit statement or not
			if(previous_block->exit_statement != NULL){
				//Switch based on what is in here
				switch(previous_block->exit_statement->statement_type){
					//And of course a return/branch statement means we can't add anything afterwards
					case THREE_ADDR_CODE_BRANCH_STMT:
					case THREE_ADDR_CODE_JUMP_STMT:
					case THREE_ADDR_CODE_RET_STMT:
						break;

					//If we get here though, we either have a conditional jump or some other statement.
					//In this case, to guarantee the fallthrough property, we must
					//add a jump here
					default:
						//Emit the direct jump. This may be optimized away in the optimizer, but we
						//need to guarantee behavior
						emit_jump(previous_block, case_default_results.starting_block);
						
						break;
				}

			//If it is null, then we definitiely need a jump here
			} else {
				//Emit the direct jump. This may be optimized away in the optimizer, but we
				//need to guarantee behavior
				emit_jump(previous_block, case_default_results.starting_block);
			}
		}

		//Now the old previous block is the current block
		previous_block = current_block;

		//Otherwise if we don't satisfy this condition, we don't need to emit any jump at all

		//Advance the cursor to the next one
		cursor = cursor->next_sibling;
	}

	/**
	 * Now we've hit the final block. If this one does not end in a jump or return,
	 * then it needs to be sent to the final block so that we guarantee the fall-through
	 * property
	 */
	if(current_block->exit_statement != NULL){
		//Switch based on what the end of the current block is
		switch(current_block->exit_statement->statement_type){
			//If it's a jump or ret statement, we don't need to add one
			case THREE_ADDR_CODE_RET_STMT:
			case THREE_ADDR_CODE_JUMP_STMT:
				break;

			//However if we have this, we need to ensure that we go from this final block
			//directly to the end
			default:
				//Emit the direct jump. This may be optimized away in the optimizer, but we
				//need to guarantee behavior
				emit_jump(current_block, ending_block);

				break;
		}

	//Otherwise it is null, so we definitely need a jump to the end here
	} else {
		//Emit the direct jump. This may be optimized away in the optimizer, but we
		//need to guarantee behavior
		emit_jump(current_block, ending_block);
	}

	//If the ending block has no successors at all, that means that we've returned through every control path. Instead
	//of using the ending block, we can change it to be the function ending block
	if(ending_block->predecessors.internal_array == NULL || ending_block->predecessors.current_index == 0){
		result_package.final_block = function_exit_block;
	}

	/**
	 * If the default clause is NULL, which it may very well be, we will created
	 * our own dummy default clause that just jumps to the end. This maintains
	 * the intention of the programmer but also allows us to reuse the code
	 * from default blocks
	 */
	if(default_block == NULL){
		//Create it
		default_block = basic_block_alloc_and_estimate();

		//Emit a jump from it to the end block
		emit_jump(default_block, result_package.final_block);
	}

	//Run through the entire jump table. Any nodes that are not occupied(meaning there's no case statement with that value)
	//will be set to point to the default block. 
	for(u_int16_t i = 0; i < jump_calculation_block->jump_table->num_nodes; i++){
		//If it's null, we'll make it the default. This should only happen in switches
		//that are non-exhaustive. For exhaustive switches, the parser has already ensured that we
		//will have a block for every case value
		if(dynamic_array_get_at(&(jump_calculation_block->jump_table->nodes), i) == NULL){
			dynamic_array_set_at(&(jump_calculation_block->jump_table->nodes), default_block, i);
		}
	}

	//Now that everything has been situated, we can start emitting the values in the initial node

	//We'll need both of these as constants for our computation
	three_addr_const_t* lower_bound = emit_direct_integer_or_char_constant(root_node->lower_bound, i32);
	three_addr_const_t* upper_bound = emit_direct_integer_or_char_constant(root_node->upper_bound, i32);

	//Now that we have our expression, we'll want to speed things up by seeing if our value is either below the lower
	//range or above the upper range. If it is, we jump to the very end

	/**
	 * Jumping(conditional or indirect), does not affect condition codes. As such, we can rely 
	 * on the condition codes being set from the operation to take us through all three
	 * jumps. We will emit a jump if we are: lower, higher or an indirect jump if we
	 * are in the range
	 */

	//Grab the type our for convenience
	generic_type_t* input_result_type = input_results.assignee->type;

	//Grab the signedness of the result
	u_int8_t is_signed = is_type_signed(input_result_type);

	//This will be used for tracking
	three_addr_var_t* lower_than_decider = emit_temp_var(input_result_type);

	//Let's first do our lower than comparison
	//First step -> if we're below the minimum, we jump to default 
	emit_binary_operation_with_constant(root_level_block, lower_than_decider, input_results.assignee, L_THAN, lower_bound, TRUE);

	//Select a branch for the lower type
	branch_type_t branch_lower_than = select_appropriate_branch_statement(L_THAN, BRANCH_CATEGORY_NORMAL, is_signed);

	/**
	 * Now we'll emit the branch like this:
	 *
	 * if lower than:
	 * 	goto default block 
	 * else:
	 * 	goto upper_bound_check
	 */
	emit_branch(root_level_block, default_block, upper_bound_check_block, branch_lower_than, lower_than_decider, BRANCH_CATEGORY_NORMAL);

	//This will be used for tracking
	three_addr_var_t* higher_than_decider = emit_temp_var(input_result_type);

	//Now we handle the case where we're above the upper bound
	emit_binary_operation_with_constant(upper_bound_check_block, higher_than_decider, input_results.assignee, G_THAN, upper_bound, TRUE);

	//Select a branch for the higher type
	branch_type_t branch_greater_than = select_appropriate_branch_statement(G_THAN, BRANCH_CATEGORY_NORMAL, is_signed);

	/**
	 * Now we'll emit the branch like this
	 *
	 * if greater than:
	 * 	goto default block
	 * else:
	 *  goto jump block calculation
	 */
	emit_branch(upper_bound_check_block, default_block, jump_calculation_block, branch_greater_than, higher_than_decider, BRANCH_CATEGORY_NORMAL);

	//To avoid violating SSA rules, we'll emit a temporary assignment here
	instruction_t* temporary_variable_assignent = emit_assignment_instruction(emit_temp_var(input_result_type), input_results.assignee);

	//This has now been used once more
	add_used_variable(root_level_block, input_results.assignee);

	//Add it into the block
	add_statement(jump_calculation_block, temporary_variable_assignent);

	//Now that all this is done, we can use our jump table for the rest
	//We'll now need to cut the value down by whatever our offset was	
	three_addr_var_t* input = emit_binary_operation_with_constant(jump_calculation_block, temporary_variable_assignent->assignee, temporary_variable_assignent->assignee, MINUS, emit_direct_integer_or_char_constant(offset, i32), TRUE);

	/**
	 * Now that we've subtracted, we'll need to do the address calculation. The address calculation is as follows:
	 * 	base address(.JT1) + input * 8 
	 *
	 * We have a special kind of statement for doing this
	 * 	
	 */
	//Emit the address first
	three_addr_var_t* address = emit_indirect_jump_address_calculation(jump_calculation_block, jump_calculation_block->jump_table, input, TRUE);

	//Now we'll emit the indirect jump to the address
	emit_indirect_jump(jump_calculation_block, address, TRUE);

	//Give back the starting block
	return result_package;
}


/**
 * Visit a switch statement. In Ollie's current implementation, 
 * the values here will not be reordered at all. Instead, they
 * will be put in the exact orientation that the user wants
 */
static cfg_result_package_t visit_switch_statement(generic_ast_node_t* root_node){
	//Declare the result package off the bat
	cfg_result_package_t result_package = {NULL, NULL, NULL, BLANK};

	//The starting block for the switch statement - we'll want this in a new
	//block
	basic_block_t* root_level_block = basic_block_alloc_and_estimate();
	//We will need to new blocks to check the bounds
	basic_block_t* upper_bound_check_block = basic_block_alloc_and_estimate();
	//This is the block where the actual jump calculation happens
	basic_block_t* jump_calculation_block = basic_block_alloc_and_estimate();
	//We also need to know the ending block here
	basic_block_t* ending_block = basic_block_alloc_and_estimate();

	//We can already fill in the result package
	result_package.starting_block = root_level_block;
	result_package.final_block = ending_block;

	//Grab a cursor to the case statements
	generic_ast_node_t* case_stmt_cursor = root_node->first_child;
	
	//Keep a reference to whatever the current switch statement block is
	basic_block_t* current_block;
	basic_block_t* default_block = NULL;
	
	//Let's first emit the expression. This will at least give us an assignee to work with
	cfg_result_package_t input_results = emit_expression(root_level_block, case_stmt_cursor, TRUE, TRUE);

	//We could have had a ternary here, so we'll need to account for that possibility
	if(root_level_block != input_results.final_block){
		//Just reassign what current is
		root_level_block = input_results.final_block;
	}

	//IMPORTANT - we'll also mark this as a block type switch, because this is where any/all switching logic
	//will be happening
	jump_calculation_block->block_type = BLOCK_TYPE_SWITCH;
	
	//Let's also allocate our jump table. We know how large the jump table needs to be from
	//data passed in by the parser
	jump_calculation_block->jump_table = jump_table_alloc(root_node->upper_bound - root_node->lower_bound + 1);

	//We'll also have some adjustment amount, since we always want the lowest value in the jump table to be 0. This
	//adjustment will be subtracted from every value at the top to "knock it down" to be within the jump table
	int32_t offset = root_node->lower_bound;

	//Wipe this out here just in case
	cfg_result_package_t case_default_results = {NULL, NULL, NULL, BLANK};

	//Get to the next statement. This is the first actual case 
	//statement
	case_stmt_cursor = case_stmt_cursor->next_sibling;

	//So long as this isn't null
	while(case_stmt_cursor != NULL){
		//Switch based on the class
		switch(case_stmt_cursor->ast_node_type){
			//Handle a case statement
			case AST_NODE_TYPE_CASE_STMT:
				//Visit our case stmt here
				case_default_results = visit_case_statement(case_stmt_cursor);

				//We'll now need to add this into the jump table. We always subtract the adjustment to ensure
				//that we start down at 0 as the lowest value
				add_jump_table_entry(jump_calculation_block->jump_table, case_stmt_cursor->constant_value.signed_int_value - offset, case_default_results.starting_block);
				break;

			//Handle a default statement
			case AST_NODE_TYPE_DEFAULT_STMT:
				//Visit the default statement
				case_default_results = visit_default_statement(case_stmt_cursor);

				//This is the default block, so save for now
				default_block = case_default_results.starting_block;

				break;

			//Otherwise we have some weird error, so we'll fail out
			default:
				exit(0);
		}

		//The starting block is a successor to the root block
		add_successor(jump_calculation_block, case_default_results.starting_block);

		//Now we'll drill down to the bottom to prime the next pass
		current_block = case_default_results.final_block;

		//If we don't have a return terminal type, we can add the ending block as a successor
		if(current_block->block_terminal_type != BLOCK_TERM_TYPE_RET){
			//We will always emit a direct jump from this block to the ending block
			emit_jump(current_block, ending_block);
		}
		
		//Move the cursor up
		case_stmt_cursor = case_stmt_cursor->next_sibling;
	}

	/**
	 * It is entirely possible that we have no default block here. In that case, we will
	 * make our own "dummy" default block that simply breaks to end and has no effect. This
	 * will preserve the intention of the programmer and keep our flow simpler here
	 */
	if(default_block == NULL){
		//Create it
		default_block = basic_block_alloc_and_estimate();

		//All that this block has is a direct jump to the end
		emit_jump(default_block, ending_block);
	}

	//Now at the ever end, we'll need to fill the remaining jump table blocks that are empty
	//with the default value
	for(u_int16_t _ = 0; _ < jump_calculation_block->jump_table->num_nodes; _++){
		//If it's null, we'll make it the default
		if(dynamic_array_get_at(&(jump_calculation_block->jump_table->nodes), _) == NULL){
			dynamic_array_set_at(&(jump_calculation_block->jump_table->nodes), default_block, _);
		}
	}

	//If we have no predecessors, that means that every case statement ended in a return statement.
	//If this is the case, then the final block should not be the ending block, it should be the function ending block
	if(ending_block->predecessors.internal_array == NULL || ending_block->predecessors.current_index == 0){
		result_package.final_block = function_exit_block;
	}

	//Now that everything has been situated, we can start emitting the values in the initial node

	//We'll need both of these as constants for our computation
	three_addr_const_t* lower_bound = emit_direct_integer_or_char_constant(root_node->lower_bound, i32);
	three_addr_const_t* upper_bound = emit_direct_integer_or_char_constant(root_node->upper_bound, i32);

	//Now that we have our expression, we'll want to speed things up by seeing if our value is either below the lower
	//range or above the upper range. If it is, we jump to the very end

	/**
	 * Jumping(conditional or indirect), does not affect condition codes. As such, we can rely 
	 * on the condition codes being set from the operation to take us through all three
	 * jumps. We will emit a jump if we are: lower, higher or an indirect jump if we
	 * are in the range
	 */

	//Grab the type our for convenience
	generic_type_t* input_result_type = input_results.assignee->type;

	//Grab the signedness of the result
	u_int8_t is_signed = is_type_signed(input_result_type);

	//This will be used for tracking
	three_addr_var_t* lower_than_decider = emit_temp_var(input_result_type);

	//Let's first do our lower than comparison
	//First step -> if we're below the minimum, we jump to default 
	emit_binary_operation_with_constant(root_level_block, lower_than_decider, input_results.assignee, L_THAN, lower_bound, TRUE);

	//Select a branch for the lower type
	branch_type_t branch_lower_than = select_appropriate_branch_statement(L_THAN, BRANCH_CATEGORY_NORMAL, is_signed);

	/**
	 * Now we'll emit the branch like this:
	 *
	 * if lower than:
	 * 	goto default block 
	 * else:
	 * 	goto upper_bound_check
	 */
	emit_branch(root_level_block, default_block, upper_bound_check_block, branch_lower_than, lower_than_decider, BRANCH_CATEGORY_NORMAL);

	//This will be used for tracking
	three_addr_var_t* higher_than_decider = emit_temp_var(input_result_type);

	//Now we handle the case where we're above the upper bound
	emit_binary_operation_with_constant(upper_bound_check_block, higher_than_decider, input_results.assignee, G_THAN, upper_bound, TRUE);

	//Select a branch for the higher type
	branch_type_t branch_greater_than = select_appropriate_branch_statement(G_THAN, BRANCH_CATEGORY_NORMAL, is_signed);

	/**
	 * Now we'll emit the branch like this
	 *
	 * if greater than:
	 * 	goto default block
	 * else:
	 *  goto jump block calculation
	 */
	emit_branch(upper_bound_check_block, default_block, jump_calculation_block, branch_greater_than, higher_than_decider, BRANCH_CATEGORY_NORMAL);

	//To avoid violating SSA rules, we'll emit a temporary assignment here
	instruction_t* temporary_variable_assignent = emit_assignment_instruction(emit_temp_var(input_result_type), input_results.assignee);

	//This has now been used once more
	add_used_variable(root_level_block, input_results.assignee);

	//Add it into the block
	add_statement(jump_calculation_block, temporary_variable_assignent);

	//Now that all this is done, we can use our jump table for the rest
	//We'll now need to cut the value down by whatever our offset was	
	three_addr_var_t* input = emit_binary_operation_with_constant(jump_calculation_block, temporary_variable_assignent->assignee, temporary_variable_assignent->assignee, MINUS, emit_direct_integer_or_char_constant(offset, i32), TRUE);

	/**
	 * Now that we've subtracted, we'll need to do the address calculation. The address calculation is as follows:
	 * 	base address(.JT1) + input * 8 
	 *
	 * We have a special kind of statement for doing this
	 * 	
	 */
	//Emit the address first
	three_addr_var_t* address = emit_indirect_jump_address_calculation(jump_calculation_block, jump_calculation_block->jump_table, input, TRUE);

	//Now we'll emit the indirect jump to the address
	emit_indirect_jump(jump_calculation_block, address, TRUE);

	//Give back the starting block
	return result_package;
}


/**
 * Visit a sequence of statements one after the other. This is used for C-style case and default
 * statement processing. Note that unlike a compound statement, there is no new lexical scope
 * initialized, and we never descend the tree. We only go from sibling to sibling
 */
static cfg_result_package_t visit_statement_chain(generic_ast_node_t* first_node){
	//A generic results package that we can use in any of our processing
	cfg_result_package_t generic_results;
	//A defer statement cursor
	generic_ast_node_t* defer_statement_cursor;

	//The global starting block
	basic_block_t* starting_block = NULL;
	//The current block
	basic_block_t* current_block = starting_block;
	//A holder for labeled blocks
	basic_block_t* labeled_block = NULL;

	//Grab our very first thing here
	generic_ast_node_t* ast_cursor = first_node;
	
	//Roll through the entire subtree
	while(ast_cursor != NULL){
		//Using switch/case for the efficiency gain
		switch(ast_cursor->ast_node_type){
			case AST_NODE_TYPE_DECL_STMT:
				//Let the helper rule handle it
				visit_declaration_statement(ast_cursor);
				break;

			case AST_NODE_TYPE_LET_STMT:
				//We'll visit the block here
				generic_results = visit_let_statement(ast_cursor, FALSE);

				//If this is not null, then we're just adding onto something
				if(starting_block != NULL){
					//Merge the two together
					current_block = merge_blocks(current_block, generic_results.starting_block); 

					//If these are not equal, we can reassign the current block to be the final block
					if(generic_results.starting_block != generic_results.final_block){
						current_block = generic_results.final_block;
					}

				//Otherwise this is the very first thing
				} else {
					starting_block = generic_results.starting_block;
					current_block = generic_results.final_block;
				}

				break;

			case AST_NODE_TYPE_RET_STMT:
				//If for whatever reason the block is null, we'll create it
				if(starting_block == NULL){
					//We assume that this only happens once
					starting_block = basic_block_alloc_and_estimate();
					current_block = starting_block;
				}

				//Emit the return statement, let the sub rule handle
			 	generic_results = emit_return(current_block, ast_cursor, FALSE);

				//If this is the case, it means that we've hit a ternary at some point and need
				//to reassign this final block
				if(generic_results.final_block != current_block){
					current_block = generic_results.final_block;
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

				//Package up the values
				generic_results.starting_block = starting_block;
				generic_results.final_block = current_block;
				generic_results.operator = BLANK;
				generic_results.assignee = NULL;

				//We're completely done here
				return generic_results;
		
			case AST_NODE_TYPE_IF_STMT:
				//We'll now enter the if statement
				generic_results = visit_if_statement(ast_cursor);
			
				//Once we have the if statement start, we'll add it in as a successor
				if(starting_block == NULL){
					//The starting block is the first one here
					starting_block = generic_results.starting_block;
					//And the final block is the end
					current_block = generic_results.final_block;
				} else {
					//Emit a jump from current to the start
					emit_jump(current_block, generic_results.starting_block);
					//The current block is just whatever is at the end
					current_block = generic_results.final_block;
				}

				break;

			case AST_NODE_TYPE_WHILE_STMT:
				//Visit the while statement
				generic_results = visit_while_statement(ast_cursor);

				//We'll now add it in
				if(starting_block == NULL){
					starting_block = generic_results.starting_block;
					current_block = generic_results.final_block;
				//We never merge these
				} else {
					//Emit a direct jump to it
					emit_jump(current_block, generic_results.starting_block);
					//And the current block is just the end block
					current_block = generic_results.final_block;
				}
	
				break;

			case AST_NODE_TYPE_DO_WHILE_STMT:
				//Visit the statement
				generic_results = visit_do_while_statement(ast_cursor);

				//We'll now add it in
				if(starting_block == NULL){
					starting_block = generic_results.starting_block;
					current_block = generic_results.final_block;
				//We never merge do-while's, they are strictly successors
				} else {
					//Emit a jump from the current block to this
					emit_jump(current_block, generic_results.starting_block);
					//And we now know that the current block is just the end block
					current_block = generic_results.final_block;
				}

				break;

			case AST_NODE_TYPE_FOR_STMT:
				//First visit the statement
				generic_results = visit_for_statement(ast_cursor);

				//Now we'll add it in
				if(starting_block == NULL){
					starting_block = generic_results.starting_block;
					current_block = generic_results.final_block;
				//We don't merge, we'll add successors
				} else {
					//We go right to the exit block here
					emit_jump(current_block, generic_results.starting_block);
					//Go right to the final block here
					current_block = generic_results.final_block;
				}

				break;

			case AST_NODE_TYPE_CONTINUE_STMT:
				//This could happen where we have nothing here
				if(starting_block == NULL){
					//We'll assume that this only happens once
					starting_block = basic_block_alloc_and_estimate();
					current_block = starting_block;
				}

				//There are two options here. We could see a regular continue or a conditional
				//continue. If the child is null, then it is a regular continue
				if(ast_cursor->first_child == NULL){
					//Mark this for later
					current_block->block_terminal_type = BLOCK_TERM_TYPE_CONTINUE;

					//Peek the continue block off of the stack
					basic_block_t* continuing_to = peek(&continue_stack);

					//We always jump to the start of the loop statement unconditionally
					emit_jump(current_block, continuing_to);

					//Package and return
					generic_results = (cfg_result_package_t){starting_block, current_block, NULL, BLANK};

					//We're done here, so return the starting block. There is no 
					//point in going on
					return generic_results;

				//Otherwise, we have a conditional continue here
				} else {
					//Emit the expression code into the current statement
					cfg_result_package_t package = emit_expression(current_block, ast_cursor->first_child, TRUE, TRUE);

					//Store for later
					three_addr_var_t* conditional_decider = package.assignee;

					//If this is blank, we'll need a test instruction
					if(package.operator == BLANK){
						conditional_decider = emit_test_code(current_block, package.assignee, package.assignee, TRUE);
					}

					//We'll need a new block here - this will count as a branch
					basic_block_t* new_block = basic_block_alloc_and_estimate();

					//Peek the continue block off of the stack
					basic_block_t* continuing_to = peek(&continue_stack);

					//Select the appropriate branch type using
					//a normal jump
					branch_type_t branch_type = select_appropriate_branch_statement(package.operator, BRANCH_CATEGORY_NORMAL, is_type_signed(conditional_decider->type));

					/**
					 * Now we will emit the branch like so
					 *
					 * if condition:
					 * 	goto continue_block
					 * else:
					 * 	goto new block
					 */
					emit_branch(current_block, continuing_to, new_block, branch_type, conditional_decider, BRANCH_CATEGORY_NORMAL);

					//And as we go forward, this new block will be the current block
					current_block = new_block;
				}

				break;

			case AST_NODE_TYPE_BREAK_STMT:
				//This could happen where we have nothing here
				if(starting_block == NULL){
					starting_block = basic_block_alloc_and_estimate();
					current_block = starting_block;
				}

				//There are two options here: We could have a conditional break
				//or a normal break. If there is no child node, we have a normal break
				if(ast_cursor->first_child == NULL){
					//Mark this for later
					current_block->block_terminal_type = BLOCK_TERM_TYPE_BREAK;

					//Peak off of the break stack to get what we're breaking to
					basic_block_t* breaking_to = peek(&break_stack);

					//We will jump to it -- this is always an uncoditional jump
					emit_jump(current_block, breaking_to);

					//Package and return
					generic_results = (cfg_result_package_t){starting_block, current_block, NULL, BLANK};

					//For a regular break statement, this is it, so we just get out
					//Give back the starting block
					return generic_results;

				//Otherwise, we have a conditional break, which will generate a conditional jump instruction
				} else {
					//We'll also need a new block to jump to, since this is a conditional break
					basic_block_t* new_block = basic_block_alloc_and_estimate();

					//First let's emit the conditional code
					cfg_result_package_t ret_package = emit_expression(current_block, ast_cursor->first_child, TRUE, TRUE);

					//Store this for later
					three_addr_var_t* conditional_decider = ret_package.assignee;

					//If this is blank, we'll need a test instruction
					if(ret_package.operator == BLANK){
						conditional_decider = emit_test_code(current_block, ret_package.assignee, ret_package.assignee, TRUE);
					}

					//First we'll select the appropriate branch type. We are using a regular branch type here
					branch_type_t branch_type = select_appropriate_branch_statement(ret_package.operator, BRANCH_CATEGORY_NORMAL, is_type_signed(conditional_decider->type));

					//Peak off of the break stack to get what we're breaking to
					basic_block_t* breaking_to = peek(&break_stack);

					/**
					 * Now we'll emit the branch like so:
					 *
					 * if conditional
					 * 	goto end block
					 * else 
					 * 	goto new block
					 */
					emit_branch(current_block, breaking_to, new_block, branch_type, conditional_decider, BRANCH_CATEGORY_NORMAL);

					//Once we're out here, the current block is now the new one
					current_block = new_block;
				}

				break;

			case AST_NODE_TYPE_DEFER_STMT:
				//Grab a cursor here
				defer_statement_cursor = ast_cursor->first_child;

				//So long as this cursor is not null, we'll keep processing and adding
				//compound statements
				while(defer_statement_cursor != NULL){
					//Let the helper process this
					cfg_result_package_t compound_statement_results = visit_compound_statement(defer_statement_cursor);

					//The successor to the current block is this block
					//If it's null then this is this block
					if(starting_block == NULL){
						starting_block = compound_statement_results.starting_block;
					} else {
						//Jump to it - important for optimizer
						emit_jump(current_block, compound_statement_results.starting_block);
					}

					//Current is now the end of the compound statement
					current_block = compound_statement_results.final_block;

					//Advance this to the next one
					defer_statement_cursor = defer_statement_cursor->next_sibling;
				}

				break;

			//Label statements are unique because they'll force the creation of a new block with a
			//given label name
			case AST_NODE_TYPE_LABEL_STMT:
				//Allocate the label statement as the current block
				labeled_block = labeled_block_alloc(ast_cursor->variable);

				//Add this into the current function's labeled blocks
				dynamic_array_add(&current_function_labeled_blocks, labeled_block);

				//If the starting block is empty, then this is the starting block
				if(starting_block == NULL){
					starting_block = labeled_block;
				//Otherwise we'll need to emit a jump to it
				} else {
					//Add it in as a successor
					emit_jump(current_block, labeled_block);
				}

				//The current block now is this labeled block
				current_block = labeled_block;

				break;
		
			//A conditional user-defined jump works somewhat like a break
			case AST_NODE_TYPE_CONDITIONAL_JUMP_STMT:
				//This really shouldn't happen, but it can't hurt
				if(starting_block == NULL){
					starting_block = basic_block_alloc_and_estimate();
					current_block = starting_block;
				}

				//The second child here should be our conditional
				generic_ast_node_t* cursor = ast_cursor->first_child;
				cursor = cursor->next_sibling;

				//We'll need to emit the conditional in the current block
				cfg_result_package_t ret_package = emit_expression(current_block, cursor, TRUE, TRUE);

				//We'll now update the current block accordingly
				if(ret_package.final_block != current_block){
					current_block = ret_package.final_block;
				}

				//We'll need a block at the very end which we'll hit after we jump
				basic_block_t* else_block = basic_block_alloc_and_estimate();

				//Save this here for later
				three_addr_var_t* conditional_decider = ret_package.assignee;

				//If the return package's operator is blank,
				//then we'll need to emit a test instruction here
				if(ret_package.operator == BLANK){
					conditional_decider = emit_test_code(current_block, ret_package.assignee, ret_package.assignee, TRUE);
				}

				//Select the needed branch statement
				branch_type_t branch_type = select_appropriate_branch_statement(ret_package.operator, BRANCH_CATEGORY_NORMAL, is_type_signed(conditional_decider->type));

				//Now we can emit the branch itself. The branch itself is incomplete right now, there is a special postprocessor
				//method for each function that handles filling the rest in
				emit_user_defined_branch(current_block, ast_cursor->variable, else_block, conditional_decider, branch_type);

				//The current block now is said jumping to block
				current_block = else_block;

				break;

			case AST_NODE_TYPE_SWITCH_STMT:
				//Visit the switch statement
				generic_results = visit_switch_statement(ast_cursor);

				//If the starting block is NULL, then this is the starting block. Otherwise, it's the 
				//starting block's direct successor
				if(starting_block == NULL){
					starting_block = generic_results.starting_block;
				} else {
					//We will also emit a jump from the current block to the entry
					emit_jump(current_block, generic_results.starting_block);
				}

				//The current block is always what's directly at the end
				current_block = generic_results.final_block;

				break;

			case AST_NODE_TYPE_C_STYLE_SWITCH_STMT:
				//Visit the switch statement
				generic_results = visit_c_style_switch_statement(ast_cursor);

				//If the starting block is NULL, then this is the starting block. Otherwise, it's the 
				//starting block's direct successor
				if(starting_block == NULL){
					starting_block = generic_results.starting_block;
				} else {
					//We will also emit a jump from the current block to the entry
					emit_jump(current_block, generic_results.starting_block);
				}

				//The current block is always what's directly at the end
				current_block = generic_results.final_block;

				break;

			case AST_NODE_TYPE_COMPOUND_STMT:
				//We'll simply recall this function and let it handle it
				generic_results = visit_compound_statement(ast_cursor);

				//Add in everything appropriately here
				if(starting_block == NULL){
					starting_block = generic_results.starting_block;
				} else {
					add_successor(current_block, generic_results.starting_block);
				}

				//Current is just the end of this block
				current_block = generic_results.final_block;

				break;
		

			case AST_NODE_TYPE_ASM_INLINE_STMT:
				//If we find an assembly inline statement, the actuality of it is
				//incredibly easy. All that we need to do is literally take the 
				//user's statement and insert it into the code

				//We'll need a new block here regardless
				if(starting_block == NULL){
					starting_block = basic_block_alloc_and_estimate();
					current_block = starting_block;
				}

				//Let the helper handle
				emit_assembly_inline(current_block, ast_cursor, FALSE);
			
				break;

			case AST_NODE_TYPE_IDLE_STMT:
				//Do we need a new block?
				if(starting_block == NULL){
					starting_block = basic_block_alloc_and_estimate();
					current_block = starting_block;
				}

				//Let the helper handle -- doesn't even need the cursor
				emit_idle(current_block, FALSE);
				
				break;

			//This means that we have some kind of expression statement
			default:
				//This could happen where we have nothing here
				if(starting_block == NULL){
					starting_block = basic_block_alloc_and_estimate();
					current_block = starting_block;
				}
				
				//Also emit the simplified machine code
				emit_expression(current_block, ast_cursor, FALSE, FALSE);
				
				break;
		}

		//If this is the exit block, it means that we returned through every control path
		//in here and there is no point in moving forward. We'll simply return
		if(current_block == function_exit_block){
			//Warn that we have unreachable code here
			if(ast_cursor->next_sibling != NULL){
				print_cfg_message(WARNING, "Unreachable code detected after segment that returns in all control paths", ast_cursor->next_sibling->line_number);
			}

			break;
		}

		//Advance to the next child
		ast_cursor = ast_cursor->next_sibling;
	}

	//If we make it down here - we still need to ensure that results are packaged properly
	generic_results.starting_block = starting_block;
	generic_results.final_block = current_block;

	//Give back results
	return generic_results;
}


/**
 * A compound statement also acts as a sort of multiplexing block. It runs through all of it's statements, calling
 * the appropriate functions and making the appropriate additions
 *
 * We make use of the "direct successor" nodes as a direct path through the compound statement, if such a path exists
 */
static cfg_result_package_t visit_compound_statement(generic_ast_node_t* root_node){
	//Everything to begin with is completely null'd out
	cfg_result_package_t results = {NULL, NULL, NULL, BLANK};
	//A generic results package that we can use in any of our processing
	cfg_result_package_t generic_results;
	//A defer statement cursor
	generic_ast_node_t* defer_statement_cursor;

	//The global starting block
	basic_block_t* starting_block = NULL;
	//The current block
	basic_block_t* current_block = starting_block;
	//A holder that we'll use for labeled statements
	basic_block_t* labeled_block = NULL;

	//Grab the initial node
	generic_ast_node_t* compound_stmt_node = root_node;

	//Grab our very first thing here
	generic_ast_node_t* ast_cursor = compound_stmt_node->first_child;
	
	//Roll through the entire subtree
	while(ast_cursor != NULL){
		//Using switch/case for the efficiency gain
		switch(ast_cursor->ast_node_type){
			case AST_NODE_TYPE_DECL_STMT:
				//Let the helper rule handle it
				visit_declaration_statement(ast_cursor);
				break;

			case AST_NODE_TYPE_LET_STMT:
				//We'll visit the block here
				generic_results = visit_let_statement(ast_cursor, FALSE);

				//If this is not null, then we're just adding onto something
				if(starting_block != NULL){
					//Merge the two together
					current_block = merge_blocks(current_block, generic_results.starting_block); 

					//If these are not equal, we can reassign the current block to be the final block
					if(generic_results.starting_block != generic_results.final_block){
						current_block = generic_results.final_block;
					}

				//Otherwise this is the very first thing
				} else {
					starting_block = generic_results.starting_block;
					current_block = generic_results.final_block;
				}

				break;

			case AST_NODE_TYPE_RET_STMT:
				//If for whatever reason the block is null, we'll create it
				if(starting_block == NULL){
					//We assume that this only happens once
					starting_block = basic_block_alloc_and_estimate();
					current_block = starting_block;
				}

				//Emit the return statement, let the sub rule handle
			 	generic_results = emit_return(current_block, ast_cursor, FALSE);

				//If this is the case, it means that we've hit a ternary at some point and need
				//to reassign this final block
				if(generic_results.final_block != current_block){
					current_block = generic_results.final_block;
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

				//Package up the values
				results.starting_block = starting_block;
				results.final_block = current_block;
				results.operator = BLANK;
				results.assignee = NULL;

				//We're completely done here
				return results;
		
			case AST_NODE_TYPE_IF_STMT:
				//We'll now enter the if statement
				generic_results = visit_if_statement(ast_cursor);
			
				//Once we have the if statement start, we'll add it in as a successor
				if(starting_block == NULL){
					//The starting block is the first one here
					starting_block = generic_results.starting_block;
					//And the final block is the end
					current_block = generic_results.final_block;
				} else {
					//Emit a jump from current to the start
					emit_jump(current_block, generic_results.starting_block);
					//The current block is just whatever is at the end
					current_block = generic_results.final_block;
				}

				break;

			case AST_NODE_TYPE_WHILE_STMT:
				//Visit the while statement
				generic_results = visit_while_statement(ast_cursor);

				//We'll now add it in
				if(starting_block == NULL){
					starting_block = generic_results.starting_block;
					current_block = generic_results.final_block;
				//We never merge these
				} else {
					//Emit a direct jump to it
					emit_jump(current_block, generic_results.starting_block);
					//And the current block is just the end block
					current_block = generic_results.final_block;
				}
	
				break;

			case AST_NODE_TYPE_DO_WHILE_STMT:
				//Visit the statement
				generic_results = visit_do_while_statement(ast_cursor);

				//We'll now add it in
				if(starting_block == NULL){
					starting_block = generic_results.starting_block;
					current_block = generic_results.final_block;
				//We never merge do-while's, they are strictly successors
				} else {
					//Emit a jump from the current block to this
					emit_jump(current_block, generic_results.starting_block);
					//And we now know that the current block is just the end block
					current_block = generic_results.final_block;
				}

				break;

			case AST_NODE_TYPE_FOR_STMT:
				//First visit the statement
				generic_results = visit_for_statement(ast_cursor);

				//Now we'll add it in
				if(starting_block == NULL){
					starting_block = generic_results.starting_block;
					current_block = generic_results.final_block;
				//We don't merge, we'll add successors
				} else {
					//We go right to the exit block here
					emit_jump(current_block, generic_results.starting_block);
					//Go right to the final block here
					current_block = generic_results.final_block;
				}

				break;

			case AST_NODE_TYPE_CONTINUE_STMT:
				//This could happen where we have nothing here
				if(starting_block == NULL){
					//We'll assume that this only happens once
					starting_block = basic_block_alloc_and_estimate();
					current_block = starting_block;
				}

				//There are two options here. We could see a regular continue or a conditional
				//continue. If the child is null, then it is a regular continue
				if(ast_cursor->first_child == NULL){
					//Mark this for later
					current_block->block_terminal_type = BLOCK_TERM_TYPE_CONTINUE;

					//Peek the continue block off of the stack
					basic_block_t* continuing_to = peek(&continue_stack);

					//We always jump to the start of the loop statement unconditionally
					emit_jump(current_block, continuing_to);

					//Package and return
					results = (cfg_result_package_t){starting_block, current_block, NULL, BLANK};

					//We're done here, so return the starting block. There is no 
					//point in going on
					return results;

				//Otherwise, we have a conditional continue here
				} else {
					//Emit the expression code into the current statement
					cfg_result_package_t package = emit_expression(current_block, ast_cursor->first_child, TRUE, TRUE);

					//Store for later
					three_addr_var_t* conditional_decider = package.assignee;

					//If this is blank, we'll need a test instruction
					if(package.operator == BLANK){
						conditional_decider = emit_test_code(current_block, package.assignee, package.assignee, TRUE);
					}

					//We'll need a new block here - this will count as a branch
					basic_block_t* new_block = basic_block_alloc_and_estimate();

					//Peek the continue block off of the stack
					basic_block_t* continuing_to = peek(&continue_stack);

					//Select the appropriate branch type, we will not use an inverse jump here
					branch_type_t branch_type = select_appropriate_branch_statement(package.operator, BRANCH_CATEGORY_NORMAL, is_type_signed(conditional_decider->type));

					/**
					 * Now we will emit the branch like so
					 *
					 * if condition:
					 * 	goto continue_block
					 * else:
					 * 	goto new block
					 */
					emit_branch(current_block, continuing_to, new_block, branch_type, conditional_decider, BRANCH_CATEGORY_NORMAL);

					//And as we go forward, this new block will be the current block
					current_block = new_block;
				}

				break;

			case AST_NODE_TYPE_BREAK_STMT:
				//This could happen where we have nothing here
				if(starting_block == NULL){
					starting_block = basic_block_alloc_and_estimate();
					current_block = starting_block;
				}

				//There are two options here: We could have a conditional break
				//or a normal break. If there is no child node, we have a normal break
				if(ast_cursor->first_child == NULL){
					//Mark this for later
					current_block->block_terminal_type = BLOCK_TERM_TYPE_BREAK;

					//Peak off of the break stack to get what we're breaking to
					basic_block_t* breaking_to = peek(&break_stack);

					//We will jump to it -- this is always an uncoditional jump
					emit_jump(current_block, breaking_to);

					//Package and return
					results = (cfg_result_package_t){starting_block, current_block, NULL, BLANK};

					//For a regular break statement, this is it, so we just get out
					//Give back the starting block
					return results;

				//Otherwise, we have a conditional break, which will generate a conditional jump instruction
				} else {
					//We'll also need a new block to jump to, since this is a conditional break
					basic_block_t* new_block = basic_block_alloc_and_estimate();

					//First let's emit the conditional code
					cfg_result_package_t ret_package = emit_expression(current_block, ast_cursor->first_child, TRUE, TRUE);

					//Store this for later
					three_addr_var_t* conditional_decider = ret_package.assignee;

					//If this is blank, we'll need a test instruction
					if(ret_package.operator == BLANK){
						conditional_decider = emit_test_code(current_block, ret_package.assignee, ret_package.assignee, TRUE);
					}

					//First we'll select the appropriate branch type. We are using a regular branch type here
					branch_type_t branch_type = select_appropriate_branch_statement(ret_package.operator, BRANCH_CATEGORY_NORMAL, is_type_signed(conditional_decider->type));

					//Peak off of the break stack to get what we're breaking to
					basic_block_t* breaking_to = peek(&break_stack);

					/**
					 * Now we'll emit the branch like so:
					 *
					 * if conditional
					 * 	goto end block
					 * else 
					 * 	goto new block
					 */
					emit_branch(current_block, breaking_to, new_block, branch_type, conditional_decider, BRANCH_CATEGORY_NORMAL);

					//Once we're out here, the current block is now the new one
					current_block = new_block;
				}

				break;

			case AST_NODE_TYPE_DEFER_STMT:
				//Grab a cursor here
				defer_statement_cursor = ast_cursor->first_child;

				//So long as this cursor is not null, we'll keep processing and adding
				//compound statements
				while(defer_statement_cursor != NULL){
					//Let the helper process this
					cfg_result_package_t compound_statement_results = visit_compound_statement(defer_statement_cursor);

					//The successor to the current block is this block
					//If it's null then this is this block
					if(starting_block == NULL){
						starting_block = compound_statement_results.starting_block;
					} else {
						//Jump to it - important for optimizer
						emit_jump(current_block, compound_statement_results.starting_block);
					}

					//Current is now the end of the compound statement
					current_block = compound_statement_results.final_block;

					//Advance this to the next one
					defer_statement_cursor = defer_statement_cursor->next_sibling;
				}

				break;

			/**
			 * A label statement is special because it creates an entirely
			 * separate basic block. This is done to maintain the rule
			 * that we have exactly one entry point to each and every block.
			 * Since a label statement will be jumped to, we need to have it 
			 * as the start of a separate block
			 */
			case AST_NODE_TYPE_LABEL_STMT:
				//Allocate the label statement as the current block
				labeled_block = labeled_block_alloc(ast_cursor->variable);

				//Add this into the current function's labeled blocks
				dynamic_array_add(&current_function_labeled_blocks, labeled_block);

				//If the starting block is empty, then this is the starting block
				if(starting_block == NULL){
					starting_block = labeled_block;
				//Otherwise we'll need to emit a jump to it
				} else {
					emit_jump(current_block, labeled_block);
				}

				//The current block now is this labeled block
				current_block = labeled_block;

				break;

			//A conditional user-defined jump works somewhat like a break
			case AST_NODE_TYPE_CONDITIONAL_JUMP_STMT:
				//This really shouldn't happen, but it can't hurt
				if(starting_block == NULL){
					starting_block = basic_block_alloc_and_estimate();
					current_block = starting_block;
				}

				//The second child here should be our conditional
				generic_ast_node_t* cursor = ast_cursor->first_child;
				cursor = cursor->next_sibling;

				//We'll need to emit the conditional in the current block
				cfg_result_package_t ret_package = emit_expression(current_block, cursor, TRUE, TRUE);

				//We'll now update the current block accordingly
				if(ret_package.final_block != current_block){
					current_block = ret_package.final_block;
				}

				//We'll need a block at the very end which we'll hit after we jump
				basic_block_t* else_block = basic_block_alloc_and_estimate();

				//Save this here for later
				three_addr_var_t* conditional_decider = ret_package.assignee;

				//If the return package's operator is blank,
				//then we'll need to emit a test instruction here
				if(ret_package.operator == BLANK){
					conditional_decider = emit_test_code(current_block, ret_package.assignee, ret_package.assignee, TRUE);
				}

				//Select the needed branch statement
				branch_type_t branch_type = select_appropriate_branch_statement(ret_package.operator, BRANCH_CATEGORY_NORMAL, is_type_signed(conditional_decider->type));

				//Now we can emit the branch itself. The branch itself is incomplete right now, there is a special postprocessor
				//method for each function that handles filling the rest in
				emit_user_defined_branch(current_block, ast_cursor->variable, else_block, conditional_decider, branch_type);

				//The current block now is said jumping to block
				current_block = else_block;

				break;

			case AST_NODE_TYPE_SWITCH_STMT:
				//Visit the switch statement
				generic_results = visit_switch_statement(ast_cursor);

				//If the starting block is NULL, then this is the starting block. Otherwise, it's the 
				//starting block's direct successor
				if(starting_block == NULL){
					starting_block = generic_results.starting_block;
				} else {
					//We will also emit a jump from the current block to the entry
					emit_jump(current_block, generic_results.starting_block);
				}

				//The current block is always what's directly at the end
				current_block = generic_results.final_block;

				break;

			case AST_NODE_TYPE_C_STYLE_SWITCH_STMT:
				//Visit the switch statement
				generic_results = visit_c_style_switch_statement(ast_cursor);

				//If the starting block is NULL, then this is the starting block. Otherwise, it's the 
				//starting block's direct successor
				if(starting_block == NULL){
					starting_block = generic_results.starting_block;
				} else {
					//We will also emit a jump from the current block to the entry
					emit_jump(current_block, generic_results.starting_block);
				}

				//The current block is always what's directly at the end
				current_block = generic_results.final_block;

				break;

			case AST_NODE_TYPE_COMPOUND_STMT:
				//We'll simply recall this function and let it handle it
				generic_results = visit_compound_statement(ast_cursor);

				//Add in everything appropriately here
				if(starting_block == NULL){
					starting_block = generic_results.starting_block;
				} else {
					add_successor(current_block, generic_results.starting_block);
				}

				//Current is just the end of this block
				current_block = generic_results.final_block;

				break;
		

			case AST_NODE_TYPE_ASM_INLINE_STMT:
				//If we find an assembly inline statement, the actuality of it is
				//incredibly easy. All that we need to do is literally take the 
				//user's statement and insert it into the code

				//We'll need a new block here regardless
				if(starting_block == NULL){
					starting_block = basic_block_alloc_and_estimate();
					current_block = starting_block;
				}

				//Let the helper handle
				emit_assembly_inline(current_block, ast_cursor, FALSE);
			
				break;

			case AST_NODE_TYPE_IDLE_STMT:
				//Do we need a new block?
				if(starting_block == NULL){
					starting_block = basic_block_alloc_and_estimate();
					current_block = starting_block;
				}

				//Let the helper handle -- doesn't even need the cursor
				emit_idle(current_block, FALSE);
				
				break;

			//This means that we have some kind of expression statement
			default:
				//This could happen where we have nothing here
				if(starting_block == NULL){
					starting_block = basic_block_alloc_and_estimate();
					current_block = starting_block;
				}
				
				//Also emit the simplified machine code
				emit_expression(current_block, ast_cursor, FALSE, FALSE);
				
				break;
		}

		//If this is the exit block, it means that we returned through every control path
		//in here and there is no point in moving forward. We'll simply return
		if(current_block == function_exit_block){
			//Warn that we have unreachable code here
			if(ast_cursor->next_sibling != NULL){
				print_cfg_message(WARNING, "Unreachable code detected after segment that returns in all control paths", ast_cursor->next_sibling->line_number);
			}

			break;
		}

		//Advance to the next child
		ast_cursor = ast_cursor->next_sibling;
	}

	//If we make it down here - we still need to ensure that results are packaged properly
	results.starting_block = starting_block;
	results.final_block = current_block;

	//Give back results
	return results;
}


/**
 * Go through a function end block and determine/insert the ret statements that we need
 *
 * In the event that a given function does not return where it should, a "ret 0" will be used.
 * This is technically undefined behavior, so users will get what they get here
 */
static void determine_and_insert_return_statements(basic_block_t* function_exit_block){
	//For convenience
	symtab_function_record_t* function_defined_in = function_exit_block->function_defined_in;

	//For accurate error printing
	u_int8_t is_main_function = strcmp(function_defined_in->func_name.string, "main") == 0 ? TRUE : FALSE;

	//Run through all of the predecessors
	for(u_int16_t i = 0; i < function_exit_block->predecessors.current_index; i++){
		//Grab the predecessor out
		basic_block_t* block = dynamic_array_get_at(&(function_exit_block->predecessors), i);

		//If the exit statement is not a return statement, we need to know what's happening here
		if(block->exit_statement == NULL || block->exit_statement->statement_type != THREE_ADDR_CODE_RET_STMT){
			//If this isn't void, then we need to throw a warning
			if((function_defined_in->return_type->type_class != TYPE_CLASS_BASIC
				|| function_defined_in->return_type->basic_type_token != VOID)
				//It's a technically supported use-case to not put a return on main
				&& is_main_function == FALSE){
				print_parse_message(WARNING, "Non-void function does not return in all control paths", 0);
			}

			//If it's not a void type, we do one thing
			if(function_defined_in->return_type->basic_type_token != VOID){
				//The appropriate type for the return variable
				generic_type_t* return_var_type;

				//Determine a type compatible for us to use
				switch(function_defined_in->return_type->type_size){
					case 1:
						return_var_type = i8;
						break;
					case 2:
						return_var_type = i16;
						break;
					case 4:
						return_var_type = i32;
						break;
					//Anything else is just going in %rax
					default:
						return_var_type = i64;
						break;
				}

				//Emit the constant with the appropriate type
				three_addr_const_t* ret_const = emit_direct_integer_or_char_constant(0, return_var_type);

				//Now emit the assignment
				instruction_t* assignment = emit_assignment_with_const_instruction(emit_temp_var(return_var_type), ret_const);
			
				//This goes into the block
				add_statement(block, assignment); 

				//We'll now manually insert a ret 0 based on whatever the return type of the function is
				instruction_t* return_instruction = emit_ret_instruction(assignment->assignee);

				//This counts as a use
				add_used_variable(block, assignment->assignee);
				
				//We'll now add this at the very end of the block
				add_statement(block, return_instruction);

			//Otherwise it is void, so we just emit a plain "ret"
			} else {
				//Void returning - just emit a plain ret
				instruction_t* return_instruction = emit_ret_instruction(NULL);
				
				//We'll now add this at the very end of the block
				add_statement(block, return_instruction);
			}
		}
	}
}


/**
 * Finalize all user defined jump statements by ensuring that these jumps are assigned the right block to go to. This
 * needs to be done after the fact because we could jump to a label statement that we have not yet seen
 */
static void finalize_all_user_defined_jump_statements(dynamic_array_t* labeled_blocks, dynamic_array_t* user_defined_jumps){
	//Run through every jump statement
	while(dynamic_array_is_empty(user_defined_jumps) == FALSE){
		//Delete from the back
		instruction_t* branch_instruction = dynamic_array_delete_from_back(user_defined_jumps);

		//We'll now need to scan through the labeled blocks to find who this should point to
		for(u_int16_t i = 0; i < labeled_blocks->current_index; i++){
			//Grab the labeled block out
			basic_block_t* labeled_block = dynamic_array_get_at(labeled_blocks, i);

			//If this labeled block doesn't have the same variable, we're out
			if(labeled_block->label != branch_instruction->var_record){
				continue;
			}

			//Otherwise if we get here we know that we found the correct label
			branch_instruction->if_block = labeled_block;

			//Add both of the successors so that we can maintain a nice order
			add_successor(branch_instruction->block_contained_in, labeled_block);
			add_successor(branch_instruction->block_contained_in, branch_instruction->else_block);
			
			//Break out of the for loop
			break;
		}
	}
}


/**
 * A function definition will always be considered a leader statement. As such, it
 * will always have it's own separate block
 */
static basic_block_t* visit_function_definition(cfg_t* cfg, generic_ast_node_t* function_node){
	//Push the nesting level that we're in
	push_nesting_level(&nesting_stack, NESTING_FUNCTION);
	
	//Grab the function record
	symtab_function_record_t* func_record = function_node->func_record;
	//We will now store this as the current function
	current_function = func_record;
	//We also need to zero out the current stack offset value
	stack_offset = 0;
	//We also need to set the labeled block array to be empty
	current_function_labeled_blocks = dynamic_array_alloc();
	//Keep an array for all of the jump statements as well
	current_function_user_defined_jump_statements = dynamic_array_alloc();

	//The starting block
	basic_block_t* function_starting_block = basic_block_alloc_and_estimate();
	//The function exit block
	function_exit_block = basic_block_alloc_and_estimate();
	//Mark that this is a starting block
	function_starting_block->block_type = BLOCK_TYPE_FUNC_ENTRY;
	//Mark that this is an exit block
	function_exit_block->block_type = BLOCK_TYPE_FUNC_EXIT;
	//Store this in the entry block
	function_starting_block->function_defined_in = func_record;

	//These will always be direct successors here
	function_starting_block->direct_successor = function_exit_block;

	/**
	 * If we have function parameters that are *also* stack variables(meaning the user will
	 * at some point want to take the memory address of them), then we need to load
	 * these variables into the stack preemptively
	 */
	for(u_int16_t i = 0; i < func_record->number_of_params; i++){
		//Extract the parameter
		symtab_variable_record_t* parameter = func_record->func_params[i];

		//If we have a stack variable that is *not* a reference type, we will
		//go through here and pre-load it onto the stack
		if(parameter->stack_variable == TRUE 
			&& parameter->type_defined_as->type_class != TYPE_CLASS_REFERENCE){

			//However if it is a stack variable, we need to add it to the stack and emit an initial store of it
			if(parameter->stack_region == NULL){
				//Add this variable onto the stack now, since we know it is not already on it
				parameter->stack_region = create_stack_region_for_type(&(current_function->data_area), parameter->type_defined_as);
			}

			//Copy the type over here
			three_addr_var_t* parameter_var = emit_memory_address_var(parameter);

			//Now we'll need to do our initial load
			instruction_t* store_code = emit_store_ir_code(parameter_var, emit_var(parameter), parameter->type_defined_as);

			//Bookkeeping here
			add_used_variable(function_starting_block, store_code->op1);
			add_used_variable(function_starting_block, store_code->assignee);

			//Add it into the starting block
			add_statement(function_starting_block, store_code);

		/**
		 * Parameter aliasing:
		 *
		 * To avoid any issues with precoloring interference way down the line
		 * in the register allocator, we will do a temp assignment/aliasing
		 * of all non-stack function parameters. This works something like this
		 *
		 * Parameter x:
		 *
		 * <function_start>
		 * 		x_alias <- x;
		 *
		 * 	<use of x>
		 * 		y <- x(replaced with x_alias) + 3;
		 *
		 *
		 * 	This allows us to avoid the need to spill function parameter variables. If this
		 * 	turns out to not be needed, then the coalescing subsystem inside of the register
		 * 	allocator will simply knock out the top assignment as if it was never there
		 */
		} else {
			//Create the aliased variable
			symtab_variable_record_t* alias = create_parameter_alias_variable(parameter, variable_symtab, increment_and_get_temp_id());

			//Very important that we emit this first for the below reason
			three_addr_var_t* parameter_var = emit_var(parameter);

			//Emit the alias that we're assigning to
			three_addr_var_t* alias_var = emit_var(alias);

			//Flag that the parameter does have this alias. Note that once we do this, any time
			//emit_var() is called on the parameter, the alias will be used instead so the order
			//here is very important. Once this is done - there is no going back
			parameter->alias = alias;

			//Emit the assignment here
			instruction_t* alias_assignment = emit_assignment_instruction(alias_var, parameter_var);

			//Counts as a use for the parameter
			add_used_variable(function_starting_block, parameter_var);
			add_assigned_variable(function_starting_block, alias_var);

			//Now add the statement in
			add_statement(function_starting_block, alias_assignment);
		}
	}

	//We don't care about anything until we reach the compound statement
	generic_ast_node_t* func_cursor = function_node->first_child;

	//It could be null, though it usually is not
	if(func_cursor != NULL){
		//Once we get here, we know that func cursor is the compound statement that we want
		cfg_result_package_t compound_statement_results = visit_compound_statement(func_cursor);

		//Once we're done with the compound statement, we will merge it into the function
	 	basic_block_t* compound_statement_exit_block = merge_blocks(function_starting_block, compound_statement_results.starting_block);

		//Only reassign here if the two are different
		if(compound_statement_results.starting_block != compound_statement_results.final_block){
			compound_statement_exit_block = compound_statement_results.final_block;
		}

		//If these 2 are not the same, then ensure this works by adding a successor
		if(compound_statement_exit_block != function_exit_block){
			add_successor(compound_statement_exit_block, function_exit_block);
		}
	
	//Otherwise, we have an empty function definition. If this is the case, then the only
	//predecessor to the exit block is the entry block
	} else {
		add_successor(function_starting_block, function_exit_block);
	}

	//Determine and insert any needed ret statements
	determine_and_insert_return_statements(function_exit_block);

	//We'll need to go through and finalize all user defined jump statements if there are any
	finalize_all_user_defined_jump_statements(&current_function_labeled_blocks, &current_function_user_defined_jump_statements);

	//Add the start and end blocks to their respective arrays
	dynamic_array_add(&(cfg->function_entry_blocks), function_starting_block);
	dynamic_array_add(&(cfg->function_exit_blocks), function_exit_block);

	//Now that we're done, we will clear this current function parameter
	current_function = NULL;

	//Mark this as NULL for the next go around
	function_exit_block = NULL;

	//Now we can scrap the labeled block array
	dynamic_array_dealloc(&current_function_labeled_blocks);

	//Deallocate the current function's user defined jumps as well
	dynamic_array_dealloc(&current_function_user_defined_jump_statements);

	//Remove it now that we're done
	pop_nesting_level(&nesting_stack);

	//We always return the start block
	return function_starting_block;
}


/**
 * Emit a global variable array initializer. Unlike a normal array initializer - we do not put values in blocks. Instead, we store
 * the constant values in a result array that is then passed along back to the caller for later use
 *
 * The switch statement here is intentional so that we may expand this later to structs, etc.
 */
static void emit_global_array_initializer(generic_ast_node_t* array_initializer, dynamic_array_t* initializer_values){
	//Grab a cursor to the child
	generic_ast_node_t* cursor = array_initializer->first_child;

	//We can either see array initializers or constants here
	while(cursor != NULL){
		//Go based on what kind of node we have
		switch(cursor->ast_node_type){
			//If we have an array initializer - simply pass this along
			case AST_NODE_TYPE_ARRAY_INITIALIZER_LIST:
				emit_global_array_initializer(cursor, initializer_values);
				break;

			//This is really our base case
			case AST_NODE_TYPE_CONSTANT:
				//Emit the constant and get it into the array
				dynamic_array_add(initializer_values, emit_global_variable_constant(cursor));
				break;

			default:
				printf("Fatal internal compiler: Invalid or unimplemented global initializer node encountered\n");
				exit(1);
		}

		//Advance to the next one
		cursor = cursor->next_sibling;
	}
}


/**
 * Visit a global let statement and handle the initializer appropriately.
 * Do note that we have already checked that the entire initialization
 * only contains constants, so we can assume we're only processing constants
 * here
 */
static void visit_global_let_statement(generic_ast_node_t* node){
	//We'll store it inside of the global variable struct. Leave it as NULL
	//here so that it's automatically initialized to 0
	global_variable_t* global_variable = create_global_variable(node->variable, NULL);

	//This has been initialized already
	global_variable->variable->initialized = TRUE;

	//And add it into the CFG
	dynamic_array_add(&(cfg->global_variables), global_variable);

	//Grab out the initializer node
	generic_ast_node_t* initializer = node->first_child;

	//We can see arrays or constants here
	switch(initializer->ast_node_type){
		//Array init list - goes to the helper
		case AST_NODE_TYPE_ARRAY_INITIALIZER_LIST:
			//Initialized to an array
			global_variable->initializer_type = GLOBAL_VAR_INITIALIZER_ARRAY;

			//Give it an array of values
			global_variable->initializer_value.array_initializer_values = dynamic_array_alloc();

			//Let the helper take care of it
			emit_global_array_initializer(initializer, &(global_variable->initializer_value.array_initializer_values));

			break;
		
		//Should be our most common case - we just have a constant
		case AST_NODE_TYPE_CONSTANT:
			//Initialized to a constant
			global_variable->initializer_type = GLOBAL_VAR_INITIALIZER_CONSTANT;

			//All we need to do here
			global_variable->initializer_value.constant_value = emit_global_variable_constant(initializer);

			break;

		//This shouldn't be reachable
		default:
			printf("Fatal internal compiler error: Unrecognized/unimplemented global initializer node type encountered\n");
			exit(1);
	}
}


/**
 * Visit a global variable declaration statement
 *
 * NOTE: declared global variables will always be initialized to be 0
 */
static void visit_global_declare_statement(generic_ast_node_t* node){
	//We'll store it inside of the global variable struct. Leave it as NULL
	//here so that it's automatically initialized to 0
	global_variable_t* global_variable = create_global_variable(node->variable, NULL);

	//This has no initializer-so flag that here
	global_variable->initializer_type = GLOBAL_VAR_INITIALIZER_NONE;

	//And add it into the CFG
	dynamic_array_add(&(cfg->global_variables), global_variable);
}


/**
 * Visit a declaration statement. If we see an actual declaration node, then
 * we know that this is either a struct, array or union - it's something that
 * has to be allocated and placed onto the stack
 */
static void visit_declaration_statement(generic_ast_node_t* node){
	//Create a stack region for this variable
	node->variable->stack_region = create_stack_region_for_type(&(current_function->data_area), node->inferred_type);
}


/**
 * Emit a base level intialization given an offset, base address and a node. When we do this,
 * we'll have something like:
 *
 * store base_address[offset] <- emit_expression(node)
 */
static cfg_result_package_t emit_final_initialization(basic_block_t* current_block, three_addr_var_t* base_address, u_int32_t offset, generic_ast_node_t* expression_node, u_int8_t is_branch_ending){
	//Initialize our final results
	cfg_result_package_t final_results = {current_block, current_block, base_address, BLANK};

	//Now let's emit the expression using the node
	cfg_result_package_t expression_results = emit_expression(current_block, expression_node, is_branch_ending, FALSE);

	//The type that we're after
	generic_type_t* inferred_type = expression_node->inferred_type;
	
	//Update this
	current_block = expression_results.final_block;

	//Grab the last instruction - we'll need this for later
	instruction_t* last_instruction = current_block->exit_statement;

	//This is now the final block
	final_results.final_block = current_block;

	//First we emit the offset
	three_addr_const_t* offset_constant = emit_direct_integer_or_char_constant(offset, u64);

	//Now we need to emit the store operation
	instruction_t* store_instruction = emit_store_with_constant_offset_ir_code(base_address, offset_constant, NULL, inferred_type);

	//This counts as a use for both of these
	add_used_variable(current_block, base_address);

	//If the last instruction is *not* a constant assignment, we can go ahead like this
	if(last_instruction == NULL
		|| last_instruction->statement_type != THREE_ADDR_CODE_ASSN_CONST_STMT){
		//This is now our op1
		store_instruction->op2 = expression_results.assignee;

		//No matter what happened, we used this
		add_used_variable(current_block, expression_results.assignee);

	//Otherwise, we can do a small optimization here by scrapping the 
	//constant assignment and just putting the constant in directly
	} else {
		//Extract it
		three_addr_const_t* constant_assignee = last_instruction->op1_const;

		//This is now useless
		delete_statement(last_instruction);

		//Set the store statement's op1_const to be this
		store_instruction->op1_const = constant_assignee;
	}

	//Add it into the block
	add_statement(current_block, store_instruction);

	//Give this back
	return final_results;
}


/**
 * Emit all array intializer assignments. To do this, we'll need the base address and the initializer
 * node that contains all elements to add in. We'll leverage the root level "emit_initializer" here
 * and let it do all of the heavy lifting in terms of assignment operations. This rule
 * will just compute the addresses that we need
 */
static cfg_result_package_t emit_array_initializer(basic_block_t* current_block, three_addr_var_t* base_address, u_int32_t current_offset, generic_ast_node_t* array_initializer, u_int8_t is_branch_ending){
	//Initialize the results package here to start
	cfg_result_package_t results = {current_block, current_block, NULL, BLANK};

	//Grab a cursor to the child
	generic_ast_node_t* cursor = array_initializer->first_child;

	//What is the current index of the initializer? We start at 0
	u_int32_t current_array_index = 0;

	//For storing all of our results
	cfg_result_package_t initializer_results;

	//Run through every child in the array_initializer node and invoke the proper address assignment and rule
	while(cursor != NULL){
		//This is the type of the value. We'll need it's size
		generic_type_t* base_type = cursor->inferred_type;

		//Calculate the correct offset for our member
		u_int32_t offset = current_offset + current_array_index * base_type->type_size;

		//Determine if we need to emit an indirection instruction or not
		switch(cursor->ast_node_type){
			//If we have special cases, then the individual rules handle these
			case AST_NODE_TYPE_ARRAY_INITIALIZER_LIST:
				//Pass the new base offset along to this rule
				initializer_results = emit_array_initializer(current_block, base_address, offset, cursor, is_branch_ending);
				break;

			case AST_NODE_TYPE_STRING_INITIALIZER:
				//Pass the new base offset along to this rule
				initializer_results = emit_string_initializer(current_block, base_address, offset, cursor, is_branch_ending);
				break;

			case AST_NODE_TYPE_STRUCT_INITIALIZER_LIST:
				//Pass the new base offset along to this rule
				initializer_results = emit_struct_initializer(current_block, base_address, offset, cursor, is_branch_ending);
				break;

			//When we hit the default case, that means that we've stopped seeing initializer values
			default:
				//Once we get here, we need to let the helper finish it off
				initializer_results = emit_final_initialization(current_block, base_address, offset, cursor, is_branch_ending);
				break;
		}

		//Change the current block if there is a change. This is possible with ternary expressions
		if(initializer_results.final_block != current_block){
			current_block = initializer_results.final_block;
		}

		//The current array index goes up by one
		current_array_index++;

		//Advance to the next one
		cursor = cursor->next_sibling;
	}

	//This could have changed throughout the function's executions
	results.final_block = current_block;
	
	//Give back the results package
	return results;
}


/**
 * Emit all string intializer assignments. To do this, we'll need the base address and the initializer's string
 * itself.
 */
static cfg_result_package_t emit_string_initializer(basic_block_t* current_block, three_addr_var_t* base_address, u_int32_t offset, generic_ast_node_t* string_initializer, u_int8_t is_branch_ending){
	//Initialize the results package here to start
	cfg_result_package_t results = {current_block, current_block, NULL, BLANK};

	//The string index starts off at 0
	u_int32_t current_index = 0;

	//Now we'll go through every single character here and emit a load instruction for them
	while(current_index <= string_initializer->string_value.current_length){
		//Grab the value that we want out
		char char_value = string_initializer->string_value.string[current_index];

		//The relative address is always just whatever offset we were given in the param plus the current index. Char size is 1 byte so
		//there's nothing to multiply by
		u_int64_t stack_offset = offset + current_index; 

		//Create the character type itself
		three_addr_const_t* constant = emit_direct_integer_or_char_constant(char_value, char_type);

		//Now finally we'll store it
		instruction_t* store_instruction = emit_store_with_constant_offset_ir_code(base_address, emit_direct_integer_or_char_constant(stack_offset, u64), NULL, char_type);
		store_instruction->is_branch_ending = is_branch_ending;

		//We can skip the assignment here and just directly put the constant in
		store_instruction->op1_const = constant;

		//These both count as used
		add_used_variable(current_block, base_address);

		//Add the instruction in
		add_statement(current_block, store_instruction);

		//Once this is all done, we'll loop back up to the top
		current_index++;
	}

	//The results package shouldn't have much at all that changes. There is no chance
	//to have any ternary operations at all here
	return results;
}


/**
 * Emit all struct intializer assignments. To do this, we'll need the base address and the initializer
 * node that contains all elements to add in
 */
static cfg_result_package_t emit_struct_initializer(basic_block_t* current_block, three_addr_var_t* base_address, u_int32_t offset, generic_ast_node_t* struct_initializer, u_int8_t is_branch_ending){
	//Initialize the results package here to start
	cfg_result_package_t results = {current_block, current_block, NULL, BLANK};

	//Grab the struct type out for reference
	generic_type_t* struct_type = struct_initializer->inferred_type;

	//Grab a cursor to the child
	generic_ast_node_t* cursor = struct_initializer->first_child;

	//The member index
	u_int32_t member_index = 0;

	//The initializer results
	cfg_result_package_t initializer_results;

	//Run through every child in the array_initializer node and invoke the proper address assignment and rule
	while(cursor != NULL){
		//Grab it out
		symtab_variable_record_t* member_variable = dynamic_array_get_at(&(struct_type->internal_types.struct_table), member_index);

		//We can calculate the offset by adding the struct offset to the starting offset
		u_int32_t current_offset = offset + member_variable->struct_offset;

		//Determine if we need to emit an indirection instruction or not
		switch(cursor->ast_node_type){
			//Handle an array initializer
			case AST_NODE_TYPE_ARRAY_INITIALIZER_LIST:
				initializer_results = emit_array_initializer(current_block, base_address, current_offset, cursor, is_branch_ending);
				break;
			case AST_NODE_TYPE_STRING_INITIALIZER:
				initializer_results = emit_string_initializer(current_block, base_address, current_offset, cursor, is_branch_ending);
				break;
			case AST_NODE_TYPE_STRUCT_INITIALIZER_LIST:
				initializer_results = emit_struct_initializer(current_block, base_address, current_offset, cursor, is_branch_ending);
				break;

			default:
				initializer_results = emit_final_initialization(current_block, base_address, current_offset, cursor, is_branch_ending);
				break;
		}

		//Change the current block if there is a change. This is possible with ternary expressions
		if(initializer_results.final_block != current_block){
			current_block = initializer_results.final_block;
		}

		//Increment this by one
		member_index++;

		//Advance to the next one
		cursor = cursor->next_sibling;
	}

	//This could have changed throughout the function's executions
	results.final_block = current_block;
	
	//Give back the results package
	return results;
}


/**
 * Emit an initialization statement given only a variable and
 * the top level of what could be a larger initialization sequence
 *
 * For more complex initializers, we're able to bypass the emitting of extra instructions and simply emit the 
 * offset that we need directly. We're able to do this because all array and struct initialization statements at
 * the end of the day just calculate offsets.
 */
static cfg_result_package_t emit_complex_initialization(basic_block_t* current_block, three_addr_var_t* base_address, generic_ast_node_t* initializer_root, u_int8_t is_branch_ending){
	switch(initializer_root->ast_node_type){
		//Make a direct call to the rule. Seed with 0 as the initial offset
		case AST_NODE_TYPE_STRING_INITIALIZER:
			return emit_string_initializer(current_block, base_address, 0, initializer_root, is_branch_ending);

		//Make a direct call to the rule. Seed with 0 as the initial offset
		case AST_NODE_TYPE_STRUCT_INITIALIZER_LIST:
			return emit_struct_initializer(current_block, base_address, 0, initializer_root, is_branch_ending);
		
		//Make a direct call to the array initializer. We'll "seed" with 0 as the starting address
		case AST_NODE_TYPE_ARRAY_INITIALIZER_LIST:
			return emit_array_initializer(current_block, base_address, 0, initializer_root, is_branch_ending);

		//We should never actually hit this. If we do it's bad news
		default:
			print_parse_message(PARSE_ERROR, "Fatal Internal Compiler Error. Unreachable path reached", 0);
			exit(1);
	}
}


/**
 * Emit a normal intialization
 *
 * We'll hit this when we have something like:
 *
 * let x:i32 = a + b + c;
 *
 * No array/string/struct initializers here
 */
static cfg_result_package_t emit_simple_initialization(basic_block_t* current_block, three_addr_var_t* let_variable, generic_ast_node_t* expression_node, u_int8_t is_branch_ending){
	//Allocate the return package here
	cfg_result_package_t let_results = {current_block, current_block, let_variable, BLANK};

	//Emit the right hand expression here
	cfg_result_package_t package = emit_expression(current_block, expression_node, is_branch_ending, FALSE);

	//Reassign what the current block is in case it's changed
	current_block = package.final_block;

	//What will our final op1 end up as?
	three_addr_var_t* final_op1 = package.assignee;

	//Store the last instruction for later use
	instruction_t* last_instruction = current_block->exit_statement;

	//If the final op1 is a memory address, we need to emit a temp assignment
	//to make it not one. This is because later on down in the instruction selector,
	//we will need to translate a memory address into an actual result and we can't
	//do that inside of a store
	if(final_op1->variable_type == VARIABLE_TYPE_MEMORY_ADDRESS){
		//Emit the assignment
		instruction_t* assignment = emit_assignment_instruction(emit_temp_var(u64), final_op1);
		assignment->is_branch_ending = is_branch_ending;

		//Counts as a use
		add_used_variable(current_block, final_op1);

		//Add this at the very end of the block. Remember - there is a chance that
		//the last instruction is NULL so we'll dodge that here by doing this
		add_statement(current_block, assignment);

		//This is now the last instruction
		last_instruction = assignment;

		//And the final op1 is this one's assignee
		final_op1 = last_instruction->assignee;
	}

	//Now update the final block
	let_results.final_block = current_block;

	/**
	 * Is the left hand variable a regular variable or is it a stack address variable? If it's a
	 * variable that is on the stack, then a regular assignment just won't do. We'll need to
	 * emit a store operation
	 */
	if(let_variable->linked_var == NULL || let_variable->linked_var->stack_variable == FALSE){
		//The actual statement is the assignment of right to left
		instruction_t* assignment_statement = emit_assignment_instruction(let_variable, final_op1);
		assignment_statement->is_branch_ending = is_branch_ending;

		//The let variable was assigned
		add_assigned_variable(current_block, let_variable);

		//If this is not temporary, then it counts as used
		add_used_variable(current_block, final_op1);

		//Finally we'll add this into the overall block
		add_statement(current_block, assignment_statement);
			
	/**
	 * Otherwise, we'll need to emit a store operation here
	 */
	} else {
		//Store the "true" stored type. This will only change if our type is a reference, because
		//we need to account for the implicit dereference that's happening
		generic_type_t* true_stored_type = let_variable->type;

		//Handle the implicit dereference here if appropriate
		if(true_stored_type->type_class == TYPE_CLASS_REFERENCE){
			//If we have something like let x:i32& = y;, we actually
			//don't need to write anything down at all because why will
			//have already been flagged as being on the stack. In this
			//instance, we can just bail out here
			if(expression_node->ast_node_type == AST_NODE_TYPE_IDENTIFIER){
				return let_results;
			}

			//Otherwise we'll need to emit something, so derefernce this
			true_stored_type = dereference_type(true_stored_type);
		}

		//NOTE: We use the type of our let variable here for the address assignment
		three_addr_var_t* base_address = emit_memory_address_var(let_variable->linked_var);
		
		//Emit the store code
		instruction_t* store_statement = emit_store_ir_code(base_address, NULL, true_stored_type);
		store_statement->is_branch_ending = is_branch_ending;

		//This counts as a use
		add_used_variable(current_block, base_address);

		//If the last instruction is *not* a constant assignment, we can go ahead like this
		if(last_instruction == NULL
			|| last_instruction->statement_type != THREE_ADDR_CODE_ASSN_CONST_STMT){

			//This is now our op1
			store_statement->op1 = final_op1;

			//No matter what happened, we used this
			add_used_variable(current_block, final_op1);

		//Otherwise, we can do a small optimization here by scrapping the 
		//constant assignment and just putting the constant in directly
		} else {
			//Extract it
			three_addr_const_t* constant_assignee = last_instruction->op1_const;

			//This is now useless
			delete_statement(last_instruction);

			//Set the store statement's op1_const to be this
			store_statement->op1_const = constant_assignee;
		}
				
		//Now add thi statement in here
		add_statement(current_block, store_statement);
	}

	//And give it back
	return let_results;
}


/**
 * Visit a let statement
 */
static cfg_result_package_t visit_let_statement(generic_ast_node_t* node, u_int8_t is_branch_ending){
	//Create the return package here
	cfg_result_package_t let_results = {NULL, NULL, NULL, BLANK};

	//What block are we emitting to?
	basic_block_t* current_block = basic_block_alloc_and_estimate();

	//Extract the type here
	generic_type_t* type = node->inferred_type;

	//The assignee of the let statement. This could either be a variable or it could represent
	//a base address for an array
	three_addr_var_t* assignee;

	//Based on what type we have, we'll need to do some special intialization
	switch(type->type_class){
		//Arrays or structs require stack allocation
		case TYPE_CLASS_ARRAY:
		case TYPE_CLASS_STRUCT:
			//Create a stack region for this variable and store it in the associated region
			node->variable->stack_region = create_stack_region_for_type(&(current_function->data_area), node->inferred_type);

			//Emit the memory address variable
			assignee = emit_memory_address_var(node->variable);

			//The left hand var is our assigned var
			let_results.assignee = assignee;

			//We know that this will be the lead block
			let_results.starting_block = current_block;
			
			//Invoke the complex initialization method. We know that we have a struct, array or string initializer here
			cfg_result_package_t package = emit_complex_initialization(current_block, assignee, node->first_child, is_branch_ending);

			//This is also the final block for now, unless a ternary comes along
			let_results.final_block = package.final_block;

			//And give the block back
			return let_results;
			
		//Otherwise we just have a garden variety variable - no stack allocation required
		default:
			//Emit it
			assignee = emit_var(node->variable);

			//Let the helper rule deal with the rest here
			return emit_simple_initialization(current_block, assignee, node->first_child, is_branch_ending);
	}
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

	//So long as the AST cursor is not null
	while(ast_cursor != NULL){
		//Switch based on the class of cursor that we have here
		switch(ast_cursor->ast_node_type){
			//We can see a function definition. In this case, we'll
			//allow the helper to do it
			case AST_NODE_TYPE_FUNC_DEF:
				//Visit the function definition
				block = visit_function_definition(cfg, ast_cursor);
			
				//If this failed, we're out
				if(block->block_id == -1){
					return FALSE;
				}

				//All good to move along
				break;

			/**
			 * We know that by nature of these variables being here that they
			 * are global variables
			 */
			case AST_NODE_TYPE_LET_STMT:
				visit_global_let_statement(ast_cursor);
				break;
		
			//Finally, we could see a declaration
			case AST_NODE_TYPE_DECL_STMT:
				visit_global_declare_statement(ast_cursor);
				
				break;

			//Some very weird error if we hit here. Hard exit to avoid dev confusion
			default:
				printf("Fatal internal compiler error: Unrecognized node type found in global scope\n");
				exit(1);
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

	//Print all global variables after the blocks
	print_all_global_variables(stdout, &(cfg->global_variables));
}


/**
 * Reset the visited status of the CFG
 */
void reset_visited_status(cfg_t* cfg, u_int8_t reset_direct_successor){
	//For each block in the CFG
	for(u_int16_t _ = 0; _ < cfg->created_blocks.current_index; _++){
		//Grab the block out
		basic_block_t* block = dynamic_array_get_at(&(cfg->created_blocks), _);

		//Set it's visited status to 0
		block->visited = FALSE;

		//If we want to reset this, we'll null it out
		if(reset_direct_successor == TRUE){
			block->direct_successor = NULL;
		}
	}
}


/**
 * Reset the visited status inside a particular function in the CFG
 */
void reset_function_visited_status(basic_block_t* function_entry_block, u_int8_t reset_direct_successor){
	//Starts with our function entry
	basic_block_t* current = function_entry_block;

	//Run through every single block
	while(current != NULL){
		//This happens regardless
		current->visited = FALSE;

		//If this is the case then just push it up
		if(reset_direct_successor == FALSE){
			//Push it up
			current = current->direct_successor;
		
		//Otherwise it's slighly more complex
		} else {
			//Hold onto it
			basic_block_t* temp = current->direct_successor;

			//Null it out
			current->direct_successor = NULL;

			//Push on
			current = temp;
		}
	}
}


/**
 * Recalculate the reverse-post-order traversals
 * and the reverse-post-order-reverse-cfg traversals
 */
void calculate_all_reverse_traversals(cfg_t* cfg){
	//Clear all of these old values out
	reset_reverse_post_order_sets(cfg);

	//For each function entry block, recompute all reverse post order
	//CFG work
	for(u_int16_t i = 0; i < cfg->function_entry_blocks.current_index; i++){
		//Grab the block out
		basic_block_t* block = dynamic_array_get_at(&(cfg->function_entry_blocks), i);

		//Reset the visited status
		reset_visited_status(cfg, FALSE);

		//Compute the reverse post order traversal
		block->reverse_post_order = compute_reverse_post_order_traversal(block);

		//Reset once more
		reset_visited_status(cfg, FALSE);

		//Now use the reverse CFG(successors are predecessors, and vice versa)
		block->reverse_post_order_reverse_cfg = compute_reverse_post_order_traversal_reverse_cfg(block);
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
void calculate_all_control_relations(cfg_t* cfg){
	//Calculate all reverse traversals
	calculate_all_reverse_traversals(cfg);

	//We first need to calculate the dominator sets of every single node
	calculate_dominator_sets(cfg);
	
	//Now we'll build the dominator tree up
	build_dominator_trees(cfg);

	//We need to calculate the dominance frontier of every single block before
	//we go any further
	calculate_dominance_frontiers(cfg);

	//Calculate the postdominator sets for later analysis in the optimizer
	calculate_postdominator_sets(cfg);

	//We'll also now calculate the reverse dominance frontier that will be used
	//in later analysis by the optimizer
	calculate_reverse_dominance_frontiers(cfg);
}


/**
 * Build a cfg from the ground up
*/
cfg_t* build_cfg(front_end_results_package_t* results, u_int32_t* num_errors, u_int32_t* num_warnings){
	//Initialize the variable system
	initialize_varible_and_constant_system();

	//Store our references here
	num_errors_ref = num_errors;
	num_warnings_ref = num_warnings;

	//Add this in
	type_symtab = results->type_symtab;
	variable_symtab = results->variable_symtab;

	//Allocate these three stacks
	break_stack = heap_stack_alloc();
	continue_stack = heap_stack_alloc(); 
	nesting_stack = nesting_stack_alloc();

	//Keep these on hand
	u64 = lookup_type_name_only(type_symtab, "u64", NOT_MUTABLE)->type;
	i64 = lookup_type_name_only(type_symtab, "i64", NOT_MUTABLE)->type;
	u32 = lookup_type_name_only(type_symtab, "u32", NOT_MUTABLE)->type;
	i32 = lookup_type_name_only(type_symtab, "i32", NOT_MUTABLE)->type;
	u16 = lookup_type_name_only(type_symtab, "u16", NOT_MUTABLE)->type;
	i16 = lookup_type_name_only(type_symtab, "i16", NOT_MUTABLE)->type;
	u8 = lookup_type_name_only(type_symtab, "u8", NOT_MUTABLE)->type;
	i8 = lookup_type_name_only(type_symtab, "i8", NOT_MUTABLE)->type;
	char_type = lookup_type_name_only(type_symtab, "char", NOT_MUTABLE)->type;

	//We'll first create the fresh CFG here
	cfg = calloc(1, sizeof(cfg_t));

	//Store this along with it
	cfg->type_symtab = type_symtab;

	//Create the dynamic arrays that we need
	cfg->created_blocks = dynamic_array_alloc();
	cfg->function_entry_blocks = dynamic_array_alloc();
	cfg->function_exit_blocks = dynamic_array_alloc();
	cfg->global_variables = dynamic_array_alloc();

	//Set this to NULL initially
	current_function = NULL;

	//Create the stack pointer
	symtab_variable_record_t* stack_pointer = initialize_stack_pointer(results->type_symtab);
	//Initialize the variable too
	three_addr_var_t* stack_pointer_var = emit_var(stack_pointer);
	//Mark it
	stack_pointer_var->is_stack_pointer = TRUE;
	//Store the stack pointer
	cfg->stack_pointer = stack_pointer_var;

	//Create the instruction pointer
	symtab_variable_record_t* instruction_pointer = initialize_instruction_pointer(results->type_symtab);
	//Initialize a three addr code var
	instruction_pointer_var = emit_var(instruction_pointer);
	//Store it in the CFG
	cfg->instruction_pointer = instruction_pointer_var;

	// -1 block ID, this means that the whole thing failed
	if(visit_prog_node(cfg, results->root) == FALSE){
		print_parse_message(PARSE_ERROR, "CFG was unable to be constructed", 0);
		(*num_errors_ref)++;
	}

	//Let the helper deal with this
	calculate_all_control_relations(cfg);

	//now we calculate the liveness sets
	calculate_liveness_sets(cfg);

	//Add all phi functions for SSA
	insert_phi_functions(cfg, results->variable_symtab);

	//Rename all variables after we're done with the phi functions
	rename_all_variables(cfg);

	//Once we get here, we're done with these two stacks
	heap_stack_dealloc(&break_stack);	
	heap_stack_dealloc(&continue_stack);	
	nesting_stack_dealloc(&nesting_stack);

	//Give back the reference
	return cfg;
}
