/**
 * A header file that references everything we should need for the control-flow-graph
*/

#ifndef CFG_H
#define CFG_H
#include <sys/types.h>
#include "../ast/ast.h"
#include "../parser/parser.h"
#include "../stack/heapstack.h"
#include "../three_addr_code/three_address_code.h"
#include "../dynamic_array/dynamic_array.h"
#include "../jump_table/jump_table.h"

#define INITIAL_STATEMENT_SIZE 2000

//The overall structure holder
typedef struct cfg_t cfg_t;
//Basic blocks in our CFG
typedef struct basic_block_t basic_block_t;
//A memory tracking structure for freeing
typedef struct cfg_node_holder_t cfg_node_holder_t;
//A memory tracking structure for freeing
typedef struct cfg_statement_holder_t cfg_statement_holder_t;


/**
 * A general block type that is used for readability. It is
 * important to note that block type is not always used. Instead,
 * it is used to mark important blocks like break statements, return statements,
 * etc, that would have an impact on control flow
 */
typedef enum {
	BLOCK_TERM_TYPE_NORMAL, //THe block ends normally, no indirection of any kind
	BLOCK_TERM_TYPE_BREAK, //Ends in a break statement
	BLOCK_TERM_TYPE_CONTINUE, //Ends in a continue statement
	BLOCK_TERM_TYPE_RET, //The block ends in a return statement
} block_terminal_type_t;

/**
 * What kind of jump do we want to select
 */
typedef enum{
	JUMP_CATEGORY_INVERSE,
	JUMP_CATEGORY_NORMAL,
} jump_category_t;

/**
 * What is the general type of the block. Again most
 * blocks are normal, but there are exceptions
 */
typedef enum{
	BLOCK_TYPE_NORMAL, //Normal block
	BLOCK_TYPE_SWITCH, //The whole block is a switch statement
	BLOCK_TYPE_ASM, //Very special case -- entire block is dedicated to asm inline
	BLOCK_TYPE_CASE, //Case statement -- it also encapsulates default(just a special kind of case)
	BLOCK_TYPE_FUNC_ENTRY, //Block is a function entry
	BLOCK_TYPE_FUNC_EXIT, //Block is a function exit
	BLOCK_TYPE_DO_WHILE_END, //End of a do-while
	BLOCK_TYPE_IF_STMT_END, //End of an if-statement
	BLOCK_TYPE_WHILE_END, //End of a while statement
	BLOCK_TYPE_FOR_STMT_END, //End of a for statement
	BLOCK_TYPE_FOR_STMT_UPDATE, //Update block of a for statement
} block_type_t;



/**
 * We have a basic CFG structure that holds these references to making freeing
 */
struct cfg_t{
	//The global variable block. This is where
	//anything that is not inside of a function is put
	basic_block_t* global_variables;
	//This dynamic array contains all of the function
	//entry blocks for each function that we have
	dynamic_array_t* function_blocks;
	//An array of all blocks that are 
	//All created blocks
	dynamic_array_t* created_blocks;
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
	//An integer ID
	int32_t block_id;
	//Does this block contain a marked record?
	u_int8_t contains_mark;
	//The function record -- we need to store this for printing
	symtab_function_record_t* func_record;
	//The function that we're defined in
	symtab_function_record_t* function_defined_in;
	//Is this a global variable block?
	u_int8_t is_global_var_block;
	//How many jump statements does the block have? This helps us avoid
	//redundant computation
	u_int16_t num_jumps;
	//What is the general classification of this block
	block_type_t block_type;
	//How does the block terminate? This is important for CFG drilling
	block_terminal_type_t block_terminal_type;
	//Was this block visited by traverser?
	u_int8_t visited;
	//Does this block have short-circuiting eligibility?
	u_int8_t is_short_circuit_eligible;
	//Predecessor nodes
	dynamic_array_t* predecessors;
	//Successor nodes
	dynamic_array_t* successors;
	//For convenience here. This is the successor that we use to
	//"drill" to the bottom
	basic_block_t* direct_successor;
	//The array of used variables
	dynamic_array_t* used_variables;
	//The array of all assigned variables
	dynamic_array_t* assigned_variables;
	//The blocks dominance frontier
	dynamic_array_t* dominance_frontier;
	//The reverse dominance frontier(for analysis)
	dynamic_array_t* reverse_dominance_frontier;
	//The reverse post order set
	dynamic_array_t* reverse_post_order;
	//The reverse post order set on the reverse cfg
	dynamic_array_t* reverse_post_order_reverse_cfg;
	//The dynamic array for the dominator set
	dynamic_array_t* dominator_set;
	//The dynamic array for the postdominator set. The postdominator
	//set is the equivalent of the dominator set on the reverse CFG
	dynamic_array_t* postdominator_set;
	//The dominator children of a basic block. These are all
	//of the blocks that this block directly dominates
	dynamic_array_t* dominator_children;
	//The "LIVE_IN" variables for this node
	dynamic_array_t* live_in;
	//The "LIVE_OUT" variables for this node
	dynamic_array_t* live_out;
	//The immediate dominator - this reference isn't always used, but if we go through the work
	//of calculating it, we may as well store it
	basic_block_t* immediate_dominator;
	//The immediate postdominator reference
	basic_block_t* immediate_postdominator;
	//The reference to a jump table. This is often not used at all
	jump_table_t jump_table;
	//The case statement value -- usually blank
	int64_t case_stmt_val;
	//There are consecutive statements(declare, define, let, assign, alias)
	//in a node. These statements are a linked list
	//Keep a reference to the "leader"(head) and "exit"(tail) statements
	three_addr_code_stmt_t* leader_statement;
	three_addr_code_stmt_t* exit_statement;
};

//Build the entire CFG from the AST. This function returns the CFG struct, which
//always has the root block
cfg_t* build_cfg(front_end_results_package_t results, u_int32_t* num_errors, u_int32_t* num_warnings);

//Add a statement to the basic block
void add_statement(basic_block_t* target, three_addr_code_stmt_t* statement_node);

/**
 * Delete a statement from the CFG - handling any/all edge cases that may arise
 */
void delete_statement(cfg_t* cfg, basic_block_t* block, three_addr_code_stmt_t* stmt);

//Add a successor to the block
void add_successor(basic_block_t* target, basic_block_t* successor);

/**
 * Select the appropriate jump type given the circumstances
 */
jump_type_t select_appropriate_jump_stmt(Token op, jump_category_t jump_type);

//Exclusively add a predecessor to a block
void add_predecessor_only(basic_block_t* target, basic_block_t* predecessor);

//Exclusively add a successor to a block
 void add_successor_only(basic_block_t* target, basic_block_t* successor);

//Deallocate our entire cfg structure
void dealloc_cfg(cfg_t* cfg);

/**
 * Destroy all old control relations in anticipation of new ones coming in
 */
void cleanup_all_control_relations(cfg_t* cfg);

/**
 * Calculate(or recalculate) all control relations in the CFG
 */
void calculate_all_control_relations(cfg_t* cfg, u_int8_t build_fresh);

/**
 * Emit a jump statement directly into a block
 */
void emit_jmp_stmt(basic_block_t* basic_block, basic_block_t* dest_block, jump_type_t type, u_int8_t is_branch_ending);

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
 * Deallocate a block
 */
void basic_block_dealloc(basic_block_t* block);

/**
 * Compute the postorder traversal for a function-level cfg
 */
dynamic_array_t* compute_post_order_traversal(basic_block_t* entry);

#endif /* CFG_H */
