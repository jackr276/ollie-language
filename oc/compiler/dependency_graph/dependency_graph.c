/**
 * Author: Jack Robbins
 * This C file contains the definitions for the APIs defined in the header file of the same name
 */

#include "dependency_graph.h"

//Maintain a unique atomically increasing node it
static u_int32_t current_node_id = 0;
