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
 * Mark that live ranges a and b interfere
 *
 * NOTE: The graph is an optional parameter. If NULL is passed in, we'll assume that
 * the graph has not yet been allocated and skip that part
 */
void add_interference(interference_graph_t* graph, live_range_t* a, live_range_t* b);

/**
 * Build the interference graph from the adjacency lists
 */
interference_graph_t* construct_interference_graph_from_adjacency_lists(dynamic_array_t* live_ranges);

/**
 * Redo the adjacency matrix after a change has been made(usually coalescing)
 */
interference_graph_t* update_interference_graph(interference_graph_t* graph);

/**
 * Mark that live ranges a and b do not interfere
 */
void remove_interference(interference_graph_t* graph, live_range_t* a, live_range_t* b);

/**
 * Coalesce a live range with another one. This will have the effect of everything in
 * said live range becoming as one. The only live range that will survive following this 
 * is the target
 */
void coalesce_live_ranges(interference_graph_t* graph, live_range_t* target, live_range_t* coalescee);

/**
 * Print out a visual representation of the interference graph
 */
void print_interference_graph(interference_graph_t* graph);

/**
 * Print out the adjacency lists of every single live range
 */
void print_adjacency_lists(dynamic_array_t* live_ranges);

/**
 * Get the number of neighbors for a certain node
 */
u_int16_t get_live_range_degree(live_range_t* a);

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


