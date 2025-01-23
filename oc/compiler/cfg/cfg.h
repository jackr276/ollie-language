/**
 * A header file that references everything we should need for the control-flow-graph
*/

#ifndef CFG_H
#define CFG_H
#include <sys/types.h>
#include "../ast/ast.h"

//These may or may not change
#define MAX_SUCCESSORS 50
#define MAX_PREDECESSORS 50
#define MAXIMUM_BLOCKS 5000

//The overall structure holder
typedef struct cfg_t cfg_t;
//Basic blocks in our CFG
typedef struct basic_block_t basic_block_t;
typedef struct top_level_statement_node_t top_level_statement_node_t;


/**
 * What is the directionality of our added node
 */
typedef enum linked_direction_t {
	LINKED_DIRECTION_BIDIRECTIONAL,
	LINKED_DIRECTION_UNIDIRECTIONAL
} linked_direction_t;

/**
 * We have a basic CFG structure that holds these references to making freeing
 */
struct cfg_t{
	//An array of every single block we have
	basic_block_t* blocks[MAXIMUM_BLOCKS];
	//The current number of blocks
	u_int32_t num_blocks;
	//The overall root node
	basic_block_t* root;
};


struct top_level_statement_node_t{
	top_level_statement_node_t* next;
	generic_ast_node_t* node;
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
	//The block ID: atomically increasing unsigned integer
	u_int16_t block_id;
	//A basic block is a doubly-linked list node
	//with a predecessor and a successor
	//Edges represent where we can go on this graph
	basic_block_t* predecessors[MAX_PREDECESSORS];
	basic_block_t* successors[MAX_SUCCESSORS];
	//Hold onto the number of both that we have
	u_int8_t num_predecessors;
	u_int8_t num_successors;
	//There are consecutive statements(declare, define, let, assign, alias)
	//in a node. These statements are a linked list
	//Keep a reference to the "leader"(head) and "exit"(tail) statements
	top_level_statement_node_t* leader_statement;
	top_level_statement_node_t* exit_statement;
};

//Allocate our global cfg structure
cfg_t* create_cfg();

//Allocate a basic block and return a reference to it
basic_block_t* basic_block_alloc(cfg_t* cfg);

//Give the ability to merge basic blocks
basic_block_t* merge_blocks(basic_block_t* a, basic_block_t* b);

//Add a predecessor to the target block
void add_predecessor(basic_block_t* target, basic_block_t* predecessor, linked_direction_t directedness);

//Add a successor to the target block
void add_successor(basic_block_t* target, basic_block_t* successor, linked_direction_t directedness);

//Add a statement AST node refence to the basic block
void add_statement(basic_block_t* target, top_level_statement_node_t* statement_node);

//Deallocate a basic block
void basic_block_dealloc(basic_block_t* block);

//Deallocate our entire cfg structure
void dealloc_cfg(cfg_t* cfg);

#endif
