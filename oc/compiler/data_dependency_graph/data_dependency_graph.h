/**
 * Author: Jack Robbins
 * This header file exposes the needed APIs for the data dependency graph to the rest of the system.
 * The data dependency graph is currently exclusively used by the instruction scheduler
*/

#ifndef DATA_DEPENDENCY_GRAPH_H
#define DATA_DEPENDENCY_GRAPH_H

//Top level struct for our dependency graph
#include <sys/types.h>
typedef struct data_dependency_graph_t data_dependency_graph_t;

/**
 * The data dependency graph will internally just be an adjacency
 * matrix. This allows us to have O(1) lookup. The O(n^2) space complexity
 * is not a big deal for us because we're just storing these as ints.
 *
 * A value of 0 in the graph means that there is no relation between the two instructions
 *
 *
 * TODO this is a first cut - we may need to use a more costly approach like pointers in
 * the instruction itself if this turns out not to work
 */
struct data_dependency_graph_t{
	u_int32_t** adjacency_matrix;

	//Number of nodes(instructions) that we have
	u_int32_t num_nodes;
};


#endif /* DATA_DEPENDENCY_GRAPH_H */
