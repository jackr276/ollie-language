/**
 * Author: Jack Robbins
 *
 * A header file that references everything we should need for the control-flow-graph
*/

#ifndef CFG_H
#define CFG_H
#include <sys/types.h>
#include "../ast/ast.h"
#include "../parser/parser.h"
#include "../utils/stack/heapstack.h"
#include "../instruction/instruction.h"
#include "../utils/dynamic_array/dynamic_array.h"
#include "../jump_table/jump_table.h"

//The overall structure holder
typedef struct cfg_t cfg_t;
//Basic blocks in our CFG
typedef struct basic_block_t basic_block_t;
//A memory tracking structure for freeing
typedef struct cfg_node_holder_t cfg_node_holder_t;
//A memory tracking structure for freeing
typedef struct cfg_statement_holder_t cfg_statement_holder_t;


/**
 * What is the general type of the block. Again most
 * blocks are normal, but there are exceptions
 */
typedef enum{
	BLOCK_TYPE_NORMAL, //Normal block
	BLOCK_TYPE_SWITCH, //The whole block is a switch statement
	BLOCK_TYPE_CASE, //Case statement -- it also encapsulates default(just a special kind of case)
	BLOCK_TYPE_FUNC_ENTRY, //Block is a function entry
	BLOCK_TYPE_FUNC_EXIT, //Block is a function exit
	BLOCK_TYPE_IF_ENTRY, //If statement entry
	BLOCK_TYPE_IF_EXIT, //End of an if-statement
	BLOCK_TYPE_LOOP_ENTRY, //Loop entry block
	BLOCK_TYPE_LOOP_EXIT, //Loop exit block
	BLOCK_TYPE_LABEL, //This block comes from a user-defined label
} block_type_t;


/**
 * We have a basic CFG structure that holds these references to making freeing
 */
struct cfg_t{
	//This dynamic array contains all of the function
	//entry blocks for each function that we have
	dynamic_array_t function_entry_blocks;
	//Store the exit blocks as well. This makes RPO traversal much easier
	dynamic_array_t function_exit_blocks;
	//An array of all blocks that are 
	//All created blocks
	dynamic_array_t created_blocks;
	//We also need to hold onto the stack pointer
	three_addr_var_t* stack_pointer;
	//We also need to hold onto the instruction pointer
	three_addr_var_t* instruction_pointer;
	//We'll want the type symtab too
	type_symtab_t* type_symtab;
	//All global variables
	dynamic_array_t global_variables;
	//Hang onto the block id
	u_int32_t block_id;
};


/**
 * Define: a basic block is a sequence of consecutive 
 * intermediate language statements in which flow of 
 * control can only enter at the beginning and leave at the end 
 *
 * A basic block has ONE entrance and ONE exit. These points are referenced 
 * by the "leader" and "exit" references for quick access
*/
struct basic_block_t{
	//There are consecutive statements(declare, define, let, assign, alias)
	//in a node. These statements are a linked list
	//Keep a reference to the "leader"(head) and "exit"(tail) statements
	instruction_t* leader_statement;
	instruction_t* exit_statement;
	//Predecessor nodes
	dynamic_array_t predecessors;
	//Successor nodes
	dynamic_array_t successors;
	//For convenience here. This is the successor that we use to
	//"drill" to the bottom
	basic_block_t* direct_successor;
	//The array of used variables
	dynamic_array_t used_variables;
	//The array of all assigned variables
	dynamic_array_t assigned_variables;
	//The blocks dominance frontier
	dynamic_array_t dominance_frontier;
	//The reverse dominance frontier(for analysis)
	dynamic_array_t reverse_dominance_frontier;
	//The reverse post order set
	dynamic_array_t reverse_post_order;
	//The reverse post order set on the reverse cfg
	dynamic_array_t reverse_post_order_reverse_cfg;
	//The dynamic array for the dominator set
	dynamic_array_t dominator_set;
	//The dynamic array for the postdominator set. The postdominator
	//set is the equivalent of the dominator set on the reverse CFG
	dynamic_array_t postdominator_set;
	//The dominator children of a basic block. These are all
	//of the blocks that this block directly dominates
	dynamic_array_t dominator_children;
	//The "LIVE_IN" variables for this node
	dynamic_array_t live_in;
	//The "LIVE_OUT" variables for this node
	dynamic_array_t live_out;
	//The immediate dominator - this reference isn't always used, but if we go through the work
	//of calculating it, we may as well store it
	basic_block_t* immediate_dominator;
	//The immediate postdominator reference
	basic_block_t* immediate_postdominator;
	//The reference to a jump table. This is often not used at all
	jump_table_t* jump_table;
	//The function that we're defined in
	symtab_function_record_t* function_defined_in;
	//The variable that this block's name draws from if this block is a label block
	symtab_variable_record_t* label;
	//The case statement value -- usually blank
	int64_t case_stmt_val;
	//An integer ID
	int32_t block_id;
	//The number of instructions that the given block has
	u_int32_t number_of_instructions;
	//The estimated execution frequency. This will change
	//if a block is in a loop, etc.
	u_int32_t estimated_execution_frequency;
	//What is the general classification of this block
	block_type_t block_type;
	//Does this block contain a marked record?
	u_int8_t contains_mark;
	//Was this block visited by traverser?
	u_int8_t visited;
};


/**
 * Reset the used, live in, live out, and assigned arrays in a block
 */
void reset_block_variable_tracking(basic_block_t* block);

/**
 * Build the entire CFG from the AST. This function returns the CFG struct, which
 * always has the root block
 */
cfg_t* build_cfg(front_end_results_package_t* results, u_int32_t* num_errors, u_int32_t* num_warnings);

/**
 * A simple helper function that allows us to add an assigned-to variable into the block's
 * header. It is important to note that only actual variables(not temp variables) count
 * as live
 */
void add_assigned_variable(basic_block_t* basic_block, three_addr_var_t* var);

/**
 * A simple helper function that allows us to add a used variable into the block's
 * header. It is important to note that only actual variables(not temp variables) count
 * as live
 */
void add_used_variable(basic_block_t* basic_block, three_addr_var_t* var);

/**
 * Add a statement to the basic block
 */
void add_statement(basic_block_t* target, instruction_t* statement_node);

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
 * Destroy all old control relations in anticipation of new ones coming in. This
 * operates on a per-function level
 */
void cleanup_all_control_relations(dynamic_array_t* function_blocks);

/**
 * Calcualte/recalculate all control relations for a given function
 */
void calculate_all_control_relations(basic_block_t* function_entry_block, dynamic_array_t* function_blocks);

/**
 * Emit a jump statement directly into a block
 */
instruction_t* emit_jump(basic_block_t* basic_block, basic_block_t* dest_block);

/**
 * Emit a branch statement directly into a block
 */
void emit_branch(basic_block_t* basic_block, basic_block_t* if_destination, basic_block_t* else_destination, branch_type_t branch_type, three_addr_var_t* conditional_result, branch_category_t branch_category, u_int8_t relies_on_fp_comparison);

/**
 * For DEBUGGING purposes - we will print all of the blocks in the control
 * flow graph. This is meant to be invoked by the programmer, and as such is exposed
 * via the header file
 */
void print_all_cfg_blocks(cfg_t* cfg);

/**
 * Reset the visited status of the CFG
 */
void reset_visited_status(cfg_t* cfg, u_int8_t reset_direct_successor);

/**
 * Reset the visited status inside a particular function in the CFG
 */
void reset_function_visited_status(basic_block_t* function_entry_block, u_int8_t reset_direct_successor);

/**
 * Deallocate a block
 */
void basic_block_dealloc(basic_block_t* block);

/**
 * Compute the postorder traversal for a function-level cfg
 */
dynamic_array_t compute_post_order_traversal(basic_block_t* entry);

/**
 * Get and return a reverse post order traversal for the CFG
 */
dynamic_array_t compute_reverse_post_order_traversal(basic_block_t* entry);

#endif /* CFG_H */
