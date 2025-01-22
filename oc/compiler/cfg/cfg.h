/**
 * A header file that references everything we should need for the control-flow-graph
*/

#ifndef CFG_H
#define CFG_H
#include <sys/types.h>
#include "../ast/ast.h"

//Basic blocks in our CFG
typedef struct basic_block_t basic_block_t;

/**
 * Define: a basic block is a sequence of consecutive 
 * intermediate language statements in which flow of 
 * control can only enter at the beginning and leave at the end 
 *
 * Only the last statement of a basic block can be a branch and 
 * the first statement of a basic block can be a target
*/
struct basic_block_t{
	//A basic block is a doubly-linked list node
	//with a predecessor and a successor
	//Edges represent where we can go on this graph
	basic_block_t* predecessors[50];
	basic_block_t* successors[50];
	//Hold onto the number of both that we have
	u_int8_t num_predecessors;
	u_int8_t num_successors;
	//What is the "leader" line
	u_int16_t leader;
	//The reference to the AST expression that we have in
	//the block
	generic_ast_node_t* expression_level_ast;





};


#endif
