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


void add_dependence(instruction_t* dependency, instruction_t* dependent);

void remove_dependence(instruction_t* dependency, instruction_t* dependent);

u_int32_t get_edge_weight(instruction_t* dependency, instruction_t* dependent);

#endif /* DATA_DEPENDENCY_GRAPH_H */
