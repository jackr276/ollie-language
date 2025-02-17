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

//These may or may not change
#define MAX_SUCCESSORS 40
#define MAX_PREDECESSORS 40
#define INITIAL_STATEMENT_SIZE 2000
//Probably way too high, but just being safe
#define MAX_LIVE_VARS 300

//The overall structure holder
typedef struct cfg_t cfg_t;
//Basic blocks in our CFG
typedef struct basic_block_t basic_block_t;
//A memory tracking structure for freeing
typedef struct cfg_node_holder_t cfg_node_holder_t;
//A memory tracking structure for freeing
typedef struct cfg_statement_holder_t cfg_statement_holder_t;


/**
 * We have a basic CFG structure that holds these references to making freeing
 */
struct cfg_t{
	//The current number of blocks
	u_int32_t num_blocks;
	//The overall root node.
	basic_block_t* root;
	//The current block of the CFG
	basic_block_t* current;
	//The currently last attached block
	basic_block_t* last_attached;
};


/**
 * We will maintain a linked list of CFG nodes that we can eventually use for
 * freeing
 */
struct cfg_node_holder_t{
	//Hold onto the basic block
	basic_block_t* block;
	//Linked list property
	cfg_node_holder_t* next;
};

/**
 * We will maintain a linked list of statements that we can eventually use for
 * freeing
 */
struct cfg_statement_holder_t{
	//Hold onto the basic block
	basic_block_t* block;
	//Linked list property
	cfg_statement_holder_t* next;
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
	//Is this a function entry block?
	u_int8_t is_func_entry;
	//Does this block ever contain an assignment?
	u_int8_t contains_assignment;
	//The function record -- we need to store this for printing
	symtab_function_record_t* func_record;
	//Is this block ok to merge?
	u_int8_t good_to_merge;
	//Is it a return statement
	u_int8_t is_return_stmt;
	//Is it a continue statement?
	u_int8_t is_cont_stmt;
	//Is it a break statement
	u_int8_t is_break_stmt;
	//Was this block visited by traverser?
	u_int8_t visited;
	//Is this block an exit block?
	u_int8_t is_exit_block;
	//A basic block is a doubly-linked list node
	//with a predecessor and a successor
	//Edges represent where we can go on this graph
	basic_block_t* predecessors[MAX_PREDECESSORS];
	basic_block_t* successors[MAX_SUCCESSORS];
	//For convenience here. This is the successor that we use to
	//"drill" to the bottom
	basic_block_t* direct_successor;
	//Hold onto the number of both that we have
	u_int8_t num_predecessors;
	u_int8_t num_successors;
	//Keep track of all active(live) variables
	three_addr_var_t* active_vars[MAX_LIVE_VARS];
	//The number of active vars
	u_int16_t active_var_count;
	//There are consecutive statements(declare, define, let, assign, alias)
	//in a node. These statements are a linked list
	//Keep a reference to the "leader"(head) and "exit"(tail) statements
	three_addr_code_stmt_t* leader_statement;
	three_addr_code_stmt_t* exit_statement;
	//The next created block
	basic_block_t* next_created;
};

//Build the entire CFG from the AST. This function returns the CFG struct, which
//always has the root block
cfg_t* build_cfg(front_end_results_package_t results, u_int32_t* num_errors, u_int32_t* num_warnings);

//Deallocate our entire cfg structure
void dealloc_cfg(cfg_t* cfg);

#endif /* CFG_H */
