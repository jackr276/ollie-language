/**
 * Author: Jack Robbins
 * This header file exposes the needed APIs for the data dependency graph to the rest of the system.
 * The data dependency graph is currently exclusively used by the instruction scheduler
*/

#ifndef DATA_DEPENDENCY_GRAPH_H
#define DATA_DEPENDENCY_GRAPH_H
#include <sys/types.h>
#include "../utils/constants.h"
#include "../utils/dynamic_array/dynamic_array.h"
#include "../instruction/instruction.h"

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
 * Get the edge weight between two instructions
 */
u_int32_t get_edge_weight(instruction_t* depends_on, instruction_t* target);


#endif /* DATA_DEPENDENCY_GRAPH_H */
