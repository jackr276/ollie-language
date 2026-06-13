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

//Are we emitting the dominance frontier or not?
typedef enum{
	EMIT_DOMINANCE_FRONTIER,
	DO_NOT_EMIT_DOMINANCE_FRONTIER
} emit_dominance_frontier_selection_t;

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
	//The "LIVE_IN" variables for this node
	dynamic_array_t live_in;
	//The "LIVE_OUT" variables for this node
	dynamic_array_t live_out;
	//The set of "used_before_definition" defines all variables that were used
	//before they were assigned in the block
	dynamic_array_t used_before_definition;
	//The array of all assigned variables
	dynamic_array_t assigned_variables;
	//For convenience here. This is the successor that we use to
	//"drill" to the bottom
	basic_block_t* direct_successor;
	//The blocks dominance frontier
	dynamic_array_t dominance_frontier;
	//The reverse dominance frontier(for analysis)
	dynamic_array_t reverse_dominance_frontier;
	//The dominator children of this block
	dynamic_array_t dominator_children;
	/**
	 * Dominator information that each and every block will own. This 
	 * information is needed whenever we compute the immediate dominator
	 * and/or postdominator
	 */
	struct {
		//Block's immediate dominator
		basic_block_t* immediate_dominator;
		//Block's immediate postdominator
		basic_block_t* immediate_postdominator;
		/**
		 * The DFS number of this block after it's been DFS(or reverse DFS)
		 * numbered
		 */
		int32_t dfs_number;
		/**
		 * The DFS number of this block's semidominator
		 */
		int32_t semidominator_number;
		/**
		 * The parent of this block(NOT the ancestor)
		 */
		basic_block_t* parent;
		/**
		 * The union-find ancestor of this
		 * block(NOT the parent)
		 */
		basic_block_t* ancestor;
		/**
		 * The node with the smallest semidominator
		 * number along the currently known path - we 
		 * cache this to avoid recomputation
		 */
		basic_block_t* optimal_candidate;
		/**
		 * The worklist is a deferred work queue that we use. It will
		 * store the list of all nodes that are semidominated by this
		 * given node
		 */
		dynamic_array_t worklist;
	} dominator_info;

	//The reference to a jump table. This is often not used at all
	jump_table_t* jump_table;
	//The case statement value -- usually blank
	int64_t case_stmt_val;
	//The function that we're defined in
	symtab_function_record_t* function_defined_in;
	//An integer ID
	int32_t block_id;
	//The number of instructions that the given block has
	u_int32_t number_of_instructions;
	//The estimated execution frequency. This will change if a block is in a loop, etc.
	u_int32_t estimated_execution_frequency;
	//What is the general classification of this block
	block_type_t block_type;
	//Does this block contain a marked record?
	u_int8_t contains_mark;
	//Was this block visited by traverser?
	u_int8_t visited;
	//Does this block already have a phi function for the given variable?
	u_int8_t already_has_phi_func;
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

#endif /* CFG_H */
