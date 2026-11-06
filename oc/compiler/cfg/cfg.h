/**
 * Author: Jack Robbins
 *
 * A header file that references everything we should need for the control-flow-graph
*/

#ifndef CFG_H
#define CFG_H
#include <sys/types.h>
#include "../utils/basic_block/basic_block.h"
#include "../ast/ast.h"
#include "../parser/parser.h"
#include "../utils/stack/heapstack.h"
#include "../instruction/instruction.h"
#include "../utils/dynamic_array/dynamic_array.h"

//The overall structure holder
typedef struct cfg_t cfg_t;

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
 * A helper function that makes a new block id. This ensures we have an atomically
 * increasing block ID
 */
static inline int32_t get_next_block_id(cfg_t* cfg){
	(cfg->block_id)++;
	return cfg->block_id;
}

/**
 * Build the entire CFG from the AST. This function returns the CFG struct, which
 * always has the root block
 */
cfg_t* build_cfg(front_end_results_package_t* results, u_int32_t* num_errors, u_int32_t* num_warnings);

/**
 * Delete a statement from the CFG - handling any/all edge cases that may arise
 */
void delete_statement(instruction_t* stmt);

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
 * Deallocate a block
 */
void basic_block_dealloc(basic_block_t* block);

#endif /* CFG_H */
