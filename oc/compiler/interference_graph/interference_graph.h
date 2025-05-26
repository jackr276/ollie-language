/**
 * Author: Jack Robbins
 *
 * This header file contains the APIs and data structures for the register allocator's interference graph
*/

//Header guards
#ifndef INTERFERENCE_GRAPH_H
#define INTERFERENCE_GRAPH_H
#include <sys/types.h>
//Link to instruction
#include "../instruction/instruction.h"

//Predeclare the graph
typedef struct interference_graph_t interference_graph_t;

/**
 * The interference graph is an undirected unweighted graph. As such,
 * we will use an adjacency matrix to represent it. The indices(row and column)
 * will correspond to live range IDs. We know how many live ranges we have by
 * the time we get here.
*/
struct interference_graph_t{
	//The nodes in our graph
	u_int8_t* nodes;
	//The number of nodes is the number of live ranges
	u_int16_t live_range_count;
};

/**
 * Allocate an interference graph. The graph itself should be stack allocated,
 * this will only serve to allocate the internal nodes
*/
void interference_graph_alloc(interference_graph_t* graph, u_int16_t num_nodes);

/**
 * Mark that live ranges a and b interfere
 */
void add_interference_relation(interference_graph_t* graph, live_range_t* a, live_range_t* b);

/**
 * Check whether or not two live ranges interfere
 *
 * Returns true if yes, false if no
 */
u_int8_t do_live_ranges_interfere(interference_graph_t* graph, live_range_t* a, live_range_t* b);

/**
 * Destroy the interference graph
*/
void interference_graph_dealloc(interference_graph_t* graph);

#endif


