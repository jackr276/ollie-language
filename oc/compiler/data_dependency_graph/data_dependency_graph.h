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
typedef struct data_dependency_graph_t data_dependency_graph_t;
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
	//The number of instructions that rely on this instruction
	u_int32_t relied_on_by_count;
	//The number of instructions that this instruction relies on
	u_int32_t relies_on_count;
};

/**
 * The overall struct contains a dynamically resizing list
 * of vertices(instructions) that are linked with their
 * adjacency lists(dynamic_arrays)
 */
struct data_dependency_graph_t {
	//Just an array of node pointers
	data_dependency_graph_node_t** nodes; 
	//The maximum node count - this is actually known at allocation time
	u_int16_t node_count;
	//The current index
	u_int16_t current_index;
};

/**
 * Create a data dependency graph. Parent struct
 * is stack allocated
 */
data_dependency_graph_t dependency_graph_alloc(u_int32_t num_nodes);

/**
 * Add a node for a given instruction. Once this function executes, there will be a dependency
 * node for the given instruction
 */
void add_data_dependency_node_for_instruction(data_dependency_graph_t* graph, instruction_t* instruction);

/**
 * Find the node for a given instruction. This function returns NULL if no node is found
 */
data_dependency_graph_node_t* get_dependency_node_for_given_instruction(data_dependency_graph_t* graph, instruction_t* instruction);

/**
 * Add a dependence between the two instructions
 *
 * NOTE: This function *assumes* that both target and depends_on already have nodes created for them
 */
void add_dependence(data_dependency_graph_t* graph, instruction_t* target, instruction_t* depends_on);

/**
 * Print out the entirety of the data dependence graph
 */
void print_data_dependence_graph(FILE* output, data_dependency_graph_t* graph);

/**
 * Free a data dependency graph
 */
void dependency_graph_dealloc(data_dependency_graph_t* graph);

#endif /* DATA_DEPENDENCY_GRAPH_H */
