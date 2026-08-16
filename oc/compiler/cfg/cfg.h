/**
 * Author: Jack Robbins
 *
 * A header file that references everything we should need for the control-flow-graph
*/

#ifndef CFG_H
#define CFG_H
#include <sys/types.h>
#include "../utils/basic_block.h"
#include "../ast/ast.h"
#include "../parser/parser.h"
#include "../utils/stack/heapstack.h"
#include "../instruction/instruction.h"
#include "../utils/dynamic_array/dynamic_array.h"
#include "../jump_table/jump_table.h"

//The overall structure holder
typedef struct cfg_t cfg_t;
//A memory tracking structure for freeing
typedef struct cfg_node_holder_t cfg_node_holder_t;
//A memory tracking structure for freeing
typedef struct cfg_statement_holder_t cfg_statement_holder_t;

//Are we emitting the dominance frontier or not?
typedef enum{
	EMIT_DOMINANCE_FRONTIER,
	DO_NOT_EMIT_DOMINANCE_FRONTIER
} emit_dominance_frontier_selection_t;

/**
 * What result do we have from this CFG? This will
 * determine if/how callers continue to process
 */
typedef enum {
	CFG_RESULT_FAILURE,
	CFG_RESULT_WARN,
	CFG_RESULT_SUCCESS,
} cfg_construction_result_type_t;


/**
 * We have a basic CFG structure that holds these references to making freeing
 */
struct cfg_t{
	//This dynamic array contains all of the function
	//entry blocks for each function that we have
	dynamic_array_t function_entry_blocks;
	//Store the exit blocks as well. This makes RPO traversal much easier
	dynamic_array_t function_exit_blocks;
	//We also need to hold onto the stack pointer
	three_addr_var_t* stack_pointer;
	//We also need to hold onto the instruction pointer
	three_addr_var_t* instruction_pointer;
	//=====================================
	// All local constnats that we could possibly
	// use are stored inside of global arrays
	// here. This is done to make access as well
	// as any needed cleanup easier
	dynamic_array_t local_string_constants;
	dynamic_array_t local_f32_constants;
	dynamic_array_t local_f64_constants;
	dynamic_array_t local_xmm128_constants;
	//=====================================
	//We'll want the type symtab too
	type_symtab_t* type_symtab;
	//All global variables
	dynamic_array_t global_variables;
	//Hang onto the block id
	u_int32_t block_id;
	//Result of the construction
	cfg_construction_result_type_t result;
};


/**
 * Build the entire CFG from the AST. This function returns the CFG struct, which
 * always has the root block
 */
cfg_t* build_cfg(front_end_results_package_t* results, u_int32_t* num_errors, u_int32_t* num_warnings);

/**
 * Add a statement to the basic block
 */
void add_statement(basic_block_t* target, instruction_t* statement_node);

/**
 * Take a statement and move it from its current blcok over to the provided
 * destination block. This will not update the use/assignment counts like
 * a regular remove still but it will still operate in much the same way.
 * The statement will always be added directly at the end of the block
 */
void move_statement(instruction_t* target, basic_block_t* destination);

/**
 * Delete a statement from the CFG - handling any/all edge cases that may arise
 */
void delete_statement(instruction_t* stmt);

/**
 * Delete a successor from a block
 */
void delete_successor_only(basic_block_t* target, basic_block_t* successor);

/**
 * Delete a predecessor from a block
 */
void delete_predecessor_only(basic_block_t* target, basic_block_t* predecessor);

/**
 * Delete a successor from a block
 */
void delete_successor(basic_block_t* target, basic_block_t* deleted_successor);

/**
 * Add a successor to the block
 */
void add_successor(basic_block_t* target, basic_block_t* successor);

/**
 * Add a predecessor to the block
 */
void add_predecessor_only(basic_block_t* target, basic_block_t* predecessor);

/**
 * Exclusively add a successor to the block
 */
 void add_successor_only(basic_block_t* target, basic_block_t* successor);

/**
 * Deallocate the entire CFG
 */
void dealloc_cfg(cfg_t* cfg);

/**
 * Emit a jump statement directly into a block
 */
instruction_t* emit_jump(basic_block_t* basic_block, basic_block_t* dest_block);

/**
 * For DEBUGGING purposes - we will print all of the blocks in the control
 * flow graph. This is meant to be invoked by the programmer, and as such is exposed
 * via the header file
 */
void print_all_cfg_blocks(cfg_t* cfg);

/**
 * Print a block our for reading
*/
void print_block_three_addr_code(basic_block_t* block, emit_dominance_frontier_selection_t print_df);

/**
 * Deallocate a block
 */
void basic_block_dealloc(basic_block_t* block);

#endif /* CFG_H */
