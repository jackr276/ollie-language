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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "../utils/queue/heap_queue.h"
#include "../jump_table/jump_table.h"
#include "../utils/stack/nesting_stack.h"
#include "../utils/constants.h"
#include "../utils/parameter_result_array/parameter_result_array.h"

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
//Hang onto the stack pointer variable(%rsp)
static three_addr_var_t* stack_pointer_variable = NULL;
//Keep a variable/record for the instruction pointer(rip)
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
static generic_type_t* f32 = NULL;
static generic_type_t* f64 = NULL;
static generic_type_t* immut_void_ptr = NULL;
/**
 * The break and continue stack will
 * hold values that we can break & continue
 * to. This is done here to avoid the need
 * to send value packages at each rule
 */
static heap_stack_t break_stack;
static heap_stack_t continue_stack;
//The overall nesting stack will tell us what level of nesting we're at(if, switch/case, loop)
static nesting_stack_t nesting_stack;
//Pointer to the symtab region for all current function blocks
static dynamic_array_t* current_function_blocks;
//Also keep a list of all custom jumps in the function
static dynamic_array_t current_function_user_defined_jump_statements;
//The current stack offset for any given function
static u_int64_t stack_offset = 0;

//Reusable memory regions for our graph traversals
static heap_queue_t traversal_queue;

//For any/all error printing
char error_info[1500];


/**
 * The actual result type that defines whether we have a struct
 * or constant or variable result
 */
typedef enum {
	CFG_RESULT_TYPE_VAR,
	CFG_RESULT_TYPE_CONST,
} cfg_result_type_t;


/**
 * CFG result packages are used to pass out the reult
 * of translating an expression. They will contain:
 * 	1.) Starting block of the statement
 * 	2.) Ending block of the statement
 * 	3.) A tagged union with either a variable or constant result
 * 	4.) The operator that was used, if any
 */
typedef struct{
	//Blocks come first
	basic_block_t* starting_block;
	basic_block_t* final_block;

	//We have a tagged union without it actually being a tagged union
	union {
		three_addr_var_t* result_var;
		three_addr_const_t* result_const;
	} result_value;

	cfg_result_type_t type;

	//The operator may or may not always be filled
	ollie_token_t operator;
} cfg_result_package_t;


//Are we emitting the dominance frontier or not?
typedef enum{
	EMIT_DOMINANCE_FRONTIER,
	DO_NOT_EMIT_DOMINANCE_FRONTIER
} emit_dominance_frontier_selection_t;


//Enum for branch conditional types
typedef enum {
	BRANCH_CONDITIONAL_UNKNOWN,
	BRANCH_CONDITIONAL_ALWAYS_TRUE,
	BRANCH_CONDITIONAL_ALWAYS_FALSE
} branch_conditional_truthfullness_t;


/**
 * An enum for declare and let statements letting us know what kind of variable
 * that we have
 */
typedef enum{
	VARIABLE_SCOPE_GLOBAL,
	VARIABLE_SCOPE_LOCAL,
} variable_scope_type_t;


/**
 * Define a simple initializer for a blank CFG
 * result type. We usually stack allocate these
 * so we can't wipe them out any other way
 */
#define INITIALIZE_BLANK_CFG_RESULT {NULL, NULL, {NULL}, CFG_RESULT_TYPE_VAR, BLANK}

//We predeclare up here to avoid needing any rearrangements
static cfg_result_package_t visit_compound_statement(generic_ast_node_t* root_node);
static cfg_result_package_t visit_let_statement(basic_block_t* basic_block, generic_ast_node_t* node);
static cfg_result_package_t visit_if_statement(generic_ast_node_t* root_node);
static cfg_result_package_t visit_while_statement(generic_ast_node_t* root_node);
static cfg_result_package_t visit_do_while_statement(generic_ast_node_t* root_node);
static cfg_result_package_t visit_for_statement(generic_ast_node_t* root_node);
static cfg_result_package_t visit_case_statement(generic_ast_node_t* root_node);
static cfg_result_package_t visit_default_statement(generic_ast_node_t* root_node);
static cfg_result_package_t visit_switch_statement(generic_ast_node_t* root_node);
static cfg_result_package_t visit_statement_chain(generic_ast_node_t* first_node);
static cfg_result_package_t emit_expression_chain(basic_block_t* basic_block, generic_ast_node_t* expression_chain_node);
static cfg_result_package_t emit_binary_expression(basic_block_t* basic_block, generic_ast_node_t* logical_or_expr);
static cfg_result_package_t emit_ternary_expression(basic_block_t* basic_block, generic_ast_node_t* ternary_operation);
static cfg_result_package_t emit_function_call(basic_block_t* basic_block, generic_ast_node_t* function_call_node);
static cfg_result_package_t emit_unary_expression(basic_block_t* basic_block, generic_ast_node_t* unary_expression);
static cfg_result_package_t emit_expression(basic_block_t* basic_block, generic_ast_node_t* expr_node);
static cfg_result_package_t emit_string_initializer(basic_block_t* current_block, three_addr_var_t* base_address, u_int32_t offset, generic_ast_node_t* string_initializer);
static cfg_result_package_t emit_struct_initializer(basic_block_t* current_block, three_addr_var_t* base_address, u_int32_t offset, generic_ast_node_t* struct_initializer);

static three_addr_var_t* emit_binary_operation_with_constant(basic_block_t* basic_block, three_addr_var_t* assignee, three_addr_var_t* op1, ollie_token_t op, three_addr_const_t* constant);
static void visit_declaration_statement(generic_ast_node_t* node);
static void visit_static_let_statement(generic_ast_node_t* node);
static inline void visit_static_declare_statement(generic_ast_node_t* node);
static inline void handle_raise_statement(basic_block_t* basic_block, generic_ast_node_t* node);


/**
 * Unpack a result package. We assume that if this function is being called that the caller
 * wants us to unpack the constant if we are able to.
 */
static inline three_addr_var_t* unpack_result_package(cfg_result_package_t* result_package, basic_block_t* block){
	//The variable that we will always end up returning
	three_addr_var_t* returned_variable;
	three_addr_const_t* constant_value;

	switch(result_package->type){
		//Variable - just give it back
		case CFG_RESULT_TYPE_VAR:
			returned_variable = result_package->result_value.result_var;
			break;

		//Constant - unpack with an assignment and give the temp var back
		case CFG_RESULT_TYPE_CONST:
			constant_value = result_package->result_value.result_const;

			//Emit the assignment
			instruction_t* const_assignment = emit_assignment_with_const_instruction(emit_temp_var(constant_value->type), constant_value);

			//Throw it into the block
			add_statement(block, const_assignment);

			//This is the variable that we end up returning
			returned_variable = const_assignment->assignee;
			break;
	}

	//Give back the returned variable in the end
	return returned_variable;
}


/**
 * Lea statements may only have: 1, 2, 4, or 8 as their scales
 * due to internal hardware constraints. This operation will find
 * if a given value is compatible
 */
static inline u_int8_t is_lea_compatible_power_of_2(int64_t value){
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
		case VARIABLE_TYPE_MEMORY_ADDRESS:
		case VARIABLE_TYPE_NON_TEMP:
			if(variable->linked_var != NULL){
				return TRUE;
			} else {
				return FALSE;
			}

		default:
			return FALSE;
	}
}


/**
 * When we do stack passed parameters, array types and 
 * pointer types are always passed as pointers, while
 * struct types and union types are passed by copy
 */
static inline u_int8_t is_type_stack_passed_by_reference(generic_type_t* type){
	switch(type->type_class){
		case TYPE_CLASS_ARRAY:
		case TYPE_CLASS_POINTER:	
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * When we do stack passed parameters, struct and union types
 * are always passed by copy whilst everything else is by reference
 */
static inline u_int8_t is_type_stack_passed_by_copy(generic_type_t* type){
	switch(type->type_class){
		case TYPE_CLASS_UNION:
		case TYPE_CLASS_STRUCT:	
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Is a given variable a data segment variable? These variables are not actually
 * stored in registers or in memory so we need to treat them a bit differently. In
 * ollie only static and global variables fit the bill for this
 */
static inline u_int8_t is_variable_data_segment_variable(symtab_variable_record_t* variable){
	switch(variable->membership){
		case GLOBAL_VARIABLE:
		case STATIC_VARIABLE:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Does a given type require copy assignment? Structs and unions fall under this category
 */
static inline u_int8_t does_type_require_parameter_copy_assignment(generic_type_t* type){
	switch(type->type_class){
		case TYPE_CLASS_STRUCT:
		case TYPE_CLASS_UNION:
			return TRUE;

		default:
			return FALSE;
	}
}


/**
 * Is the given three address code statement a binary operation?
 */
static inline u_int8_t is_binary_operation(instruction_t* statement){
	if(statement == NULL){
		return FALSE;
	}

	switch(statement->statement_type){
		case THREE_ADDR_CODE_BIN_OP_STMT:
		case THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Is the given three address code statement a constant assignment
 */
static inline u_int8_t is_constant_assignment(instruction_t* statement){
	if(statement == NULL){
		return FALSE;
	}

	if(statement->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT){
		return TRUE;
	} else {
		return FALSE;
	}
}


/**
 * Do the values on the left and right hand side of the expression require a copy assignment? This is 
 * going to be true if we have structs or unions on both sides of the equation
 */
static inline u_int8_t is_copy_assignment_required(generic_type_t* destination_type, generic_type_t* source_type){
	switch(destination_type->type_class){
		case TYPE_CLASS_STRUCT:
			if(source_type->type_class == TYPE_CLASS_STRUCT){
				return TRUE;
			} else {
				return FALSE;
			}

		case TYPE_CLASS_UNION:
			if(source_type->type_class == TYPE_CLASS_UNION){
				return TRUE;
			} else {
				return FALSE;
			}

		default:
			return FALSE;
	}
}


/**
 * Is the type a struct or union type that requires copy assignment?
 */
static inline u_int8_t does_type_require_copy_assignment(generic_type_t* type){
	switch(type->type_class){
		case TYPE_CLASS_STRUCT:
		case TYPE_CLASS_UNION:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Delete a block, including all of the needed internal
 * bookkeeping
 */
static inline void delete_block(basic_block_t* block){
	//Remove it from the overall structure
	dynamic_array_delete(&(cfg->created_blocks), block);

	//And delete it from this function's blocks too
	dynamic_array_delete(current_function_blocks, block);
}


/**
 * Run through an entire array of function blocks and reset the status for
 * every single one. We assume that the caller knows what they are doing, and
 * that the blocks inside of the array are really the correct blocks
 */
static inline void reset_visit_status_for_function(dynamic_array_t* function_blocks){
	//Run through all of the blocks
	for(u_int32_t i = 0; i < function_blocks->current_index; i++){
		//Extract the current block
		basic_block_t* current = dynamic_array_get_at(function_blocks, i);

		//Flag it as false
		current->visited = FALSE;
	}
}


/**
 * Determine the number of parameters that do not count as elaborative
 */
static inline u_int32_t get_non_elaborative_parameter_count(function_type_t* function_type){
	//Get the initial count here
	u_int32_t count = function_type->function_parameters.current_index;

	//Count is more than 0 - we need to check for elaborative params and update the count
	if(count != 0){
		//The last index is where an elaborative param would be
		u_int32_t last_index = function_type->function_parameters.current_index - 1;

		//Extract the type at the very last index
		generic_type_t* parameter_type = dynamic_array_get_at(&(function_type->function_parameters), last_index);

		//Bump the count down by one if this is the case
		if(parameter_type->type_class == TYPE_CLASS_ELABORATIVE){
			count--;
		}
	}

	return count;
}


/**
 * For any blocks that are completely impossible to reach, we will scrap them all now
 * to avoid any confusion later in the process
 *
 * We consider any block with no predecessors that *is not* a function entry block
 * to be unreachable. We must also be mindful that, once we start deleting blocks, we may
 * be creating even more unreachable blocks, so we need to take care of those too 
 */
static inline void delete_all_unreachable_blocks(dynamic_array_t* function_blocks){
	//Array of all blocks that are to be deleted
	dynamic_array_t to_be_deleted = dynamic_array_alloc();
	dynamic_array_t to_be_deleted_successors = dynamic_array_alloc();

	//First bulid the array of things that need to go. A block is considered
	//unreachable if it has no predecessors and it is *not* an entry block
	for(u_int32_t i = 0; i < function_blocks->current_index; i++){
		basic_block_t* current_block = dynamic_array_get_at(function_blocks, i);

		//Doesn't count
		if(current_block->block_type == BLOCK_TYPE_FUNC_ENTRY){
			continue;
		}

		//This is our case for something that has to go
		if(current_block->predecessors.current_index == 0){
			dynamic_array_add(&to_be_deleted, current_block);
		}
	}

	//Run through all of the blocks that need to be deleted
	while(dynamic_array_is_empty(&to_be_deleted) == FALSE){
		//O(1) removal
		basic_block_t* target = dynamic_array_delete_from_back(&to_be_deleted);

		//Every successor needs to be uncoupled
		for(u_int32_t i = 0; i < target->successors.current_index; i++){
			//Extract it
			basic_block_t* successor = dynamic_array_get_at(&(target->successors), i);

			//Add this link in
			dynamic_array_add(&to_be_deleted_successors, successor);
		}

		//Now run through all of the successors that we need to delete. This is done to avoid
		//any funniness with the indices
		while(dynamic_array_is_empty(&to_be_deleted_successors) == FALSE){
			//Extract the successor
			basic_block_t* successor = dynamic_array_delete_from_back(&(to_be_deleted_successors));

			//Undo the link
			delete_successor(target, successor);

			//What if the successor now has now predecessors? That means it needs to go too
			if(successor->predecessors.current_index == 0){
				dynamic_array_add(&to_be_deleted, successor);
			}
		}

		//Delete the target block now
		delete_block(target);
	}

	//Deallocate this once we're done
	dynamic_array_dealloc(&to_be_deleted);
	dynamic_array_dealloc(&to_be_deleted_successors);
}


/**
 * A helper function that makes a new block id. This ensures we have an atomically
 * increasing block ID
 */
static inline int32_t increment_and_get(){
	(cfg->block_id)++;
	return cfg->block_id;
}


/**
 * A helper that determines if a block ends in a statement that would
 * exclude us from putting another jump right after it. Such
 * blocks end in: ret, branch, or jmp statements
 */
static inline u_int8_t does_block_end_in_terminal_statement(basic_block_t* basic_block){
	//Just a catch here if it's null
	if(basic_block->exit_statement == NULL){
		return FALSE;
	}

	//Just checking for these 3
	switch(basic_block->exit_statement->statement_type){
		case THREE_ADDR_CODE_JUMP_STMT:
		case THREE_ADDR_CODE_RET_STMT:
		//Raise statements are functionally equivalent to ret statements
		case THREE_ADDR_CODE_RAISE_STMT:
		case THREE_ADDR_CODE_BRANCH_STMT:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Does a block end in a function terminating statement? The only 2 such statements
 * are "raise" and "ret" statements
 */
static inline u_int8_t does_block_end_in_function_termination_statement(basic_block_t* basic_block){
	//Just a catch here if it's null
	if(basic_block->exit_statement == NULL){
		return FALSE;
	}

	//Checking for raise or ret
	switch(basic_block->exit_statement->statement_type){
		case THREE_ADDR_CODE_RET_STMT:
		case THREE_ADDR_CODE_RAISE_STMT:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Simple helper that will add a local constant onto the cfg in the appropriate region
 *
 * This helper will also initialize the appropriate array if it is found to be null. This is
 * done so that we aren't allocating them all unnecessarily at the beginning
 */
static inline void add_local_constant_to_cfg(cfg_t* cfg, local_constant_t* local_constant){
	//Go based on what type it is
	switch(local_constant->local_constant_type){
		case LOCAL_CONSTANT_TYPE_F32:
			if(cfg->local_f32_constants.internal_array == NULL){
				cfg->local_f32_constants = dynamic_array_alloc();
			}

			dynamic_array_add(&(cfg->local_f32_constants), local_constant);

			break;

		case LOCAL_CONSTANT_TYPE_F64:
			if(cfg->local_f64_constants.internal_array == NULL){
				cfg->local_f64_constants = dynamic_array_alloc();
			}

			dynamic_array_add(&(cfg->local_f64_constants), local_constant);

			break;

		case LOCAL_CONSTANT_TYPE_STRING:
			if(cfg->local_string_constants.internal_array == NULL){
				cfg->local_string_constants = dynamic_array_alloc();
			}

			dynamic_array_add(&(cfg->local_string_constants), local_constant);

			break;

		case LOCAL_CONSTANT_TYPE_XMM128:
			if(cfg->local_xmm128_constants.internal_array == NULL){
				cfg->local_xmm128_constants = dynamic_array_alloc();
			}

			dynamic_array_add(&(cfg->local_xmm128_constants), local_constant);

			break;
	}
}


/**
 * Emit a three_addr_const_t value that is a local constant(.LCx) reference
 */
static inline three_addr_var_t* emit_string_local_constant(cfg_t* cfg, generic_ast_node_t* const_node){
	//Let's create the local constant first.
	local_constant_t* local_constant = string_local_constant_alloc(const_node->inferred_type, &(const_node->string_value));

	//Once this has been made, we can add it to the function
	add_local_constant_to_cfg(cfg, local_constant);

	//Now allocate the variable that will hold this
	three_addr_var_t* local_constant_variable = emit_local_constant_temp_var(local_constant);

	//And give this back
	return local_constant_variable;
}


/**
 * Emit a three_addr_var_t value that is a local constant(.LCx) reference. This helper function
 * will also help us add the f32 constant to the function as a local function reference
 */
static inline three_addr_var_t* emit_f32_local_constant(cfg_t* cfg, generic_ast_node_t* const_node){
	//Let's create the local constant first.
	local_constant_t* local_constant = f32_local_constant_alloc(const_node->inferred_type, const_node->constant_value.float_value);

	//Once this has been made, we can add it to the function
	add_local_constant_to_cfg(cfg, local_constant);

	//Now allocate the variable that will hold this
	three_addr_var_t* local_constant_variable = emit_local_constant_temp_var(local_constant);

	//And give this back
	return local_constant_variable;
}


/**
 * Emit a three_addr_var_t value that is a local constant(.LCx) reference. This helper function
 * will also help us add the f64 constant to the function as a local function reference
 */
static inline three_addr_var_t* emit_f64_local_constant(cfg_t* cfg, generic_ast_node_t* const_node){
	//Let's create the local constant first.
	local_constant_t* local_constant = f64_local_constant_alloc(const_node->inferred_type, const_node->constant_value.double_value);

	//Once this has been made, we can add it to the function
	add_local_constant_to_cfg(cfg, local_constant);

	//Now allocate the variable that will hold this
	three_addr_var_t* local_constant_variable = emit_local_constant_temp_var(local_constant);

	//And give this back
	return local_constant_variable;
}


/**
 * A helper function that will directly emit either an f32 or f64
 * constant value and place said value into the appropriate location
 * for a function. This will return a variable that corresponds
 * to the address load for that new constant(remember we can't put
 * floats in directly)
 */
static inline three_addr_var_t* emit_direct_floating_point_constant(basic_block_t* block, double constant_value, ollie_token_t constant_type){
	three_addr_var_t* local_constant_temp_var;
	local_constant_t* local_constant;

	switch(constant_type){
		case F32:
			//Let's first see if we're able to extract the local constant
			local_constant = get_f32_local_constant(&(cfg->local_f32_constants), constant_value);

			//We had a miss here, so this is a never before seen value that
			//we need to create ourselves
			if(local_constant == NULL){
				//Allocate and add it in
				local_constant = f32_local_constant_alloc(f32, constant_value);
				add_local_constant_to_cfg(cfg, local_constant);
			}

			//Emit the temp var for this local function. Note that this temp
			//var is also how we deal with reference counting for it
			local_constant_temp_var = emit_local_constant_temp_var(local_constant);

			//Emit the load and add it into the block
			instruction_t* f32_lea_load = emit_lea_rip_relative_constant(emit_temp_var(u64), local_constant_temp_var, instruction_pointer_var);
			add_statement(block, f32_lea_load);

			//Now that we have an address, we can get the actual constant out by doing a load
			instruction_t* load_f32 = emit_load_ir_code(emit_temp_var(f32), f32_lea_load->assignee, f32);
			add_statement(block, load_f32);

			//Give back whatever assignee we've got
			return load_f32->assignee;
		
		case F64:
			//Like above let's first try to extract it
			local_constant = get_f64_local_constant(&(cfg->local_f64_constants), constant_value);

			//If we couldn't find it, then we must add it ourselves
			if(local_constant == NULL){
				local_constant = f64_local_constant_alloc(f64, constant_value);
				add_local_constant_to_cfg(cfg, local_constant);
			}

			//Emit the temp var for it. This temp var will also handle all of our
			//reference count tracking
			local_constant_temp_var = emit_local_constant_temp_var(local_constant);

			//Emit the load and add it into the block
			instruction_t* f64_lea_load = emit_lea_rip_relative_constant(emit_temp_var(u64), local_constant_temp_var, instruction_pointer_var);
			add_statement(block, f64_lea_load);

			//Now that we have an address, we can get the actual constant out by doing a load
			instruction_t* load_f64 = emit_load_ir_code(emit_temp_var(f64), f64_lea_load->assignee, f64);
			add_statement(block, f64_lea_load);

			//Give back whatever assignee we've got
			return load_f64->assignee;

		default:
			printf("Fatal internal compiler error: attempt to allocate a non-float constant in the floating point allocator\n");
			exit(1);
	}
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

	//Add it into the function's block array
	dynamic_array_add(current_function_blocks, created);

	//Give it back
	return created;
}


/**
 * Allocate a basic block that comes from a user-defined label statement
*/
static basic_block_t* labeled_block_alloc(symtab_label_record_t* label){
	//Allocate the block
	basic_block_t* created = calloc(1, sizeof(basic_block_t));

	//Put the block ID in even though it is a labeled block
	created->block_id = increment_and_get();

	//We'll mark this to indicate that this is a labeled block
	created->block_type = BLOCK_TYPE_LABEL;

	//What is the estimated execution cost of this block? Rely on the nesting stack
	//to do this
	created->estimated_execution_frequency = get_estimated_execution_frequency_from_nesting_stack(&nesting_stack);

	//Let's add in what function this block came from
	created->function_defined_in = current_function;

	/**
	 * We will store the block itself alongside with the label. This
	 * will make lookup/cross reference easier when we have to
	 * match jump statements to labels
	 */
	label->block = created;

	//Add this into the dynamic array
	dynamic_array_add(&(cfg->created_blocks), created);

	//Add it into the function's block array
	dynamic_array_add(current_function_blocks, created);

	//Give it back
	return created;
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
 *
 * NOTE: This assumes that the caller has already wiped the function's visited
 * status clean
 */
dynamic_array_t compute_post_order_traversal(basic_block_t* entry){
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
static inline void print_cfg_message(error_message_type_t message_type, char* info, u_int32_t line_number){
	//Now print it
	//Mapped by index to the enum values
	const char* type[] = {"WARNING", "ERROR", "INFO", "DEBUG"};

	//Print this out on a single line
	fprintf(stdout, "\n[LINE %d: COMPILER %s]: %s\n", line_number, type[message_type], info);
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
			//Now the block name
			printf("%s", block->function_defined_in->func_name.string);
			break;

		//By default it's just the .L printing
		default:
			printf(".L%d", block->block_id);
	
	}


	//Now, we will print all of the active variables that this block has
	if(block->used_before_definition.current_index != 0){
		printf("(");

		//Run through all of the live variables and print them out
		for(u_int16_t i = 0; i < block->used_before_definition.current_index; i++){
			//Print it out
			print_variable(stdout, block->used_before_definition.internal_array[i], PRINTING_VAR_BLOCK_HEADER);

			//If it isn't the very last one, we need a comma
			if(i != block->used_before_definition.current_index - 1){
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
	if(block->assigned_variables.current_index != 0){
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
		print_parse_message(MESSAGE_TYPE_ERROR, "NULL BASIC BLOCK FOUND", 0);
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
		print_parse_message(MESSAGE_TYPE_ERROR, "NULL BASIC BLOCK FOUND", 0);
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
	if(B->immediate_postdominator != NULL){
		return B->immediate_postdominator;
	}

	//First we'll wipe the queue clean
	heap_queue_clear(&traversal_queue);

	//The visited array
	dynamic_array_t visited = dynamic_array_alloc();

	//Save this for when we find it
	basic_block_t* ipdom = NULL;

	//Extract the postdominator set
	dynamic_array_t postdominator_set = B->postdominator_set;

	//Seed the search with B
	enqueue(&traversal_queue, B);

	//So long as the queue isn't empty
	while(queue_is_empty(&traversal_queue) == FALSE){
		//Pop off of the queue
		basic_block_t* current = dequeue(&traversal_queue);

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
				enqueue(&traversal_queue, successor);
			}
		}
	}

	//Destroy visited
	dynamic_array_dealloc(&visited);

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
static inline void calculate_dominance_frontiers(dynamic_array_t* function_blocks){
	//Our metastructure will come in handy here, we'll be able to run through 
	//node by node and ensure that we've gotten everything
	
	//Run through every block
	for(u_int32_t i = 0; i < function_blocks->current_index; i++){
		//Grab this from the array
		basic_block_t* block = dynamic_array_get_at(function_blocks, i);

		//If we have less than 2 successors,the rest of 
		//the search here is useless
		if(block->predecessors.internal_array == NULL || block->predecessors.current_index < 2){
			//Hop out here, there is no need to analyze further
			continue;
		}

		//A cursor for traversing our predecessors
		basic_block_t* cursor;

		//Now we run through every predecessor of the block
		for(u_int32_t i = 0; i < block->predecessors.current_index; i++){
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
static inline void calculate_reverse_dominance_frontiers(dynamic_array_t* function_blocks){
	//Run through every block
	for(u_int32_t i = 0; i < function_blocks->current_index; i++){
		//Grab this from the array
		basic_block_t* block = dynamic_array_get_at(function_blocks, i);

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
static void calculate_postdominator_sets(basic_block_t* function_entry_block, dynamic_array_t* function_blocks){
	//Reset the function visited status
	reset_visit_status_for_function(function_blocks);
	//The current block
	basic_block_t* current;

	//We'll first initialize everything here
	for(u_int32_t i = 0; i < function_blocks->current_index; i++){
		//Grab the block out
		current = dynamic_array_get_at(function_blocks, i);

		//If it's an exit block, then it's postdominator set just has itself
		if(current->block_type == BLOCK_TYPE_FUNC_EXIT){
			//If it's an exit block, then this set just contains itself
			current->postdominator_set = dynamic_array_alloc();
			//Add the block to it's own set
			dynamic_array_add(&(current->postdominator_set), current);

		} else {
			//If it's not an exit block, then we set this to be the entire body of blocks
			current->postdominator_set = clone_dynamic_array(function_blocks);
		}
	}

	//Copy over for our current function block
	basic_block_t* current_function_block = function_entry_block;

	//Have we seen a change
	u_int8_t changed;
	
	//Now we will go through everything in this blocks reverse post order set
	do {
		//By default, we'll assume there was no change
		changed = FALSE;

		//Now for each basic block in the reverse post order set
		for(u_int32_t _ = 0; _ < current_function_block->reverse_post_order.current_index; _++){
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
static void calculate_dominator_sets(basic_block_t* function_entry_block, dynamic_array_t* function_blocks){
	//Every node in the CFG has a dominator set that is set
	//to be identical to the list of all nodes
	for(u_int16_t i = 0; i < function_blocks->current_index; i++){
		//Grab this out
		basic_block_t* block = dynamic_array_get_at(function_blocks, i);

		//We will initialize the block's dominator set to be the entire set of nodes
		//in the given function
		block->dominator_set = clone_dynamic_array(function_blocks);
	}

	//Initialize a "worklist" dynamic array for this particular function
	dynamic_array_t worklist = dynamic_array_alloc();

	//Add this into the worklist as a seed
	dynamic_array_add(&worklist, function_entry_block);
	
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
 * Add a variable into the USE set *if* it's appropriate. Remember that we do not care
 * about temporary variables here, and we need to ensure that this variable is not
 * also in the DEF set when we're adding this, because USE specifically is for
 * variables that are used in a block *before* they're defined in the block
 */
static void add_variable_to_use_set(three_addr_var_t* variable, basic_block_t* block){
	//Is the variable NULL? If so then return
	if(variable == NULL){
		return;
	}

	//We do not need to bother tracking these variables - they are a sure thing
	if(variable == instruction_pointer_var || variable == stack_pointer_variable){
		return;
	}

	//Update the USE count regardless
	variable->use_count++;

	/**
	 * If we have variables that are temporary or "memory addresses", then
	 * they are not going to change so we do not need to track them
	 */
	switch(variable->variable_type){
		case VARIABLE_TYPE_TEMP:
		case VARIABLE_TYPE_MEMORY_ADDRESS:
		case VARIABLE_TYPE_FUNCTION_ADDRESS:
		case VARIABLE_TYPE_STACK_PARAM_MEMORY_ADDRESS:
		case VARIABLE_TYPE_LOCAL_CONSTANT:
			return;
		default:
			break;
	}

	//Extract the two sets we'll be working with
	dynamic_array_t* def_set = &(block->assigned_variables);
	dynamic_array_t* use_set = &(block->used_before_definition);

	//Otherwise, let's make sure it's not also in DEF
	for(u_int32_t i = 0; i < def_set->current_index; i++){
		//Grab it out
		three_addr_var_t* defined = dynamic_array_get_at(def_set, i);

		//It's been defined in this block, so we don't care
		if(variables_equal_no_ssa(defined, variable, TRUE) == TRUE){
			return;
		}
	}

	//Otherwise, we need to add this into the USE set *if* it's unique. We don't want to add
	//things more than once
	for(u_int32_t i = 0; i < use_set->current_index; i++){
		//Grab it out
		three_addr_var_t* used = dynamic_array_get_at(use_set, i);

		//If it's already been used then we don't need to care
		if(variables_equal_no_ssa(used, variable, TRUE) == TRUE){
			return;
		}
	}

	//If we make it all of the way down here, then we can add it
	dynamic_array_add(use_set, variable);
}



/**
 * Add a variable into the DEF set. Unlike the use set, the only thing that we need to check and make sure of here
 * is that the variable isn't already in there
 */
static inline void add_variable_to_def_set(three_addr_var_t* variable, basic_block_t* block){
	//Is the variable NULL? If so then return
	if(variable == NULL){
		return;
	}

	//We do not need to bother tracking these variables - they are a sure thing
	if(variable == instruction_pointer_var || variable == stack_pointer_variable){
		return;
	}

	/**
	 * If we have variables that are temporary or "memory addresses", then
	 * they are not going to change so we do not need to track them
	 */
	switch(variable->variable_type){
		case VARIABLE_TYPE_TEMP:
		case VARIABLE_TYPE_MEMORY_ADDRESS:
		case VARIABLE_TYPE_FUNCTION_ADDRESS:
		case VARIABLE_TYPE_STACK_PARAM_MEMORY_ADDRESS:
		case VARIABLE_TYPE_LOCAL_CONSTANT:
			return;
		default:
			break;
	}

	//Extract the set that we'll be working with
	dynamic_array_t* def_set = &(block->assigned_variables);

	//Otherwise, let's make sure it's not also in DEF
	for(u_int32_t i = 0; i < def_set->current_index; i++){
		//Grab it out
		three_addr_var_t* defined = dynamic_array_get_at(def_set, i);

		//It's been defined in this block, so we don't care
		if(variables_equal_no_ssa(defined, variable, TRUE) == TRUE){
			return;
		}
	}

	//If we make it all of the way down here, then we can add it
	dynamic_array_add(def_set, variable);
}




/**
 * Compute the USE and DEF sets for every single block inside of a function
 *
 * USE[b] -> the set of all variables that are used *before assignment* in block b
 * DEF[b] -> the set of all variables that are defined(assigned) in block b
 *
 * Algorithm comute_use_def_sets:
 * 	for each block b in a function:
 * 		USE[b] <- {}
 * 		DEF[b] <- {}
 *
 * 		for each instruction i in b:
 * 			for each variable v that i uses:
 * 				if(v not in DEF[b]):
 * 					USE[b] U {v}
 *
 * 			for each variable v that i defines:
 * 				DEF[b] U {v}
 *
 *
 * We will of course need to make some special caveats here like for example function
 * entry blocks with functino parameters. Those parameters really were assigned
 * at the very top, but we just didn't see it
 */
static void compute_use_and_def_sets_for_function(dynamic_array_t* function_blocks){
	//For every single block in the set of all function blocks
	for(u_int32_t i = 0; i < function_blocks->current_index; i++){
		//Grab the block out
		basic_block_t* block = dynamic_array_get_at(function_blocks, i);

		//Let's allocate the USE/DEF sets for each block
		block->used_before_definition = dynamic_array_alloc();
		block->assigned_variables = dynamic_array_alloc();

		//Now grab a cursor for our instruction call
		instruction_t* cursor = block->leader_statement;

		//Run through everything
		while(cursor != NULL){
			switch(cursor->statement_type){
				//Function calls have parameters - so special case
				case THREE_ADDR_CODE_FUNC_CALL:
					//Run through the params and add them
					for(u_int32_t _ = 0; _ < cursor->parameters.current_index; _++){
						//Grab the param out
						three_addr_var_t* parameter = dynamic_array_get_at(&(cursor->parameters), _);

						//Add it into the USE set
						add_variable_to_use_set(parameter, block);

					}

					//Add the DEF var in
					add_variable_to_def_set(cursor->assignee, block);

					break;

				//Same for indirect function calls - also have params
				case THREE_ADDR_CODE_INDIRECT_FUNC_CALL:
					//Indirect function calls also have their op1's used
					add_variable_to_use_set(cursor->op1, block);

					//Run through the params and add them
					for(u_int32_t _ = 0; _ < cursor->parameters.current_index; _++){
						//Grab the param out
						three_addr_var_t* parameter = dynamic_array_get_at(&(cursor->parameters), _);

						//Add it into the USE set
						add_variable_to_use_set(parameter, block);
					}

					//Add the DEF var in
					add_variable_to_def_set(cursor->assignee, block);

					break;

				//For STOREs, the assignee is a memory address, so it's actually
				//used but not defined
				case THREE_ADDR_CODE_STORE_STATEMENT:
				case THREE_ADDR_CODE_STORE_WITH_CONSTANT_OFFSET:
				case THREE_ADDR_CODE_STORE_WITH_VARIABLE_OFFSET:
					//Op1/Op2 go into use if they exist
					add_variable_to_use_set(cursor->op1, block);
					add_variable_to_use_set(cursor->op2, block);
					
					//Assignee is defined in this unique case
					add_variable_to_use_set(cursor->assignee, block);

					break;

				//In the default case, we just add the USE/DEF for each 
				//variable that we can see
				default:
					//Op1/Op2 go into use if they exist
					add_variable_to_use_set(cursor->op1, block);
					add_variable_to_use_set(cursor->op2, block);
					
					//The assignee is in the def set
					add_variable_to_def_set(cursor->assignee, block);
					break;
			}

			//Bump it up
			cursor = cursor->next_statement;
		}
	}
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
 * This assumes that the USE and DEF sets have already been computed for each
 * and every block. Remember that USE is the set of all variables that are
 * used *before* being assigned in block b and DEF is the set of all variables that
 * are assigned inside of the block. If these sets are wrong, then the liveness
 * will be way overblown
 *
 * Algorithm calculateLiveness:
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
 */
static void calculate_liveness_sets(dynamic_array_t* function_blocks, basic_block_t* function_entry_block){
	//Reset the visited status for the function
	reset_visit_status_for_function(function_blocks);
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
	//Run the algorithm until we have no difference found
	do {
		//We'll assume we didn't find a difference each iteration
		difference_found = FALSE;

		//Now we can go through the entire RPO set
		for(u_int16_t _ = 0; _ < function_entry_block->reverse_post_order_reverse_cfg.current_index; _++){
			//The current block is whichever we grab
			current = dynamic_array_get_at(&(function_entry_block->reverse_post_order_reverse_cfg), _);

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
			current->live_in = clone_dynamic_array(&(current->used_before_definition));

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


/**
 * Build the dominator tree for a given function's blocks
 */
static inline void build_dominator_trees(dynamic_array_t* function_blocks){
	//For each node in the function, we will use that node's immediate dominators to
	//build a dominator tree
	
	//Hold the current block
	basic_block_t* current;

	//For each block in the CFG
	for(int32_t _ = function_blocks->current_index - 1; _ >= 0; _--){
		//Grab out whatever block we're on
		current = dynamic_array_get_at(function_blocks, _);

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
						if(symtab_record_variable_dynamic_array_contains(&(df_node->used_before_definition), record) == NOT_FOUND
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
		for(u_int16_t i = 0; i < function_defined_in->function_parameters.current_index; i++){
			//make the new name here
			lhs_new_name_direct(dynamic_array_get_at(&(function_defined_in->function_parameters), i));
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
				if(is_variable_ssa_eligible(cursor->op1) == TRUE){
					rhs_new_name(cursor->op1);
				}

				//Same goes for the assignee, except this one is the LHS
				if(is_variable_ssa_eligible(cursor->assignee) == TRUE){
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
					if(is_variable_ssa_eligible(current_param) == TRUE){
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
				if(is_variable_ssa_eligible(cursor->op1) == TRUE){
					rhs_new_name(cursor->op1);
				}

				//If we have a non-temp variable, rename it
				if(is_variable_ssa_eligible(cursor->op2) == TRUE){
					rhs_new_name(cursor->op2);
				}

				//UNIQUE CASE - rhs also gets a new name here
				if(is_variable_ssa_eligible(cursor->assignee) == TRUE){
					rhs_new_name(cursor->assignee);
				}

				break;

			//And now if it's anything else that has an assignee, operands, etc,
			//we'll need to rewrite all of those as well
			//We'll exclude direct jump statements, these we don't care about
			default:
				//If we have a non-temp variable, rename it
				if(is_variable_ssa_eligible(cursor->op1) == TRUE){
					rhs_new_name(cursor->op1);
				}

				//If we have a non-temp variable, rename it
				if(is_variable_ssa_eligible(cursor->op2) == TRUE){
					rhs_new_name(cursor->op2);
				}

				//Same goes for the assignee, except this one is the LHS
				if(is_variable_ssa_eligible(cursor->assignee) == TRUE){
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
		for(u_int16_t i = 0; i < function_defined_in->function_parameters.current_index; i++){
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
				if(is_variable_ssa_eligible(cursor->assignee) == TRUE){
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
static inline void rename_all_variables(cfg_t* cfg){
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
 *
 * my_ptr++ will become my_ptr = my_ptr + ____
 */
static three_addr_var_t* handle_pointer_arithmetic(basic_block_t* basic_block, ollie_token_t operator, three_addr_var_t* assignee){
	//Emit the constant size
	three_addr_const_t* constant = emit_direct_integer_or_char_constant(assignee->type->internal_types.points_to->type_size, u64);

	//Decide what the op is
	ollie_token_t op = operator == PLUSPLUS ? PLUS : MINUS;

	//We need to emit a temp assignment for the assignee
	instruction_t* operation = emit_binary_operation_with_const_instruction(assignee, emit_var_copy(assignee), op, constant);

	//Add this to the block
	add_statement(basic_block, operation);

	//Give back the assignee
	return assignee;
}


/**
 * Emit the appropriate address calculation for a given array member, based on what is given in the parameters. This will
 * result in either a lea or a binary operation and then a lea
 */
static three_addr_var_t* emit_array_address_calculation(basic_block_t* basic_block, three_addr_var_t* base_addr, three_addr_var_t* offset, generic_type_t* member_type){
	//We need a new temp var for the assignee. We know it's an address always
	three_addr_var_t* assignee = emit_temp_var(i64);

	//Is this a lea compatible power of 2? If so we will use the lea shortcut
	if(is_lea_compatible_power_of_2(member_type->type_size) == TRUE){
		//Let the helper emit the lea
		instruction_t* address_calculation = emit_lea_multiplier_and_operands(assignee, base_addr, offset, member_type->type_size);

		//Get this into the block
		add_statement(basic_block, address_calculation);

	//Otherwise, we can't fully do a lea here so we'll need to instead
	//use a binary operation to multiply followed by a different kind of lea
	} else {
		//We'll need the size to multiply by
		three_addr_const_t* type_size = emit_direct_integer_or_char_constant(member_type->type_size, u64);

		//Let the helper emit the entire thing. We'll store into a temp var there
		three_addr_var_t* final_offset = emit_binary_operation_with_constant(basic_block, emit_temp_var(u64), offset, STAR, type_size);

		//And now that we have the incompatible multiplication over with, we can use a lea to add
		instruction_t* lea_statement = emit_lea_operands_only(assignee, base_addr, final_offset);

		//Insert into the block
		add_statement(basic_block, lea_statement);
	}

	//Whatever happened return the assignee
	return assignee;
}


/**
 * Emit a struct access lea statement
 */
static three_addr_var_t* emit_struct_address_calculation(basic_block_t* basic_block, generic_type_t* struct_type, three_addr_var_t* current_offset, three_addr_const_t* offset){
	//We need a new temp var for the assignee. We know it's an address always
	three_addr_var_t* assignee = emit_temp_var(struct_type);

	//Use the lea helper to emit this
	instruction_t* stmt = emit_lea_offset_only(assignee, current_offset, offset);

	//Now add the statement into the block
	add_statement(basic_block, stmt);

	//And give back the assignee
	return assignee;
}


/**
 * Emit an indirect jump statement
 */
static three_addr_var_t* emit_indirect_jump_address_calculation(basic_block_t* basic_block, jump_table_t* initial_address, three_addr_var_t* mutliplicand){
	//We'll need a new temp var for the assignee
	three_addr_var_t* assignee = emit_temp_var(u64);

	//Use the helper to emit it - type size is 8 because it's an address
	instruction_t* stmt = emit_indir_jump_address_calc_instruction(assignee, initial_address, mutliplicand, 8);

	//Add it in
	add_statement(basic_block, stmt);

	//Give back the assignee
	return assignee;
}


/**
 * Directly emit the assembly nop instruction
 */
static inline void emit_idle(basic_block_t* basic_block){
	//Use the helper
	instruction_t* idle_stmt = emit_idle_instruction();
	
	//Add it into the block
	add_statement(basic_block, idle_stmt);
}


/**
 * Directly emit the assembly code for an inlined statement. Users who write assembly inline
 * want it directly inserted in order, nothing more, nothing less
 */
static inline void emit_assembly_inline(basic_block_t* basic_block, generic_ast_node_t* asm_inline_node){
	//First we allocate the whole thing
	instruction_t* asm_inline_stmt = emit_asm_inline_instruction(asm_inline_node); 
	
	//Once done we add it into the block
	add_statement(basic_block, asm_inline_stmt);
}


/**
 * Emit the abstract machine code for a return statement
 */
static cfg_result_package_t emit_return(basic_block_t* basic_block, generic_ast_node_t* ret_node){
	//For holding our temporary return variable
	cfg_result_package_t return_package = INITIALIZE_BLANK_CFG_RESULT;

	//Keep track of a current block here for our purposes
	basic_block_t* current = basic_block;

	//This is what we'll be using to return
	three_addr_var_t* return_variable = NULL;

	/**
	 * If the ret node's first child is not null, we'll let the expression rule
	 * handle it. We'll always do an assignment here because return statements present
	 * a special case. We always need our return variable to be in %rax, and that may
	 * not happen all the time naturally. As such, we need this assignment here
	 */
	if(ret_node->first_child != NULL){
		//Perform the binary operation here
		cfg_result_package_t expression_package = emit_expression(current, ret_node->first_child);

		//Update the current block
		current = expression_package.final_block;

		/**
		 * Since we have a result that could either be a constant or a variable,
		 * we will need to unpack the result here and act accordingly
		 */
		switch(expression_package.type){
			case CFG_RESULT_TYPE_VAR:
				//Grab the return var that we need
				return_variable = expression_package.result_value.result_var;

				/**
				 * If the variable is not a temp *or* we have a need for a converting move, we will emit
				 * the extra assignment here. If it's already temp and we don't need a converting move, we won't
				 * bother with inserting the extra statements
				 */
				if(return_variable->variable_type != VARIABLE_TYPE_TEMP
					|| is_converting_move_required(ret_node->inferred_type, return_variable->type) == TRUE){
					/**
					 * The type of this final assignee will *always* be the inferred type of the node. We need to ensure that
					 * the function is returning the type as promised, and not what is done through type coercion
					 */
					instruction_t* assignment = emit_assignment_instruction(emit_temp_var(ret_node->inferred_type), return_variable);

					//Add it into the block
					add_statement(current, assignment);

					//The return variable is now what was assigned
					return_variable	= assignment->assignee;
				}

				break;

			case CFG_RESULT_TYPE_CONST:
				//For a const type we'll need our own returned variable
				return_variable = emit_temp_var(ret_node->inferred_type);

				//Emit the assignment that we need
				instruction_t* const_assignment = emit_assignment_with_const_instruction(return_variable, expression_package.result_value.result_const);

				//And throw the assignment into the block
				add_statement(current, const_assignment);

				break;
		}

		//Flag for later on in the compiler that this variable has been returned
		return_variable->membership = RETURNED_VARIABLE;
	}

	//We'll use the ret stmt feature here
	instruction_t* ret_stmt = emit_ret_instruction(return_variable);

	//Once it's been emitted, we'll add it in as a statement
	add_statement(current, ret_stmt);

	//This is always a predecessor of the function exit statement
	add_successor(current, function_exit_block);

	//Update the final block
	return_package.final_block = current;

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
 * Is an operator short circuit eligible? Return TRUE if yes, FALSE
 * if no
 */
static inline u_int8_t is_op_short_circuit_eligible(ollie_token_t op){
	switch(op){
		case DOUBLE_AND:
		case DOUBLE_OR:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Does a given operator set condition codes? Return true if yes, false
 * if no. Yes all ops can set condition codes, but we're referring to
 * specifically relational operators here
 */
static inline u_int8_t does_operator_set_condition_codes(ollie_token_t op){
	switch(op){
		case L_THAN:
		case L_THAN_OR_EQ:
		case G_THAN:
		case G_THAN_OR_EQ:
		case DOUBLE_EQUALS:
		case NOT_EQUALS:
		//Logical not sets it
		case EXCLAMATION:
			return TRUE;
		default:
			return FALSE;
	}
}


/**
 * Emit a test instruction. Note that this is different depending on what kind of testing that we're doing(GP vs SSE)
 *
 * Note that for the operator input, we will use this to modify the given operator *if* we have a floating point operation.
 * This is because the eventual selected code for floating point will turn if(x) into if(x != 0) essentially, so we need to
 * have that logic already in for when the branch statements are selected
 */
static inline three_addr_var_t* emit_test_not_zero(basic_block_t* basic_block, three_addr_var_t* tested_variable, ollie_token_t* operator){
	//If we don't have a temp var, then we just go right to the emission
	if(tested_variable->variable_type != VARIABLE_TYPE_TEMP
		|| IS_FLOATING_POINT(tested_variable->type) == TRUE){

		//Emit the instruction
		instruction_t* test_if_not_zero = emit_test_if_not_zero_statement(emit_temp_var(u8), tested_variable);

		//Now we'll add it into the block
		add_statement(basic_block, test_if_not_zero);

		/**
		 * If this is a floating point variable, update the pass-by-reference
		 * operator
		 */
		if(IS_FLOATING_POINT(tested_variable->type) == TRUE){
			//This is needed for branching later on
			*operator = NOT_EQUALS;

			//Flag that this comes out of an FP comparsion
			test_if_not_zero->assignee->comes_from_fp_comparison = TRUE;
		}

		//Give back the final assignee
		return test_if_not_zero->assignee;

	/**
	 * Otherwise if we have a temp variable, we should look to see if this is really
	 * a constant assignment. If it is a constant then the constant that was assigned
	 * will be the last statement in the current block, and its assignee will be equal
	 * to the result that we got
	 */
	} else {
		if(basic_block->exit_statement != NULL
	 		&& basic_block->exit_statement->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT
			&& variables_equal(basic_block->exit_statement->assignee, tested_variable, FALSE) == TRUE){
			//Use the constant enhancment to make this happen
			instruction_t* test_if_not_zero = emit_test_if_not_zero_for_const_statement(emit_temp_var(u8), basic_block->exit_statement->op1_const);

			//Add it in
			add_statement(basic_block, test_if_not_zero);

			//Give back the result
			return test_if_not_zero->assignee;

		} else {
			//Emit the instruction
			instruction_t* test_if_not_zero = emit_test_if_not_zero_statement(emit_temp_var(u8), tested_variable);

			//Now we'll add it into the block
			add_statement(basic_block, test_if_not_zero);

			//Give back the final assignee
			return test_if_not_zero->assignee;
		}
	}
}


/**
 * Emit the conditional for a branch. This can be a ternary or binary expression
 */
static inline cfg_result_package_t emit_branch_conditional_expression(basic_block_t* starting_block, generic_ast_node_t* branch_node){
	if(branch_node->ast_node_type == AST_NODE_TYPE_TERNARY_EXPRESSION){
		return emit_ternary_expression(starting_block, branch_node);
	} else {
		return emit_binary_expression(starting_block, branch_node);
	}
}


/**
 * Is a branch conditional always true? We can detect this by determining if something is a plain
 * constant(think while(true)) or not.
 */
static inline branch_conditional_truthfullness_t get_branch_conditional_truthfullness(generic_ast_node_t* branch_conditional){
	//Not a constant then don't care, just get out
	if(branch_conditional->ast_node_type != AST_NODE_TYPE_CONSTANT){
		return BRANCH_CONDITIONAL_UNKNOWN;
	}

	/**
	 * If we know the result is 0, then we're always false. Otherwise
	 * we know it's always true
	 */
	if(is_constant_node_value_0(branch_conditional) == TRUE){
		return BRANCH_CONDITIONAL_ALWAYS_FALSE;
	} else {
		return BRANCH_CONDITIONAL_ALWAYS_TRUE;
	}
}


/**
 * Emit a branch statement with an if destination and else destination. We will also handle the emittal of the conditional node here
 * for the purposes of doing the branch. The returned result package will include the starting & ending blocks for the branch. The
 * branch category is also given and will be used when we're accounting for how to place things
 *
 * The "virtual_out_edge_required" will be used for loop statements, where we require, for dominator analysis, that a virtual
 * edge(not a jump, but a successor) be added *if* we have a nonterminating loop(always true/false loop statement)
 */
static cfg_result_package_t emit_branch(basic_block_t* starting_block, generic_ast_node_t* conditional_node, basic_block_t* if_block, basic_block_t* else_block, branch_category_t branch_category, u_int8_t virtual_out_edge_required){
	instruction_t* constant_assignment;

	//Allcoate the results
	cfg_result_package_t results = INITIALIZE_BLANK_CFG_RESULT;

	//Keep track of the current block
	basic_block_t* current_block = starting_block;

	/**
	 * The first thing that we need to do is emit the conditional. However - this conditional
	 * could potentially be something that we'd like to short circuit(&& / ||). We assume that 
	 * this doesn't happen most of the time, but when it does, we will implement the
	 * short circuit optimization here
	 */
	if(is_op_short_circuit_eligible(conditional_node->binary_operator) == FALSE){
		/**
		 * If we are able to off the bat determine whether or not this is true, then this
		 * branch itself is going to be useless. Instead of emitting things, we will just
		 * optimize as we go right here and jump either to if or else based on
		 * the branch type and trutfulness
		 */
		branch_conditional_truthfullness_t truthfullness = get_branch_conditional_truthfullness(conditional_node);

		switch(truthfullness){
			/**
			 * We have something that is provable to always be true. 
			 *
			 * If regular branch -> jump to if always
			 * If inverse branch -> jump to else always
			 *
			 * The "virtual_out_edge_required" concept has to do with the way that
			 * we currently handle postdominator analysis. If we have an infinite loop,
			 * the analysis will fail. We can fix this by adding virtual out edges
			 * when we do have such cases. These add successors, but they do not add jumps.
			 * This allows us to maintain the loop properties without looping forever
			 */
			case BRANCH_CONDITIONAL_ALWAYS_TRUE:
				if(branch_category == BRANCH_CATEGORY_NORMAL){
					emit_jump(current_block, if_block);

					if(virtual_out_edge_required == TRUE){
						add_successor(current_block, else_block);
					}

				} else {
					emit_jump(current_block, else_block);

					if(virtual_out_edge_required == TRUE){
						add_successor(current_block, if_block);
					}
				}

				return results;

			/**
			 * We have something that is provable to always be false. 
			 *
			 * If regular branch -> jump to else always
			 * If inverse branch -> jump to if always
			 */
			case BRANCH_CONDITIONAL_ALWAYS_FALSE:
				if(branch_category == BRANCH_CATEGORY_NORMAL){
					emit_jump(current_block, else_block);

					if(virtual_out_edge_required == TRUE){
						add_successor(current_block, if_block);
					}

				} else {
					emit_jump(current_block, if_block);

					if(virtual_out_edge_required == TRUE){
						add_successor(current_block, else_block);
					}
				}

				return results;

			//Anything else it's impossible to determine so we process normally
			default:
				break;
		}

		//First let the helper emit it
		cfg_result_package_t binary_results = emit_branch_conditional_expression(current_block, conditional_node);

		//Update the final block
		current_block = binary_results.final_block;

		//The conditional decider comes from the result type itself
		three_addr_var_t* conditional_decider;

		switch(binary_results.type){
			//For a constant type, we are going to need to emit an assignment
			case CFG_RESULT_TYPE_CONST:
				constant_assignment = emit_assignment_with_const_instruction(emit_temp_var(binary_results.result_value.result_const->type), binary_results.result_value.result_const);

				//Get it in the block
				add_statement(current_block, constant_assignment);

				//This now is our decider
				conditional_decider = constant_assignment->assignee;

				break;

			//For this we can extract the result var
			case CFG_RESULT_TYPE_VAR:
				conditional_decider = binary_results.result_value.result_var;

				break;
		}

		/**
		 * If the given operator does not set condition codes appropriately, then
		 * we'll need to make that happen here. We pass the operator
		 * in by reference so that we may change it later on
		 */
		if(does_operator_set_condition_codes(binary_results.operator) == FALSE){
			conditional_decider = emit_test_not_zero(current_block, conditional_decider, &(binary_results.operator));
		}

		/**
		 * Let's try to grab the final result type. Remember that the comparison type may be different than
		 * the actual assignee type. If we can grab it then we will use that, otherwise we will use
		 * the assignee type
		 */
		u_int8_t type_signed;
		if(current_block->exit_statement != NULL
			&& is_binary_operation(current_block->exit_statement)
			&& variables_equal(current_block->exit_statement->assignee, conditional_decider, FALSE) == TRUE){

			//If we have a result type use that, otherwise take from op1
			if(current_block->exit_statement->type_storage.result_type != NULL){
				type_signed = is_type_signed(current_block->exit_statement->type_storage.result_type);
			} else {
				type_signed = is_type_signed(current_block->exit_statement->op1->type);
			}

		} else {
			type_signed = is_type_signed(conditional_decider->type);
		}

		//Flag that this sets condition codes
		conditional_decider->sets_cc = TRUE;

		//Now let's try to decide the branch type
		branch_type_t branch_type = select_appropriate_branch_statement(binary_results.operator, branch_category, type_signed);

		//Now we can finall spit this one out
		instruction_t* branch_statement = emit_branch_statement(if_block, else_block, conditional_decider, branch_type);

		//Add it into the block
		add_statement(current_block, branch_statement);

		/**
		 * Based on what the category is, we can add the successors in a different
		 * order just so that the IRs look somewhat nicer. This has no effect on
		 * functionality, purely visual
		 */
		if(branch_category == BRANCH_CATEGORY_NORMAL){
			add_successor(current_block, if_block);
			add_successor(current_block, else_block);
			branch_statement->inverse_branch = FALSE;

		} else {
			add_successor(current_block, else_block);
			add_successor(current_block, if_block);
			branch_statement->inverse_branch = TRUE;
		}

	/**
	 * If we got here then we are short circuiting based on the operator that we were given. The
	 * short circuit process will create several additional blocks that we will use instead of 
	 * just using this one block
	 */
	} else {
		//Create our secondary block to use
		basic_block_t* secondary_block = basic_block_alloc_and_estimate();

		if(conditional_node->binary_operator == DOUBLE_AND){
			/**
			 * 	Regular Branch:
			 * .L2
			 * t5 <- x_0 < 3 
			 * t7 <- x_0 != 1
			 * t5 <- t5 && t7
			 * cbranch_nz .L12 else .L13
			 *
			 * Turn this into:
			 * .L2:
			 * t5 <- x_0 < 3 <---- if this is false, we leave(inverse branch)
			 * cbranch_ge .L13 else .L3
			 *
			 * .L3 <----- The *only* way we get here is if the first condition is true
			 * t7 <- x_0 != 1 <------- If this is true, jump to if(regular branch)
			 * cbranch_ne .L12 else .L13
			 */
			if(branch_category == BRANCH_CATEGORY_NORMAL){
				//Get the first child for the left statement
				generic_ast_node_t* child_cursor = conditional_node->first_child;

				/**
				 * Left side: IF FAIL -> else block, else secondary block
				 */
				emit_branch(current_block, child_cursor, else_block, secondary_block, BRANCH_CATEGORY_INVERSE, virtual_out_edge_required);

				//Current block now is our secondary
				current_block = secondary_block;

				/**
				 * Right side: IF SUCCESS -> if block, else else block
				 */
				cfg_result_package_t right_side_results = emit_branch(current_block, child_cursor->next_sibling, if_block, else_block, BRANCH_CATEGORY_NORMAL, virtual_out_edge_required);

				//Update the current block once again
				current_block = right_side_results.final_block;

			/**
			 * 	Inverse Branch:
			 * .L2
			 * t5 <- x_0 < 3 
			 * t7 <- x_0 != 1
			 * t5 <- t5 && t7
			 * cbranch_z .L12 else .L13 <--- notice how it's branch if zero, we go to if if this fails(hence inverse)
			 *
			 * Turn this into:
			 *
			 * .L2:
			 * t5 <- x_0 < 3 <---- if this doesn't work, we're done. We can go to *if case*
			 * cbranch_ge .L12 else .L3
			 *
			 * .L3 <----- The *only* way we get here is if the first condition is true
			 * t7 <- x_0 != 1 <------- Remember we're looking for a failure, so if this fails to go *if*, otherwise *else*
			 * cbranch_e .L12 else .L13
			 *
			 */
			} else {
				//Get the first child for the left statement
				generic_ast_node_t* child_cursor = conditional_node->first_child;

				/**
				 * Left side: IF FAIL -> if block, else secondary block
				 */
				emit_branch(current_block, child_cursor, if_block, secondary_block, BRANCH_CATEGORY_INVERSE, virtual_out_edge_required);

				//Current block now is our secondary
				current_block = secondary_block;

				/**
				 * Left Side: IF FAIL -> if block, else else block
				 */
				cfg_result_package_t right_side_results = emit_branch(current_block, child_cursor->next_sibling, if_block, else_block, BRANCH_CATEGORY_INVERSE, virtual_out_edge_required);

				//Update the current block once again
				current_block = right_side_results.final_block;
			}

		} else {
			/**
			 * .L5:
			 * t4 <- x + y
			 * t5 <- x > t4
			 * t6 <- y_0 || t5
			 * cbranch_nz .L13 else .L12
			 *
			 * Transforms into
			 *
			 * .L5:
			 * t8 <- test if not zero y_0
			 * cbranch_nz .L13 else .L6 <-- go to if, else second block
			 *
			 * .L6:
			 * t4 <- x + y
			 * t5 <- x > t4
			 * cbranch_nz .L13 else .L12 <-- go to if, else else block
			 */
			if(branch_category == BRANCH_CATEGORY_NORMAL){
				//Get the first child for the left statement
				generic_ast_node_t* child_cursor = conditional_node->first_child;

				/**
				 * Left side: IF SUCCESS -> if block, else secondary block
				 */
				emit_branch(current_block, child_cursor, if_block, secondary_block, BRANCH_CATEGORY_NORMAL, virtual_out_edge_required);

				//Current block now is our secondary
				current_block = secondary_block;

				/**
				 * Left Side: IF SUCCESS -> if block, else else block
				 */
				cfg_result_package_t right_side_results = emit_branch(current_block, child_cursor->next_sibling, if_block, else_block, BRANCH_CATEGORY_NORMAL, virtual_out_edge_required);

				//Update the current block once again
				current_block = right_side_results.final_block;

			/**
			 * .L2
			 * t7 <- b_1 != 0 
			 * t10 <- a_1 < 33
			 * t11 <- t7 || t10
			 * cbranch_z .L9 else .L13 <-- Notice how it goes to if on failure
			 *
			 * Turn this into:
			 *
			 * .L2:
			 * t7 <- b_1 != 0 <--- If this does work, we know that t11 will *not* be zero, so jump to else
			 * cbranch_ne .L13 else .L3
			 *
			 * .L3: <----- We only get here if the first one is false
			 * t10 <- a_1 < 33
			 * cbranch_ge .L9 else .L13 <-- If this also fails, we've satisfied the initial condition
			 */
			} else {
				//Get the first child for the left statement
				generic_ast_node_t* child_cursor = conditional_node->first_child;

				/**
				 * Left side: IF SUCCESS -> else, else secondary block
				 */
				emit_branch(current_block, child_cursor, else_block, secondary_block, BRANCH_CATEGORY_NORMAL, virtual_out_edge_required);

				//Current block now is our secondary
				current_block = secondary_block;

				/**
				 * Left Side: IF FAILS -> if block, else else block
				 */
				cfg_result_package_t right_side_results = emit_branch(current_block, child_cursor->next_sibling, if_block, else_block, BRANCH_CATEGORY_INVERSE, virtual_out_edge_required);

				//Update the current block once again
				current_block = right_side_results.final_block;
			}
		}
	}

	//Update this - odds are it's changed
	results.final_block = current_block;

	//Give back our final results
	return results;
}


/**
 * Emit a user defined jump statement that points to a label, not to a block
 *
 * We'll leave out all of the successor logic here as well, until we reach the end
 */
static cfg_result_package_t emit_user_defined_branch(basic_block_t* starting_block, generic_ast_node_t* conditional_node, symtab_label_record_t* if_destination_label, basic_block_t* else_block){
	instruction_t* constant_assignment;

	//Allcoate the results
	cfg_result_package_t results = INITIALIZE_BLANK_CFG_RESULT;

	/**
	 * We only allocate this storage if it is needed. If we get here and it's unallocated, now
	 * is the time to do that
	 */
	if(current_function_user_defined_jump_statements.internal_array == NULL){
		current_function_user_defined_jump_statements = dynamic_array_alloc();
	}

	//Keep track of the current block
	basic_block_t* current_block = starting_block;

	/**
	 * Case 1: we are not at all eligible to short circuit. If this is the case then
	 * we will just go through the motions here of a regular branch
	 */
	if(is_op_short_circuit_eligible(conditional_node->binary_operator) == FALSE){
		//First let the helper emit it
		cfg_result_package_t conditional_results = emit_branch_conditional_expression(current_block, conditional_node);

		//Update the final block
		current_block = conditional_results.final_block;

		//Extract the actual result assignee
		three_addr_var_t* conditional_decider;

		switch(conditional_results.type){
			//For a constant type, we are going to need to emit an assignment
			case CFG_RESULT_TYPE_CONST:
				constant_assignment = emit_assignment_with_const_instruction(emit_temp_var(conditional_results.result_value.result_const->type), conditional_results.result_value.result_const);

				//Get it in the block
				add_statement(current_block, constant_assignment);

				//This now is our decider
				conditional_decider = constant_assignment->assignee;

				break;

			//For this we can extract the result var
			case CFG_RESULT_TYPE_VAR:
				conditional_decider = conditional_results.result_value.result_var;

				break;
		}

		/**
		 * If the given operator does not set condition codes appropriately, then
		 * we'll need to make that happen here
		 */
		if(does_operator_set_condition_codes(conditional_results.operator) == FALSE){
			conditional_decider = emit_test_not_zero(current_block, conditional_decider, &(conditional_results.operator));
		}

		/**
		 * Let's try to grab the final result type. Remember that the comparison type may be different than
		 * the actual assignee type. If we can grab it then we will use that, otherwise we will use
		 * the assignee type
		 */
		u_int8_t type_signed;
		if(current_block->exit_statement != NULL
			&& is_binary_operation(current_block->exit_statement)
			&& variables_equal(current_block->exit_statement->assignee, conditional_decider, FALSE) == TRUE){

			//If we have a result type use that, otherwise take from op1
			if(current_block->exit_statement->type_storage.result_type != NULL){
				type_signed = is_type_signed(current_block->exit_statement->type_storage.result_type);
			} else {
				type_signed = is_type_signed(current_block->exit_statement->op1->type);
			}

		} else {
			type_signed = is_type_signed(conditional_decider->type);
		}

		//Flag that this sets condition codes
		conditional_decider->sets_cc = TRUE;

		//Now let's try to decide the branch type
		branch_type_t branch_type = select_appropriate_branch_statement(conditional_results.operator, BRANCH_CATEGORY_NORMAL, type_signed);

		//Now we can finally spit this one out
		instruction_t* branch_statement = emit_branch_statement(NULL, else_block, conditional_decider, branch_type);
		
		//Store the if destination for later
		branch_statement->optional_storage.jumping_to_label = if_destination_label;

		//Add it into the block
		add_statement(current_block, branch_statement);

		//Add this into the array for later
		dynamic_array_add(&current_function_user_defined_jump_statements, branch_statement);

	/**
	 * Otherwise we are eligible to short circuit. It is worth noting here that user defined
	 * branches never perform any kind of inverse jumping, so the logic here is going to be simplified
	 * when compared to our regular branch
	 */
	} else {
		//We're going to need a second block so make it now
		basic_block_t* secondary_block = basic_block_alloc_and_estimate();

		/**
		 * Logical and branch:
		 * .L2
		 * t5 <- x_0 < 3 
		 * t7 <- x_0 != 1
		 * t5 <- t5 && t7
		 * cbranch_nz .L12 else .L13
		 *
		 * Turn this into:
		 * .L2:
		 * t5 <- x_0 < 3 <---- if this is false, we leave(inverse branch)
		 * cbranch_ge .L13 else .L3
		 *
		 * .L3 <----- The *only* way we get here is if the first condition is true
		 * t7 <- x_0 != 1 <------- If this is true, jump to if(regular branch)
		 * cbranch_ne .L12 else .L13
		 */
		if(conditional_node->binary_operator == DOUBLE_AND){
			//Get the first child for the left statement
			generic_ast_node_t* child_cursor = conditional_node->first_child;

			/**
			 * Left side: IF FAIL -> else block, else secondary block. We are using the regular branch
			 * emitter here because we are not dealing with any user defined blocks in this branch
			 */
			emit_branch(current_block, child_cursor, else_block, secondary_block, BRANCH_CATEGORY_INVERSE, FALSE);

			//Current block now is our secondary
			current_block = secondary_block;

			/**
			 * Right side: IF SUCCESS -> if block, else else block. We need to use the user defined version for this because
			 * we are still using the if label
			 */
			cfg_result_package_t right_side_results = emit_user_defined_branch(current_block, child_cursor->next_sibling, if_destination_label, else_block);

			//Update the current block once again
			current_block = right_side_results.final_block;

		/**
		 * Logical or branch
		 *
		 * .L5:
		 * t4 <- x + y
		 * t5 <- x > t4
		 * t6 <- y_0 || t5
		 * cbranch_nz .L13 else .L12
		 *
		 * Transforms into
		 *
		 * .L5:
		 * t8 <- test if not zero y_0
		 * cbranch_nz .L13 else .L6 <-- go to if, else second block
		 *
		 * .L6:
		 * t4 <- x + y
		 * t5 <- x > t4
		 * cbranch_nz .L13 else .L12 <-- go to if, else else block
		 */
		} else {
			//Get the first child for the left statement
			generic_ast_node_t* child_cursor = conditional_node->first_child;

			/**
			 * Left side: IF SUCCESS -> if block, else secondary block. We need to use the user
			 * defined version for this because we are going to a label
			 */
			emit_user_defined_branch(current_block, child_cursor, if_destination_label, secondary_block);

			//Current block now is our secondary
			current_block = secondary_block;

			/**
			 * Left Side: IF SUCCESS -> if block, else else block. We need to use our user defined block
			 * helper for this because we are using the destination label
			 */
			cfg_result_package_t right_side_results = emit_user_defined_branch(current_block, child_cursor->next_sibling, if_destination_label, else_block);

			//Update the current block once again
			current_block = right_side_results.final_block;
		}
	}

	//Update the final block
	results.final_block = current_block;

	return results;
}


/**
 * Emit a user defined jump statement that points to a label, not to a block
 */
static inline void emit_user_defined_jump(basic_block_t* basic_block, symtab_label_record_t* jump_target){
	/**
	 * We only allocate this storage if it is needed. If we get here and it's unallocated, now
	 * is the time to do that
	 */
	if(current_function_user_defined_jump_statements.internal_array == NULL){
		current_function_user_defined_jump_statements = dynamic_array_alloc();
	}

	//Allocate it
	instruction_t* jump_statement = emit_jmp_instruction(NULL);

	//Jumps to the jump target
	jump_statement->optional_storage.jumping_to_label = jump_target; 

	//Add this to the array of user defined jumps
	dynamic_array_add(&current_function_user_defined_jump_statements, jump_statement);

	//Add this into the first block
	add_statement(basic_block, jump_statement);
}


/**
 * Emit an indirect jump statement
 *
 * Indirect jumps are written in the form:
 * 	jump *__var__, where var holds the address that we need
 */
void emit_indirect_jump(basic_block_t* basic_block, three_addr_var_t* dest_addr){
	//Use the helper function to create it
	instruction_t* indirect_jump = emit_indirect_jmp_instruction(dest_addr);

	//Now we'll add it into the block
	add_statement(basic_block, indirect_jump);
}


/**
 * Emit the abstract machine code for a constant to variable assignment. 
 */
static cfg_result_package_t emit_constant_from_node(basic_block_t* basic_block, generic_ast_node_t* constant_node){
	//Initialize the constant result package
	cfg_result_package_t constant_result_package = INITIALIZE_BLANK_CFG_RESULT;
	constant_result_package.starting_block = basic_block;
	constant_result_package.final_block = basic_block;

	//Placeholders for constant/var values
	three_addr_const_t* emitted_constant;
	three_addr_var_t* local_constant_val;
	three_addr_var_t* function_pointer_variable;
	local_constant_t* local_constant;
	//Holder for the constant assignment
	instruction_t* const_assignment;
	instruction_t* address_load;

	/**
	 * Constants that are: strings, f32, f64, and function pointers require
	 * special attention here since they use rip-relative addressing/local constants
	 * to work. All other constants do not require this special treatment and are
	 * handled in the catch-all default bucket
	 */
	switch(constant_node->constant_type){
		case STR_CONST:
			//Let's first see if we already have it
			local_constant = get_string_local_constant(&(cfg->local_string_constants), constant_node->string_value.string);

			/**
			 * If we couldn't find it, we'll create it. Otherwise, we'll just use what we found
			 * to get our temp var
			 */
			if(local_constant == NULL){
				local_constant_val = emit_string_local_constant(cfg, constant_node);
			} else {
				local_constant_val = emit_local_constant_temp_var(local_constant);
			}

			//We'll emit an instruction that adds this constant value to the %rip to accurately calculate an address to jump to
			const_assignment = emit_lea_rip_relative_constant(emit_temp_var(constant_node->inferred_type), local_constant_val, instruction_pointer_var);

			//Add this into the block
			add_statement(basic_block, const_assignment);

			/**
			 * This is always a variable constant - we will package it up and return now
			 */
			constant_result_package.type = CFG_RESULT_TYPE_VAR;
			constant_result_package.result_value.result_var = const_assignment->assignee;
			return constant_result_package;

		//For float constants, we need to emit the local constant equivalent via the helper
		case FLOAT_CONST:
			/**
			 * For a floating point constant, if the value is 0 we can avoid all of this mess by emitting a PXOR clear
			 * instruction on a variable. That will allow us to avoid emitting a constant here if we don't need to. Let's
			 * first check if the constant value is 0 to see if that's a viable option
			 */
			if(constant_node->constant_value.float_value == 0.0f){
				//Emit a temp var for this value
				three_addr_var_t* cleared_var = emit_temp_var(constant_node->inferred_type);

				/**
				 * We will now use a specialized IR instruction to clear this variable out. In reality
				 * this clearing will be a PXOR statement
				 */
				instruction_t* clear_instruction = emit_floating_point_clear_instruction(cleared_var);

				//Add it into the block
				add_statement(basic_block, clear_instruction);

				/**
				 * We are done - let's now package up and return the variable that we need to
				 */
				constant_result_package.type = CFG_RESULT_TYPE_VAR;
				constant_result_package.result_value.result_var = cleared_var;
				return constant_result_package;
			}

			//Let's first see if we can find it
			local_constant = get_f32_local_constant(&(cfg->local_f32_constants), constant_node->constant_value.float_value);

			//Either create a new local constant or update it accordingly
			if(local_constant == NULL){
				local_constant_val = emit_f32_local_constant(cfg, constant_node);
			} else {
				local_constant_val = emit_local_constant_temp_var(local_constant);
			}

			/**
			 * We'll emit an instruction that adds this constant value to the %rip to accurately calculate an address to jump to
			 * This only gets the address, we still need to do extra work for our constants
			 */
			address_load = emit_lea_rip_relative_constant(emit_temp_var(u64), local_constant_val, instruction_pointer_var);

			//Add it into the block
			add_statement(basic_block, address_load);

			//Emit a load instruction to grab the constant from said address
			const_assignment = emit_load_ir_code(emit_temp_var(f32), address_load->assignee, f32);

			//Now add the actual assignment into the block
			add_statement(basic_block, const_assignment);

			/**
			 * Package up and get out of here with our final result
			 */
			constant_result_package.type = CFG_RESULT_TYPE_VAR;
			constant_result_package.result_value.result_var = const_assignment->assignee;
			return constant_result_package;

		//For double constants, we need to emit the local constant equivalent via the helper
		case DOUBLE_CONST:
			/**
			 * For a floating point constant, if the value is 0 we can avoid all of this mess by emitting a PXOR clear
			 * instruction on a variable. That will allow us to avoid emitting a constant here if we don't need to. Let's
			 * first check if the constant value is 0 to see if that's a viable option
			 */
			if(constant_node->constant_value.double_value == 0.0){
				//Emit a temp var for this value
				three_addr_var_t* cleared_var = emit_temp_var(constant_node->inferred_type);

				/**
				 * We will now use a specialized IR instruction to clear this variable out. In reality
				 * this clearing will be a PXOR statement
				 */
				instruction_t* clear_instruction = emit_floating_point_clear_instruction(cleared_var);

				//Add it into the block
				add_statement(basic_block, clear_instruction);

				/**
				 * Package up and return the final result type
				 */
				constant_result_package.type = CFG_RESULT_TYPE_VAR;
				constant_result_package.result_value.result_var = cleared_var;
				return constant_result_package;
			}

			//Let's first see if we can find it
			local_constant = get_f64_local_constant(&(cfg->local_f64_constants), constant_node->constant_value.double_value);

			//Either create a new local constant or update it accordingly
			if(local_constant == NULL){
				local_constant_val = emit_f64_local_constant(cfg, constant_node);
			} else {
				local_constant_val = emit_local_constant_temp_var(local_constant);
			}

			/**
			 * We'll emit an instruction that adds this constant value to the %rip to accurately calculate an address to jump to
			 * This only gets the address, we still need to do extra work for our constants
			 */
			address_load = emit_lea_rip_relative_constant(emit_temp_var(u64), local_constant_val, instruction_pointer_var);

			//Add it into the block
			add_statement(basic_block, address_load);

			//Emit a load instruction to grab the constant from the above address
			const_assignment = emit_load_ir_code(emit_temp_var(f64), address_load->assignee, f64);

			//Get this into the block
			add_statement(basic_block, const_assignment);

			/**
			 * Now we can package up and return the entire result struct
			 */
			constant_result_package.type = CFG_RESULT_TYPE_VAR;
			constant_result_package.result_value.result_var = const_assignment->assignee;
			return constant_result_package;

		//Special case here - we need to emit a variable for the function pointer itself
		case FUNC_CONST:
			//Emit the variable first
			function_pointer_variable = emit_function_pointer_temp_var(constant_node->func_record);

			//Now emit the rip-relative assignment used to load the address
			const_assignment = emit_lea_rip_relative_constant(emit_temp_var(constant_node->inferred_type), function_pointer_variable, instruction_pointer_var);

			//Get this into the block
			add_statement(basic_block, const_assignment);

			/**
			 * Package up and return the resulting constant that we got
			 */
			constant_result_package.type = CFG_RESULT_TYPE_VAR;
			constant_result_package.result_value.result_var = const_assignment->assignee;
			return constant_result_package;

		/**
		 * For every other constant type that we have, we will have a result type that
		 * actually is a constant itself. To keep things as segmented as possible
		 * each area will allocate the constant itself
		 */
		case CHAR_CONST:
			emitted_constant = calloc(1, sizeof(three_addr_const_t));
			emitted_constant->type = constant_node->inferred_type;
			emitted_constant->const_type = CHAR_CONST;

			emitted_constant->constant_value.char_constant = constant_node->constant_value.char_value;

			constant_result_package.type = CFG_RESULT_TYPE_CONST;
			constant_result_package.result_value.result_const = emitted_constant;
			return constant_result_package;

		case BYTE_CONST:
			emitted_constant = calloc(1, sizeof(three_addr_const_t));
			emitted_constant->type = constant_node->inferred_type;
			emitted_constant->const_type = BYTE_CONST;

			emitted_constant->constant_value.signed_byte_constant = constant_node->constant_value.signed_byte_value;

			constant_result_package.type = CFG_RESULT_TYPE_CONST;
			constant_result_package.result_value.result_const = emitted_constant;
			return constant_result_package;

		case BYTE_CONST_FORCE_U:
			emitted_constant = calloc(1, sizeof(three_addr_const_t));
			emitted_constant->type = constant_node->inferred_type;
			emitted_constant->const_type = BYTE_CONST_FORCE_U;

			emitted_constant->constant_value.unsigned_byte_constant = constant_node->constant_value.unsigned_byte_value;

			constant_result_package.type = CFG_RESULT_TYPE_CONST;
			constant_result_package.result_value.result_const = emitted_constant;
			return constant_result_package;

		case SHORT_CONST:
			emitted_constant = calloc(1, sizeof(three_addr_const_t));
			emitted_constant->type = constant_node->inferred_type;
			emitted_constant->const_type = SHORT_CONST;

			emitted_constant->constant_value.signed_short_constant = constant_node->constant_value.signed_short_value;

			constant_result_package.type = CFG_RESULT_TYPE_CONST;
			constant_result_package.result_value.result_const = emitted_constant;
			return constant_result_package;

		case SHORT_CONST_FORCE_U:
			emitted_constant = calloc(1, sizeof(three_addr_const_t));
			emitted_constant->type = constant_node->inferred_type;
			emitted_constant->const_type = SHORT_CONST_FORCE_U;

			emitted_constant->constant_value.unsigned_short_constant = constant_node->constant_value.unsigned_short_value;

			constant_result_package.type = CFG_RESULT_TYPE_CONST;
			constant_result_package.result_value.result_const = emitted_constant;
			return constant_result_package;

		case INT_CONST:
			emitted_constant = calloc(1, sizeof(three_addr_const_t));
			emitted_constant->type = constant_node->inferred_type;
			emitted_constant->const_type = INT_CONST;

			emitted_constant->constant_value.signed_integer_constant = constant_node->constant_value.signed_int_value;

			constant_result_package.type = CFG_RESULT_TYPE_CONST;
			constant_result_package.result_value.result_const = emitted_constant;
			return constant_result_package;

		case INT_CONST_FORCE_U:
			emitted_constant = calloc(1, sizeof(three_addr_const_t));
			emitted_constant->type = constant_node->inferred_type;
			emitted_constant->const_type = INT_CONST_FORCE_U;

			emitted_constant->constant_value.unsigned_integer_constant = constant_node->constant_value.unsigned_int_value;

			constant_result_package.type = CFG_RESULT_TYPE_CONST;
			constant_result_package.result_value.result_const = emitted_constant;
			return constant_result_package;

		case LONG_CONST:
			emitted_constant = calloc(1, sizeof(three_addr_const_t));
			emitted_constant->type = constant_node->inferred_type;
			emitted_constant->const_type = LONG_CONST;

			emitted_constant->constant_value.signed_long_constant = constant_node->constant_value.signed_long_value;

			constant_result_package.type = CFG_RESULT_TYPE_CONST;
			constant_result_package.result_value.result_const = emitted_constant;
			return constant_result_package;

		case LONG_CONST_FORCE_U:
			emitted_constant = calloc(1, sizeof(three_addr_const_t));
			emitted_constant->type = constant_node->inferred_type;
			emitted_constant->const_type = LONG_CONST_FORCE_U;

			emitted_constant->constant_value.unsigned_long_constant = constant_node->constant_value.unsigned_long_value;

			constant_result_package.type = CFG_RESULT_TYPE_CONST;
			constant_result_package.result_value.result_const = emitted_constant;
			return constant_result_package;
			
		//Some weird error if we get here - hard exit
		default:
			fprintf(stderr, "Fatal internal compiler error: unrecognized constant type detected in CFG.\n");
			exit(1);
	}
}


/**
 * Emit the abstract machine code for a constant to variable assignment. 
 */
static three_addr_var_t* emit_direct_constant_assignment(basic_block_t* basic_block, three_addr_const_t* constant, generic_type_t* inferred_type){
	//We'll use the constant var feature here
	instruction_t* const_var = emit_assignment_with_const_instruction(emit_temp_var(inferred_type), constant);

	//Add this into the basic block
	add_statement(basic_block, const_var);

	//Now give back the assignee variable
	return const_var->assignee;
}


/**
 * There are several cases when we are emitting an identifier that we want to automatically emit a load
 * from memory. In these cases, we will call out to this function. This function creates a load instruction
 * that automatically grabs the value at the variable memory address
 */
static inline three_addr_var_t* emit_automatic_load_from_memory(basic_block_t* block, symtab_variable_record_t* variable){
	//Extract for use
	generic_type_t* type = variable->type_defined_as;

	//Emit the memory address var for later on
	three_addr_var_t* memory_address = emit_memory_address_var(variable);

	//Emit the load instruction. We need to be sure to use the "true type" here in case we are dealing with 
	//a reference
	instruction_t* load_instruction = emit_load_ir_code(emit_temp_var(type), memory_address, type);

	//Add it to the block
	add_statement(block, load_instruction);

	//Just give back the temp var here
	return load_instruction->assignee;
}


/**
 * Emit the identifier machine code. This function is to be used in the instance where we want
 * to move an identifier to some temporary location
 */
static three_addr_var_t* emit_identifier(basic_block_t* basic_block, generic_ast_node_t* ident_node){
	//Extract the variable
	symtab_variable_record_t* variable = ident_node->variable;
	//Are we on the left or right of an equation?
	side_type_t side = ident_node->side;

	//Go based on it's membership
	switch(variable->membership){
		//For an enum just turn it into a constant
		case ENUM_MEMBER:
			return emit_direct_constant_assignment(basic_block, emit_direct_integer_or_char_constant(variable->enum_member_value, variable->type_defined_as), variable->type_defined_as);

		/**
		 * For a global variable, if we are on the RHS of an equation and we're trying to
		 * use this, we really are looking to load it out of memory. So, we will
		 * help out here by emitting a load to get this out
		 */
		case GLOBAL_VARIABLE:
			/**
			 * Emit a special variable that denotes that we are seeking the memory address of this variable,
			 * not anything else with it
			 */
			if(is_memory_region(variable->type_defined_as) == TRUE){
				return emit_memory_address_var(variable);
			}

			/**
			 * Otherwise it's not a memory address. Depending on what side of the equation that we're
			 * on, we're going to either emit a normal variable or load the variable out of memory. If
			 * we're on the RHS of the equation, we'll want to auto-load the variable for the caller
			 */
			if(side == SIDE_TYPE_RIGHT){
				//Let the helper emit our load from memory
				return emit_automatic_load_from_memory(basic_block, variable);

			//Otherwise emit a normal variable
			} else {
				return emit_var(variable);
			}

		/**
		 * For a static variable, if we are on the RHS of an equation and we're trying to
		 * use this, we really are looking to load it out of memory. So, we will
		 * help out here by emitting a load to get this out
		 */
		case STATIC_VARIABLE:
			/**
			 * Emit a special variable that denotes that we are seeking the memory address of this variable,
			 * not anything else with it
			 */
			if(is_memory_region(variable->type_defined_as) == TRUE){
				return emit_memory_address_var(variable);
			}

			/**
			 * Otherwise it's not a memory address. Depending on what side of the equation that we're
			 * on, we're going to either emit a normal variable or load the variable out of memory. If
			 * we're on the RHS of the equation, we'll want to auto-load the variable for the caller
			 */
			if(side == SIDE_TYPE_RIGHT){
				//Let the helper emit our load from memory
				return emit_automatic_load_from_memory(basic_block, variable);

			//Otherwise emit a normal variable
			} else {
				return emit_var(variable);
			}

		/**
		 * Most function parameters are simple variable emittals. We do need to account for the case where
		 * we have function parameters that are passed in via the stack however
		 */
		case FUNCTION_PARAMETER:
			/**
			 * Elaborative param types are special - there is no circumstance where an elaborative
			 * param is not a memory address variable ever. We can skip all of the fluff and just
			 * emit it as such now
			 */
			if(variable->type_defined_as->type_class == TYPE_CLASS_ELABORATIVE){
				return emit_memory_address_var(variable);
			}

			//Most common case - not passed by stack
			if(variable->passed_by_stack == FALSE){
				//RHS can have special rules
				if(side == SIDE_TYPE_RIGHT){
					/**
					 * If we're on the RHS and we have a special "stack variable", we need to automatically
					 * load that variable out of memory for use in whatever is happening in the caller. The
					 * only exception to this rule are elaborative stack params. Those may never be loaded 
					 * from memory in any way
					 */
					if(variable->stack_variable == TRUE){
						//Let the helper emit our load from memory
						return emit_automatic_load_from_memory(basic_block, variable);

					//Otherwise again just emit the variable
					} else {
						return emit_var(variable);
					}

				//Otherwise we're just emitting the variable
				} else {
					return emit_var(variable);
				}

			//Otherwise we are passed via stack so we'll need some special rules
			} else {
				/**
				 * If we're here then we need to emit an automatic dereference for the caller.
				 * The only exception to this is stack passed parameters. Those may never have an automatic
				 * dereference emitted because they can only be accessed via the array accessor
				 */
				if(side == SIDE_TYPE_RIGHT){
					/**
					 * If the given type is a struct or union, we expect it to be passed
					 * via copy. As such, we do not need to do any kind of automatic
					 * loading/unloading from memory, we can instead just emit the memory
					 * address
					 */
					if(is_type_stack_passed_by_copy(variable->type_defined_as) == FALSE){
						return emit_automatic_load_from_memory(basic_block, variable);
					} else {
						return emit_memory_address_var(variable);
					}

				} else {
					/**
					 * If we have a stack passed by copy variable, we need the memory address. This
					 * will only happen for structs and unions. Otherwise we just have a regular 
					 * variable(most common)
					 */
					if(is_type_stack_passed_by_copy(variable->type_defined_as) == FALSE){
						return emit_var(variable);
					} else {
						return emit_memory_address_var(variable);
					}
				}
			}

		/**
		 * Handle all of our other cases. These follow mostly the same rules as the other variables
		 */
		default:
			/**
			 * Emit a special variable that denotes that we are seeking the memory address of this variable,
			 * not anything else with it
			 */
			if(is_memory_region(variable->type_defined_as) == TRUE){
				return emit_memory_address_var(variable);
			}

			//RHS can have special rules
			if(side == SIDE_TYPE_RIGHT){
				/**
				 * If we're on the RHS and we have a special "stack variable", we need to automatically
				 * load that variable out of memory for use in whatever is happening in the caller
				 */
				if(variable->stack_variable == TRUE){
					//Let the helper emit our load from memory
					return emit_automatic_load_from_memory(basic_block, variable);

				//Otherwise again just emit the variable
				} else {
					return emit_var(variable);
				}

			//Otherwise we're just emitting the variable
			} else {
				return emit_var(variable);
			}
	}
}


/**
 * Emit increment three adress code for general purpose variables
 */
static inline three_addr_var_t* emit_general_purpose_inc_code(basic_block_t* basic_block, three_addr_var_t* incrementee){
	//Create the code
	instruction_t* inc_code = emit_inc_instruction(incrementee);

	//Add it into the block
	add_statement(basic_block, inc_code);

	//Return the incrementee
	return inc_code->assignee;
}


/**
 * Emit increment code for an SSE variable. Since SSE variables are incompatible with standard "inc" instructions, we need
 * to emit this as a var + 1.0 type statement
 */
static inline three_addr_var_t* emit_sse_inc_code(basic_block_t* basic_block, three_addr_var_t* incrementee){
	//We need a "1" float constant
	three_addr_var_t* constant_value;

	//Emit the proper constant based on the type
	switch(incrementee->type->basic_type_token){
		case F32:
			constant_value = emit_direct_floating_point_constant(basic_block, 1, F32);
			break;

		case F64:
			constant_value = emit_direct_floating_point_constant(basic_block, 1, F64);
			break;
		
		default:
			printf("Fatal internal compiler error: invalid variable type for SSE load\n");
			exit(1);
	}

	//The final assignee needs to be separate for SSA reasons. It will
	//have a different "name" than the RHS one
	three_addr_var_t* final_assignee = emit_var_copy(incrementee);

	//Emit the final addition and get it into the block
	instruction_t* final_addition = emit_binary_operation_instruction(final_assignee, incrementee, PLUS, constant_value);

	add_statement(basic_block, final_addition);

	//Finally, the result that we give back is the incrementee
	return final_assignee;
}


/**
 * Emit decrement three address code
 */
static inline three_addr_var_t* emit_general_purpose_dec_code(basic_block_t* basic_block, three_addr_var_t* decrementee){
	//Create the code
	instruction_t* dec_code = emit_dec_instruction(decrementee);

	//Add it into the block
	add_statement(basic_block, dec_code);

	//Return the incrementee
	return dec_code->assignee;
}


/**
 * Emit increment decrement for an SSE variable. Since SSE variables are incompatible with standard "decrement" instructions, we need
 * to emit this as a var - 1.0 type statement
 */
static inline three_addr_var_t* emit_sse_dec_code(basic_block_t* basic_block, three_addr_var_t* decrementee){
	//We need a "1" float constant
	three_addr_var_t* constant_value;

	//Emit the proper constant based on the type
	switch(decrementee->type->basic_type_token){
		case F32:
			constant_value = emit_direct_floating_point_constant(basic_block, 1, F32);
			break;

		case F64:
			constant_value = emit_direct_floating_point_constant(basic_block, 1, F64);
			break;
		
		default:
			printf("Fatal internal compiler error: invalid variable type for SSE load\n");
			exit(1);
	}

	//The final assignee needs to be separate for SSA reasons. It will
	//have a different "name" than the RHS one
	three_addr_var_t* final_assignee = emit_var_copy(decrementee);

	//Emit the final addition and get it into the block
	instruction_t* final_addition = emit_binary_operation_instruction(final_assignee, decrementee, MINUS, constant_value);

	add_statement(basic_block, final_addition);

	//Finally, the result that we give back is the incrementee
	return final_assignee;
}


/**
 * Emit a bitwise not statement 
 */
static inline three_addr_var_t* emit_bitwise_not_expr_code(basic_block_t* basic_block, three_addr_var_t* var){
	//Emit a copy so that we are distinct
	three_addr_var_t* assignee = emit_var_copy(var);

	//First we'll create it here
	instruction_t* not_stmt = emit_not_instruction(assignee);

	//We will still save op1 here, for tracking reasons
	not_stmt->op1 = var;

	//Add this into the block
	add_statement(basic_block, not_stmt);

	//Give back the assignee
	return not_stmt->assignee;
}


/**
 * Emit a binary operation statement with a constant built in
 */
static three_addr_var_t* emit_binary_operation_with_constant(basic_block_t* basic_block, three_addr_var_t* assignee, three_addr_var_t* op1, ollie_token_t op, three_addr_const_t* constant){
	//First let's create it
	instruction_t* stmt = emit_binary_operation_with_const_instruction(assignee, op1, op, constant);

	//Then we'll add it into the block
	add_statement(basic_block, stmt);

	//Finally we'll return it
	return assignee;
}


/**
 * Emit the abstract machine code for a primary expression. Remember that a primary
 * expression could be an identifier, a constant, a function call, or a nested expression
 * tree
 */
static inline cfg_result_package_t emit_primary_expr_code(basic_block_t* basic_block, generic_ast_node_t* primary_parent){
	//Holder for the result var/constant
	three_addr_var_t* result_variable;

	//Switch based on what kind of expression we have. This mainly just calls the appropriate rules
	switch(primary_parent->ast_node_type){
		case AST_NODE_TYPE_IDENTIFIER:
			//Let the helper do it
		 	result_variable = emit_identifier(basic_block, primary_parent);

			//Initialize and give back the result
			cfg_result_package_t result_package = {basic_block, basic_block, {result_variable}, CFG_RESULT_TYPE_VAR, BLANK};
			return result_package;

		//Same in this case - just an assignee in basic block
		case AST_NODE_TYPE_CONSTANT:
			return emit_constant_from_node(basic_block, primary_parent);

		//We handle direct/indirect calls all in the same rule
		case AST_NODE_TYPE_FUNCTION_CALL:
		case AST_NODE_TYPE_INDIRECT_FUNCTION_CALL:
			return emit_function_call(basic_block, primary_parent);

		//By default, we're emitting some kind of expression here
		default:
			return emit_expression(basic_block, primary_parent);
	}
}


/**
 * Emit the code needed to perform an array access
 *
 * This rule handles the dynamic decision to derference mid-processing using the "came_from_non_contiguous_region" parameter
 *
 * Pointers are non-contiguous in memory, because they point to other memory regions(obviously). In Ollie, when the user defines
 * something like "define x:mut char*[3]", that's an array of 3 char*, each one pointing somewhere different in memory. When the user
 * does something like "let y:char* = x[2]", this is defined as "contiguous memory access" because we're just going in that one chunk
 * of memory(x) and grabbing out the 3rd value at index 2. However, when we do something like "let y:char = x[0][1]", then we are in
 * reality making two hops. The first hop will get us to the address of the char* that we want. However, we aren't sitting at the base address
 * of the actual array of chars in memory, we're sitting at the address of that address. It is for this reason that we set the "came_from_non_contiguous_region"
 * flag as true here. This flag is always set to true whenever we access a non-contiguous memory region, *but it is not acted on unless we need to go further in*.
 * So in this example, when this function is called for a second time, that flag will be set to TRUE. We will see that and emit a load so that our base address
 * is no longer the memory address in the original array, but instead is the memory address of the first byte that the original char* in that array
 * pointed to. This becomes our new base address and we go from there. This process can be repeated as many times as need be for this to work
 *
 * For example:
 * pub fn triple_pointer(x:char***) -> i32 {
 * 		ret x[1][2][3];
 * }
 *
 * This is going to set that flag twice. Once for the first load [1], and then again for the second load [2], so that by the time
 * we hit the third load, we've already done two dereferences to truly get down to the base char* that we're after
 *
 * movq 8(%rdi), %rax <- base address of the underlying char** array
 * movq 16(%rax), %rax <- base address of the underlying char*
 * movsbl 3(%rax), %eax <- indexing 3 off of that base address for the actual value
 *
 */
static cfg_result_package_t emit_array_offset_calculation(basic_block_t* block, generic_type_t* memory_region_type, generic_ast_node_t* array_accessor, three_addr_var_t** base_address,
														  three_addr_var_t** current_offset, u_int8_t* came_from_non_contiguous_region){
	//Keep track of whatever the current block is
	basic_block_t* current_block = block;

	/**
	 * If we came from a non-contiguous memory region and we're now trying to use the [] access again,
	 * we need to emit an intermediary load here in order to keep everything in order. This has the
	 * effect of wiping the deck clean with what we had prior and starting fresh with a new base address,
	 * current offest, etc
	 */
	if(*came_from_non_contiguous_region == TRUE){
		//Now we need to emit the load by doing our offset calculation to get out of the pointer
		//space and into memory
		instruction_t* load_instruction;

		//The current offset is not null, we need to emit some calculation here
		if(*current_offset != NULL){
			//Emit the load
			load_instruction = emit_load_with_variable_offset_ir_code(emit_temp_var(u64), *base_address, *current_offset, (*base_address)->type);

			//Add it into the block
			add_statement(current_block, load_instruction);

			//The new base address now is the load instruction's assignee
			*base_address = load_instruction->assignee;

			//And the offset is now nothing
			*current_offset = NULL;

		//If we get here, we have an empty offset so we just need a regular load
		} else {
			//Regular load here
			load_instruction = emit_load_ir_code(emit_temp_var(u64), *base_address, (*base_address)->type);
			
			//Get it into the block
			add_statement(current_block, load_instruction);

			//Again this now is the base address
			*base_address = load_instruction->assignee;
		}
	}

	//The first thing we'll see is the value in the brackets([value]). We'll let the helper emit this
	cfg_result_package_t expression_package = emit_expression(current_block, array_accessor->first_child);

	//Set this to be at the end
	current_block = expression_package.final_block;
	//The current type will always be what was inferred here
	generic_type_t* member_type = array_accessor->inferred_type;

	/**
	 * We may have a constant type here *or* a variable type. Either
	 * way, we will emit what we are able to
	 */
	switch(expression_package.type){
		/**
		 * For a variable result type, we cannot rely on any constant optimizations
		 * so we have to trust and emit the expression as-is
		 */
		case CFG_RESULT_TYPE_VAR:
			/**
			 * If this is not null, we'll be adding on top of it
			 * with this rule and eventually reassigning what the current offset
			 * actually is
			 */
			if(*current_offset != NULL){
				//This is whatever was emitted by the expression
				three_addr_var_t* array_offset = expression_package.result_value.result_var;

				/**
				 * The formula for array subscript is: base_address + type_size * subscript
				 * 
				 * However, if we're on our second or third round, the current var may be an address
				 *
				 * This can be done using a lea instruction, so we will emit that directly
				 */
				three_addr_var_t* address = emit_array_address_calculation(current_block, *current_offset, array_offset, member_type);

				//And finally - our current offset is no longer the actual offset
				*current_offset = address;

			/**
			 * If this is NULL, then we can just make the current offset be
			 * the result + the array offset * member type
			 */
			} else {
				//Emit the variable directly here
				*current_offset = emit_temp_var(u64);
				
				//This is whatever was emitted by the expression
				three_addr_var_t* array_offset = expression_package.result_value.result_var;

				//We're using a lea if we can
				if(is_lea_compatible_power_of_2(member_type->type_size) == TRUE){
					//Emit the lea
					instruction_t* lea = emit_lea_index_and_scale_only(*current_offset, array_offset, member_type->type_size);

					//Add it in
					add_statement(current_block, lea);

				//Otherwise just a multiplication statement
				} else {
					three_addr_const_t* type_size_const = emit_direct_integer_or_char_constant(member_type->type_size, u64);

					//Emit the binary operation directly with this. The current offset remains unchanged
					emit_binary_operation_with_constant(current_block, *current_offset, array_offset, STAR, type_size_const);
				}
			}

			break;

		case CFG_RESULT_TYPE_CONST:
			/**
			 * If this is not null, we'll be adding on top of it
			 * with this rule and eventually reassigning what the current offset
			 * actually is
			 */
			if(*current_offset != NULL){
				/**
				 * The formula for array subscript is: base_address + type_size * subscript
				 * 
				 * However, luckily for us, we know that the offset itself is a constant, so
				 * we can skip a lot of the actual computation work here
				 */
				three_addr_const_t* constant_value = expression_package.result_value.result_const;

				//Emit the actual const over here
				three_addr_const_t* type_size_const = emit_direct_integer_or_char_constant(member_type->type_size, u64);
				
				//Multiply them together
				multiply_constants(type_size_const, constant_value);

				//Emit the calculation
				instruction_t* address_calculation = emit_binary_operation_with_const_instruction(emit_temp_var(u64), *current_offset, PLUS, constant_value);

				//Get it into the block
				add_statement(current_block, address_calculation);

				//And finally - our current offset is no longer the actual offset
				*current_offset = address_calculation->assignee;

			/**
			 * Otherwise this is NULL, so we're starting from scratch. Again we know that this is 
			 * a constant, so we are able to just emit that assignment here
			 */
			} else {
				//Emit the variable directly here
				*current_offset = emit_temp_var(u64);

				//Extract the result constant out
				three_addr_const_t* constant_value = expression_package.result_value.result_const;

				//Emit the actual const over here
				three_addr_const_t* type_size_const = emit_direct_integer_or_char_constant(member_type->type_size, u64);

				//Multiply them together
				multiply_constants(type_size_const, constant_value);

				//This just becomes an assignment expression
				instruction_t* assignment = emit_assignment_with_const_instruction(*current_offset, type_size_const);

				//Add it into the block
				add_statement(current_block, assignment);
			}

			break;
	}

	/**
	 * IMPORTANT: if what we just calculated came specifically from a non-contiguous memory
	 * region, then we need make a note of that just in case there are more [] accessors coming
	 * down the line here. If the next guy sees that this prior address is non-contiguous, it knows
	 * that the memory structure is not flat and it is going to need to perform a derefence to make
	 * this work poperly
	 */
	if(memory_region_type->memory_layout_type == MEMORY_LAYOUT_TYPE_NON_CONTIGUOUS){
		*came_from_non_contiguous_region = TRUE;
	} else {
		*came_from_non_contiguous_region = FALSE;
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
static cfg_result_package_t emit_struct_accessor_expression(basic_block_t* block, generic_type_t* struct_type, generic_ast_node_t* struct_accessor, three_addr_var_t** base_address, three_addr_var_t** current_offset,
															u_int8_t* came_from_non_contiguous_region){
	/**
	 * If our current address is from a non-contiguous region, we are going to need to
	 * load in the value at that address to set up properly here
	 */
	if(*came_from_non_contiguous_region == TRUE){
		//Now we need to emit the load by doing our offset calculation to get out of the pointer
		//space and into memory
		instruction_t* load_instruction;

		//The current offset is not null, we need to emit some calculation here
		if(*current_offset != NULL){
			//Emit the load
			load_instruction = emit_load_with_variable_offset_ir_code(emit_temp_var(u64), *base_address, *current_offset, (*base_address)->type);

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
			
			//Get it into the block
			add_statement(block, load_instruction);

			//Again this now is the base address
			*base_address = load_instruction->assignee;
		}
	}

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
		three_addr_var_t* offset_calculation_result = emit_struct_address_calculation(block, struct_type, *current_offset, struct_offset);

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

		//Add it into the block
		add_statement(block, assignment_instruction);
	}

	/**
	 * IMPORTANT: if what we just calculated came specifically from a non-contiguous memory
	 * region, then we need make a note of that just in case there are more [] accessors coming
	 * down the line here. If the next guy sees that this prior address is non-contiguous, it knows
	 * that the memory structure is not flat and it is going to need to perform a derefence to make
	 * this work poperly. This value is not a pointer, so it is contiguous
	 */
	if(struct_accessor->variable->type_defined_as->type_class == TYPE_CLASS_POINTER){
		*came_from_non_contiguous_region = TRUE;
	} else {
		*came_from_non_contiguous_region = FALSE;
	}

	//Package & return the results
	cfg_result_package_t results = {block, block, {*current_offset}, CFG_RESULT_TYPE_VAR, BLANK};
	return results;
}


/**
 * Emit the code needed to perform a struct pointer access
 *
 * This rule returns *the offset* of the value that we want. It has
 * no idea what the base address of the memory region it's in is
 */
static cfg_result_package_t emit_struct_pointer_accessor_expression(basic_block_t* block, generic_type_t* struct_pointer_type, generic_ast_node_t* struct_accessor, three_addr_var_t** base_address, three_addr_var_t** current_offset,
																	u_int8_t* came_from_non_contiguous_region){
	//Get what the raw struct type is
	generic_type_t* raw_struct_type = struct_pointer_type->internal_types.points_to;

	/**
	 * If our current address is from a non-contiguous region, we are going to need to
	 * load in the value at that address to set up properly here
	 */
	if(*came_from_non_contiguous_region == TRUE){
		//Now we need to emit the load by doing our offset calculation to get out of the pointer
		//space and into memory
		instruction_t* load_instruction;

		//The current offset is not null, we need to emit some calculation here
		if(*current_offset != NULL){
			//Emit the load
			load_instruction = emit_load_with_variable_offset_ir_code(emit_temp_var(u64), *base_address, *current_offset, raw_struct_type);

			//Add it into the block
			add_statement(block, load_instruction);

			//The new base address now is the load instruction's assignee
			*base_address = load_instruction->assignee;

			//And the offset is now nothing
			*current_offset = NULL;

		//If we get here, we have an empty offset so we just need a regular load
		} else {
			//Regular load here
			load_instruction = emit_load_ir_code(emit_temp_var(u64), *base_address, raw_struct_type);
			
			//Get it into the block
			add_statement(block, load_instruction);

			//Again this now is the base address
			*base_address = load_instruction->assignee;
		}
	}

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

	/**
	 * IMPORTANT: if what we just calculated came specifically from a non-contiguous memory
	 * region, then we need make a note of that just in case there are more [] accessors coming
	 * down the line here. If the next guy sees that this prior address is non-contiguous, it knows
	 * that the memory structure is not flat and it is going to need to perform a derefence to make
	 * this work poperly
	 */
	if(struct_accessor->variable->type_defined_as->type_class == TYPE_CLASS_POINTER){
		*came_from_non_contiguous_region = TRUE;
	} else {
		*came_from_non_contiguous_region = FALSE;
	}

	//And we're done here, we can package and return what we have
	cfg_result_package_t results = {block, block, {*base_address}, CFG_RESULT_TYPE_VAR, BLANK};
	return results;
}


/**
 * Emit the code needed to perform a regular union access
 *
 * This rule returns *the address* of the value that we've asked for
 */
static cfg_result_package_t emit_union_accessor_expression(basic_block_t* block, generic_ast_node_t* union_accessor, three_addr_var_t** base_address, three_addr_var_t** current_offset,
														   u_int8_t* came_from_non_contiguous_region){
	/**
	 * If this came from a non-contiguous region, then we're going to need to deal with it accordingly
	 */
	if(*came_from_non_contiguous_region == TRUE){
		//Now we need to emit the load by doing our offset calculation to get out of the pointer
		//space and into memory
		instruction_t* load_instruction;

		//The current offset is not null, we need to emit some calculation here
		if(*current_offset != NULL){
			//Emit the load
			load_instruction = emit_load_with_variable_offset_ir_code(emit_temp_var(u64), *base_address, *current_offset, (*base_address)->type);

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
			
			//Get it into the block
			add_statement(block, load_instruction);

			//Again this now is the base address
			*base_address = load_instruction->assignee;
		}
	}

	/**
	 * IMPORTANT: if what we just calculated came specifically from a non-contiguous memory
	 * region, then we need make a note of that just in case there are more [] accessors coming
	 * down the line here. If the next guy sees that this prior address is non-contiguous, it knows
	 * that the memory structure is not flat and it is going to need to perform a derefence to make
	 * this work poperly
	 */
	if(union_accessor->variable->type_defined_as->type_class == TYPE_CLASS_POINTER){
		*came_from_non_contiguous_region = TRUE;
	} else {
		*came_from_non_contiguous_region = FALSE;
	}

	//Very simple rule, we just have this for consistency
	cfg_result_package_t accessor = {block, block, {*base_address}, CFG_RESULT_TYPE_VAR, BLANK};

	//Give it back
	return accessor;
}


/**
 * Emit the code needed to perform a union pointer access
 *
 * This rule returns *the address* of the value that we've asked for
 */
static cfg_result_package_t emit_union_pointer_accessor_expression(basic_block_t* block, generic_ast_node_t* union_accessor, generic_type_t* union_pointer_type, three_addr_var_t** base_address, three_addr_var_t** current_offset,
																	u_int8_t* came_from_non_contiguous_region){
	//Get the current type
	generic_type_t* raw_union_type = union_pointer_type->internal_types.points_to;

	/**
	 * If this came from a non-contiguous region, then we're going to need to deal with it accordingly
	 */
	if(*came_from_non_contiguous_region == TRUE){
		//Now we need to emit the load by doing our offset calculation to get out of the pointer
		//space and into memory
		instruction_t* load_instruction;

		//The current offset is not null, we need to emit some calculation here
		if(*current_offset != NULL){
			//Emit the load
			load_instruction = emit_load_with_variable_offset_ir_code(emit_temp_var(u64), *base_address, *current_offset, raw_union_type);

			//Add it into the block
			add_statement(block, load_instruction);

			//The new base address now is the load instruction's assignee
			*base_address = load_instruction->assignee;

			//And the offset is now nothing
			*current_offset = NULL;

		//If we get here, we have an empty offset so we just need a regular load
		} else {
			//Regular load here
			load_instruction = emit_load_ir_code(emit_temp_var(u64), *base_address, raw_union_type);
			
			//Get it into the block
			add_statement(block, load_instruction);

			//Again this now is the base address
			*base_address = load_instruction->assignee;
		}
	}

	/**
	 * IMPORTANT: if what we just calculated came specifically from a non-contiguous memory
	 * region, then we need make a note of that just in case there are more [] accessors coming
	 * down the line here. If the next guy sees that this prior address is non-contiguous, it knows
	 * that the memory structure is not flat and it is going to need to perform a derefence to make
	 * this work poperly
	 */
	if(union_accessor->variable->type_defined_as->type_class == TYPE_CLASS_POINTER){
		*came_from_non_contiguous_region = TRUE;
	} else {
		*came_from_non_contiguous_region = FALSE;
	}

	/**
	 * By the time we get out here, we have performed a dereference and loaded whatever our offset
	 * math was before into the new base address variable. The current offset will be NULL again
	 * because we need to start over if we have any more offsets
	 */
	cfg_result_package_t return_package = {block, block, {*base_address}, CFG_RESULT_TYPE_VAR, BLANK};
	return return_package;
}


/**
 * The helper will process the postfix expression for us in a recursive way. We will pass along the "base_address" variable which
 * will eventually be populated by the root level expression
 *
 * Special cases to be aware of - non-contiguous memory regions:
 *
 * Something like: x:char*[3];
 *
 *  In memory x is : |char* | char* | char *|
 *
 *  So if we do something like x[2][1], we need to account for the fact that x is "non-contiguous". This is actually already accounted
 *  for by the type system so it's not something that we need to be aware of here. Non-contiguous memory regions require intermediary loads
 *  in order to work properly
 */
static cfg_result_package_t emit_postfix_expression_rec(basic_block_t* basic_block, generic_ast_node_t* root, three_addr_var_t** base_address, three_addr_var_t** current_offset, u_int8_t* came_from_non_contiguous_region){
	//A tracker for what the current block actually is(this can change)
	basic_block_t* current = basic_block;

	/**
	 * If we make it here, this is actually our base address emittal. We will use the
	 * results from here to hang onto our base address
	 */
	if(root->ast_node_type != AST_NODE_TYPE_POSTFIX_EXPR){
		//Run the primary results function - we know that this is not going to be a constant
		cfg_result_package_t primary_results = emit_primary_expr_code(basic_block, root);

		//The current block now is this ones final block
		current = primary_results.final_block;

		//Extract for some analysis
		three_addr_var_t* assignee = primary_results.result_value.result_var;

		//Get this if there is one
		symtab_variable_record_t* base_address_variable = assignee->linked_var;

		/**
		 * If we have a linked variable that is coming to us from the stack, we'll
		 * need to automatically get this out of the stack for our uses here. Remember
		 * that these types(arrays and pointers) live on the stack as references to other
		 * areas in memory. The array itself is not on the stack
		 */
		if(base_address_variable != NULL 
			&& base_address_variable->passed_by_stack == TRUE){

			/**
			 * Is this an elaborative param type? If so, we'll need to add on the automatic
			 * 4 byte offset to this base address that all stack passed parameters have to account
			 * for the stored paramcount
			 *
			 * We know that we have *at least* 4 bytes here on the end, and we may have more padding based
			 * on what has been passed through the elaborative param
			 */
			if(base_address_variable->type_defined_as->type_class == TYPE_CLASS_ELABORATIVE){
				//Emit a new current offset
				three_addr_var_t* new_current_offset = emit_temp_var(u64);

				//Emit a special instruction for IR clarity
				instruction_t* elaborative_param_offset = emit_elaborative_param_starting_offset_calculation(new_current_offset, emit_var(base_address_variable));

				//Put it into the block
				add_statement(current, elaborative_param_offset);

				//This now is the current offset so we're going to denote that
				*current_offset = new_current_offset;

				//The base address is just the assignee in this case
				*base_address = assignee;

			/**
			 * Otherwise we still have to account for the case where we have reference types that need
			 * to automatically be loaded(like arrays)
			 */
			} else if(is_type_stack_passed_by_reference(base_address_variable->type_defined_as) == TRUE){
				*base_address = emit_automatic_load_from_memory(basic_block, base_address_variable);

			/**
			 * Otherwise in our final case we'll need to account for the case where we have a struct
			 * or union that is passed by copy. In that case no automatic load is needed
			 */
			} else {
				*base_address = assignee;
			}

		//Else just update the base address
		} else {
			//The base address is whatever this assignee is
			*base_address = assignee;
		}

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
	cfg_result_package_t left_child_results = emit_postfix_expression_rec(basic_block, left_child, base_address, current_offset, came_from_non_contiguous_region);

	//Update whatever the last block may be
	current = left_child_results.final_block;

	//The postfix results package
	cfg_result_package_t postfix_results;

	//NOTE: by the time we get down here, base address will have been populated with an actual value(usually "memory address of")

	//Now we need to go through and calculate the offset
	switch(right_child->ast_node_type){
		//Handle an array accessor
		case AST_NODE_TYPE_ARRAY_ACCESSOR:
			postfix_results = emit_array_offset_calculation(current, memory_region_type, right_child, base_address, current_offset, came_from_non_contiguous_region);
			break;

		//Handle a regular struct accessor(: access)
		case AST_NODE_TYPE_STRUCT_ACCESSOR:
			postfix_results = emit_struct_accessor_expression(current, memory_region_type, right_child, base_address, current_offset, came_from_non_contiguous_region);
			break;

		//Handle a struct pointer access
		case AST_NODE_TYPE_STRUCT_POINTER_ACCESSOR:
			postfix_results = emit_struct_pointer_accessor_expression(current, memory_region_type, right_child, base_address, current_offset, came_from_non_contiguous_region);
			break;

		//Handle a regular union access(. access)
		case AST_NODE_TYPE_UNION_ACCESSOR:
			postfix_results = emit_union_accessor_expression(current, right_child, base_address, current_offset, came_from_non_contiguous_region);
			break;

		//Handle a union pointer access (-> access)
		case AST_NODE_TYPE_UNION_POINTER_ACCESSOR:
			postfix_results = emit_union_pointer_accessor_expression(current, right_child, memory_region_type, base_address, current_offset, came_from_non_contiguous_region);
			break;
			
		//We should never actually hit this, it's just so the compiler is happy
		default:
			break;
	}

	//Give back our final results(assignee is not needed here)
	cfg_result_package_t final_results = {current, postfix_results.final_block, {NULL}, CFG_RESULT_TYPE_VAR, BLANK};
	return final_results;
}


/**
 * Emit a postfix expression
 *
 * Postfix expressions have their left subtree most developed. The root of the subtree
 * is the very last postfix expression. This is because we need to "execute" the first 
 * the deepest(first) part first and the highest(root) part last
 */
static cfg_result_package_t emit_postfix_expression(basic_block_t* basic_block, generic_ast_node_t* root){
	//This is our "base case". If it's not a postfix expression,
	//just move out
	if(root->ast_node_type != AST_NODE_TYPE_POSTFIX_EXPR){
		return emit_primary_expr_code(basic_block, root);
	}

	//Did the current result come from a non-contiguous computation
	u_int8_t came_from_non_continguous_region = FALSE;

	//Hold onto what our current block is, it may change
	basic_block_t* current_block = basic_block;

	//A variable for our base address(it starts off as null, the recursive rule will modify it)
	three_addr_var_t* base_address = NULL;

	//Another variable for our current offset(again it starts as NULL, the rule will populate if need be)
	three_addr_var_t* current_offset = NULL;
	
	//Let the recursive rule do all the work
	cfg_result_package_t postfix_results = emit_postfix_expression_rec(basic_block, root, &base_address, &current_offset, &came_from_non_continguous_region);

	//Grab htese out for later
	generic_ast_node_t* left_child = root->first_child;
	generic_ast_node_t* right_child = left_child->next_sibling;

	/**
	 * For this rule, we care about the parent node's type(after cast/coercion) and
	 * the original memory access type(before cast/coercion). We will use these 2 to 
	 * determine if a converting operation is needed
	 */
	generic_type_t* parent_node_type = root->inferred_type;
	generic_type_t* original_memory_access_type = right_child->inferred_type;

	//This is whatever the final block is
	current_block = postfix_results.final_block;

	//IMPORTANT - the result of this is always going to be a variable
	postfix_results.type = CFG_RESULT_TYPE_VAR;

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

					//Add it into the block
					add_statement(current_block, store_instruction);

					//Give back the base address as the assignee(even though it's not really)
					postfix_results.result_value.result_var = base_address;

				//Otherwise, this means that the current offset is null
				} else {
					//Emit the store here - remember we leave the op1 NULL so that
					//a later rule can fill it in
					store_instruction = emit_store_ir_code(base_address, NULL, original_memory_access_type);

					//Add it into our block
					add_statement(current_block, store_instruction);

					//Give back the base address as the assignee(even though it's not really)
					postfix_results.result_value.result_var = base_address;
				}

				break;

			//Right side = load statement
			case SIDE_TYPE_RIGHT:
				//This will not be null in the case of structs & arrays
				if(current_offset != NULL){
					//Calculate our load here
					load_instruction = emit_load_with_variable_offset_ir_code(emit_temp_var(parent_node_type), base_address, current_offset, original_memory_access_type);

					//Add it into the block
					add_statement(current_block, load_instruction);

					//Now the final assignee here is important - it's what we give it here
					postfix_results.result_value.result_var = load_instruction->assignee;

				//Otherwise we have a null current offset, so we're just relying on the base address
				} else {
					//Emit the load instruction between the base address and the parent node type
					load_instruction = emit_load_ir_code(emit_temp_var(parent_node_type), base_address, original_memory_access_type);

					//Add it into the block
					add_statement(current_block, load_instruction);

					//This is our final assignee
					postfix_results.result_value.result_var = load_instruction->assignee;
				}

				break;
		}

	//Otherwise it's just a memory address call, just emit the base address plus the offset
	} else {
		//If the current offset is not NULL, we'll need to do some calculations here
		if(current_offset != NULL){
			//Just do base address + offset
			instruction_t* address_calculation = emit_binary_operation_instruction(emit_temp_var(base_address->type), base_address, PLUS, current_offset);

			//Add the instruction in
			add_statement(current_block, address_calculation);

			//This is what we're returning
			postfix_results.result_value.result_var = address_calculation->assignee;

		//Otherwise it is null, so we can just use the base address
		} else {
			postfix_results.result_value.result_var = base_address;
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
static cfg_result_package_t emit_postoperation_code(basic_block_t* basic_block, generic_ast_node_t* node){
	//Store the current block
	basic_block_t* current_block = basic_block;

	//The postfix node is always the first child
	generic_ast_node_t* postfix_node = node->first_child;

	//We will first emit the postfix expression code that comes from this
	cfg_result_package_t postfix_expression_results = emit_postfix_expression(current_block, postfix_node);

	//Update the end block
	current_block = postfix_expression_results.final_block;

	//This is the value that we will be modifying. It will always be a variable
	three_addr_var_t* assignee = unpack_result_package(&postfix_expression_results, current_block);

	/**
	 * Remember that for a postoperation, we save the value that we get before
	 * we apply the operation. The "postoperation" does not happen until after the value is
	 * used. To facilitate this, we will perform a temp assignment here. The result of
	 * this temp assignment is actually what the user will be using
	 */

	//Emit the assignment
	instruction_t* temp_assignment = emit_assignment_instruction(emit_temp_var(assignee->type), assignee);

	//Add this statement in
	add_statement(current_block, temp_assignment);

	//Initialize this off the bat
	cfg_result_package_t postoperation_package = {basic_block, current_block, {temp_assignment->assignee}, CFG_RESULT_TYPE_VAR, BLANK};

	//If the assignee is not a pointer, we'll handle the normal case
	switch(assignee->type->type_class){
		//If we have basic or reference types, we emit the
		//inc codes
		case TYPE_CLASS_BASIC:
			switch(node->unary_operator){
				case PLUSPLUS:
					//Go based on the token type. If we have floating
					//point operations here, we need special handling
					switch(assignee->type->basic_type_token){
						case F32:
						case F64:
							//Let the special helper deal with it
							assignee = emit_sse_inc_code(current_block, assignee);
							break;
							
						default:
							//We really just have an "inc" instruction here
							assignee = emit_general_purpose_inc_code(current_block, assignee);
							break;
					}

					break;
					
				case MINUSMINUS:
					//Go based on the token type. If we have floating
					//point operations here, we need special handling
					switch(assignee->type->basic_type_token){
						case F32:
						case F64:
							//Call out to the helper to deal with the special float case
							assignee = emit_sse_dec_code(current_block, assignee);
							break;

						default:
							//We really just have an "inc" instruction here
							assignee = emit_general_purpose_dec_code(current_block, assignee);
							break;
					}

					break;

				//We shouldn't ever hit here
				default:
					break;
			}

			break;

		//A pointer type is a special case
		case TYPE_CLASS_POINTER:
			//Let the helper deal with this
			assignee = handle_pointer_arithmetic(current_block, node->unary_operator, assignee);
			break;

		//Everything else should be impossible
		default:
			printf("Fatal internal compiler error: Unreachable path hit for postinc in the CFG\n");
			exit(1);
	}

	/**
	 * Logic here: if this is not some simple identifier(it could be array access, struct access, etc.), then
	 * we'll need to perform this duplication. If it is just an identifier, then we're able to leave this be
	 */
	if(postfix_node->ast_node_type != AST_NODE_TYPE_IDENTIFIER){
		//Duplicate the subtree here for us to use
		generic_ast_node_t* copy = duplicate_subtree(postfix_node, SIDE_TYPE_LEFT);

		//Now we emit the copied package
		cfg_result_package_t copied_package = emit_postfix_expression(current_block, copy);

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

		//Otherwise we just have a regular assignment
		} else {
			//And finally, we'll emit the save instruction that stores the value that we've incremented into the location we got it from
			instruction_t* assignment_instruction = emit_assignment_instruction(unpack_result_package(&copied_package, current_block), assignee);

			//Add this into the block
			add_statement(current_block, assignment_instruction);
		}

		//This is always the new final block
		postoperation_package.final_block = current_block;

	//Otherwise - it is possible that we have a stack variable or reference here. In that case, we'll need to emit a
	//store to get the variable back to where it needs to be
	} else if (postfix_node->variable->stack_variable == TRUE){
		generic_type_t* type = postfix_node->variable->type_defined_as; 

		//Get the version that represents our memory indirection. Be sure to use the "true type" here
		//just in case we were dealing with a reference
		three_addr_var_t* memory_address_var = emit_memory_address_var(postfix_node->variable);

		//Now we need to add the final store
		instruction_t* store_instruction = emit_store_ir_code(memory_address_var, assignee, type);

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
static cfg_result_package_t emit_unary_operation(basic_block_t* basic_block, generic_ast_node_t* unary_expression_parent){
	//Top level declarations to avoid using them in the switch statement
	instruction_t* assignment;
	three_addr_var_t* assignee;
	//The unary expression package
	cfg_result_package_t unary_package = INITIALIZE_BLANK_CFG_RESULT;

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
			unary_package = emit_unary_expression(current_block, unary_expression_child);

			//Update the current block
			current_block = unary_package.final_block;

			//The assignee comes from our package. This is what we are ultimately using in the final result
			assignee = unpack_result_package(&unary_package, current_block);
		
			//Go based on what we have here
			switch(assignee->type->type_class){
				case TYPE_CLASS_BASIC:
					//Go based on the op here
					switch(unary_operator_node->unary_operator){
						case PLUSPLUS:
							/**
							 * Go based on the basic type. Since SSE variables are not
							 * compatible with normal inc instructions, we need to
							 * break out like this
							 */
							switch(assignee->type->basic_type_token){
								case F32:
								case F64:
									//Let the special helper deal with it
									assignee = emit_sse_inc_code(current_block, assignee) ;
									break;

								default:
									//We really just have an "inc" instruction here
									assignee = emit_general_purpose_inc_code(current_block, assignee);
									break;
							}

							break;
							
						case MINUSMINUS:
							/**
							 * Go based on the basic type. Since SSE variables are not
							 * compatible with normal dec instructions, we need to
							 * break out like this
							 */
							switch(assignee->type->basic_type_token){
								case F32:
								case F64:
									//Call out to the helper to deal with the special float case
									assignee = emit_sse_dec_code(current_block, assignee);
									break;

								default:
									//We really just have an "inc" instruction here
									assignee = emit_general_purpose_dec_code(current_block, assignee);
									break;
							}

							break;

						//We shouldn't ever hit here
						default:	
							break;
					}

					break;

				//The pointer type is a special case
				case TYPE_CLASS_POINTER:
					//Let the helper deal with this
					assignee = handle_pointer_arithmetic(current_block, unary_operator_node->unary_operator, assignee);

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
				cfg_result_package_t copied_package = emit_unary_expression(current_block, copy);

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

				//Otherwise we just have a regular assignment
				} else {
					//And finally, we'll emit the save instruction that stores the value that we've incremented into the location we got it from
					instruction_t* assignment_instruction = emit_assignment_instruction(copied_package.result_value.result_var, assignee);

					//Add this into the block
					add_statement(current_block, assignment_instruction);
				}

			//Otherwise - it is possible that we have a stack variable or reference here. In that case, we'll need to emit a
			//store to get the variable back to where it needs to be
			} else if (unary_expression_child->variable->stack_variable == TRUE){
				//Type of the variable
				generic_type_t* type = unary_expression_child->variable->type_defined_as; 

				//Get the version that represents our memory indirection
				three_addr_var_t* memory_address_var = emit_memory_address_var(unary_expression_child->variable);

				//Now we need to add the final store
				instruction_t* store_instruction = emit_store_ir_code(memory_address_var, assignee, type);

				//Get this in there
				add_statement(current_block, store_instruction);
				
				//This is always the new final block
				unary_package.final_block = current_block;
			}

			//Store the assignee as this
			unary_package.type = CFG_RESULT_TYPE_VAR;
			unary_package.result_value.result_var = assignee;
			//Update the final block if it has change
			unary_package.final_block = current_block;
		
			//Give back the final unary package
			return unary_package;

		//Handle a dereference
		case STAR:
			//The very first thing that we'll do is emit the assignee that comes after the unary expression
			unary_package = emit_unary_expression(current_block, unary_expression_child);

			//Update the block
			current_block = unary_package.final_block;

			//The assignee comes from the package
			assignee = unpack_result_package(&unary_package, current_block);

			//The pointer will be on the unary expression child
			generic_type_t* pointer_type = unary_expression_child->inferred_type;
			//And the final type comes from when we dereference it
			generic_type_t* dereferenced_type = dereference_type(pointer_type);

			/**
			 * If we what we have is an array pointer, then we don't need to do anything besides assign the
			 * value over. This is because when we take the address of an array, all that we do is load
			 * the rsp offset in memory
			 *
			 * Example:
			 * 	 declare x:i32[5]; //Suppose that this is at rsp + 20
			 *
			 * 	 let y:i32[5]* = &x;
			 *
			 * All that this really is in our assembly is:
			 * 		y <- 20 + rsp
			 *
			 * So when we go to "dereference" y, we just need to assign the value over in a simple
			 * move instruction. This is the same kind of rule for structs & unions, which is why
			 * we use the "is_memory_region" check to see what kind of pointer we have. Again, a pointer
			 * to a struct is simply that struct's address that's been copied in memory, so there's no additional
			 * indirection that we need to do
			 */
			if(is_memory_region(dereferenced_type) == TRUE){
				//Emit the assignment
				instruction_t* assignment_instruction = emit_assignment_instruction(emit_temp_var(dereferenced_type), assignee);

				//Get this into the block
				add_statement(current_block, assignment_instruction);

				//Package this up and get out
				unary_package.type = CFG_RESULT_TYPE_VAR;
				unary_package.result_value.result_var = assignment_instruction->assignee;
				return unary_package;
			}

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

				//Now let's get this into the block
				add_statement(current_block, store_instruction);

				//Add the assignee in here
				unary_package.type = CFG_RESULT_TYPE_VAR;
				unary_package.result_value.result_var = assignee;

			/**
			 * If we get here, we know that we either have a right hand operation or we're on the left hand
			 * side but we're not entirely done with the unary operations yet. Either way, we'll need a load
			 */
			} else {
				//If the side type here is right, we'll need a load instruction
				instruction_t* load_instruction = emit_load_ir_code(emit_temp_var(unary_expression_parent->inferred_type), assignee, dereferenced_type);

				//Add it in
				add_statement(current_block, load_instruction);

				//This one's assignee is our overall assignee
				unary_package.type = CFG_RESULT_TYPE_VAR;
				unary_package.result_value.result_var = load_instruction->assignee;
			}

			//Give back the final unary package
			return unary_package;
	
		//Bitwise not operator
		case B_NOT:
			//The very first thing that we'll do is emit the assignee that comes after the unary expression
			unary_package = emit_unary_expression(current_block, unary_expression_child);

			//Update the current block
			current_block = unary_package.final_block;

			//The assignee comes from the package
			assignee = unpack_result_package(&unary_package, current_block);

			//The new assignee will come from this helper
			unary_package.type = CFG_RESULT_TYPE_VAR;
			unary_package.result_value.result_var = emit_bitwise_not_expr_code(current_block, assignee);

			//Give the package back
			return unary_package;

		//Logical not operator
		case EXCLAMATION:
			//The very first thing that we'll do is emit the assignee that comes after the unary expression
			unary_package = emit_unary_expression(current_block, unary_expression_child);

			//Update the current block
			current_block = unary_package.final_block;

			//The assignee comes from the package
			assignee = unpack_result_package(&unary_package, current_block);

			//This will always overwrite the other value
			instruction_t* logical_not_statement = emit_logical_not_instruction(emit_temp_var(u8), assignee);

			/**
			 * If we came from a floating point operation, then we will just flag as such here
			 */
			if(IS_FLOATING_POINT(assignee->type) == TRUE){
				logical_not_statement->assignee->comes_from_fp_comparison = TRUE;
			}

			//Get it into the block right after the unary expression
			add_statement(current_block, logical_not_statement);

			//The package's assignee is now the result of this logical not instruction
			unary_package.type = CFG_RESULT_TYPE_VAR;
			unary_package.result_value.result_var = logical_not_statement->assignee;

			//The operator here is logical not
			unary_package.operator = EXCLAMATION;

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
			unary_package = emit_unary_expression(current_block, unary_expression_child);

			//Update the current block
			current_block = unary_package.final_block;

			//The assignee comes from the package
			assignee = unpack_result_package(&unary_package, current_block);

			//We'll need to assign to a temp here, these are only ever on the RHS
			assignment = emit_assignment_instruction(emit_temp_var(assignee->type), assignee);

			//Add this into the block
			add_statement(current_block, assignment);

			//Now emit the instruction itself
			instruction_t* negation_instruction = emit_neg_instruction(assignment->assignee);

			//Now get the whole statement into the block
			add_statement(current_block, negation_instruction);

			//Rewrite the assignee to be this now
			unary_package.type = CFG_RESULT_TYPE_VAR;
			unary_package.result_value.result_var = negation_instruction->assignee;
			
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
					 * We do not do this if it's a global/static variable, because global/static variables have their own unique storage
					 * mechanism that is not stack related
					 *
					 *
					 * Another nuance, if we have an array, say and int[], and we take the address, the user will
					 * receive a type of int[]*(pointer to an array). In order to achieve this, we will need to create
					 * a whole new stack variable to save the array
					 */
					if(is_variable_data_segment_variable(unary_expression_child->variable) == FALSE 
						//Is it not on the stack already?
						&& unary_expression_child->variable->stack_region == NULL) {
						//Create the stack region and store it in the variable
						unary_expression_child->variable->stack_region = create_stack_region_for_type(&(current_function->local_stack), unary_expression_child->variable->type_defined_as);
					} 

					/**
					 * Otherwise, this variable is already on the stack. As such, to get it's memory address,
					 * all we need to do is take emit a specialized "memory address var" from the existing
					 * stack region and slap it into a variable
					 */
					//The memory address itself
					three_addr_var_t* memory_address_var = emit_memory_address_var(unary_expression_child->variable);

					//And package the value up as what we want here
					unary_package.type = CFG_RESULT_TYPE_VAR;
					unary_package.result_value.result_var = memory_address_var;

					break;

				//The other case here
				case AST_NODE_TYPE_POSTFIX_EXPR:
					//Set the deref flag to false so we don't deref
					unary_expression_child->dereference_needed = FALSE;
					//Emit the whole thing
					cfg_result_package_t postfix_results = emit_postfix_expression(current_block, unary_expression_child);

					//Update the current block
					current_block = postfix_results.final_block;

					//And package the value up as what we want here
					unary_package.type = CFG_RESULT_TYPE_VAR;
					unary_package.result_value.result_var = unpack_result_package(&postfix_results, current_block);
					break;

				//This should never occur
				default:
					print_parse_message(MESSAGE_TYPE_ERROR, "Fatal internal compiler error. Unrecognized node type for address operation", unary_expression_child->line_number);
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
static inline cfg_result_package_t emit_unary_expression(basic_block_t* basic_block, generic_ast_node_t* unary_expression){
	//Switch based on what class this node actually is
	switch(unary_expression->ast_node_type){
		//If we see the actual node here, we know that we are actually doing a unary operation
		case AST_NODE_TYPE_UNARY_EXPR:	
			return emit_unary_operation(basic_block, unary_expression);
		case AST_NODE_TYPE_POSTOPERATION:
			return emit_postoperation_code(basic_block, unary_expression);
		//Otherwise if we don't see this node, we instead know that this is really a postfix expression of some kind
		default:
			return emit_postfix_expression(basic_block, unary_expression);
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
static cfg_result_package_t emit_ternary_expression(basic_block_t* starting_block, generic_ast_node_t* ternary_operation){
	instruction_t* if_assignment;
	instruction_t* else_assignment;
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
	symtab_variable_record_t* ternary_variable = create_ssa_compatible_temp_var(current_function, ternary_operation->inferred_type, variable_symtab, increment_and_get_temp_id());

	//Let's first create the final result variable here
	three_addr_var_t* if_result = emit_var(ternary_variable);
	three_addr_var_t* else_result = emit_var(ternary_variable);
	three_addr_var_t* final_result = emit_var(ternary_variable);

	//Grab a cursor to the first child
	generic_ast_node_t* cursor = ternary_operation->first_child;

	/**
	 * Let the helper emit the normal if branch/conditional expression here
	 */
	emit_branch(current_block, cursor, if_block, else_block, BRANCH_CATEGORY_NORMAL, FALSE);
	
	//Now we'll go through and process the two children
	cursor = cursor->next_sibling;

	//Emit this in our new if block
	cfg_result_package_t if_branch = emit_expression(if_block, cursor);

	//Again here we could have multiple blocks, so we'll need to account for this and reassign if necessary
	if_block = if_branch.final_block;

	/**
	 * Unpack the result type accordingly for the if branch's
	 * assignee
	 */
	switch(if_branch.type){
		case CFG_RESULT_TYPE_VAR:
			if_assignment = emit_assignment_instruction(if_result, if_branch.result_value.result_var);
			break;

		case CFG_RESULT_TYPE_CONST:
			if_assignment = emit_assignment_with_const_instruction(if_result, if_branch.result_value.result_const);
			break;
	}

	//Add this into the if block regardless of the result
	add_statement(if_block, if_assignment);

	//Now add a direct jump to the end
	emit_jump(if_block, end_block);

	//Process the else branch
	cursor = cursor->next_sibling;

	//Emit this in our else block
	cfg_result_package_t else_branch = emit_expression(else_block, cursor);

	//Again here we could have multiple blocks, so we'll need to account for this and reassign if necessary
	else_block = else_branch.final_block;

	/**
	 * Unpack the result type accordingly for the else branch's
	 * assignee
	 */
	switch(else_branch.type){
		case CFG_RESULT_TYPE_VAR:
			else_assignment = emit_assignment_instruction(else_result, else_branch.result_value.result_var);
			break;

		case CFG_RESULT_TYPE_CONST:
			else_assignment = emit_assignment_with_const_instruction(else_result, else_branch.result_value.result_const);
			break;
	}

	//Add this into the else block
	add_statement(else_block, else_assignment);

	//Now add a direct jump to the end
	emit_jump(else_block, end_block);

	//Add the final things in here
	return_package.starting_block = starting_block;
	return_package.final_block = end_block;
	//The final assignee is the temp var that we assigned to
	return_package.type = CFG_RESULT_TYPE_VAR;
	return_package.result_value.result_var =  final_result;
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
 *
 * We will not be handling the quirks of the x86 arithmetic operations inside of this block. Rather we will
 * just be emitting the straight values and letting everything else deal with that
 */
static cfg_result_package_t emit_binary_expression(basic_block_t* basic_block, generic_ast_node_t* logical_or_expr){
	//The return package here
	cfg_result_package_t package = {basic_block, basic_block, {NULL}, CFG_RESULT_TYPE_VAR, BLANK};

	//Current block may change as time goes on, so we'll use the term current block up here to refer to it
	basic_block_t* current_block = basic_block;
	
	//Any other declaration that we'll need
	three_addr_var_t* op1 = NULL;
	three_addr_var_t* op2 = NULL;
	//Optional op1_const
	three_addr_const_t* op1_const = NULL;
	three_addr_var_t* assignee;

	//What is the final result type?
	generic_type_t* final_result_type = logical_or_expr->inferred_type;

	/**
	 * Base case - we call out to the unary expression emitter from here and
	 * leave the rule
	 */
	if(logical_or_expr->ast_node_type != AST_NODE_TYPE_BINARY_EXPR){
		return emit_unary_expression(current_block, logical_or_expr);
	}

	/**
	 * Keep track of the cursor here. We will traverse in order
	 * and emit the left and right hand sides of the expression first
	 */
	generic_ast_node_t* expression_cursor = logical_or_expr->first_child;

	//Left first
	cfg_result_package_t left_side = emit_binary_expression(current_block, expression_cursor);

	//Advance it and update the block pointer
	expression_cursor = expression_cursor->next_sibling;
	current_block = left_side.final_block;

	//Then the right
	cfg_result_package_t right_side = emit_binary_expression(current_block, expression_cursor);
	//Update the block pointer
	current_block = right_side.final_block;

	//The assignee is always the same type as the expression
	assignee = emit_temp_var(logical_or_expr->inferred_type);

	/**
	 * Logical or/and expression mandate that we have
	 * temp assignments for the short circuit optimizer. It is
	 * much simpler to have temp assignments here as opposed
	 * to parsing the 4 combinations of op1/op2 being non-temp.
	 * As such if we see either of these ops we will take
	 * steps to ensure that they are temp vars
	 */
	switch(logical_or_expr->binary_operator){
		case DOUBLE_AND:
		case DOUBLE_OR:
			/**
			 * For op1 and op2, we always unpack any/all constants here if need be. That being
			 * said there shouldn't even be any constants because of the way that the parser
			 * works with these expressions, but either way we will do this for future-proofing
			 */
			op1 = unpack_result_package(&left_side, current_block);
			op2 = unpack_result_package(&right_side, current_block);

			/**
			 * IMPORTANT - for operations like these, our final result type is always a boolean. However,
			 * for the actual operation, we may have floats, ints, etc. To stop this from causing problems,
			 * we will just use the type off of op1 for our final result type here inside of the instruction
			 * itself
			 */
			final_result_type = get_operand_type_for_logical_operation(type_symtab, op1->type, op2->type);
			
			break;

		/**
		 * For all other relational operators, we do not need to do the temp assignment
		 * but we do need to get the operand type since the result type will always be
		 * a boolean which is not enough to decide what our true types are
		 */
		case L_THAN:
		case L_THAN_OR_EQ:
		case G_THAN:
		case G_THAN_OR_EQ:
		case DOUBLE_EQUALS:
		case NOT_EQUALS:
			/**
			 * Always unpack op1 - in all reality it shouldn't be a constant but just to be safe we will
			 */
			op1 = unpack_result_package(&left_side, current_block);

			/**
			 * For op2, OIR supports constants in the right operand of a binary expression
			 * so we will unpack the value here and go for it from there
			 */
			switch(right_side.type){
				/**
				 * If we have a constant, we can go straight for a bin_op_with_const statement
				 * and save the extra assignments and simplifications down the road
				 */
				case CFG_RESULT_TYPE_CONST:
					op1_const = right_side.result_value.result_const;

					//We default to op1 for a constant
					final_result_type = op1->type;

					break;

				/**
				 * Otherwise we have a regular variable value so we will
				 * unpack it accordingly and use it to help use get the result type
				 */
				case CFG_RESULT_TYPE_VAR:
					op2 = right_side.result_value.result_var;

					//Now use the helper to get the final result type
					final_result_type = get_operand_type_for_relational_operation(type_symtab, op1->type, op2->type);

					break;
			}

			/**
			 * If the final result type is an FP comparison, we need to flag this for later on down
			 * the line when we need to perform instruction selection
			 */
			if(IS_FLOATING_POINT(final_result_type) == TRUE){
				assignee->comes_from_fp_comparison = TRUE;
			}

			break;

		//Otherwise default rules are in effect
		default:
			/**
			 * We always unpack op1. In reality it should not be a constant but we will do this
			 * just to be sure
			 */
			op1 = unpack_result_package(&left_side, current_block);

			/**
			 * For op2, OIR supports constants in the right operand of a binary expression
			 * so we will unpack the value here and go for it from there
			 */
			switch(right_side.type){
				/**
				 * If we have a constant, we can go straight for a bin_op_with_const statement
				 * and save the extra assignments and simplifications down the road
				 */
				case CFG_RESULT_TYPE_CONST:
					op1_const = right_side.result_value.result_const;

					break;

				/**
				 * Otherwise we have a regular variable value so we will
				 * unpack it accordingly and use it to help use get the result type
				 */
				case CFG_RESULT_TYPE_VAR:
					op2 = right_side.result_value.result_var;

					break;
			}

			break;
	}

	//Here's the final statement
	instruction_t* binary_operation;

	/**
	 * If we don't have an op1_const, we will spit out a binary operation. If we do
	 * have an op1_const, then it will be a bin_op_with_const instruction
	 */
	if(op1_const == NULL){
		binary_operation = emit_binary_operation_instruction(assignee, op1, logical_or_expr->binary_operator, op2);
	} else {
		binary_operation = emit_binary_operation_with_const_instruction(assignee, op1, logical_or_expr->binary_operator, op1_const);
	}

	/**
	 * IMPORTANT: we will store the result type inside of the binary operation itself
	 * for down the road. This is because we may compress the instruction and end
	 * up with something like u32 = i32 * i32. Even though the result is a u32, the
	 * RHS should still be doing signed multiplication in this example. This field
	 * will help us with that
	 */
	binary_operation->type_storage.result_type = final_result_type;

	//Throw this into the current block
	add_statement(current_block, binary_operation);

	//Package up and return
	package.final_block = current_block;
	package.type = CFG_RESULT_TYPE_VAR;
	package.result_value.result_var = binary_operation->assignee;
	package.operator = logical_or_expr->binary_operator;	

	return package;
}


/**
 * Handle an assignment expression and all of the required bookkeeping that comes 
 * with it
 */
static cfg_result_package_t emit_assignment_expression(basic_block_t* basic_block, generic_ast_node_t* parent_node){
	//For unpacking
	three_addr_var_t* result_var;
	//Final return package here - this will be updated as we go
	cfg_result_package_t result_package = {basic_block, basic_block, {NULL}, CFG_RESULT_TYPE_VAR, BLANK};

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
	cfg_result_package_t right_hand_package = emit_expression(current_block, right_child);

	//Hold onto this for later
	instruction_t* last_instruction = current_block->exit_statement;

	//Reassign current to be at the end
	current_block = right_hand_package.final_block;

	//Emit the left hand unary expression
	cfg_result_package_t unary_package = emit_unary_expression(current_block, left_child);

	//Reassign current to be at the end
	current_block = unary_package.final_block;

	//This is always a var but we call the unpacker for safety
	three_addr_var_t* left_hand_var = unpack_result_package(&unary_package, current_block);

	/**
	 * Based on what kind of result we have on the right hand package,
	 * we will process accordingly
	 */
	switch(right_hand_package.type){
		/**
		 * For result types, there are many different things that we need to account
		 * for like copy assignees, store operations, and optimizations for binary expressions
		 */
		case CFG_RESULT_TYPE_VAR:
			//Extract the result variable
			result_var = right_hand_package.result_value.result_var;

			/**
			 * Is a copy assignment required between the destination and source types? This
			 * is going to have to be our first case because it may be incorrectly
			 * identified by the other checks down the road
			 */
			if(is_copy_assignment_required(left_child->inferred_type, right_child->inferred_type) == TRUE){
				//Emit the copy from the left hand var to the final op1
				instruction_t* copy_statement = emit_memory_copy_instruction(left_hand_var, result_var, parent_node->optional_storage.bytes_to_copy);

				//Get it into the block
				add_statement(current_block, copy_statement);		

			/**
			 * Do we have a pre-loaded up store statement ready for us to go? If so, then
			 * we'll need to handle this appropriately. We need to be sure that we aren't
			 * invalidly hitting this though
			 *
			 * NOTE: We check for 100% *exact* equality between the exit statement's assignee
			 * and the left hand var here. That is the only way that we can know for sure 
			 * that we aren't having a false positive
			 */
			} else if(current_block->exit_statement != NULL
						&& is_store_operation(current_block->exit_statement) == TRUE
						&& current_block->exit_statement->assignee == left_hand_var){
				//This is our store statement
				instruction_t* store_statement = current_block->exit_statement;

				/**
				 * Different store statement types have different areas where the operands go
				 */
				switch(store_statement->statement_type){
					//Store statements have the storee in op1
					case THREE_ADDR_CODE_STORE_STATEMENT:
						store_statement->op1 = result_var;
						break;

					//When we have offsets, the storee goes into op2
					case THREE_ADDR_CODE_STORE_WITH_CONSTANT_OFFSET:
					case THREE_ADDR_CODE_STORE_WITH_VARIABLE_OFFSET:
						store_statement->op2 = result_var;
						break;

					//This is unreachable, just so the compiler is happy
					default:
						break;
				}

			/**
			 * If we have a variable that is on the stack or is a global variable, then a regular assignment won't
			 * work. We'll need to do a store here
			 */
			} else if(left_hand_var->linked_var != NULL
						&& (left_hand_var->linked_var->stack_variable == TRUE
						|| is_variable_data_segment_variable(left_hand_var->linked_var) == TRUE)){

				//Emit the memory address var for this variable
				three_addr_var_t* memory_address = emit_memory_address_var(left_hand_var->linked_var);

				//Now for the final store code
				instruction_t* final_assignment = emit_store_ir_code(memory_address, result_var, left_hand_var->type);

				//Now add thi statement in here
				add_statement(current_block, final_assignment);
			
			/**
			 * If we get here, then we just have a regular variable that is not on the stack at all and is not a global variable,
			 * so a regular assignment will work just fine
			 */
			} else {
				/**
				 * If this is not a binary operation, then we will just copy it over. If it is, then we will
				 * use that binary operation for our own purposes here with the left hand var
				 */
				instruction_t* binary_expression;
				instruction_t* final_assignment;

				/**
				 * Reach back into the block to see if the last instruction that we emitted for our op1
				 * here is a constant assignment or a binary expression. If it's either, we can avoid
				 * the copy assignment for this assignment expression and go to emitting directly
				 */
				if(last_instruction != NULL
					&& last_instruction->assignee != NULL
					&& last_instruction->assignee->variable_type == VARIABLE_TYPE_TEMP
					&& variables_equal_no_ssa(last_instruction->assignee, result_var, FALSE) == TRUE){

					switch(last_instruction->statement_type){
						case THREE_ADDR_CODE_BIN_OP_STMT:
						case THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT:
							binary_expression = last_instruction;

							//Make this one's assignee the left hand var
							binary_expression->assignee = left_hand_var;
							
							break;

						/**
						 * If we have this then there's no clever optimization that we can do, we'll
						 * just emti a copy assignment
						 */
						default:
							//Finally we'll struct the whole thing
							final_assignment = emit_assignment_instruction(left_hand_var, result_var);

							//Copy this over if there is one
							left_hand_var->associated_memory_region.stack_region = result_var->associated_memory_region.stack_region;
							
							//Now add thi statement in here
							add_statement(current_block, final_assignment);

							break;
					}

				/**
				 * Otherwise there is no clever optimization that we could do, so we'll need to emit an assignment
				 * from the left hand var over to the final op1
				 */
				} else {
					//Finally we'll struct the whole thing
					final_assignment = emit_assignment_instruction(left_hand_var, result_var);

					//Copy this over if there is one
					left_hand_var->associated_memory_region.stack_region = result_var->associated_memory_region.stack_region;
					
					//Now add thi statement in here
					add_statement(current_block, final_assignment);
				}
			}
			
			break;

		/**
		 * For constant result types, really the only thing that we have
		 * to worry about is whether or not we have a store.
		 */
		case CFG_RESULT_TYPE_CONST:
			/**
			 * First case: we have a store statement that just needs a constant put to it
			 */
			if(current_block->exit_statement != NULL
				&& is_store_operation(current_block->exit_statement)
				&& current_block->exit_statement->assignee == left_hand_var){

				//Simply give this one the constant that we had
				current_block->exit_statement->op1_const = right_hand_package.result_value.result_const;

			/**
			 * Second case: If we have a variable that is on the stack or is a global variable, then a regular assignment won't
			 * work. We'll need to do a store here and emit this one ourselves
			 */
			} else if(left_hand_var->linked_var != NULL
						&& (left_hand_var->linked_var->stack_variable == TRUE 
						|| is_variable_data_segment_variable(left_hand_var->linked_var) == TRUE)){
				//Emit the memory address var for this variable
				three_addr_var_t* memory_address = emit_memory_address_var(left_hand_var->linked_var);

				//Now for the final store code
				instruction_t* final_assignment = emit_store_ir_code(memory_address, NULL, left_hand_var->type);

				//This guy's operand is the result constant
				final_assignment->op1_const = right_hand_package.result_value.result_const;

				//Now add thi statement in here
				add_statement(current_block, final_assignment);

			/**
			 * Otherwise there is nothing else for us to do here but emit a regular
			 * assign const from our result to the given one
			 */
			} else {
				//Emit it
				instruction_t* const_assignment = emit_assignment_with_const_instruction(left_hand_var, right_hand_package.result_value.result_const);

				//Throw it into the block
				add_statement(current_block, const_assignment);
			}
			
			break;
	}

	//Now pack the return value here - this is always a variable type
	result_package.type = CFG_RESULT_TYPE_VAR;
	result_package.result_value.result_var = left_hand_var;
	//This is whatever the current block is
	result_package.final_block = current_block;

	//And give the results back
	return result_package;
}


/**
 * Handle a paramcount statement. All that a paramcount statement does is load the first 4 bytes out of the elaborative
 * param stack region. This is because those first 4 bytes are where we store the parameter count
 */
static cfg_result_package_t visit_paramcount_statement(basic_block_t* basic_block, generic_ast_node_t* paramcount_node){
	cfg_result_package_t results = {basic_block, basic_block, {NULL}, CFG_RESULT_TYPE_VAR, BLANK};

	//Extract the variable
	symtab_variable_record_t* paramcount_var = paramcount_node->variable;

	//Spit out the memory address variable for this
	three_addr_var_t* base_address = emit_memory_address_var(paramcount_var);

	//Emit a temp var that is going to store our result. This will always be a u32
	three_addr_var_t* paramcount_result = emit_temp_var(paramcount_node->inferred_type);

	//Read the first 4 bytes(so in reality we have no offset)
	instruction_t* paramcount_load = emit_load_ir_code(paramcount_result, base_address, paramcount_node->inferred_type);

	//Add it into the block
	add_statement(basic_block, paramcount_load);

	//Package up the assignee inisde of these results
	results.type = CFG_RESULT_TYPE_VAR; 
	results.result_value.result_var = paramcount_result;

	//Give back the results
	return results;
}


/**
 * Emit abstract machine code for an expression. This is a top level statement.
 * These statements almost always involve some kind of assignment "<-" and generate temporary
 * variables
 */
static cfg_result_package_t emit_expression(basic_block_t* basic_block, generic_ast_node_t* expr_node){
	//We'll process based on the class of our expression node
	switch(expr_node->ast_node_type){
		case AST_NODE_TYPE_DECL_STMT:
			//Split based on the kind of variable that we have
			if(expr_node->variable->membership != STATIC_VARIABLE){
				visit_declaration_statement(expr_node);
			} else {
				visit_static_declare_statement(expr_node);
			}

			return (cfg_result_package_t)INITIALIZE_BLANK_CFG_RESULT;

		case AST_NODE_TYPE_LET_STMT:
			//Split based on the kind of variable that we have
			if(expr_node->variable->membership != STATIC_VARIABLE){
				return visit_let_statement(basic_block, expr_node);
			} else {
				visit_static_let_statement(expr_node);
				return (cfg_result_package_t)INITIALIZE_BLANK_CFG_RESULT;
			}
		
		case AST_NODE_TYPE_PARAMCOUNT_STMT:
			return visit_paramcount_statement(basic_block, expr_node);

		case AST_NODE_TYPE_ASNMNT_EXPR:
			return emit_assignment_expression(basic_block, expr_node);
	
		case AST_NODE_TYPE_BINARY_EXPR:
			return emit_binary_expression(basic_block, expr_node);

		case AST_NODE_TYPE_FUNCTION_CALL:
		case AST_NODE_TYPE_INDIRECT_FUNCTION_CALL:
			return emit_function_call(basic_block, expr_node);

		case AST_NODE_TYPE_TERNARY_EXPRESSION:
			return emit_ternary_expression(basic_block, expr_node);

		//Default is a unary expression
		default:
			return emit_unary_expression(basic_block, expr_node);
	}
}


/**
 * Emit abstract machine code for a comma separated expression chain. This rule mainly just involves
 * invoking the lower level "emit-expression" over and over until we're done
 */
static cfg_result_package_t emit_expression_chain(basic_block_t* basic_block, generic_ast_node_t* expression_chain_node){
	//Maintain a pointer to the current block
	basic_block_t* current_block = basic_block;

	//Grab a cursor to the first child
	generic_ast_node_t* expression_cursor = expression_chain_node->first_child;

	//The current expression result
	cfg_result_package_t expression_result;

	while(expression_cursor != NULL){
		//Let the helper emit it
		expression_result = emit_expression(current_block, expression_cursor);

		//Update the current block
		current_block = expression_result.final_block;

		//Push the cursor up
		expression_cursor = expression_cursor->next_sibling;
	}

	/**
	 * We are completely biased towards the last expression that we saw. As such, we will
	 * just hijack the starting block field here and use the expression result as-is
	 */
	expression_result.starting_block = basic_block;
	expression_result.final_block = current_block;

	return expression_result;
}


/**
 * Simple helper that gets the total number of general purpose parameters
 */
static inline u_int32_t get_number_of_gp_params(function_type_t* signature){
	//Initialize
	u_int32_t number_of_gp_params = 0;

	//Run through all of them
	for(u_int32_t i = 0; i < signature->function_parameters.current_index; i++){
		//Extract it
		generic_type_t* parameter_type = dynamic_array_get_at(&(signature->function_parameters), i);

		//Bump the count if it's not an FP param
		if(IS_FLOATING_POINT(parameter_type) == FALSE){
			number_of_gp_params++;
		}
	}

	//The final result
	return number_of_gp_params;
}


/**
 * Simple helper that gets the total number of floating point(SSE) parameters
 */
static inline u_int32_t get_number_of_sse_params(function_type_t* signature){
	//Initialize
	u_int32_t number_of_sse_params = 0;

	//Run through all of them
	for(u_int32_t i = 0; i < signature->function_parameters.current_index; i++){
		//Extract it
		generic_type_t* parameter_type = dynamic_array_get_at(&(signature->function_parameters), i);

		//Bump the count if is an FP param
		if(IS_FLOATING_POINT(parameter_type) == TRUE){
			number_of_sse_params++;
		}
	}

	//The final result
	return number_of_sse_params;
}


/**
 * Emit the no_error case for our handle statement. The no error case simply assigns the
 * value of what's in %rax to the overall function return value which is passed in here
 * as the function assignee
 *
 * The only thing that will be in this block is an assignment from the function's result(in %rax)
 * to the pseudo-non-temp-var that we are using for the assignee
 */
static inline basic_block_t* emit_no_error_block_for_handle(symtab_variable_record_t* result_assignee, three_addr_var_t* function_assignee){
	//This is technically a case statement, so we will add the nesting as such
	push_nesting_level(&nesting_stack, NESTING_CASE_STATEMENT);

	//Allocate and estimate the block
	basic_block_t* no_error_block = basic_block_alloc_and_estimate();

	//Emit the assignment
	instruction_t* final_assignment = emit_assignment_instruction(emit_var(result_assignee), function_assignee);
	add_statement(no_error_block, final_assignment);

	//Pop the nesting level out
	pop_nesting_level(&nesting_stack);

	//And give the block back
	return no_error_block;
}


/**
 * Emit a basic block for the case where we have a void returning function. If this
 * is the case then the no_error block is simply going to be a basic block that
 * does nothing but jump to the end. It will likely be optimized away by the optimizer
 */
static inline basic_block_t* emit_no_error_block_for_void_returning_handle(){
	//This is a case statement
	push_nesting_level(&nesting_stack, NESTING_CASE_STATEMENT);

	//We just need to allocate it inside of this nesting level
	basic_block_t* no_error_block = basic_block_alloc_and_estimate();

	//Remove it
	pop_nesting_level(&nesting_stack);

	return no_error_block;
}


/**
 * Emit the handling for the error handle instruction itself. As a reminder, the only options
 * here are: return, raise another error, or do an expression that will eventually get assigned 
 * to the overall function result. All of these options will result in a new block being
 * made and return from here
 */
static cfg_result_package_t emit_error_handle_statement(generic_ast_node_t* error_handle_node){
	//This is technically a case statement, so we will add the nesting as such
	push_nesting_level(&nesting_stack, NESTING_CASE_STATEMENT);

	//We always have a fresh block here
	basic_block_t* handler_block = basic_block_alloc_and_estimate();

	//Our overall result package
	cfg_result_package_t results = {handler_block, handler_block, {NULL}, CFG_RESULT_TYPE_VAR, BLANK};

	/**
	 * There are only 3 types that we could have - ret, raise or some expression. We will
	 * handle each accordingly
	 */
	switch(error_handle_node->first_child->ast_node_type){
		case AST_NODE_TYPE_RET_STMT:
			results = emit_return(handler_block, error_handle_node->first_child);
			break;

		case AST_NODE_TYPE_RAISE_STMT:
			//No results to capture here as this is very simple
			handle_raise_statement(handler_block, error_handle_node->first_child);
			break;

		//Nothing to do here at all, we just consume it and keep going
		case AST_NODE_TYPE_IGNORE_STMT:
			break;

		default:
			results = emit_expression(handler_block, error_handle_node->first_child);
			break;
	}

	//Pop the nesting level out
	pop_nesting_level(&nesting_stack);

	//Give back the block in the end
	return results;
}


/**
 * Emit a branch for a switch/handle statement. This is very different than the other branches that we are used to. Mainly, it will
 * not attempt to perform *any* short circuiting. It also assumes that the caller has already set everything up that is needed for the conditional.
 *
 * These are always normal branches - there is not an option for an inverse branch here
 */
static inline void emit_branch_for_switch_statement(basic_block_t* basic_block, basic_block_t* if_destination, basic_block_t* else_destination, branch_type_t branch_type, three_addr_var_t* conditional_result){
	//Emit the actual instruction here
	instruction_t* branch_instruction = emit_branch_statement(if_destination, else_destination, conditional_result, branch_type);

	/**
	 * We need to flag for later on that this conditional result is being used to set condition
	 * codes. This is especially important for the value numberer because we need to make
	 * sure that we don't optimize this away if we need said condition codes
	 */
	conditional_result->sets_cc = TRUE;

	//Mark this as the op1 so that we can track in the optimizer
	branch_instruction->op1 = conditional_result;

	//Add the statement into the block
	add_statement(basic_block, branch_instruction);

	add_successor(basic_block, if_destination);
	add_successor(basic_block, else_destination);

	//These are always normal branches
	branch_instruction->inverse_branch = FALSE;
}


/**
 * A handle statement internally becomes a switch statement based on the returned error of the function(%rdx). We will switch
 * based on %rdx and handle things accordingly. Remember that this is only a thing that exists for functions that error, non-errorable
 * functions should never have handle statements. The function itself is going to pass us its result in %rax, but that doesn't mean
 * that the final result here needs to be from %rax
 *
 * Let's also remember that every value here is unsigned. There is no such thing as a "negative" error value. Since the lowest value is
 * always 0, we only need to bother checking the upper bound here
 *
 * call my_func() handle (divide_by_zero_error_t => -1, error => raise error)
 *
 * Should translate to something like
 * switch(%rdx){
 * 		case 0 -> {
 * 			final_assignee = %rax
 * 		}
 *
 * 		case 2 -> {
 * 			final_assignee = -1
 * 		}
 *
 * 		//The catch-all error is always our default
 * 		default -> {
 * 			%rdx = 1
 * 			ret
 * 		}
 * }
 *
 * final_result_var = final_assignee
 *
 * The overall result of the function call itself will be stored in the final result var
 */
static cfg_result_package_t emit_handle_statement(basic_block_t* starting_block, generic_ast_node_t* handle_node, three_addr_var_t* function_assignee, three_addr_var_t* error_assignee){
	//Allocate the results
	cfg_result_package_t result_package;

	/**
	 * We are going to use a pseudo temp var for our final result here. We will do this because we'll
	 * need to assign to it over multiple blocks potentially. This is a temp var on the surface but
	 * under the hood it is SSA compatible so phi-functions will be inserted as needed
	 */
	symtab_variable_record_t* function_result_var = NULL;
	if(function_assignee != NULL){
		function_result_var = create_ssa_compatible_temp_var(current_function, function_assignee->type, variable_symtab, increment_and_get_temp_id());
	}

	/**
	 * Emit all of the control blocks that we'll need. When we tie this in we'll
	 * jump from the call block to the error handling block
	 */
	basic_block_t* jump_calculation_block = basic_block_alloc_and_estimate();
	basic_block_t* error_handling_ending_block = basic_block_alloc_and_estimate();
	//Holder for the default block
	basic_block_t* default_block = NULL;

	//The lower bound is always 0, and the upper bound is determined by the parser
	u_int32_t lower_bound = handle_node->lower_bound;
	u_int32_t upper_bound = handle_node->upper_bound;

	/**
	 * Let's mark this block as a switch block and at the same time create our jump table. We'll
	 * need this to be allocated before we start entering in destination blocks
	 */
	jump_calculation_block->block_type = BLOCK_TYPE_SWITCH;
	jump_calculation_block->jump_table = jump_table_alloc(upper_bound - lower_bound + 1);

	/**
	 * Let's first emit the no_error block as it is the only one that will not
	 * be inside of the handle statement. There are two separate functions
	 * to call here based on whether or not we have a void returning call
	 * or not
	 */
	basic_block_t* no_error_block;

	if(function_assignee != NULL){
		no_error_block = emit_no_error_block_for_handle(function_result_var, function_assignee);
	} else {
		no_error_block = emit_no_error_block_for_void_returning_handle();
	}

	//This one will always jump to the end block
	emit_jump(no_error_block, error_handling_ending_block);

	//Add it to the jump table. NO_ERROR is equal to 0 in %rdx
	add_jump_table_entry(jump_calculation_block->jump_table, NO_ERROR, no_error_block);

	//This is also a successor of the jump calculation block
	add_successor(jump_calculation_block, no_error_block);

	/**
	 * Run through every single error handle clause inside
	 * of the param cursor itself. Each one will receive
	 * its own block
	 */
	generic_ast_node_t* error_handle_cursor = handle_node->first_child;

	while(error_handle_cursor != NULL){
		//Let the helper do all of the emitting
		cfg_result_package_t handle_results = emit_error_handle_statement(error_handle_cursor);

		//Grab the error id out of here
		u_int32_t error_handle_value = error_handle_cursor->optional_storage.error_type->internal_types.error_type_id;

		//Add this into the jump table
		add_jump_table_entry(jump_calculation_block->jump_table, error_handle_value, handle_results.starting_block);

		//Add it in as a successor to the start block as well
		add_successor(jump_calculation_block, handle_results.starting_block);

		//Grab a pointer to the last instruction here
		instruction_t* last_instruction = handle_results.final_block->exit_statement;

		/**
		 * If we have a "raise" statement, then we may have a NULL last instruction. We'll
		 * need to couch for this case here
		 */
		if(last_instruction != NULL){
			/**
			 * If we have a ret or raise statement, we don't need to do any
			 * bookkeeping. However, if we have something else, we'll
			 * need to do 2 things:
			 * 	1.) Emit a final assignment from the result to the function_result_var
			 * 	2.) Emit a jump to the end block
			 */
			switch(last_instruction->statement_type){
				case THREE_ADDR_CODE_RET_STMT:
				case THREE_ADDR_CODE_RAISE_STMT:
					break;

				/**
				 * Note that if the function result var is in fact NULL we will never hit this - the parser doesn't
				 * allow anything besides ret, raise or ignore through for void returning functions
				 */
				default:
					//Jump from the final block to the end block
					emit_jump(handle_results.final_block, error_handling_ending_block);

					/**
					 * Based on what kind of type we got, we will emit either a constant
					 * assignment or a regular variable assignment for the handles statement
					 */
					instruction_t* result_assignment;
					switch(handle_results.type){
						case CFG_RESULT_TYPE_CONST:
							result_assignment = emit_assignment_with_const_instruction(emit_var(function_result_var), handle_results.result_value.result_const);
							break;

						case CFG_RESULT_TYPE_VAR:
							result_assignment = emit_assignment_instruction(emit_var(function_result_var), handle_results.result_value.result_var);
							break;
					}

					//Add this into the final block. It will go right before the exit
					insert_instruction_before_given(result_assignment, handle_results.final_block->exit_statement);
					break;
			}

		//It's NULL, all we need to do is emit the jump
		} else {
			//Jump from the final block to the end block
			emit_jump(handle_results.final_block, error_handling_ending_block);
		}

		/**
		 * Is this our default block? If so we need to hang onto it for later so that
		 * we can go through and populate the jump table
		 */
		if(error_handle_cursor->optional_storage.error_type->internal_types.error_type_id == GENERIC_ERROR){
			default_block = handle_results.starting_block;
		}

		//Advance the cursor up
		error_handle_cursor = error_handle_cursor->next_sibling;
	}

	/**
	 * Run through the jump table here - anything that isn't handled goes to the default
	 * generic error clause
	 */
	for(u_int32_t i = 0; i < jump_calculation_block->jump_table->nodes.current_index; i++){
		//If it's null, it's going to the default
		if(dynamic_array_get_at(&(jump_calculation_block->jump_table->nodes), i) == NULL){
			dynamic_array_set_at(&(jump_calculation_block->jump_table->nodes), default_block, i);
		}
	}

	//Emit the upper bound constant - this like everything is a u64
	three_addr_const_t* upper_bound_constant = emit_direct_integer_or_char_constant(upper_bound, u64);

	/**
	 * We will jump to the default error block if we are above this value. The branch will take this
	 * all into account
	 */
	instruction_t* comparison = emit_binary_operation_with_const_instruction(emit_temp_var(u64), error_assignee, G_THAN, upper_bound_constant);
	//Add the comparsion
	add_statement(starting_block, comparison);
	//Get the branch out - this handles everything for us
	emit_branch_for_switch_statement(starting_block, default_block, jump_calculation_block, BRANCH_A, comparison->assignee);

	/**
	 * Now we can do the indirect jump calculation and emit the indirect jump. Remember that we're already starting at 0, so we don't
	 * need to do any subtraction here
	 */
	three_addr_var_t* address = emit_indirect_jump_address_calculation(jump_calculation_block, jump_calculation_block->jump_table, error_assignee);
	emit_indirect_jump(jump_calculation_block, address);

	/**
	 * The final thing that we need to do is emit one final assignment for the error result var
	 * into some generic temporary holder. This is what we will give back to the
	 * caller as the assignee
	 */
	if(function_result_var != NULL){
		instruction_t* final_result_assingnment = emit_assignment_instruction(emit_temp_var(function_result_var->type_defined_as), emit_var(function_result_var));
		add_statement(error_handling_ending_block, final_result_assingnment);

		//This is the final assignee for the result package
		result_package.type = CFG_RESULT_TYPE_VAR;
		result_package.result_value.result_var = final_result_assingnment->assignee;
	}

	//We can already fill in the result package
	result_package.starting_block = starting_block;
	result_package.final_block = error_handling_ending_block;

	//Give back the final result package
	return result_package;
}


/**
 * Handle the parsing for a normal function parameter. This is different than the parsing for an elaborative
 * parameter, which is handled by an overloaded method
 */
static inline cfg_result_package_t emit_parameter_expression(basic_block_t* basic_block, generic_ast_node_t* parameter_node,
															  	parameter_results_array_t* parameter_results, dynamic_array_t* memory_addresses_to_adjust,
															 	u_int8_t has_stack_params){
	//Holder for the result variable;
	three_addr_var_t* result_var;
	//Keep track of our current block
	basic_block_t* current_block = basic_block;

	//Emit whatever we have here into the basic block
	cfg_result_package_t results_package = emit_expression(current_block, parameter_node);

	//Always reassign this
	current_block = results_package.final_block;

	/**
	 * Based on the result package type, we will unpack and store
	 * the results themselves accordingly
	 */
	switch(results_package.type){
		/**
		 * For a consant there are no other checks, we just throw
		 * it into the parameter result array
		 */
		case CFG_RESULT_TYPE_CONST:
			add_parameter_result_to_results_array(parameter_results, results_package.result_value.result_const, PARAM_RESULT_TYPE_CONST);
			break;

		/**
		 * For a variable result type, there will be some more work to do around
		 * memory addresses/special cases
		 */
		case CFG_RESULT_TYPE_VAR:
			//Extract the result var
			result_var = results_package.result_value.result_var;

			/**
			 * For future reference - we store all of the memory address
			 * and stack param memory address variable results that we 
			 * end up with
			 */
			if(has_stack_params == TRUE){
				switch(result_var->variable_type){
					case VARIABLE_TYPE_MEMORY_ADDRESS:
					case VARIABLE_TYPE_STACK_PARAM_MEMORY_ADDRESS:
						//Allocate it if need be
						if(memory_addresses_to_adjust->internal_array == NULL){
							*memory_addresses_to_adjust = dynamic_array_alloc();
						}

						//Throw this into storage for later
						dynamic_array_add(memory_addresses_to_adjust, result_var);

						break;

					//If it's not a memory address do nothing
					default:	
						break;
				}
			}

			//Regardless of the type we now add this in as a result
			add_parameter_result_to_results_array(parameter_results, result_var, PARAM_RESULT_TYPE_VAR);
			break;
	}

	//Give back the results in the end
	return results_package;
}


/**
 * Handle the parsing for an elaborative parameter. This rule is just meant to help
 * neaten things up because it's going to involve looping over all of the children
 * inside of the elaborative param node and emitting them separately. Note that
 * we are not going to do any kind of stack management here, that all is going
 * to come afterwards when we do the final result assignment
 */
static inline cfg_result_package_t emit_elaborative_param_expressions(basic_block_t* basic_block, generic_ast_node_t* elaborative_param_node,
																	  	parameter_results_array_t* elaborative_param_results, dynamic_array_t* memory_addresses_to_adjust){
	three_addr_var_t* result_var;
	//NOTE: we will never have an assignee here
	cfg_result_package_t result_package = INITIALIZE_BLANK_CFG_RESULT;

	//Keep track of the current block
	basic_block_t* current_block = basic_block;

	//Extract the first child here
	generic_ast_node_t* child_cursor = elaborative_param_node->first_child;
	
	/**
	 * Run through everything. Do note that it's possible to have nothing in here
	 * which is why we're not using a do-while
	 */
	while(child_cursor != NULL){
		//Emit each expression
		cfg_result_package_t expression_results = emit_expression(current_block, child_cursor);

		//Always reassign to be the final block that we got back
		current_block = expression_results.final_block;

		/**
		 * Based on the result package type, we will unpack and store
		 * the results themselves accordingly
		 */
		switch(expression_results.type){
			/**
			 * For a consant there are no other checks, we just throw
			 * it into the parameter result array
			 */
			case CFG_RESULT_TYPE_CONST:
				add_parameter_result_to_results_array(elaborative_param_results, expression_results.result_value.result_const, PARAM_RESULT_TYPE_CONST);
				break;

			/**
			 * For a variable result type, there will be some more work to do around
			 * memory addresses/special cases
			 */
			case CFG_RESULT_TYPE_VAR:
				//Extract the result var
				result_var = expression_results.result_value.result_var;

				/**
				 * For future reference - we store all of the memory address
				 * and stack param memory address variable results that we 
				 * end up with
				 */
				switch(result_var->variable_type){
					case VARIABLE_TYPE_MEMORY_ADDRESS:
					case VARIABLE_TYPE_STACK_PARAM_MEMORY_ADDRESS:
						//Allocate it if need be
						if(memory_addresses_to_adjust->internal_array == NULL){
							*memory_addresses_to_adjust = dynamic_array_alloc();
						}

						//Throw this into storage for later
						dynamic_array_add(memory_addresses_to_adjust, result_var);

						break;

					//If it's not a memory address do nothing
					default:	
						break;
				}

				//Regardless of the type we now add this in as a result
				add_parameter_result_to_results_array(elaborative_param_results, result_var, PARAM_RESULT_TYPE_VAR);
				break;
		}

		//Advance it up here
		child_cursor = child_cursor->next_sibling;
	}

	//Assign this over in case it changed
	result_package.final_block = current_block;

	//Give back the result package
	return result_package;
}


/**
 * Handle all of the storage for regular, non-elaborative parameters. This method handles everything involved, including the minutia
 * around memory address and stack parameter saving
 */
static inline void handle_parameter_storage(basic_block_t* basic_block, parameter_results_array_t* non_elaborative_parameter_results,
												stack_data_area_t* stack_passed_parameters, function_type_t* signature,
												dynamic_array_t* function_call_statement_parameters, instruction_t** first_assignment_instruction){
	//Keep track of the indices for our specific counts. This will be important if we have to do stack-saving
	u_int32_t current_sse_index = 1;
	u_int32_t current_gp_index = 1;

	//Now that we have all of this, we need to go through and emit our final assignments for the function calls
	//themselves
	for(u_int32_t i = 0; i < non_elaborative_parameter_results->current_index; i++){
		//For any/all call side regions that we need
		stack_region_t* call_side_region;

		//Get the result
		parameter_result_t* result = get_result_at_index(non_elaborative_parameter_results, i);

		//Extract the parameter type here
		generic_type_t* parameter_type = dynamic_array_get_at(&(signature->function_parameters), i);

		/**
		 * Based on what the type here is we will add stack regions/copy assignments as
		 * is appropriate
		 */
		switch(parameter_type->type_class){
			/**
			 * Unions and structs are always, without exception, passed by copy. As such we don't need
			 * to bother with anything else here. By this point we should have a memory region for these values
			 * so we can copy from the local memory region to the struct memory region
			 *
			 * NOTE: structs/unions do not count as sse or gp params, so we don't need to increment either
			 * of the counters. We also don't need to add anything to the parameter list here, the storage
			 * will be enough
			 */
			case TYPE_CLASS_UNION:
			case TYPE_CLASS_STRUCT:
				//Create it
				call_side_region = create_stack_region_for_type(stack_passed_parameters, parameter_type);

				//We only ever have variable results for this
				three_addr_var_t* variable_result = result->param_result.variable_result;

				//We'll use a dummy variable for the stack region
				three_addr_var_t* dummy_stack_region = emit_memory_address_temp_var(parameter_type, call_side_region);

				//Now we'll copy from the variable result into the dummy region
				instruction_t* memory_copy = emit_memory_copy_instruction(dummy_stack_region, variable_result, call_side_region->size);

				//Add this into the block
				add_statement(basic_block, memory_copy);

				//This is the first assignment if it's NULL
				if(*first_assignment_instruction == NULL){
					*first_assignment_instruction = memory_copy;
				}

				/**
				 * This function performs a copy assignment, so we need to make sure everything here 
				 * is going to be aligned
				 */
				current_function->requires_initial_alignment = TRUE;

				break;

			/**
			 * Everything else we can handle normally as it's not going to be passed by
			 * copy like structs or unions are
			 */
			default:
				/**
				 * Deconstruct our processing to be by-class. This is going to be important for tracking
				 * when/for which parameter we need to start doing stack allocations for(if any)
				 */
				if(IS_FLOATING_POINT(parameter_type) == FALSE){
					//We're under the limit, we don't need a stack allocation
					if(current_gp_index <= MAX_GP_REGISTER_PASSED_PARAMS){
						//Add the final assignment
						instruction_t* assignment;

						//Based on the result type we dynamically create the right assignment
						switch(result->result_type){
							case PARAM_RESULT_TYPE_CONST:
								assignment = emit_assignment_with_const_instruction(emit_temp_var(parameter_type), result->param_result.constant_result);
								break;

							case PARAM_RESULT_TYPE_VAR:
								assignment = emit_assignment_instruction(emit_temp_var(parameter_type), result->param_result.variable_result);
								break;
						}

						//This is the first assignment if it's NULL
						if(*first_assignment_instruction == NULL){
							*first_assignment_instruction = assignment;
						}

						//Add this into the block
						add_statement(basic_block, assignment);

						//Add the parameter in
						dynamic_array_add(function_call_statement_parameters, assignment->assignee);

					//If we get here then we need to do a stack allocation
					} else {
						/**
						 * If we have an array type here, we need to convert this into an equivalent pointer type
						 * instead. Since we are just using memory addresses, we can use a void* to represent
						 * this and it will be fine
						 */
						if(parameter_type->type_class == TYPE_CLASS_ARRAY){
							parameter_type = immut_void_ptr;
						}

						//Create it
						call_side_region = create_stack_region_for_type(stack_passed_parameters, parameter_type);

						//The offset. Note that this comes from the function local base address because there is no offset to add here
						three_addr_const_t* stack_offset = emit_direct_integer_or_char_constant(call_side_region->function_local_base_address, u64);

						//We need to emit a store statement now for our result
						instruction_t* store_operation;

						//Create the proper kind of store instruction based on the result that we're given
						switch(result->result_type){
							case PARAM_RESULT_TYPE_CONST:
								store_operation = emit_store_const_with_constant_offset_ir_code(stack_pointer_variable, stack_offset, result->param_result.constant_result, parameter_type);
								break;

							case PARAM_RESULT_TYPE_VAR:
								store_operation = emit_store_with_constant_offset_ir_code(stack_pointer_variable, stack_offset, result->param_result.variable_result, parameter_type);
								break;
						}

						//This is the first assignment if it's NULL
						if(*first_assignment_instruction == NULL){
							*first_assignment_instruction = store_operation;
						}

						//Add the store operation in
						add_statement(basic_block, store_operation);
					}

					//Bump the current index at the end
					current_gp_index++;

				} else {
					//We're under the limit, so we don't need a stack allocation
					if(current_sse_index <= MAX_SSE_REGISTER_PASSED_PARAMS){
						instruction_t* assignment;

						//We need a different assignment based on what kind of result it is
						switch(result->result_type){
							case PARAM_RESULT_TYPE_CONST:
								assignment = emit_assignment_with_const_instruction(emit_temp_var(parameter_type), result->param_result.constant_result);
								break;

							case PARAM_RESULT_TYPE_VAR:
								assignment = emit_assignment_instruction(emit_temp_var(parameter_type), result->param_result.variable_result);
								break;
						}

						//Add this into the block
						add_statement(basic_block, assignment);

						//This is the first assignment if it's NULL
						if(*first_assignment_instruction == NULL){
							*first_assignment_instruction = assignment;
						}

						//Add the parameter in
						dynamic_array_add(function_call_statement_parameters, assignment->assignee);

					//If we get here then we need to do a stack allocation
					} else {
						/**
						 * If we have an array type here, we need to convert this into an equivalent pointer type
						 * instead. Since we are just using memory addresses, we can use a void* to represent
						 * this and it will be fine
						 */
						if(parameter_type->type_class == TYPE_CLASS_ARRAY){
							parameter_type = immut_void_ptr;
						}

						//Create it
						call_side_region = create_stack_region_for_type(stack_passed_parameters, parameter_type);

						//The offset. Note that this comes from the function local base address because we are in the function that has
						//allocated this value
						three_addr_const_t* stack_offset = emit_direct_integer_or_char_constant(call_side_region->function_local_base_address, u64);

						//We need to emit a store statement now for our result
						instruction_t* store_operation;

						//We need a different assignment based on what kind of result it is
						switch(result->result_type){
							case PARAM_RESULT_TYPE_CONST:
								store_operation = emit_store_const_with_constant_offset_ir_code(stack_pointer_variable, stack_offset, result->param_result.constant_result, parameter_type);
								break;

							case PARAM_RESULT_TYPE_VAR:
								store_operation = emit_store_with_constant_offset_ir_code(stack_pointer_variable, stack_offset, result->param_result.variable_result, parameter_type);
								break;
						}

						//This is the first assignment if it's NULL
						if(*first_assignment_instruction == NULL){
							*first_assignment_instruction = store_operation;
						}

						//Add the store operation in
						add_statement(basic_block, store_operation);
					}

					//Bump the index at the end
					current_sse_index++;
					break;
			}
		}
	}
}


/**
 * Handle the storage for elaborative stack params. This also includes handling of the first 4 byte "count" section
 * that we also need to account for
 */
static inline void handle_elaborative_stack_param_storage(basic_block_t* basic_block, parameter_results_array_t* elaborative_param_results,
														  	stack_data_area_t* stack_passed_parameters, instruction_t** first_assignment_instruction){
	//The very first thing that we need to do is emit the paramcount helper
	u_int32_t paramcount = elaborative_param_results->current_index; 

	//This is always a u32 type
	stack_region_t* paramcount_region = create_stack_region_for_type(stack_passed_parameters, u32);

	//Emit the storage offset here
	three_addr_const_t* storage_offset = emit_direct_integer_or_char_constant(paramcount_region->function_local_base_address, u64);

	//We'll also need the paramcount constant here
	three_addr_const_t* paramcount_constant = emit_direct_integer_or_char_constant(paramcount, u32);

	//Now we have the paramcount store instruction as the very first 4 bytes in this specific region
	instruction_t* paramcount_store = emit_store_const_with_constant_offset_ir_code(stack_pointer_variable, storage_offset, paramcount_constant, u32);

	//Update this value for our stack management insertion later
	if(*first_assignment_instruction == NULL){
		*first_assignment_instruction = paramcount_store;
	}

	//Add this statement into the block
	add_statement(basic_block, paramcount_store);

	/**
	 * Now that we've accounted for the first 4 bytes, we will go through the entire list of
	 * results and create the stack regions/store those
	 */
	for(u_int32_t i = 0; i < elaborative_param_results->current_index; i++){
		//Extract the result
		parameter_result_t* elaborative_param_result = get_result_at_index(elaborative_param_results, i); 

		three_addr_var_t* result_var;
		three_addr_const_t* result_const;
		stack_region_t* variable_result_region;
		three_addr_const_t* var_storage_offset;

		/**
		 * Based on what kind of result that we have, we will handle
		 * slightly differently. Doing this allows us to condense
		 * 2 assignments for constants into just one assignment(or a store
		 * in this case)
		 */
		switch(elaborative_param_result->result_type){
			case PARAM_RESULT_TYPE_CONST:
				//Extract the constant
				result_const = elaborative_param_result->param_result.constant_result;

				//Create this one's stack region
				stack_region_t* constant_result_region = create_stack_region_for_type(stack_passed_parameters, result_const->type);

				//Emit the storage offset for this value
				three_addr_const_t* const_storage_offset = emit_direct_integer_or_char_constant(constant_result_region->function_local_base_address, u64);

				//Now emit the store instruction for the result
				instruction_t* const_elaborative_param_store = emit_store_const_with_constant_offset_ir_code(stack_pointer_variable, const_storage_offset, result_const, result_const->type); 

				//Add it into the block
				add_statement(basic_block, const_elaborative_param_store);

				break;

			case PARAM_RESULT_TYPE_VAR:
				//Extract the constant
				result_var = elaborative_param_result->param_result.variable_result;

				switch(result_var->type->type_class){
					/**
					 * For unions and structs, we perform a parameter pass by copy, and that same logic
					 * applies here for the elaborative parameter type
					 */
					case TYPE_CLASS_UNION:
					case TYPE_CLASS_STRUCT:
						//Create this one's stack region
						variable_result_region = create_stack_region_for_type(stack_passed_parameters, result_var->type);

						//We'll use a dummy variable for the stack region
						three_addr_var_t* dummy_stack_region = emit_memory_address_temp_var(result_var->type, variable_result_region);

						//Now we'll copy from the variable result into the dummy region
						instruction_t* memory_copy = emit_memory_copy_instruction(dummy_stack_region, result_var, variable_result_region->size);

						//Add this into the block
						add_statement(basic_block, memory_copy);

						/**
						 * This function performs a copy assignment, so we need to make sure everything here 
						 * is going to be aligned
						 */
						current_function->requires_initial_alignment = TRUE;
						
						break;

					/**
					 * Everything else we perform a normal store. There is no copy assignment required to make this happen
					 */
					default:
						//Create this one's stack region
						variable_result_region = create_stack_region_for_type(stack_passed_parameters, result_var->type);

						//Emit the storage offset for this value
						var_storage_offset = emit_direct_integer_or_char_constant(variable_result_region->function_local_base_address, u64);

						//Now emit the store instruction for the result
						instruction_t* var_elaborative_param_store = emit_store_with_constant_offset_ir_code(stack_pointer_variable, var_storage_offset, result_var, result_var->type); 

						//Add it into the block
						add_statement(basic_block, var_elaborative_param_store);
							
						break;
				}

				//Break from upper switch there
				break;
		}
	}
}


/**
 * Emit a call statement like such:
 *
 * call *<function_name>
 *
 *		OR
 *
 *	call <function-name>
 *
 *	How we call it depends on whether or not it's an indirect call or not
 *
 *
 * This presents an issue when dealing with variables that are stored on the stack. Basically, we're going to need to do what the parser had to
 * for regular function calls all over again here to determine what our stack region is going to look like. We don't need to do any allocations for it,
 * but we are going to need to keep track of things
 *
 * For example:
 *
 * Function 1 param stack(total size 8)
 * ------------------------------------
 * int x   Relative Offset: 4
 * int y   Relative Offset: 0
 * ------------------------------------
 *
 * .....
 * subq $8, %rsp //Create the stack fram
 * ....
 * movq param7, (%rsp)  //populate
 * movq param8, 4(%rsp) //populate
 * call function1 -> %eax
 * addq $8, %rsp //cleanup post-call
 */
static cfg_result_package_t emit_function_call(basic_block_t* basic_block, generic_ast_node_t* function_call_node){
	//Initially we'll emit this, though it may change
 	cfg_result_package_t result_package = INITIALIZE_BLANK_CFG_RESULT;

	/**
	 * Store a stack data area variable in the uppermost scope. This will only be acted upon if we see that we
	 * have stack parameters though
	 */
	stack_data_area_t stack_passed_parameters;

	/**
	 * Keep track of the first assignment instruction. We're going to need to insert
	 * the stack allocation before it
	 */
	instruction_t* first_assignment_instruction = NULL;

	//We will need the function's signature. How we get it depends on the type of call
	function_type_t* signature;

	//This is either a direct or indirect call. It doesn't matter once we get passed the conditional processing
	instruction_t* function_call_statement;

	//The function's assignee
	three_addr_var_t* function_assignee = NULL;

	/**
	 * Any/all of our conditional processing will be done here. After this everything
	 * needs to be agnostic to the node type
	 */
	switch(function_call_node->ast_node_type){
		case AST_NODE_TYPE_INDIRECT_FUNCTION_CALL:
			signature = function_call_node->variable->type_defined_as->internal_types.function_type;

			//May be NULL or not based on what we have as the return type
			if(signature->returns_void == FALSE){
				function_assignee = emit_temp_var(signature->return_type);
			}

			//We first need to emit the function pointer variable
			three_addr_var_t* function_pointer_var = emit_var(function_call_node->variable);

			//Now we can emit the indirect call statement
			function_call_statement = emit_indirect_function_call_instruction(function_pointer_var, function_assignee);

			break;

		case AST_NODE_TYPE_FUNCTION_CALL:
			signature = function_call_node->func_record->signature->internal_types.function_type;

			//May be NULL or not based on what we have as the return type
			if(signature->returns_void == FALSE){
				function_assignee = emit_temp_var(signature->return_type);
			}

			//Now we can emit the direct call statement
			function_call_statement = emit_function_call_instruction(function_call_node->func_record, function_assignee);

			break;

		//This should be unreachable but just to be sure
		default:
			fprintf(stderr, "Fatal internal compiler error. Incompatible node type found in function call handler\n");
			exit(1);
	}

	//Does the function signature contain stack params or not?
	u_int8_t has_stack_params = signature->contains_stack_params;

	/**
	 * If a function call contains stack params, we are going to have to allocate the stack data area
	 * for our stack passed parameters. This needs to be done on every function call
	 * for an indirect call, regardless of whether the stack is dynamic or static
	 */
	if(signature->contains_elaborative_stack_param == TRUE){
		stack_data_area_alloc(&stack_passed_parameters, STACK_TYPE_TEMP_USE, STACK_DATA_AREA_SIZE_TYPE_DYNAMIC);

	} else if(signature->contains_stack_params == TRUE){
		stack_data_area_alloc(&stack_passed_parameters, STACK_TYPE_TEMP_USE, STACK_DATA_AREA_SIZE_TYPE_STATIC);
	}
	
	//We'll assign the first basic block to be "current" - this could change if we hit ternary operations
	basic_block_t* current_block = basic_block;

	/**
	 * Our two result arrays will remain nulled out unless or until it can be determined
	 * that an allocation here is actually needed
	 */
	parameter_results_array_t non_elaborative_parameter_results = NULL_PARAMETER_RESULT_ARRAY_INITIALIZER;
	parameter_results_array_t elaborative_parameter_results = NULL_PARAMETER_RESULT_ARRAY_INITIALIZER;

	//Let's grab a param cursor for ourselves
	generic_ast_node_t* param_cursor = function_call_node->first_child;

	//If this isn't NULL, we have parameters
	if(param_cursor != NULL){
		function_call_statement->parameters = dynamic_array_alloc();
	}

	//Determine what the non-elaborative parameter count is
	u_int32_t non_elaborative_parameter_count = get_non_elaborative_parameter_count(signature);

	/**
	 * If we do have non-elaborative parameter results then we're ok to allocate
	 * here
	 */
	if(non_elaborative_parameter_count != 0){
		 non_elaborative_parameter_results = parameter_results_array_alloc(non_elaborative_parameter_count); 
	}

	/**
	 * If the signature itself contains an elaborative stack
	 * param then allocate a default sized one. Default sized because
	 * we cannot ever know how many there are in an elaborative param
	 */
	if(signature->contains_elaborative_stack_param == TRUE){
		elaborative_parameter_results = parameter_results_array_alloc_default_size();
	}

	/**
	 * If we have memory address variables, we are going to need to emit adjustments after
	 * we have a stack allocation statement. This will be done after the stack allocation
	 * happens, but we will need to hold onto these variables in here
	 */
	dynamic_array_t memory_addresses_to_adjust;
	INITIALIZE_NULL_DYNAMIC_ARRAY(memory_addresses_to_adjust);

	//So long as this isn't NULL
	while(param_cursor != NULL 
		&& param_cursor->ast_node_type != AST_NODE_TYPE_HANDLE_STMT){
		/**
		 * For everything that is not an elaborative param statement, we'll
		 * handle it internally to this function
		 */
		if(param_cursor->ast_node_type != AST_NODE_TYPE_ELABORATIVE_PARAM_STMT){
			//Let the helper do all of this - do note that there is not going to be anything in the "assignee" here - it's all handled internally
			cfg_result_package_t results = emit_parameter_expression(current_block, param_cursor, &non_elaborative_parameter_results, &memory_addresses_to_adjust, has_stack_params);

			//Update the final block
			current_block = results.final_block;

		/**
		 * Otherwise we have an elaborative param. Unrelated but worth nothing that this will
		 * always be the last parameter in our list. Elaborative params no matter what always
		 * come after everything else. We will let the helper emit everything here and then 
		 * handle the stack management later
		 */
		} else {
			//Let the helper do all of this - do note that there is not going to be anything in the "assignee" here - it's all handled internally
			cfg_result_package_t results = emit_elaborative_param_expressions(current_block, param_cursor, &elaborative_parameter_results, &memory_addresses_to_adjust);

			//Update the final block
			current_block = results.final_block;
		}

		//And move up
		param_cursor = param_cursor->next_sibling;
	}

	/**
	 * Now that we've emitted all of the parameters, we will let the helper deal with
	 * the storage of them
	 */
	handle_parameter_storage(current_block, &non_elaborative_parameter_results, &stack_passed_parameters, signature, &(function_call_statement->parameters), &first_assignment_instruction);

	/**
	 * If we do have elaborative stack params to manage, we will do so here
	 * using the helper method
	 */
	if(signature->contains_elaborative_stack_param == TRUE){
		handle_elaborative_stack_param_storage(current_block, &elaborative_parameter_results, &stack_passed_parameters, &first_assignment_instruction);
	}

	//We can now add the function call statement in
	add_statement(current_block, function_call_statement);

	/**
	 * Let's now handle everything that we need to do with the stack(if we're touching it at all)
	 *
	 * If we have stack parameters, then we need to emit an allocation and deallocation statement. The
	 * allocation has to go before all of the parameter assignments, and the deallocation must go immediately
	 * after the function call. However before we do all that, we need to make sure that stack memory is
	 * properly aligned, so we'll call out to the stack aligner first
	 */
	if(has_stack_params == TRUE){
		//First thing we do is align it
		align_stack_data_area(&stack_passed_parameters);

		//Now we'll emit the stack constant
		three_addr_const_t* stack_allocation_constant = emit_direct_integer_or_char_constant(stack_passed_parameters.total_size, u64);

		//Now we'll emit the allocation
		instruction_t* stack_allocation = emit_stack_allocation_ir_statement(stack_allocation_constant);

		//This must go before the first assignment that we have for our parameters
		insert_instruction_before_given(stack_allocation, first_assignment_instruction);

		//Now we'll emit the stack deallocation constant. The memory has to be separate in case of future optimization
		three_addr_const_t* stack_deallocation_constant = emit_direct_integer_or_char_constant(stack_passed_parameters.total_size, u64);

		//And then the stack deallocation statement
		instruction_t* stack_deallocation = emit_stack_deallocation_ir_statement(stack_deallocation_constant);

		//This goes right after the function call statement
		insert_instruction_after_given(stack_deallocation, function_call_statement);

		/**
		 * If we have memory addresses before stack allocations, we need to adjust the offset
		 * of the source memory region because we've emitted new stack allocation statements
		 * for it
		 */
		for(u_int32_t i = 0; i < memory_addresses_to_adjust.current_index; i++){
			//Get the variable out
			three_addr_var_t* memory_address = dynamic_array_get_at(&memory_addresses_to_adjust, i);

			//Add this in here from the memory address adjustment
			memory_address->memory_address_base_adjustment = stack_passed_parameters.total_size;
		}
	}

	/**
	 * Deallocate the two function parameter result arrays now that we're
	 * done. Also the pass by copy statements if need be
	 */
	parameter_results_array_dealloc(&non_elaborative_parameter_results);
	parameter_results_array_dealloc(&elaborative_parameter_results);
	dynamic_array_dealloc(&memory_addresses_to_adjust);

	/**
	 * If we get here and we have a handles statement, we will let our special rule
	 * translate it into a switch statement. Our strategy here is to only emit
	 * the final result assignment once we're inside of the handle statement itself
	 */
	if(param_cursor != NULL && param_cursor->ast_node_type == AST_NODE_TYPE_HANDLE_STMT){
		/**
		 * Since we have a handle statement, we have to have an error assignee. Let's also now emit that and
		 * the result assignment that comes with it
		 */
		three_addr_var_t* error_assignee = emit_temp_var(u64);

		//This is stored in the optional second assignee slot
		function_call_statement->optional_storage.error_assignee = error_assignee;

		//Now we'll have a move statement just for register allocation reasons
		instruction_t* assignment = emit_assignment_instruction(emit_temp_var(error_assignee->type), error_assignee);

		//Add it into the block
		add_statement(current_block, assignment);

		//This now is our error assignee that will be used in the CFG
		error_assignee = assignment->assignee;

		//Let the helper do the rest. It will spit back the results of the final assignment for us
		cfg_result_package_t handle_results = emit_handle_statement(current_block, param_cursor, function_assignee, error_assignee);

		//Just give back these overall results
		return handle_results;

	//If there's no error handling then we just do a regular assignment
	} else {
		//If this is not a void return type, we'll need to emit this temp assignment
		if(signature->returns_void == FALSE){
			//Emit an assignment instruction. This will become very important way down the line in register
			//allocation to avoid interference
			instruction_t* assignment = emit_assignment_instruction(emit_temp_var(function_assignee->type), function_assignee);

			//Reassign this value
			function_assignee = assignment->assignee;

			//Add it in
			add_statement(current_block, assignment);
		}

		/**
		 * This is always the assignee we gave above.
		 * Note that this is nullable, but we do always have a variable type
		 */
		result_package.type = CFG_RESULT_TYPE_VAR;
		result_package.result_value.result_var = function_assignee;

		//Always bump this up too just in case
		result_package.final_block = current_block;

		//Give back what we assigned to
		return result_package;
	}
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
	for(u_int32_t i = 0; i < cfg->function_entry_blocks.current_index; i++){
		//Clear the traversal queue
		heap_queue_clear(&traversal_queue);

		//Grab this out for convenience
		basic_block_t* function_entry_block = dynamic_array_get_at(&(cfg->function_entry_blocks), i);

		//We'll want to see what the both stacks look like
		print_passed_parameter_stack_data_area(&(function_entry_block->function_defined_in->stack_passed_parameters));
		print_local_stack_data_area(&(function_entry_block->function_defined_in->local_stack));

		//Seed the search by adding the funciton block into the queue
		enqueue(&traversal_queue, function_entry_block);

		//So long as the queue isn't empty
		while(queue_is_empty(&traversal_queue) == FALSE){
			//Pop off of the queue
			block = dequeue(&traversal_queue);

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
					enqueue(&traversal_queue, successor);
				}
			}
		}
	}
}


/**
 * Destroy all old control relations in anticipation of new ones coming in. This 
 * operates on a per-function level
 */
void cleanup_all_control_relations(dynamic_array_t* function_blocks){
	//For each block in the CFG
	for(u_int16_t _ = 0; _ < function_blocks->current_index; _++){
		//Grab the block out
		basic_block_t* block = dynamic_array_get_at(function_blocks, _);

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
	if(block->used_before_definition.internal_array != NULL){
		dynamic_array_dealloc(&(block->used_before_definition));
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

	if(cfg->local_f32_constants.internal_array != NULL){
		//Run through all of the local constants and deallcoate them
		for(u_int32_t i = 0; i < cfg->local_f32_constants.current_index; i++){
			local_constant_t* target = dynamic_array_get_at((&cfg->local_f32_constants), i);
			local_constant_dealloc(target);
		}

		//Deallocate the internal array
		dynamic_array_dealloc(&(cfg->local_f32_constants));
	}

	if(cfg->local_f64_constants.internal_array != NULL){
		for(u_int32_t i = 0; i < cfg->local_f64_constants.current_index; i++){
			local_constant_t* target = dynamic_array_get_at((&cfg->local_f64_constants), i);
			local_constant_dealloc(target);
		}

		//Deallocate the internal array
		dynamic_array_dealloc(&(cfg->local_f64_constants));
	}

	if(cfg->local_string_constants.internal_array != NULL){
		for(u_int32_t i = 0; i < cfg->local_string_constants.current_index; i++){
			local_constant_t* target = dynamic_array_get_at((&cfg->local_string_constants), i);
			local_constant_dealloc(target);
		}

		//Deallocate the internal array
		dynamic_array_dealloc(&(cfg->local_string_constants));
	}

	if(cfg->local_xmm128_constants.internal_array != NULL){
		for(u_int32_t i = 0; i < cfg->local_xmm128_constants.current_index; i++){
			local_constant_t* target = dynamic_array_get_at((&cfg->local_xmm128_constants), i);
			local_constant_dealloc(target);
		}

		//Deallocate the internal array
		dynamic_array_dealloc(&(cfg->local_xmm128_constants));
	}

	
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

	//Update the instruction counts
	a->number_of_instructions += b->number_of_instructions;

	//Delete block b now
	delete_block(b);

	//And finally we'll deallocate b
	basic_block_dealloc(b);

	//Give back the pointer to a
	return a;
}


/**
 * Can two blocks actually be merged together? We need to check their block types
 * to see that we're not violating any rules. The biggest case of this is loop
 * entry blocks and function entry blocks. Currently I only know of the one restriction
 * but we could have more get added on here as time goes on
 */
static inline u_int8_t can_blocks_be_merged(basic_block_t* a, basic_block_t* b){
	switch(a->block_type){
		/**
		 * Main example that inspired all of this - we cannot merge a function entry
		 * block and a loop block together as it creates confusion in the jump
		 * system
		 */
		case BLOCK_TYPE_FUNC_ENTRY:
			switch(b->block_type){
				case BLOCK_TYPE_LOOP_ENTRY:
					return FALSE;
				default:
					return TRUE;
			}

		case BLOCK_TYPE_NORMAL:
			return TRUE;
		default:
			return TRUE;
	}
}


/**
 * A for-statement is another kind of control flow construct
 *
 * 	For statement architecture
 *
 * 							<top-level>
 * 	      /						|			\ 
 * 	 0 or more expressions condition node  0 or more expressions
 * 	 							|
 * 	 					logical or statement?
 */
static cfg_result_package_t visit_for_statement(generic_ast_node_t* root_node){
	//Initialize the return package
	cfg_result_package_t result_package = INITIALIZE_BLANK_CFG_RESULT;

	//Create our entry block. The entry block also only executes once
	basic_block_t* for_stmt_entry_block = basic_block_alloc_and_estimate();
	//Create our exit block. We assume that the exit only happens once
	basic_block_t* for_stmt_exit_block = basic_block_alloc_and_estimate();
	//We will explicitly declare that this is an exit here
	for_stmt_exit_block->block_type = BLOCK_TYPE_LOOP_EXIT;

	/**
	 * All breaks will go to the exit block
	 * Hold off on the continue block for now
	 */
	push(&break_stack, for_stmt_exit_block);

	//Once we get here, we already know what the start and exit are for this statement
	result_package.starting_block = for_stmt_entry_block;
	result_package.final_block = for_stmt_exit_block;
	
	//Grab a cursor for our traversal
	generic_ast_node_t* cursor = root_node->first_child;

	/**
	 * The first thing that we are able to see is a chain of statements that may
	 * or may not be there. We will let our given rule handle it
	 */
	if(cursor->ast_node_type == AST_NODE_TYPE_EXPR_CHAIN){
		cfg_result_package_t expression_chain_result = emit_expression_chain(for_stmt_entry_block, cursor);

		//Update the block if need be
		for_stmt_entry_block = expression_chain_result.final_block;

		//Push the cusor up now that we're done with the expression chain
		cursor = cursor->next_sibling;
	}

	/**
	 * Once we reach here, we are officially in the loop. Everything beyond this point
	 * is going to happen repeatedly
	 */
	push_nesting_level(&nesting_stack, NESTING_LOOP_STATEMENT);

	/**
	 * We'll now need to create our repeating node. This is the node that will actually repeat from the for loop.
	 * The second and third condition in the for loop are the ones that execute continously. The third condition
	 * always executes at the end of each iteration
	 */
	basic_block_t* condition_block = basic_block_alloc_and_estimate();
	//Flag that this is a loop start
	condition_block->block_type = BLOCK_TYPE_LOOP_ENTRY;

	//We will now emit a jump from the entry block, to the condition block
	emit_jump(for_stmt_entry_block, condition_block);

	//Grab the node for the condition block - save for later
	generic_ast_node_t* condition_block_node = cursor->first_child;
	
	//Now push up to the third condition
	cursor = cursor->next_sibling;

	//Create the update block
	basic_block_t* for_stmt_update_block = basic_block_alloc_and_estimate();
	//In case stuff gets pushed around
	basic_block_t* for_stmt_update_block_end = for_stmt_update_block;

	//If we see an expression chain, we need to parse it out there
	if(cursor->ast_node_type == AST_NODE_TYPE_EXPR_CHAIN){
		cfg_result_package_t expression_chain_result = emit_expression_chain(for_stmt_update_block, cursor);

		//Update the block if need be
		for_stmt_update_block_end = expression_chain_result.final_block;

		//Push the cusor up now that we're done with the expression chain
		cursor = cursor->next_sibling;
	}
	
	//Unconditional jump to condition block
	emit_jump(for_stmt_update_block_end, condition_block);

	//All continues will go to the update block
	push(&continue_stack, for_stmt_update_block);
	
	//Otherwise, we will allow the subsidiary to handle that. The loop statement here is the condition block,
	//because that is what repeats on continue
	cfg_result_package_t compound_statement_results = visit_compound_statement(cursor);

	//Once we're done with the compound statement, we are no longer in the loop
	pop_nesting_level(&nesting_stack);

	//If we have an empty interior just emit a dummy block. It will be optimized away regardless
	if(compound_statement_results.starting_block == NULL){
		compound_statement_results.starting_block = basic_block_alloc_and_estimate();
		compound_statement_results.final_block = compound_statement_results.starting_block;
	}

	/**
	 * If we have a conditional at all, we will emit appropriate branch
	 * logic
	 *
	 * Inverse jumping logic so
	 *
	 * if not condition 
	 * 	goto exit
	 * else
	 * 	goto update
	 */
	if(condition_block_node != NULL){
		emit_branch(condition_block, condition_block_node, for_stmt_exit_block, compound_statement_results.starting_block, BRANCH_CATEGORY_INVERSE, TRUE);

	//Otherwise - we're doing as the user wants here and just emitting a straight jump from this block to the body
	} else {
		emit_jump(condition_block, compound_statement_results.starting_block);
	}

	//However if it isn't NULL, we'll need to find the end of this compound statement
	basic_block_t* compound_stmt_end = compound_statement_results.final_block;

	if(does_block_end_in_terminal_statement(compound_stmt_end) == FALSE){
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
	cfg_result_package_t result_package = INITIALIZE_BLANK_CFG_RESULT;

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
	if(does_block_end_in_function_termination_statement(compound_stmt_end) == TRUE){
		//Since we have a return block here, we know that everything else is unreachable
		result_package.final_block = compound_stmt_end;
		//And give it back
		return result_package;
	}

	/**
	 * Now we can use the helper to emit our branch 
	 *
	 * Branch works in a regular way
	 *
	 * If condition 
	 * 	goto entry
	 * else
	 * 	exit
	 */
	emit_branch(compound_stmt_end, ast_cursor->next_sibling, do_while_stmt_entry_block, do_while_stmt_exit_block, BRANCH_CATEGORY_NORMAL, TRUE);

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
	cfg_result_package_t result_package = INITIALIZE_BLANK_CFG_RESULT;

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

	//We'll need this for later
	generic_ast_node_t* conditional_cursor = ast_cursor;

	//The very next node is a compound statement
	ast_cursor = ast_cursor->next_sibling;

	//Now that we know it's a compound statement, we'll let the subsidiary handle it
	cfg_result_package_t compound_statement_results = visit_compound_statement(ast_cursor);

	//We're out of the compound statement - pop the nesting level
	pop_nesting_level(&nesting_stack);

	/**
	 * If it's null, that means that we were given an empty while loop here.
	 * We'll just allocate our own and use that
	 */
	if(compound_statement_results.starting_block == NULL){
		//Just give a dummy here
		compound_statement_results.starting_block = basic_block_alloc_and_estimate();
		compound_statement_results.final_block = compound_statement_results.starting_block;
	}

	/**
	 * Inverse jump out of the while loop to the end if bad
	 *
	 * If destination -> end of loop
	 * Else destination -> loop body
	 */
	emit_branch(while_statement_entry_block, conditional_cursor, while_statement_end_block, compound_statement_results.starting_block, BRANCH_CATEGORY_INVERSE, TRUE);

	//Let's now find the end of the compound statement
	basic_block_t* compound_stmt_end = compound_statement_results.final_block;

	/**
	 * If the block does not end in a termianl statement, we will need to emit a jump right back
	 * up to the entry block
	 */
	if(does_block_end_in_terminal_statement(compound_stmt_end) == FALSE){
		emit_jump(compound_stmt_end, while_statement_entry_block);
	}

	//Now that we're done, pop these both off their respective stacks
	pop(&break_stack);
	pop(&continue_stack);

	//Now we're done, so
	return result_package;
}


/**
 * Translate an if-else-if-else statement into CFG form, handling all possible contingencies
 * and control flow situations
 *
 *
 * Remember how a full if-else-if-else is handled, with the first child nodes being a conditional
 * and a compound statement, followed by any else if and/or else compound statements. A full
 * example is below
 *
 * 	if(x == 0) {
 * 		x += 1;
 *
 * 	} else if (x == 1) {
 * 		x += 2;
 *
 * 	} else if (x == 2) {
 * 		x += 3;
 *
 * 	} else {
 * 		x += 4;
 * 	}
 *
 * 	In AST format:
 * 										 <if - statement>
 * 			/			/						|			 					\				\
 * 	<conditional>	<compound-stmt>	 		<else-if>					   <else-if>		<compound-stmt>
 * 											/	\						 / 	 		\
 * 								<conditional>   <compound-stmt>		<conditional>  <compound-stmt>
 *
 * 	This is what we'll need to parse through and translate. This structure is chosen so that we have a minimal
 * 	memory footprint. We rely on this context being completely understood by the CFG converter here to work
 *
 * 	If we're clever about this, we can write the whole thing as one big do-while
 */
static cfg_result_package_t visit_if_statement(generic_ast_node_t* root_node){
	//Final result package
	cfg_result_package_t if_results_package = INITIALIZE_BLANK_CFG_RESULT;

	/**
	 * We maintain a current entry block for our uses. Since this is 
	 * the very first one we know that it goes up front
	 */
	basic_block_t* old_entry_block;
	basic_block_t* current_entry_block = basic_block_alloc_and_estimate();
	if_results_package.starting_block = current_entry_block;

	/**
	 * The overall exit block is where everything goes to in the end to get out
	 * of the if execution. This may change to be a function exit block if we return
	 * through every single control path
	 */
	basic_block_t* overall_exit_block = basic_block_alloc_and_estimate();

	//This cursor will help us traverse the overall if statement
	generic_ast_node_t* cursor = root_node->first_child;

	/**
	 * We will be using an infinite loop pattern for this. We have specific conditions
	 * on when this is done
	 */
	while(TRUE) {
		//An internal cursor for traversing the statement
		generic_ast_node_t* conditional_node = NULL;
		generic_ast_node_t* compound_statement_node = NULL;

		/**
		 * Look at the structure above - the way we traverse
		 * the statement internally depends on whether the cursor is an
		 * if or not
		 */
		switch(cursor->ast_node_type){
			/**
			 * For the else if we'll need to go into the child nodes
			 * of the current cursor
			 */
			case AST_NODE_TYPE_ELSE_IF_STMT:
				conditional_node = cursor->first_child;
				compound_statement_node = conditional_node->next_sibling;
				break;

			/**
			 * Otherwise we have our if statement. The conditional is the first
			 * child, compound statement is the next one. Advance the cursor as 
			 * we go
			 */
			default:
				conditional_node = cursor;
				cursor = cursor->next_sibling;
				compound_statement_node = cursor;
				break;
		}

		//Signify that this is happening inside of an IF
		push_nesting_level(&nesting_stack, NESTING_IF_STATEMENT);

		//Let the helper emit the entire compound statement
		cfg_result_package_t compound_statement_results = visit_compound_statement(compound_statement_node);

		//Remove the IF nester
		pop_nesting_level(&nesting_stack);

		/**
		 * If we have an empty if statement(possible), then we'll just go about creating
		 * a block here so we don't have any weird behavior
		 */
		if(compound_statement_results.starting_block == NULL){
			compound_statement_results.starting_block = basic_block_alloc_and_estimate();
			compound_statement_results.final_block = compound_statement_results.starting_block;
		}

		/**
		 * If the compound statement final block does not end in a return, we'll need to make
		 * it jump to the exit block. This is the overall exit block, not the current exit block
		 * which is just where we go if something didn't work
		 */
		if(does_block_end_in_terminal_statement(compound_statement_results.final_block) == FALSE){
			emit_jump(compound_statement_results.final_block, overall_exit_block);
		}

		//Bump the cursor up to the next statement
		cursor = cursor->next_sibling;

		/**
		 * CONDITIONS:
		 * 	We have another else-if coming up:
		 * 		allocate a new starting block, emit the branch, and repeat
		 *
		 * 	We have a compound statement(else condition) coming up
		 * 		emit the else compound statement, emit the branch, TERMINATE
		 *
		 * 	We have nothing(think if{} or if{}else if{} with not else) coming pu
		 * 		emit the branch to the overall exit block, TERMINATE
		 */
		if(cursor != NULL){
			switch(cursor->ast_node_type){
				/**
				 * We have an else-if statement. If this is the case, we are safe to emit
				 * a new block for the next conditional and jump to that
				 */
				case AST_NODE_TYPE_ELSE_IF_STMT:
					//Hang onto the old "entry"
					old_entry_block = current_entry_block;

					//We'll make a fresh new entry block for our else-if/else
					current_entry_block = basic_block_alloc_and_estimate();

					//Now branch out to the new current entry block
					emit_branch(old_entry_block, conditional_node, compound_statement_results.starting_block, current_entry_block, BRANCH_CATEGORY_NORMAL, FALSE);

					//Next iteration of the loop, break out of switch
					break;

				/**
				 * We have an if-else statement. If this is the case, we'll just emit the else here for ourselves
				 * and then leave when it is appropriate. Note that this is a terminal case, we are out of here 
				 * when this happens
				 */
				case AST_NODE_TYPE_COMPOUND_STMT:
					//Push the if nesting level
					push_nesting_level(&nesting_stack, NESTING_IF_STATEMENT);

					//Get the results for the else statement
					cfg_result_package_t else_compound_statement_values = visit_compound_statement(cursor);

					//Pop it off
					pop_nesting_level(&nesting_stack);

					/**
					 * If we have an empty if statement(possible), then we'll just go about creating
					 * a block here so we don't have any weird behavior
					 */
					if(else_compound_statement_values.starting_block == NULL){
						else_compound_statement_values.starting_block = basic_block_alloc_and_estimate();
						else_compound_statement_values.final_block = else_compound_statement_values.starting_block;
					}

					//Emit the final branch out
					emit_branch(current_entry_block, conditional_node, compound_statement_results.starting_block, else_compound_statement_values.starting_block, BRANCH_CATEGORY_NORMAL, FALSE);

					/**
					 * If the else if compound statement final block does not end in a return, we'll need to make
					 * it jump to the overall exit block
					 */
					if(does_block_end_in_terminal_statement(else_compound_statement_values.final_block) == FALSE){
						emit_jump(else_compound_statement_values.final_block, overall_exit_block);
					}

					/**
					 * If we have an exit block that has no predecessors, that means that we return through every
					 * control path. In this instance, we need to set the result package's final block to be the
					 * exit block
					 */
					if(overall_exit_block->predecessors.current_index == 0){
						if_results_package.final_block = function_exit_block;
					} else {
						if_results_package.final_block = overall_exit_block;
					}

					return if_results_package;

				//Should never happen
				default:
					fprintf(stderr, "Fatal internal compiler error: invalid node found in if statement processor\n");
					exit(1);
			}
		
		/**
		 * This is a terminal case - we're done so we can set the final block and get out. We'd get here if 
		 * we had something like if(){} and then nothing else. Note that we don't need to check the overall
		 * exit block here because we know that it's going to have at least one predecessor(this block)
		 */
		} else {
			emit_branch(current_entry_block, conditional_node, compound_statement_results.starting_block, overall_exit_block, BRANCH_CATEGORY_NORMAL, FALSE);
			if_results_package.final_block = overall_exit_block;

			return if_results_package;
		}
	}
}


/**
 * Visit a default statement.  These statements are also handled like individual blocks that can 
 * be jumped to
 */
static cfg_result_package_t visit_default_statement(generic_ast_node_t* root_node){
	//Declare and prepack our results
	cfg_result_package_t results = INITIALIZE_BLANK_CFG_RESULT;

	//This is nesting inside of a case statement
	push_nesting_level(&nesting_stack, NESTING_CASE_STATEMENT);

	/**
	 * For a default statement, it performs very similarly to a case statement. 
	 * It will be handled slightly differently in the jump table, but we'll get to that 
	 * later on
	 */

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
	cfg_result_package_t results = INITIALIZE_BLANK_CFG_RESULT;

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
	cfg_result_package_t result_package = INITIALIZE_BLANK_CFG_RESULT;

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
	cfg_result_package_t result_package = INITIALIZE_BLANK_CFG_RESULT;

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
	cfg_result_package_t result_package = INITIALIZE_BLANK_CFG_RESULT;

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
	cfg_result_package_t input_results = emit_expression(root_level_block, cursor);

	//Update the block
	root_level_block = input_results.final_block;

	//This is a switch type block
	jump_calculation_block->block_type = BLOCK_TYPE_SWITCH;

	//We'll now allocate this one's jump table
	jump_calculation_block->jump_table = jump_table_alloc(root_node->upper_bound - root_node->lower_bound + 1);

	//The offset(amount that we'll need to knock down any case values by) is always the 
	//case statement's value subtracted by the lower bound. We'll call it offset here
	//for consistency
	int32_t offset = root_node->lower_bound;

	//A generic result package for all of our case/default statements
	cfg_result_package_t case_default_results = INITIALIZE_BLANK_CFG_RESULT;

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
					case THREE_ADDR_CODE_RAISE_STMT:
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
			case THREE_ADDR_CODE_RAISE_STMT:
			case THREE_ADDR_CODE_JUMP_STMT:
				break;

			/**
			 * However if we have this, we need to ensure that we go from this final block
			 * directly to the end
			 */
			default:
				/**
				 * Emit the direct jump. This may be optimized away in the optimizer, but we
				 * need to guarantee behavior
				 */
				emit_jump(current_block, ending_block);

				break;
		}

	//Otherwise it is null, so we definitely need a jump to the end here
	} else {
		/**
		 * Emit the direct jump. This may be optimized away in the optimizer, but we
		 * need to guarantee behavior
		 */
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

	/**
	 * Run through the entire jump table. Any nodes that are not occupied(meaning there's no case statement with that value)
	 * will be set to point to the default block. 
	 */
	for(u_int16_t i = 0; i < jump_calculation_block->jump_table->num_nodes; i++){
		/**
		 * If it's null, we'll make it the default. This should only happen in switches
		 * that are non-exhaustive. For exhaustive switches, the parser has already ensured that we
		 * will have a block for every case value
		 */
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

	//Unpack the results from the result package
	three_addr_var_t* input_result = unpack_result_package(&input_results, root_level_block);

	//Grab the type our for convenience
	generic_type_t* input_result_type = input_result->type;

	//Grab the signedness of the result
	u_int8_t is_signed = is_type_signed(input_result_type);

	//This will be used for tracking
	three_addr_var_t* lower_than_decider = emit_temp_var(input_result_type);

	//Let's first do our lower than comparison
	//First step -> if we're below the minimum, we jump to default 
	emit_binary_operation_with_constant(root_level_block, lower_than_decider, input_result, L_THAN, lower_bound);

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
	emit_branch_for_switch_statement(root_level_block, default_block, upper_bound_check_block, branch_lower_than, lower_than_decider);

	//This will be used for tracking
	three_addr_var_t* higher_than_decider = emit_temp_var(input_result_type);

	//Now we handle the case where we're above the upper bound
	emit_binary_operation_with_constant(upper_bound_check_block, higher_than_decider, input_result, G_THAN, upper_bound);

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
	emit_branch_for_switch_statement(upper_bound_check_block, default_block, jump_calculation_block, branch_greater_than, higher_than_decider);

	//To avoid violating SSA rules, we'll emit a temporary assignment here
	instruction_t* temporary_variable_assignent = emit_assignment_instruction(emit_temp_var(input_result_type), input_result);

	//Add it into the block
	add_statement(jump_calculation_block, temporary_variable_assignent);

	//Now that all this is done, we can use our jump table for the rest
	//We'll now need to cut the value down by whatever our offset was	
	three_addr_var_t* input = emit_binary_operation_with_constant(jump_calculation_block, temporary_variable_assignent->assignee, temporary_variable_assignent->assignee, MINUS, emit_direct_integer_or_char_constant(offset, i32));

	/**
	 * Now that we've subtracted, we'll need to do the address calculation. The address calculation is as follows:
	 * 	base address(.JT1) + input * 8 
	 *
	 * We have a special kind of statement for doing this
	 * 	
	 */
	//Emit the address first
	three_addr_var_t* address = emit_indirect_jump_address_calculation(jump_calculation_block, jump_calculation_block->jump_table, input);

	//Now we'll emit the indirect jump to the address
	emit_indirect_jump(jump_calculation_block, address);

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
	cfg_result_package_t result_package = INITIALIZE_BLANK_CFG_RESULT;

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
	cfg_result_package_t input_results = emit_expression(root_level_block, case_stmt_cursor);

	//We could have had a ternary here, so we'll need to account for that possibility
	root_level_block = input_results.final_block;

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
	cfg_result_package_t case_default_results = INITIALIZE_BLANK_CFG_RESULT;

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

		//If the block is empty *or* it doesn't end in a return, add the jump
		if(does_block_end_in_terminal_statement(current_block) == FALSE){
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

	//Unpack the result to get our actual variable out
	three_addr_var_t* input_result = unpack_result_package(&input_results, root_level_block);

	//Grab the type our for convenience
	generic_type_t* input_result_type = input_result->type;

	//Grab the signedness of the result
	u_int8_t is_signed = is_type_signed(input_result_type);

	//This will be used for tracking
	three_addr_var_t* lower_than_decider = emit_temp_var(input_result_type);

	//Let's first do our lower than comparison
	//First step -> if we're below the minimum, we jump to default 
	emit_binary_operation_with_constant(root_level_block, lower_than_decider, input_result, L_THAN, lower_bound);

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
	emit_branch_for_switch_statement(root_level_block, default_block, upper_bound_check_block, branch_lower_than, lower_than_decider);

	//This will be used for tracking
	three_addr_var_t* higher_than_decider = emit_temp_var(input_result_type);

	//Now we handle the case where we're above the upper bound
	emit_binary_operation_with_constant(upper_bound_check_block, higher_than_decider, input_result, G_THAN, upper_bound);

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
	emit_branch_for_switch_statement(upper_bound_check_block, default_block, jump_calculation_block, branch_greater_than, higher_than_decider);

	//To avoid violating SSA rules, we'll emit a temporary assignment here
	instruction_t* temporary_variable_assignent = emit_assignment_instruction(emit_temp_var(input_result_type), input_result);

	//Add it into the block
	add_statement(jump_calculation_block, temporary_variable_assignent);

	//Now that all this is done, we can use our jump table for the rest
	//We'll now need to cut the value down by whatever our offset was	
	three_addr_var_t* input = emit_binary_operation_with_constant(jump_calculation_block, temporary_variable_assignent->assignee, temporary_variable_assignent->assignee, MINUS, emit_direct_integer_or_char_constant(offset, i32));

	/**
	 * Now that we've subtracted, we'll need to do the address calculation. The address calculation is as follows:
	 * 	base address(.JT1) + input * 8 
	 *
	 * We have a special kind of statement for doing this
	 * 	
	 */
	//Emit the address first
	three_addr_var_t* address = emit_indirect_jump_address_calculation(jump_calculation_block, jump_calculation_block->jump_table, input);

	//Now we'll emit the indirect jump to the address
	emit_indirect_jump(jump_calculation_block, address);

	//Give back the starting block
	return result_package;
}


/**
 * Handle everything needed to emit a raise statement successfully. This includes updating the successor
 * of this current block to be the function's exit block
 */
static inline void handle_raise_statement(basic_block_t* basic_block, generic_ast_node_t* node){
	//Let's first extract the error value from this
	u_int64_t error_value = node->optional_storage.error_id;

	//Now we'll emit the error constant
	three_addr_const_t* error_constant = emit_direct_integer_or_char_constant(error_value, i64);

	//Following that we'll need a temporary assignment for this
	instruction_t* temp_assignment = emit_assignment_with_const_instruction(emit_temp_var(i64), error_constant);

	//Add this into the block
	add_statement(basic_block, temp_assignment);

	//Now we can emit the raises statement itself
	instruction_t* raise_statement = emit_raise_instruction(temp_assignment->assignee);

	//Add this into the block
	add_statement(basic_block, raise_statement);

	//The successor of this block is now the function end block(remember that raise = ret)
	add_successor(basic_block, function_exit_block);
}


/**
 * Visit a sequence of statements one after the other. This is used for C-style case and default
 * statement processing. Note that unlike a compound statement, there is no new lexical scope
 * initialized, and we never descend the tree. We only go from sibling to sibling
 */
static cfg_result_package_t visit_statement_chain(generic_ast_node_t* first_node){
	//A generic results package that we can use in any of our processing
	cfg_result_package_t generic_results = INITIALIZE_BLANK_CFG_RESULT;
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
			case AST_NODE_TYPE_RAISE_STMT:
				//Allocate if we don't have
				if(starting_block == NULL){
					starting_block = basic_block_alloc_and_estimate();
					current_block = starting_block;
				}

				//Let the helper do the work
				handle_raise_statement(current_block, ast_cursor); 

				//If there is anything after this statement, it is UNREACHABLE
				if(ast_cursor->next_sibling != NULL){
					print_cfg_message(MESSAGE_TYPE_WARNING, "Unreachable code detected after raise statement", ast_cursor->next_sibling->line_number);
					(*num_warnings_ref)++;
				}

				//Package up the results package
				generic_results.starting_block = current_block;
				generic_results.final_block = current_block;

				//We're done here - get out
				return generic_results;

			case AST_NODE_TYPE_RET_STMT:
				//If for whatever reason the block is null, we'll create it
				if(starting_block == NULL){
					//We assume that this only happens once
					starting_block = basic_block_alloc_and_estimate();
					current_block = starting_block;
				}

				//Emit the return statement, let the sub rule handle
			 	generic_results = emit_return(current_block, ast_cursor);

				//Update the current block
				current_block = generic_results.final_block;

				//If there is anything after this statement, it is UNREACHABLE
				if(ast_cursor->next_sibling != NULL){
					print_cfg_message(MESSAGE_TYPE_WARNING, "Unreachable code detected after return statement", ast_cursor->next_sibling->line_number);
					(*num_warnings_ref)++;
				}

				//Package up the values
				generic_results.starting_block = starting_block;
				generic_results.final_block = current_block;

				//We're completely done here
				return generic_results;
		
			case AST_NODE_TYPE_IF_STMT:
				//We'll now enter the if statement
				generic_results = visit_if_statement(ast_cursor);
			
				//Once we have the if statement start, we'll add it in as a successor
				if(starting_block == NULL){
					starting_block = generic_results.starting_block;
					current_block = generic_results.final_block;

				} else {
					emit_jump(current_block, generic_results.starting_block);
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

				/**
				 * There are two options here. We could see a regular continue or a conditional
				 * continue. If the child is null, then it is a regular continue
				 */
				if(ast_cursor->first_child == NULL){
					//Peek the continue block off of the stack
					basic_block_t* continuing_to = peek(&continue_stack);

					//We always jump to the start of the loop statement unconditionally
					emit_jump(current_block, continuing_to);

					//Package and return
					generic_results = (cfg_result_package_t)INITIALIZE_BLANK_CFG_RESULT;

					/**
					 * We're done here, so return the starting block. There is no 
					 * point in going on
					 */
					return generic_results;

				//Otherwise, we have a conditional continue here
				} else {
					//Grab the conditional cursor
					generic_ast_node_t* conditional_expression = ast_cursor->first_child;

					//We'll need a new block here - this will count as a branch
					basic_block_t* new_block = basic_block_alloc_and_estimate();

					//Peek the continue block off of the stack
					basic_block_t* continuing_to = peek(&continue_stack);

					/**
					 * Now we will emit the branch like so
					 *
					 * if condition:
					 * 	goto continue_block
					 * else:
					 * 	goto new block
					 */
					emit_branch(current_block, conditional_expression, continuing_to, new_block, BRANCH_CATEGORY_NORMAL, TRUE);

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

				/**
				 * There are two options here: We could have a conditional break
				 * or a normal break. If there is no child node, we have a normal break
				 */
				if(ast_cursor->first_child == NULL){
					//Peak off of the break stack to get what we're breaking to
					basic_block_t* breaking_to = peek(&break_stack);

					//We will jump to it -- this is always an uncoditional jump
					emit_jump(current_block, breaking_to);

					//Package and return
					generic_results = (cfg_result_package_t)INITIALIZE_BLANK_CFG_RESULT;

					/**
					 * For a regular break statement, this is it, so we just get out
					 * and give back the starting block
					 */
					return generic_results;

				//Otherwise, we have a conditional break, which will generate a conditional jump instruction
				} else {
					generic_ast_node_t* conditional_node = ast_cursor->first_child;

					//We'll also need a new block to jump to, since this is a conditional break
					basic_block_t* new_block = basic_block_alloc_and_estimate();

					//Peak off of the break stack to get what we're breaking to
					basic_block_t* breaking_to = peek(&break_stack);

					/**
					 * Let the helper come here and emit the branch for us
					 *
					 * if conditional
					 * 	goto end block
					 * else 
					 * 	goto new block
					 */
					emit_branch(current_block, conditional_node, breaking_to, new_block, BRANCH_CATEGORY_NORMAL, TRUE);

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
			 * Label statements are unique because they'll force the creation of a new block with a
			 * given label name. We will store the block with the label to make future correlations 
			 * easier
			 */
			case AST_NODE_TYPE_LABEL_STMT:
				//Allocate the label statement as the current block
				labeled_block = labeled_block_alloc(ast_cursor->optional_storage.label_record);

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

			//A straight unconditional jump statement
			case AST_NODE_TYPE_JUMP_STMT:
				//This really shouldn't happen, but it can't hurt
				if(starting_block == NULL){
					starting_block = basic_block_alloc_and_estimate();
					current_block = starting_block;
				}

				//Let the helper emit it
				emit_user_defined_jump(current_block, ast_cursor->optional_storage.label_record);

				/**
				 * The new current block will be the block that comes after this one. It will
				 * be completely disconnected(at least on paper)
				 */
				current_block = basic_block_alloc_and_estimate();

				break;
		
			//A conditional user-defined jump works somewhat like a break
			case AST_NODE_TYPE_CONDITIONAL_JUMP_STMT:
				//This really shouldn't happen, but it can't hurt
				if(starting_block == NULL){
					starting_block = basic_block_alloc_and_estimate();
					current_block = starting_block;
				}

				//First child is the conditional
				generic_ast_node_t* binary_expression_cursor = ast_cursor->first_child;

				/**
				 * The if block comes from the ast cursor's variable, the else
				 * block will be allocated fresh
				 */
				symtab_label_record_t* if_destination_label = ast_cursor->optional_storage.label_record; 
				basic_block_t* else_block = basic_block_alloc_and_estimate();

				//Let the helper emit the actual branch
				emit_user_defined_branch(current_block, binary_expression_cursor, if_destination_label, else_block);

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
				emit_assembly_inline(current_block, ast_cursor);
			
				break;

			case AST_NODE_TYPE_IDLE_STMT:
				//Do we need a new block?
				if(starting_block == NULL){
					starting_block = basic_block_alloc_and_estimate();
					current_block = starting_block;
				}

				//Let the helper handle -- doesn't even need the cursor
				emit_idle(current_block);
				
				break;

			//This means that we have some kind of expression statement
			case AST_NODE_TYPE_EXPR_CHAIN:
				//This could happen where we have nothing here
				if(starting_block == NULL){
					starting_block = basic_block_alloc_and_estimate();
					current_block = starting_block;
				}
				
				//Also emit the simplified machine code
				generic_results = emit_expression_chain(current_block, ast_cursor);

				//Update the end block
				current_block = generic_results.final_block;
				
				break;
				
			//Shouldn't ever happen
			default:
				printf("Fatal internal compiler error: unreachable path hit in CFG\n");
				exit(1);
		}

		//If this is the exit block, it means that we returned through every control path
		//in here and there is no point in moving forward. We'll simply return
		if(current_block == function_exit_block){
			//Warn that we have unreachable code here
			if(ast_cursor->next_sibling != NULL){
				print_cfg_message(MESSAGE_TYPE_WARNING, "Unreachable code detected after segment that returns in all control paths", ast_cursor->next_sibling->line_number);
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
	cfg_result_package_t results = INITIALIZE_BLANK_CFG_RESULT;
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
			/**
			 * A raise statement is what allows a function to effectively
			 * throw errors. Raise statements are final and unrecoverable,
			 * the function must and will exit when one of these is hit
			 */
			case AST_NODE_TYPE_RAISE_STMT:
				//Allocate if we don't have
				if(starting_block == NULL){
					starting_block = basic_block_alloc_and_estimate();
					current_block = starting_block;
				}

				//Let the helper do the work
				handle_raise_statement(current_block, ast_cursor); 

				//If there is anything after this statement, it is UNREACHABLE
				if(ast_cursor->next_sibling != NULL){
					print_cfg_message(MESSAGE_TYPE_WARNING, "Unreachable code detected after raise statement", ast_cursor->next_sibling->line_number);
					(*num_warnings_ref)++;
				}

				//Package up the results package
				results.starting_block = starting_block;
				results.final_block = current_block;

				//We're done here - get out
				return results;

			case AST_NODE_TYPE_RET_STMT:
				//If for whatever reason the block is null, we'll create it
				if(starting_block == NULL){
					//We assume that this only happens once
					starting_block = basic_block_alloc_and_estimate();
					current_block = starting_block;
				}

				//Emit the return statement, let the sub rule handle
			 	generic_results = emit_return(current_block, ast_cursor);

				//Update the current block
				current_block = generic_results.final_block;

				//If there is anything after this statement, it is UNREACHABLE
				if(ast_cursor->next_sibling != NULL){
					print_cfg_message(MESSAGE_TYPE_WARNING, "Unreachable code detected after return statement", ast_cursor->next_sibling->line_number);
					(*num_warnings_ref)++;
				}

				//Package up the values
				results.starting_block = starting_block;
				results.final_block = current_block;

				//We're completely done here
				return results;
		
			case AST_NODE_TYPE_IF_STMT:
				//We'll now enter the if statement
				generic_results = visit_if_statement(ast_cursor);
			
				//Once we have the if statement start, we'll add it in as a successor
				if(starting_block == NULL){
					starting_block = generic_results.starting_block;
					current_block = generic_results.final_block;

				} else {
					emit_jump(current_block, generic_results.starting_block);
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

				/**
				 * There are two options here. We could see a regular continue or a conditional
				 * continue. If the child is null, then it is a regular continue
				 */
				if(ast_cursor->first_child == NULL){
					//Peek the continue block off of the stack
					basic_block_t* continuing_to = peek(&continue_stack);

					//We always jump to the start of the loop statement unconditionally
					emit_jump(current_block, continuing_to);

					//Package and return
					results = (cfg_result_package_t)INITIALIZE_BLANK_CFG_RESULT;

					/**
					 * We're done here, so return the starting block. There is no 
					 * point in going on
					 */
					return results;

				//Otherwise, we have a conditional continue here
				} else {
					//Grab the conditional cursor
					generic_ast_node_t* conditional_expression = ast_cursor->first_child;

					//We'll need a new block here - this will count as a branch
					basic_block_t* new_block = basic_block_alloc_and_estimate();

					//Peek the continue block off of the stack
					basic_block_t* continuing_to = peek(&continue_stack);

					/**
					 * Now we will emit the branch like so
					 *
					 * if condition:
					 * 	goto continue_block
					 * else:
					 * 	goto new block
					 */
					emit_branch(current_block, conditional_expression, continuing_to, new_block, BRANCH_CATEGORY_NORMAL, TRUE);

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

				/**
				 * There are two options here: We could have a conditional break
				 * or a normal break. If there is no child node, we have a normal break
				 */
				if(ast_cursor->first_child == NULL){
					//Peak off of the break stack to get what we're breaking to
					basic_block_t* breaking_to = peek(&break_stack);

					//We will jump to it -- this is always an uncoditional jump
					emit_jump(current_block, breaking_to);

					//Package and return
					results = (cfg_result_package_t)INITIALIZE_BLANK_CFG_RESULT;

					//For a regular break statement, this is it, so we just get out
					return results;

				//Otherwise, we have a conditional break, which will generate a conditional jump instruction
				} else {
					generic_ast_node_t* conditional_node = ast_cursor->first_child;

					//We'll also need a new block to jump to, since this is a conditional break
					basic_block_t* new_block = basic_block_alloc_and_estimate();

					//Peak off of the break stack to get what we're breaking to
					basic_block_t* breaking_to = peek(&break_stack);

					/**
					 * Let the helper come here and emit the branch for us
					 *
					 * if conditional
					 * 	goto end block
					 * else 
					 * 	goto new block
					 */
					emit_branch(current_block, conditional_node, breaking_to, new_block, BRANCH_CATEGORY_NORMAL, TRUE);

					//Once we're out here, the current block is now the new one
					current_block = new_block;
				}

				break;

			case AST_NODE_TYPE_DEFER_STMT:
				//Grab a cursor here
				defer_statement_cursor = ast_cursor->first_child;

				/**
				 * So long as this cursor is not null, we'll keep processing and adding
				 * compound statements
				 */
				while(defer_statement_cursor != NULL){
					//Let the helper process this
					cfg_result_package_t compound_statement_results = visit_compound_statement(defer_statement_cursor);

					/**
					 * The successor to the current block is this block
					 * If it's null then this is this block
					 */
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
				labeled_block = labeled_block_alloc(ast_cursor->optional_storage.label_record);

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

			//A straight unconditional jump statement
			case AST_NODE_TYPE_JUMP_STMT:
				//This really shouldn't happen, but it can't hurt
				if(starting_block == NULL){
					starting_block = basic_block_alloc_and_estimate();
					current_block = starting_block;
				}

				//Let the helper emit it
				emit_user_defined_jump(current_block, ast_cursor->optional_storage.label_record);

				/**
				 * The new current block will be the block that comes after this one. It will
				 * be completely disconnected(at least on paper)
				 */
				current_block = basic_block_alloc_and_estimate();

				break;

			//A conditional user-defined jump works somewhat like a break
			case AST_NODE_TYPE_CONDITIONAL_JUMP_STMT:
				//This really shouldn't happen, but it can't hurt
				if(starting_block == NULL){
					starting_block = basic_block_alloc_and_estimate();
					current_block = starting_block;
				}

				//First child is the conditional
				generic_ast_node_t* binary_expression_cursor = ast_cursor->first_child;

				/**
				 * The if block comes from the ast cursor's variable, the else
				 * block will be allocated fresh
				 */
				symtab_label_record_t* if_destination_label = ast_cursor->optional_storage.label_record;
				basic_block_t* else_block = basic_block_alloc_and_estimate();

				//Let the helper emit the actual branch
				emit_user_defined_branch(current_block, binary_expression_cursor, if_destination_label, else_block);

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
				emit_assembly_inline(current_block, ast_cursor);
			
				break;

			case AST_NODE_TYPE_IDLE_STMT:
				//Do we need a new block?
				if(starting_block == NULL){
					starting_block = basic_block_alloc_and_estimate();
					current_block = starting_block;
				}

				//Let the helper handle -- doesn't even need the cursor
				emit_idle(current_block);
				
				break;

			//This means that we have some kind of expression statement
			case AST_NODE_TYPE_EXPR_CHAIN:
				//This could happen where we have nothing here
				if(starting_block == NULL){
					starting_block = basic_block_alloc_and_estimate();
					current_block = starting_block;
				}
				
				//Also emit the simplified machine code
				generic_results = emit_expression_chain(current_block, ast_cursor);

				//Update the end block
				current_block = generic_results.final_block;
				
				break;
				
			//Shouldn't ever happen
			default:
				printf("Fatal internal compiler error: unreachable path hit in CFG\n");
				exit(1);
		}

		//If this is the exit block, it means that we returned through every control path
		//in here and there is no point in moving forward. We'll simply return
		if(current_block == function_exit_block){
			//Warn that we have unreachable code here
			if(ast_cursor->next_sibling != NULL){
				print_cfg_message(MESSAGE_TYPE_WARNING, "Unreachable code detected after segment that returns in all control paths", ast_cursor->next_sibling->line_number);
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

		//If the exit statement is not a return statement or is null, we need to know what's happening here
		if(does_block_end_in_function_termination_statement(block) == FALSE){
			//If this isn't void, then we need to throw a warning
			if((function_defined_in->return_type->type_class != TYPE_CLASS_BASIC
				|| function_defined_in->return_type->basic_type_token != VOID)
				//It's a technically supported use-case to not put a return on main
				&& is_main_function == FALSE){
				print_parse_message(MESSAGE_TYPE_WARNING, "Non-void function does not return in all control paths", 0);
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
static inline void finalize_all_user_defined_jump_statements(dynamic_array_t* user_defined_jumps){
	//Run through every jump statement in here
	for(u_int32_t i = 0; i < user_defined_jumps->current_index; i++){
		//Grab the jump/branch out
		instruction_t* instruction = dynamic_array_get_at(user_defined_jumps, i);

		/**
		 * We expect that, by this point, all of our labeled blocks have been allocated. Label
		 * records store the block inside of them, so all that we should need to do is
		 * verify that there is a block, and replace internally if so
		 */
		symtab_label_record_t* jumping_to_label = instruction->optional_storage.jumping_to_label;

		//Something went very wrong if this is the case
		if(jumping_to_label->block == NULL){
			fprintf(stderr, "Fatal internal compiler error: labeled block not properly handled\n");
			exit(1);
		}

		//Make this our if block
		instruction->if_block = jumping_to_label->block;

		//Add this in as a successor
		add_successor(instruction->block_contained_in, instruction->if_block);

		//If it was a branch, add the else block as a successor too
		if(instruction->statement_type == THREE_ADDR_CODE_BRANCH_STMT){
			add_successor(instruction->block_contained_in, instruction->else_block);
		}
	}
}


/**
 * Setup all of the parameters that we need for our function. This will handle the
 * parameter aliasing that we're all used to, and it will handle the use of stack
 * parameters as well. It is vital that this function execute *before* any actual
 * user code is translated. 
 *
 * If a function has more than 6 of any kind of parameter, this is how the stack frame
 * will look
 *
 * --------------- Function caller ----------------
 * --------
 * ALL STACK PASSED PARAMS
 * --------
 * --------------- Function caller ----------------
 * --------
 * Return address(8 bytes)
 * --------
 * --------------- Function x ----------------
 * ----------
 * All other storage(arrays, structs, etc)
 * ----------
 * ----------
 * Storage for normal params that we take the address of
 * ----------
 * --------------- Function x ----------------
 *
 *
 * Stack passed parameters convention: any/all stack passed parameters will be *at the bottom* of the stack frame, and will
 * be stored in the order that they are declared via the function signature. This means that if we have float, int, float, they
 * will be stored at the very bottom in that order
 *
 * NOTE: if we are passing in primitive types via the stack(basically anything that isn't a struct or a union of a struct), we
 * are going to copy those values inside of this parameter setup. We do this because we don't want to be unnecessarily grabbing
 * values out of the stack over and over again if we can help it. For non-primitive types *or* types whose memory address we
 * take, this is going to be a different story
 */
static inline void setup_function_parameters(symtab_function_record_t* function_record, basic_block_t* function_entry_block){
	/**
	 * If we have function parameters that are *also* stack variables(meaning the user will
	 * at some point want to take the memory address of them), then we need to load
	 * these variables into the stack preemptively
	 */
	/**
	 * Now we are going to process all of our normal function parameters. There is a special
	 * case that we need to account for: if the user takes that address of a *non stack-passed*
	 */
	for(u_int32_t i = 0; i < function_record->function_parameters.current_index; i++){
		//Extract the parameter
		symtab_variable_record_t* parameter = dynamic_array_get_at(&(function_record->function_parameters), i);

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
		if(parameter->stack_variable == FALSE 
			&& parameter->type_defined_as->type_class != TYPE_CLASS_ELABORATIVE){
			//Create the aliased variable
			symtab_variable_record_t* alias = create_parameter_alias_variable(current_function, parameter, variable_symtab, increment_and_get_temp_id());

			//Very important that we emit this first for the below reason
			three_addr_var_t* parameter_var = emit_var(parameter);

			//Emit the alias that we're assigning to
			three_addr_var_t* alias_var = emit_var(alias);

			/**
			 * Flag that the parameter does have this alias. Note that once we do this, any time
			 * emit_var() is called on the parameter, the alias will be used instead so the order
			 * here is very important. Once this is done - there is no going back
			 */
			parameter->alias = alias;

			//Emit the assignment here
			instruction_t* alias_assignment = emit_assignment_instruction(alias_var, parameter_var);

			//Now add the statement in
			add_statement(function_entry_block, alias_assignment);
			
		/**
		 * Otherwise, if we have a stack variable *and* it's not been passed
		 * along by the stack, we need to create a stack region for it. It is critical
		 * that we only do this if this is *not* passed via the stack
		 */
		} else if(parameter->passed_by_stack == FALSE){
			//Add this variable onto the stack now, since we know it is not already on it
			parameter->stack_region = create_stack_region_for_type(&(current_function->local_stack), parameter->type_defined_as);

			//Copy the type over here
			three_addr_var_t* parameter_var = emit_memory_address_var(parameter);

			//Now we'll need to do our initial load
			instruction_t* store_code = emit_store_ir_code(parameter_var, emit_var(parameter), parameter->type_defined_as);

			//Add it into the starting block
			add_statement(function_entry_block, store_code);
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
	//Store the pointer to this function's array of blocks too. This will be used by every basic_block_alloc() call
	current_function_blocks = &(func_record->function_blocks);
	//We also need to zero out the current stack offset value
	stack_offset = 0;
	//Set this to NULL initially - we will only allocate if we need it
	INITIALIZE_NULL_DYNAMIC_ARRAY(current_function_user_defined_jump_statements);
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

	//Setup the function parameters. The helper does all of this
	setup_function_parameters(func_record, function_starting_block); 

	//We don't care about anything until we reach the compound statement
	generic_ast_node_t* func_cursor = function_node->first_child;

	//It could be null, though it usually is not
	if(func_cursor != NULL){
		//Once we get here, we know that func cursor is the compound statement that we want
		cfg_result_package_t compound_statement_results = visit_compound_statement(func_cursor);

		//Holder for the exit block
		basic_block_t* compound_statement_exit_block;

		/**
		 * If we are able to merge these two blocks, then we will. If we are not, then we will
		 * emit a jump from the function's start to the compound statement start
		 */
		if(can_blocks_be_merged(function_starting_block, compound_statement_results.starting_block) == TRUE){
			//Merge the two since we can
 			compound_statement_exit_block = merge_blocks(function_starting_block, compound_statement_results.starting_block);

			/**
			 * Due to the way that the merging works, we need to make sure that the reassignment
			 * is valid. If it's just one big block, then the prior assignment above is actually
			 * fine
			 */
			if(compound_statement_results.starting_block != compound_statement_results.final_block){
				compound_statement_exit_block = compound_statement_results.final_block;
			}

		} else {
			//Could not merge, just jump into this block
			emit_jump(function_starting_block, compound_statement_results.starting_block);

			//We can just straight assign here
			compound_statement_exit_block = compound_statement_results.final_block;
		}

		//If these two are not equal, we'll add a successor as the exit block
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
	finalize_all_user_defined_jump_statements(&current_function_user_defined_jump_statements);

	//Add the start and end blocks to their respective arrays
	dynamic_array_add(&(cfg->function_entry_blocks), function_starting_block);
	dynamic_array_add(&(cfg->function_exit_blocks), function_exit_block);

	//Remove it now that we're done
	pop_nesting_level(&nesting_stack);

	/**
	 * Once we are fully complete, we will go through and search for any/all useless
	 * statements to delete
	 */
	delete_all_unreachable_blocks(current_function_blocks);

	/**
	 * Let's now use the helper to compute all of the USE/DEF sets for each
	 * block in the function
	 */
	compute_use_and_def_sets_for_function(current_function_blocks);

	/**
	 * Now compute the dominance relations
	 */
	calculate_all_control_relations(function_starting_block, current_function_blocks);

	/**
	 * Finally, we will calculate the liveness sets for this function
	 */
	calculate_liveness_sets(current_function_blocks, function_starting_block);

	//Now that we're done, we will clear this current function parameter
	current_function = NULL;

	//Mark this as NULL for the next go around
	function_exit_block = NULL;

	//Deallocate the current function's user defined jumps as well
	dynamic_array_dealloc(&current_function_user_defined_jump_statements);

	//We always return the start block
	return function_starting_block;
}


/**
 * Specifically emit a global variable string constant. This is only valid for strings in the global
 * variable context. No other context for this will work
 */
static inline three_addr_const_t* emit_global_variable_string_constant(generic_ast_node_t* string_initializer){
	//First we'll dynamically allocate the constant
	three_addr_const_t* constant = calloc(1, sizeof(three_addr_const_t));

	//Now we'll assign the appropriate values
	constant->const_type = STR_CONST;
	constant->type = string_initializer->inferred_type;

	//Extract what we need out of it
	constant->constant_value.string_constant = string_initializer->string_value.string;

	return constant;
}


/**
 * Emit a constant for the express purpose of being used in a global variable. Such
 * a constant does not need to abide by the same rules that non-global constants
 * need to because it is already in the ELF text and not trapped in the assembly
 */
static three_addr_const_t* emit_global_variable_constant(generic_ast_node_t* const_node){
	//First we'll dynamically allocate the constant
	three_addr_const_t* constant = calloc(1, sizeof(three_addr_const_t));

	//A holder for later if need be
	three_addr_var_t* string_local_constant;

	//Now we'll assign the appropriate values
	constant->const_type = const_node->constant_type; 
	constant->type = const_node->inferred_type;

	//Based on the type we'll make assignments. It'll be said again here - this is the only
	//time that double/float constants can be emitted directly without the use of the .LC system
	switch(constant->const_type){
		case CHAR_CONST:
			constant->constant_value.char_constant = const_node->constant_value.char_value;
			break;
		case BYTE_CONST:
			constant->constant_value.signed_byte_constant = const_node->constant_value.signed_byte_value;
			break;
		case BYTE_CONST_FORCE_U:
			constant->constant_value.unsigned_byte_constant = const_node->constant_value.unsigned_byte_value;
			break;
		case INT_CONST:
			constant->constant_value.signed_integer_constant = const_node->constant_value.signed_int_value;
			break;
		case INT_CONST_FORCE_U:
			constant->constant_value.unsigned_integer_constant = const_node->constant_value.unsigned_int_value;
			break;
		case SHORT_CONST:
			constant->constant_value.signed_short_constant = const_node->constant_value.signed_short_value;
			break;
		case SHORT_CONST_FORCE_U:
			constant->constant_value.unsigned_short_constant = const_node->constant_value.unsigned_short_value;
			break;
		case LONG_CONST:
			constant->constant_value.signed_long_constant = const_node->constant_value.signed_long_value;
			break;
		case LONG_CONST_FORCE_U:
			constant->constant_value.unsigned_long_constant = const_node->constant_value.unsigned_long_value;
			break;
		case DOUBLE_CONST:
			constant->constant_value.double_constant = const_node->constant_value.double_value;
			break;
		case FLOAT_CONST:
			constant->constant_value.float_constant = const_node->constant_value.float_value;
			break;
		/**
		 * If we made it here, that specifically means that we are dealing with a char* constant. This is
		 * an important distinction, because it will require that we emit a .LC local constant value and
		 * then a pointer to it
		 */
		case STR_CONST:
			//Let's first emit the string local constant
			string_local_constant = emit_string_local_constant(cfg, const_node);

			//Now we'll assign the appropriate values
			constant->const_type = REL_ADDRESS_CONST;

			//Extract what we need out of it
			constant->constant_value.local_constant_address = string_local_constant;

			break;
			
		//Some very weird error here
		default:
			printf("Fatal internal compiler error: unrecognizable constant type found in constant\n");
			exit(1);
	}
	
	//Once all that is done, we can leave
	return constant;
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

			//Another base case of sorts - this is for char[] variables
			case AST_NODE_TYPE_STRING_INITIALIZER:
				//Emit it and add it into the array
				dynamic_array_add(initializer_values, emit_global_variable_string_constant(cursor));
				break;

			//This is really our base case
			case AST_NODE_TYPE_CONSTANT:
				//Emit the constant and get it into the array
				dynamic_array_add(initializer_values, emit_global_variable_constant(cursor));
				break;

			default:
				printf("%d\n\n\n", cursor->ast_node_type);
				printf("Fatal internal compiler: Invalid or unimplemented global initializer node encountered\n");
				exit(1);
		}

		//Advance to the next one
		cursor = cursor->next_sibling;
	}
}


/**
 * This helper function is used to determine if we need to place a global variable
 * in the ".rel.local" section. This is only done for char* variables *or* anything
 * that decays into a char*
 */
static u_int8_t does_type_decay_to_char_pointer(generic_type_t* type){
	switch(type->type_class){
		case TYPE_CLASS_ARRAY:
			return does_type_decay_to_char_pointer(type->internal_types.member_type);

		case TYPE_CLASS_POINTER:
			//This is what we're after
			if(type->internal_types.points_to == char_type){
				return TRUE;
			}
			
			return does_type_decay_to_char_pointer(type->internal_types.points_to);

		case TYPE_CLASS_BASIC:
			return FALSE;

		default:
			return FALSE;
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

	//Figure out what this decays into
	global_variable->is_relative = does_type_decay_to_char_pointer(node->variable->type_defined_as);

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

		//Let the helper take over with this one as well
		case AST_NODE_TYPE_STRING_INITIALIZER:
			//This is a special kind of constant
			global_variable->initializer_type = GLOBAL_VAR_INITIALIZER_STRING;

			//This will handle a variety of cases for us
			global_variable->initializer_value.constant_value = emit_global_variable_string_constant(initializer);

			break;

		//This shouldn't be reachable
		default:
			printf("Fatal internal compiler error: Unrecognized/unimplemented global initializer node type encountered\n");
			exit(1);
	}
}


/**
 * Visit a static let statement and handle the initializer appropriately.
 * Do note that we have already checked that the entire initialization
 * only contains constants, so we can assume we're only processing constants
 * here
 */
static void visit_static_let_statement(generic_ast_node_t* node){
	/**
	 * We'll store it inside of the global variable struct. Leave it as NULL
	 * here so that it's automatically initialized to 0
	 */
	global_variable_t* static_variable = create_global_variable(node->variable, NULL);

	//This has been initialized already
	static_variable->variable->initialized = TRUE;

	//Figure out what this decays into
	static_variable->is_relative = does_type_decay_to_char_pointer(node->variable->type_defined_as);

	//And add it into the CFG
	dynamic_array_add(&(cfg->global_variables), static_variable);

	//Grab out the initializer node
	generic_ast_node_t* initializer = node->first_child;

	//We can see arrays or constants here
	switch(initializer->ast_node_type){
		//Array init list - goes to the helper
		case AST_NODE_TYPE_ARRAY_INITIALIZER_LIST:
			//Initialized to an array
			static_variable->initializer_type = GLOBAL_VAR_INITIALIZER_ARRAY;

			//Give it an array of values
			static_variable->initializer_value.array_initializer_values = dynamic_array_alloc();

			//Let the helper take care of it
			emit_global_array_initializer(initializer, &(static_variable->initializer_value.array_initializer_values));

			break;
		
		//Should be our most common case - we just have a constant
		case AST_NODE_TYPE_CONSTANT:
			//Initialized to a constant
			static_variable->initializer_type = GLOBAL_VAR_INITIALIZER_CONSTANT;

			//All we need to do here
			static_variable->initializer_value.constant_value = emit_global_variable_constant(initializer);

			break;

		//Let the helper take over with this one as well
		case AST_NODE_TYPE_STRING_INITIALIZER:
			//This is a special kind of constant
			static_variable->initializer_type = GLOBAL_VAR_INITIALIZER_STRING;

			//This will handle a variety of cases for us
			static_variable->initializer_value.constant_value = emit_global_variable_string_constant(initializer);

			break;

		//This shouldn't be reachable
		default:
			printf("Fatal internal compiler error: Unrecognized/unimplemented static initializer node type encountered\n");
			exit(1);
	}
}


/**
 * Visit a global variable declaration statement
 *
 * NOTE: declared global variables will always be initialized to be 0
 */
static inline void visit_global_declare_statement(generic_ast_node_t* node){
	//We'll store it inside of the global variable struct. Leave it as NULL
	//here so that it's automatically initialized to 0
	global_variable_t* global_variable = create_global_variable(node->variable, NULL);

	//This has no initializer-so flag that here
	global_variable->initializer_type = GLOBAL_VAR_INITIALIZER_NONE;

	//And add it into the CFG
	dynamic_array_add(&(cfg->global_variables), global_variable);
}


/**
 * Visit a static variable declaration statement
 *
 * NOTE: declared static variables will always be initialized to be 0
 */
static inline void visit_static_declare_statement(generic_ast_node_t* node){
	//We'll store it inside of the global variable struct. Leave it as NULL
	//here so that it's automatically initialized to 0
	global_variable_t* static_variable = create_global_variable(node->variable, NULL);

	//This has no initializer-so flag that here
	static_variable->initializer_type = GLOBAL_VAR_INITIALIZER_NONE;

	//And add it into the CFG
	dynamic_array_add(&(cfg->global_variables), static_variable);
}

/**
 * Visit a declaration statement. If we see an actual declaration node, then
 * we know that this is either a struct, array or union - it's something that
 * has to be allocated and placed onto the stack
 */
static void visit_declaration_statement(generic_ast_node_t* node){
	//Create a stack region for this variable
	node->variable->stack_region = create_stack_region_for_type(&(current_function->local_stack), node->inferred_type);
}


/**
 * Emit a base level intialization given an offset, base address and a node. When we do this,
 * we'll have something like:
 *
 * store base_address[offset] <- emit_expression(node)
 */
static cfg_result_package_t emit_final_initialization(basic_block_t* current_block, three_addr_var_t* base_address, u_int32_t offset, generic_ast_node_t* expression_node){
	//Holder for the final assignee
	three_addr_var_t* final_assignee;
	//Initialize our final results
	cfg_result_package_t final_results = {current_block, current_block, {base_address}, CFG_RESULT_TYPE_VAR, BLANK};

	//Now let's emit the expression using the node
	cfg_result_package_t expression_results = emit_expression(current_block, expression_node);

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

	/**
	 * Based on what result type we have we can process accordingly
	 */
	switch(expression_results.type){
		/**
		 * Constant type is simple - just assign over the result value
		 */
		case CFG_RESULT_TYPE_CONST:
			store_instruction->op1_const = expression_results.result_value.result_const;
			break;

		/**
		 * For variable types we have some specialized rules around memory address variables
		 * that we need to account for
		 */
		case CFG_RESULT_TYPE_VAR:
			//Extract the final assignee
			final_assignee = expression_results.result_value.result_var;

			/**
			 * If we have a memory address variable, we need to emit a final assignment
			 * to because our instruction selector is not designed to handle MEM<> variables
			 * on the RHS of an initializer equation. This is an easy fix
			 */
			if(final_assignee->variable_type == VARIABLE_TYPE_MEMORY_ADDRESS){
				//Assign this over
				instruction_t* temp_assignment = emit_assignment_instruction(emit_temp_var(final_assignee->type), final_assignee);

				//Add it into the block
				add_statement(current_block, temp_assignment);

				//This now is the final assignee
				final_assignee = temp_assignment->assignee;
			}

			//This is now our op2
			store_instruction->op2 = final_assignee;
			break;
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
static cfg_result_package_t emit_array_initializer(basic_block_t* current_block, three_addr_var_t* base_address, u_int32_t current_offset, generic_ast_node_t* array_initializer){
	//Initialize the results package here to start
	cfg_result_package_t results = {current_block, current_block, {NULL}, CFG_RESULT_TYPE_VAR, BLANK};

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
				initializer_results = emit_array_initializer(current_block, base_address, offset, cursor);
				break;

			case AST_NODE_TYPE_STRING_INITIALIZER:
				//Pass the new base offset along to this rule
				initializer_results = emit_string_initializer(current_block, base_address, offset, cursor);
				break;

			case AST_NODE_TYPE_STRUCT_INITIALIZER_LIST:
				//Pass the new base offset along to this rule
				initializer_results = emit_struct_initializer(current_block, base_address, offset, cursor);
				break;

			//When we hit the default case, that means that we've stopped seeing initializer values
			default:
				//Once we get here, we need to let the helper finish it off
				initializer_results = emit_final_initialization(current_block, base_address, offset, cursor);
				break;
		}

		//Update the current block
		current_block = initializer_results.final_block;

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
static cfg_result_package_t emit_string_initializer(basic_block_t* current_block, three_addr_var_t* base_address, u_int32_t offset, generic_ast_node_t* string_initializer ){
	//Initialize the results package here to start
	cfg_result_package_t results = {current_block, current_block, {NULL}, CFG_RESULT_TYPE_VAR, BLANK};

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

		//We can skip the assignment here and just directly put the constant in
		store_instruction->op1_const = constant;

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
static cfg_result_package_t emit_struct_initializer(basic_block_t* current_block, three_addr_var_t* base_address, u_int32_t offset, generic_ast_node_t* struct_initializer){
	//Initialize the results package here to start
	cfg_result_package_t results = {current_block, current_block, {NULL}, CFG_RESULT_TYPE_VAR, BLANK};

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
				initializer_results = emit_array_initializer(current_block, base_address, current_offset, cursor);
				break;
			case AST_NODE_TYPE_STRING_INITIALIZER:
				initializer_results = emit_string_initializer(current_block, base_address, current_offset, cursor);
				break;
			case AST_NODE_TYPE_STRUCT_INITIALIZER_LIST:
				initializer_results = emit_struct_initializer(current_block, base_address, current_offset, cursor);
				break;

			default:
				initializer_results = emit_final_initialization(current_block, base_address, current_offset, cursor);
				break;
		}

		//Update the current block
		current_block = initializer_results.final_block;

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
 * Emit a normal intialization
 *
 * We'll hit this when we have something like:
 *
 * let x:i32 = a + b + c;
 *
 * No array/string/struct initializers here
 */
static cfg_result_package_t emit_simple_initialization(basic_block_t* current_block, three_addr_var_t* let_variable, generic_ast_node_t* expression_node){
	//Allocate the return package here
	cfg_result_package_t let_results = {current_block, current_block, {let_variable}, CFG_RESULT_TYPE_VAR, BLANK};

	//Emit the right hand expression here
	cfg_result_package_t package = emit_expression(current_block, expression_node);

	//Reassign what the current block is in case it's changed
	current_block = package.final_block;

	//Store the last instruction for later use
	instruction_t* last_instruction = current_block->exit_statement;

	//Now update the final block
	let_results.final_block = current_block;

	/**
	 * Go based on what the final result type is
	 */
	switch(let_results.type){
		case CFG_RESULT_TYPE_VAR:
			break;

		case CFG_RESULT_TYPE_CONST:
			/**
			 * If we have a variable that requires a store assignment, we will
			 * emit that now
			 */
			if(let_variable->linked_var != NULL
				&& (let_variable->linked_var->stack_variable == TRUE)
					|| is_variable_data_segment_variable(let_variable->linked_var) == TRUE){
				/**
				 * Store the "true" stored type. This will only change if our type is a reference, because
				 * we need to account for the implicit dereference that's happening
				 */
				generic_type_t* true_stored_type = let_variable->type;

				//NOTE: We use the type of our let variable here for the address assignment
				three_addr_var_t* base_address = emit_memory_address_var(let_variable->linked_var);
				
				//Emit the store code
				instruction_t* store_statement = emit_store_ir_code(base_address, NULL, true_stored_type);

				//Set the store statement's op1_const to be this
				store_statement->op1_const = let_results.result_value.result_const;

				//Now add thi statement in here
				add_statement(current_block, store_statement);
			}


			break;
	}


	/**
	 * Is a copy assignment required between the two variables? This will only
	 * occur if we have a struct to struct or union to union assignment but if we do,
	 * we'll need some special handling for it
	 */
	if(is_copy_assignment_required(let_variable->type, expression_node->inferred_type) == TRUE){
		//Emit the copy from the left hand var to the final op1. The copy size is always the let variable's size
		instruction_t* copy_statement = emit_memory_copy_instruction(let_variable, final_op1, let_variable->type->type_size);

		//Get it into the block
		add_statement(current_block, copy_statement);

	/**
	 * Is the left hand variable a regular variable or is it a stack address variable? If it's a
	 * variable that is on the stack, then a regular assignment just won't do. We'll need to
	 * emit a store operation
	 */
	} else if(let_variable->linked_var == NULL || let_variable->linked_var->stack_variable == FALSE){
		instruction_t* assignment_statement;
		instruction_t* binary_operation;
		instruction_t* const_assignment;

		/**
		 * If we have an exit statement *and* we are dealing with what the final_op1 is, we may
		 * be able to shrink our footprint here
		 */
		if(current_block->exit_statement != NULL
			&& current_block->exit_statement->assignee != NULL
			&& current_block->exit_statement->assignee->variable_type == VARIABLE_TYPE_TEMP
			&& variables_equal_no_ssa(final_op1, current_block->exit_statement->assignee, FALSE) == TRUE){

			switch(current_block->exit_statement->statement_type){
				/**
				 * For binary operations we can hijack the statement itself
				 */
				case THREE_ADDR_CODE_BIN_OP_STMT:
				case THREE_ADDR_CODE_BIN_OP_WITH_CONST_STMT:
					binary_operation = current_block->exit_statement;

					//Just replace it with our variable
					binary_operation->assignee = let_variable;

					break;

				/**
				 * Same with a constant assignment statement
				 */
				case THREE_ADDR_CODE_ASSN_CONST_STMT:
					const_assignment = current_block->exit_statement;

					//Just replace it with our variable
					const_assignment->assignee = let_variable;

					break;

				/**
				 * Something else here - don't know what it is but we play it safe
				 * and assign things over
				 */
				default:
					//The actual statement is the assignment of right to left
					assignment_statement = emit_assignment_instruction(let_variable, final_op1);

					//Finally we'll add this into the overall block
					add_statement(current_block, assignment_statement);
			}

		/**
		 * No fancy optimizations here - just emit an assignment over and we'll be
		 * fine here
		 */
		} else {
			//The actual statement is the assignment of right to left
			instruction_t* assignment_statement = emit_assignment_instruction(let_variable, final_op1);

			//Finally we'll add this into the overall block
			add_statement(current_block, assignment_statement);
		}
			
	/**
	 * Otherwise, we'll need to emit a store operation here
	 */
	} else {
		/**
		 * Store the "true" stored type. This will only change if our type is a reference, because
		 * we need to account for the implicit dereference that's happening
		 */
		generic_type_t* true_stored_type = let_variable->type;

		//NOTE: We use the type of our let variable here for the address assignment
		three_addr_var_t* base_address = emit_memory_address_var(let_variable->linked_var);
		
		//Emit the store code
		instruction_t* store_statement = emit_store_ir_code(base_address, NULL, true_stored_type);

		/**
		 * If we have a constant assignment, then we can do a small optimization 
		 * here by scrapping the  constant assignment and just putting the
		 * constant in directly
		 */
		if(last_instruction != NULL
			&& last_instruction->statement_type == THREE_ADDR_CODE_ASSN_CONST_STMT
			&& last_instruction->assignee->variable_type == VARIABLE_TYPE_TEMP
			&& variables_equal_no_ssa(last_instruction->assignee, final_op1, FALSE) == TRUE){

			//Extract it
			three_addr_const_t* constant_assignee = last_instruction->op1_const;

			//This is now useless
			delete_statement(last_instruction);

			//Set the store statement's op1_const to be this
			store_statement->op1_const = constant_assignee;

		/**
		 * Otherwise no fancy optimizations are possible, we'll just store this as the op1
		 */
		} else {
			store_statement->op1 = final_op1;
		}
				
		//Now add thi statement in here
		add_statement(current_block, store_statement);
	}

	//And give it back
	return let_results;
}


/**
 * Emit an initialization statement given only a variable and
 * the top level of what could be a larger initialization sequence
 *
 * For more complex initializers, we're able to bypass the emitting of extra instructions and simply emit the 
 * offset that we need directly. We're able to do this because all array and struct initialization statements at
 * the end of the day just calculate offsets.
 *
 * There is chance that we'll need to just default to a regular simple initialization here in the event that
 * we have a struct copy. This is no big deal and a supported use case
 */
static inline cfg_result_package_t emit_complex_initialization(basic_block_t* current_block, three_addr_var_t* base_address, generic_ast_node_t* initializer_root){
	switch(initializer_root->ast_node_type){
		//Make a direct call to the rule. Seed with 0 as the initial offset
		case AST_NODE_TYPE_STRING_INITIALIZER:
			return emit_string_initializer(current_block, base_address, 0, initializer_root);

		//Make a direct call to the rule. Seed with 0 as the initial offset
		case AST_NODE_TYPE_STRUCT_INITIALIZER_LIST:
			return emit_struct_initializer(current_block, base_address, 0, initializer_root);
		
		//Make a direct call to the array initializer. We'll "seed" with 0 as the starting address
		case AST_NODE_TYPE_ARRAY_INITIALIZER_LIST:
			return emit_array_initializer(current_block, base_address, 0, initializer_root);

		/**
		 * It is possible for our struct copy assignments that we may hit a simple initialization here. This
		 * is perfectly fine, and we will just let this rule handle it
		 */
		default:
			return emit_simple_initialization(current_block, base_address, initializer_root);
	}
}


/**
 * Visit a let statement
 */
static cfg_result_package_t visit_let_statement(basic_block_t* starting_block, generic_ast_node_t* node){
	//Create the return package here
	cfg_result_package_t let_results = {starting_block, starting_block, {NULL}, CFG_RESULT_TYPE_VAR, BLANK};

	//The current block is the start block
	basic_block_t* current_block = starting_block;

	//Extract the type here
	generic_type_t* type = node->inferred_type;

	//The assignee of the let statement. This could either be a variable or it could represent
	//a base address for an array
	three_addr_var_t* assignee;

	//Based on what type we have, we'll need to do some special intialization
	switch(type->type_class){
		/**
		 * Array, structures and unions are all stored on the stack. So, when
		 * we see one, we need to make sure that we are actually allocating the stack space for it
		 */
		case TYPE_CLASS_ARRAY:
		case TYPE_CLASS_STRUCT:
		case TYPE_CLASS_UNION:
			//Create a stack region for this variable and store it in the associated region
			node->variable->stack_region = create_stack_region_for_type(&(current_function->local_stack), node->inferred_type);

			//Emit the memory address variable
			assignee = emit_memory_address_var(node->variable);

			//The left hand var is our assigned var
			let_results.type = CFG_RESULT_TYPE_VAR;
			let_results.result_value.result_var = assignee;

			//We know that this will be the lead block
			let_results.starting_block = current_block;
			
			//Invoke the complex initialization method. We know that we have a struct, array or string initializer here
			cfg_result_package_t package = emit_complex_initialization(current_block, assignee, node->first_child);

			//This is also the final block for now, unless a ternary comes along
			let_results.final_block = package.final_block;

			//And give the block back
			return let_results;
			
		//Otherwise we just have a garden variety variable - no stack allocation required
		default:
			//Emit it
			assignee = emit_var(node->variable);

			//Let the helper rule deal with the rest here
			return emit_simple_initialization(current_block, assignee, node->first_child);
	}
}


/**
 * Run through a namespace declaration and output all of its members. The members
 * that it could have are function defintions or more namespace decalarations, making
 * this rule recursive
 */
static u_int8_t visit_namespace_declaration(cfg_t* cfg, generic_ast_node_t* namespace_declaration_node){
	//Grab a cursor to traverse
	generic_ast_node_t* namespace_child = namespace_declaration_node->first_child;
	//For our function definitions
	basic_block_t* block;

	//So long as we still have children
	while(namespace_child != NULL){
		switch(namespace_child->ast_node_type){
			case AST_NODE_TYPE_FUNC_DEF:
				block = visit_function_definition(cfg, namespace_child);

				//-1 block id means it failed(very rare)
				if(block->block_id == -1){
					return FAILURE;
				}
				
				break;

			case AST_NODE_TYPE_NAMESPACE_DECLARATION:
				if(visit_namespace_declaration(cfg, namespace_child) == FAILURE){
					return FAILURE;
				}

				break;
				
			//Some very weird error if we hit here. Hard exit to avoid dev confusion
			default:
				fprintf(stderr, "Fatal internal compiler error: Unrecognized node type found in namespace scope\n");
				exit(1);
		}

		//Bump it up
		namespace_child = namespace_child->next_sibling;
	}

	//It worked so give back success
	return SUCCESS;
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
	//Did we succeed or not?
	u_int8_t success = TRUE;

	//So long as the AST cursor is not null
	while(ast_cursor != NULL){
		//Switch based on the class of cursor that we have here
		switch(ast_cursor->ast_node_type){
			/**
			 * We've seen a function defintion. In this case we'll
			 * let the helper deal with it
			 */
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

			case AST_NODE_TYPE_NAMESPACE_DECLARATION:
				success = visit_namespace_declaration(cfg, ast_cursor);

				if(success == FAILURE){
					return FAILURE;
				}

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
	return SUCCESS;
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

	//Now print all of the local constants
	print_local_constants(stdout, &(cfg->local_string_constants), &(cfg->local_f32_constants), &(cfg->local_f64_constants), &(cfg->local_xmm128_constants));
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
 * Calculate all reverse traversals for a given function
 */
void calculate_all_reverse_traversals(basic_block_t* function_entry_block, dynamic_array_t* function_blocks){
	//Set the RPO to be null
	if(function_entry_block->reverse_post_order.internal_array != NULL){
		dynamic_array_dealloc(&(function_entry_block->reverse_post_order));
	}

	//Set the RPO reverse CFG to be null
	if(function_entry_block->reverse_post_order_reverse_cfg.internal_array != NULL){
		dynamic_array_dealloc(&(function_entry_block->reverse_post_order_reverse_cfg));
	}

	//Reset the function visited status
	reset_visit_status_for_function(function_blocks);

	//Compute the reverse post order traversal
	function_entry_block->reverse_post_order = compute_post_order_traversal(function_entry_block);

	//Reset the function visited status
	reset_visit_status_for_function(function_blocks);

	//Now use the reverse CFG(successors are predecessors, and vice versa)
	function_entry_block->reverse_post_order_reverse_cfg = compute_reverse_post_order_traversal_reverse_cfg(function_entry_block);
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
 * For every block in the given function
 */
void calculate_all_control_relations(basic_block_t* function_entry_block, dynamic_array_t* function_blocks){
	//Calculate all reverse traversals
	calculate_all_reverse_traversals(function_entry_block, function_blocks);
	
	//We first need to calculate the dominator sets of every single node
	calculate_dominator_sets(function_entry_block, function_blocks);

	//Now we'll build the dominator tree up
	build_dominator_trees(function_blocks);

	//Now calculate the dominance frontier for every single block
	calculate_dominance_frontiers(function_blocks);

	//Calculate the postdominator sets for later analysis in the optimizer
	calculate_postdominator_sets(function_entry_block, function_blocks);

	//We'll also now calculate the reverse dominance frontier that will be used
	//in later analysis by the optimizer
	calculate_reverse_dominance_frontiers(function_blocks);
}


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
	for(u_int32_t i = 0; i < global_variables->current_index; i++){
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
	traversal_queue = heap_queue_alloc();

	//Keep these on hand
	f64 = lookup_type_name_only(type_symtab, "f64", NOT_MUTABLE)->type;
	f32 = lookup_type_name_only(type_symtab, "f32", NOT_MUTABLE)->type;
	u64 = lookup_type_name_only(type_symtab, "u64", NOT_MUTABLE)->type;
	i64 = lookup_type_name_only(type_symtab, "i64", NOT_MUTABLE)->type;
	u32 = lookup_type_name_only(type_symtab, "u32", NOT_MUTABLE)->type;
	i32 = lookup_type_name_only(type_symtab, "i32", NOT_MUTABLE)->type;
	u16 = lookup_type_name_only(type_symtab, "u16", NOT_MUTABLE)->type;
	i16 = lookup_type_name_only(type_symtab, "i16", NOT_MUTABLE)->type;
	u8 = lookup_type_name_only(type_symtab, "u8", NOT_MUTABLE)->type;
	i8 = lookup_type_name_only(type_symtab, "i8", NOT_MUTABLE)->type;
	immut_void_ptr = lookup_type_name_only(type_symtab, "void*", NOT_MUTABLE)->type;
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
	//Store the stack pointer
	cfg->stack_pointer = stack_pointer_var;

	//Store it in the global context as well
	stack_pointer_variable = stack_pointer_var;

	//Create the instruction pointer
	symtab_variable_record_t* instruction_pointer = initialize_instruction_pointer(results->type_symtab);
	//Initialize a three addr code var
	instruction_pointer_var = emit_var(instruction_pointer);
	//Store it in the CFG
	cfg->instruction_pointer = instruction_pointer_var;

	// -1 block ID, this means that the whole thing failed
	if(visit_prog_node(cfg, results->root) == FALSE){
		print_parse_message(MESSAGE_TYPE_ERROR, "CFG was unable to be constructed", 0);
		(*num_errors_ref)++;
	}

	/**
	 * Correct any static variable name collisions that we may run into
	 */
	mangle_static_variable_names(&(cfg->global_variables));

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
