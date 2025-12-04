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
	//The priority of the instruction
	int32_t priority;
	//The number of instructions that rely on this instruction
	u_int32_t relied_on_by_count;
	//The number of instructions that this instruction relies on
	u_int32_t relies_on_count;
	//What is the index of this node in the list
	u_int16_t index;
	//Has this been visited or not(for our traversals)
	u_int8_t visited;
};

/**
 * The overall struct contains a dynamically resizing list
 * of vertices(instructions) that are linked with their
 * adjacency lists(dynamic_arrays)
 */
struct data_dependency_graph_t {
	//Just an array of node pointers
	data_dependency_graph_node_t** nodes; 
	//The adjacency matrix(stored to make lookups faster)
	u_int8_t* adjacency_matrix;
	//We may need to compute the closure. If we do, we'll store
	//it here
	u_int8_t* transitive_closure;
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
 * Perform an inplace topological sort on the graph. This is a necessary
 * step before we attempt to find any priorities. This sort happens *inplace*,
 * meaning that it will modify the internal array of the graph
 */
void inplace_topological_sort(data_dependency_graph_t* graph);

/**
 * Add a node for a given instruction. Once this function executes, there will be a dependency
 * node for the given instruction
 */
void add_data_dependency_node_for_instruction(data_dependency_graph_t* graph, instruction_t* instruction);

/**
 * Get the leaves of the data DAG. The leaves are simply instructions that have no dependencies
 */
dynamic_array_t* get_data_dependency_graph_leaf_nodes(data_dependency_graph_t* graph);

/**
 * Get the roots of the data DAG. The roots are simply instructions that have nothing
 * else depends on. There will often be more than one root
 */
dynamic_array_t* get_data_dependency_graph_root_nodes(data_dependency_graph_t* graph);

/**
 * Compute the cycle counts for load operations using a special algorithm. This will
 * help us in getting more accurate delay counts that somewhat account for the
 * possibility of cache misses. This should only be run on graphs that definitely
 * have load instructions in them, otherwise we are wasting our time on this
 */
void compute_cycle_counts_for_load_operations(data_dependency_graph_t* graph);

/**
 * Find the node for a given instruction. This function returns NULL if no node is found
 */
data_dependency_graph_node_t* get_dependency_node_for_given_instruction(data_dependency_graph_t* graph, instruction_t* instruction);

/**
 * Given two nodes "a" and "b" that are tied, use several other heuristics to break the tie
 */
data_dependency_graph_node_t* tie_break(data_dependency_graph_node_t* a, data_dependency_graph_node_t* b);

/**
 * Find the priority for all nodes in the dependency graph D. This is done
 * internally using the longest path between a given node and a root
 */
void compute_priorities_for_all_nodes(data_dependency_graph_t* graph);

/**
 * Finalize the data dependency graph by:
 * 	1.) topologically sorting it
 * 	2.) constructing the adjacency matrix
 * 	3.) constructing the transitive closure
 *
 * This needs to be done before we start thinking about anything else
 */
void finalize_data_dependency_graph(data_dependency_graph_t* graph);

/**
 * Add a dependence between the two instructions
 *
 * NOTE: This function *assumes* that both target and depends_on already have nodes created for them
 */
void add_dependence(data_dependency_graph_t* graph, instruction_t* target, instruction_t* depends_on);

/**
 * Construct the adjacency matrix for a given graph
 *
 * NOTE: This should be done *after* the topological sort has been done
 *
 * We can assume that the matrix has already been made by the allocator
 */
void construct_adjacency_matrix(data_dependency_graph_t* graph);

/**
 * Print out the entirety of the data dependence graph
 */
void print_data_dependence_graph(FILE* output, data_dependency_graph_t* graph);

/**
 * Free a data dependency graph
 */
void dependency_graph_dealloc(data_dependency_graph_t* graph);

#endif /* DATA_DEPENDENCY_GRAPH_H */
