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


//Basic blocks in our CFG
typedef struct basic_block_t basic_block_t;

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
	//Keep a reference to the "leader" and "exit" statements
	generic_ast_node_t* leader_statement;
	generic_ast_node_t* exit_statement;
	//There are consecutive statements(declare, define, let, assign, alias)
	//in a node. These statements are a linked list
	generic_ast_node_t* statements;
};

//Allocate a basic block and return a reference to it
basic_block_t* basic_block_alloc();

//Add a predecessor to the target block
void add_predecessor(basic_block_t* target, basic_block_t* predecessor);

//Add a successor to the target bloc
void add_successor(basic_block_t* target, basic_block_t* successor);

//Deallocate a basic block
void basic_block_dealloc(basic_block_t* block);

#endif
