/**
 * Author: Jack Robbins
 * This header file exposes the needed APIs for the data dependency graph to the rest of the system.
 * The data dependency graph is currently exclusively used by the instruction scheduler
 * 
 * Internally, the DAG itself is represented by adjacency lists. This allows us to iterate over neighbors
 * quickly an efficiently. Instructions internally keep a count of their dependencies and dependees that is 
 * utilized here
*/

#ifndef DATA_DEPENDENCY_GRAPH_H
#define DATA_DEPENDENCY_GRAPH_H
#include <sys/types.h>
#include "../utils/constants.h"
#include "../utils/dynamic_array/dynamic_array.h"
#include "../instruction/instruction.h"

//Top level type definition
typedef struct data_depency_graph_t data_depency_graph_t;
//The individual nodes on the graph
typedef struct data_dependency_graph_node_t data_dependency_graph_node_t;

/**
 * Dependency graph nodes carry:
 * 	1.) The instruction that they are referencing
 * 	2.) The adjacency list that they have
 * 	3.) The time(in cycles) that this node would take to execute
 */
struct data_dependency_graph_node_t {
	//What instruction does this graph reference
	instruction_t* instruction;
	//The list of all other nodes that *depend* on this instruction
	//This is a strict one-way relationship. The neighbors depend on this
	//node, and this node does not depend on the neighbors
	dynamic_array_t* neighbors;
	//The cycle time that this instruction takes
	u_int32_t cycles_to_complete;
};

/**
 * The overall struct contains a dynamically resizing list
 * of vertices(instructions) that are linked with their
 * adjacency lists(dynamic_arrays)
 */
struct data_depency_graph_t {
	//Just an array of nodes
	dynamic_array_t* nodes; 
};

/**
 * Create a data dependency graph. Parent struct
 * is stack allocated
 */
data_depency_graph_t dependency_graph_alloc();

/**
 * Add a dependence between the two instructions
 */
void add_dependence(instruction_t* depends_on, instruction_t* target);

/**
 * Print out the entirety of the data dependence graph
 */
void print_data_dependence_graph(FILE* output, instruction_t** graph, u_int32_t num_nodes);

/**
 * Remove a dependence between the two instructions
 */
void remove_dependence(instruction_t* depends_on, instruction_t* target);

/**
 * Free a data dependency graph
 */
void dependency_graph_dealloc(data_depency_graph_t* graph);

#endif /* DATA_DEPENDENCY_GRAPH_H */
