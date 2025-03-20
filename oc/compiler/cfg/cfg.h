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
//This can always be reupped dynamically
#define MAX_LIVE_VARS 5
#define MAX_ASSIGNED_VARS 5

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
	BLOCK_TYPE_DO_WHILE_END, //End of a do-while
	BLOCK_TYPE_IF_STMT_END, //End of an if-statement
	BLOCK_TYPE_WHILE_END, //End of a while statement
	BLOCK_TYPE_FOR_STMT_END, //End of a for statement
} block_type_t;



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
	//Is this block ok to merge?
	u_int8_t good_to_merge;
	//What is the general classification of this block
	block_type_t block_type;
	//How does the block terminate? This is important for CFG drilling
	block_terminal_type_t block_terminal_type;
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
	//If we have a case block, what does it break to?
	basic_block_t* case_block_breaks_to;
	//Hold onto the number of both that we have
	u_int8_t num_predecessors;
	u_int8_t num_successors;
	//Keep track of all active(live) variables
	three_addr_var_t** live_variables;
	//The number of active vars
	u_int16_t live_variable_count;
	//The current maximum number of live variables
	u_int16_t max_live_variable_count;
	//Keep track of all variables that are assigned to
	three_addr_var_t** assigned_variables;
	//The number of assigned vars
	u_int16_t assigned_variable_count;
	//The current maximum number of assigned variables
	u_int16_t max_assigned_variable_count;
	//The case statement value -- usually blank
	int64_t case_stmt_val;
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
