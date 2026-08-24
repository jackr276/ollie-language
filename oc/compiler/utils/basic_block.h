/**
 * Author: Jack Robbins
 * This header file defines all that is needed for the Ollie basic block. The basic
 * block is the fundamental building block of all Ollie functions
 *
 * This file contains the basic block struct itself as well as some related enumerations
 */

#ifndef BASIC_BLOCK_H
#define BASIC_BLOCK_H

#include "./dynamic_array/dynamic_array.h"
#include "../jump_table/jump_table.h"
#include "ollie_instruction.h"

//Basic blocks in our CFG
typedef struct basic_block_t basic_block_t;

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
 * Define: a basic block is a sequence of consecutive 
 * intermediate language statements in which flow of 
 * control can only enter at the beginning and leave at the end 
 *
 * A basic block has ONE entrance and ONE exit. These points are referenced 
 * by the "leader" and "exit" references for quick access
*/
struct basic_block_t{
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

	/**
	 * When we inline functions(among other things), we may need to maintain
	 * a 1-to-1 relationship where a block maps to another block. This is wrapped
	 * in "mapping info" for clarity
	 */
	struct {
		basic_block_t* maps_to;
	} mapping_info;

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

#endif /* BASIC_BLOCK_H */
