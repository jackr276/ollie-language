/**
 * Author: Jack Robbins
 *
 * This test suite will verify the functionality of the adjacency matrix that
 * underpins the interference graph
*/

#include "../interference_graph/interference_graph.h"


/**
 * Everything in here is run through the main function, we're
 * just verifying functionality
*/
int main(int argc, char** argv){
	//Stack allocate the graph
	interference_graph_t graph;

	//First we'll create it. We'll use 35 nodes
	//for the test
	interference_graph_alloc(&graph, 35);

	//Once we're done, deallocate it
	interference_graph_dealloc(&graph);

	//Success
	return 0;
}

