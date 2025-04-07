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
	//The current number of blocks
	u_int32_t num_blocks;
	//The global variable block. This is where
	//anything that is not inside of a function is put
	basic_block_t* global_variables;
	//This dynamic array contains all of the function
	//entry blocks for each function that we have
	dynamic_array_t* function_blocks;
	//An array of all blocks that are 
	//All created blocks
	dynamic_array_t* created_blocks;
	//The structure that contains all temporary variables
	variable_symtab_t* temp_vars;
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
	//Does this block ever contain an assignment?
	u_int8_t contains_assignment;
	//The function record -- we need to store this for printing
	symtab_function_record_t* func_record;
	//Is this a global variable block?
	u_int8_t is_global_var_block;
	//What is the general classification of this block
	block_type_t block_type;
	//How does the block terminate? This is important for CFG drilling
	block_terminal_type_t block_terminal_type;
	//Was this block visited by traverser?
	u_int8_t visited;
	//Predecessor nodes
	dynamic_array_t* predecessors;
	//Successor nodes
	dynamic_array_t* successors;
	//For convenience here. This is the successor that we use to
	//"drill" to the bottom
	basic_block_t* direct_successor;
	//If we have a case block, what does it break to?
	basic_block_t* case_block_breaks_to;
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

//Deallocate our entire cfg structure
void dealloc_cfg(cfg_t* cfg);

/**
 * For DEBUGGING purposes - we will print all of the blocks in the control
 * flow graph. This is meant to be invoked by the programmer, and as such is exposed
 * via the header file
 */
void print_all_cfg_blocks(cfg_t* cfg);

/**
 * Reset the visited status of the CFG
 */
void reset_visited_status(cfg_t* cfg);

#endif /* CFG_H */
