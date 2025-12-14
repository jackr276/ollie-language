/**
 * Author: Jack Robbins
 *
 * This test suite will verify the functionality of the adjacency matrix that
 * underpins the interference graph
*/

#include "../interference_graph/interference_graph.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>


/**
 * Create a live range
 */
static live_range_t* live_range_alloc(u_int16_t live_range_id){
	//Calloc it
	live_range_t* live_range = calloc(1, sizeof(live_range_t));

	//Give it a unique id
	live_range->live_range_id = live_range_id;

	//And create it's dynamic array
	live_range->variables = dynamic_array_alloc();

	//Create it's adjacency list
	live_range->neighbors = dynamic_array_alloc();

	//Finally we'll return it
	return live_range;
}


/**
 * Everything in here is run through the main function, we're
 * just verifying functionality
*/
int main(int argc, char** argv){
	//Here's our graph
	interference_graph_t* graph = NULL;
	
	dynamic_array_t live_ranges = dynamic_array_alloc();

	//Just make some values
	for(u_int16_t i = 0; i < 20; i++){
		live_range_t* new_live_range = live_range_alloc(i);
		dynamic_array_add(&live_ranges, new_live_range);
	}

	//Now add some more interference
	for(u_int16_t i = 0; i < 13; i++){
		int32_t i = rand() % 20;
		int32_t j = rand() % 20;

		if(j != i){
			add_interference(graph, dynamic_array_get_at(&live_ranges, i), dynamic_array_get_at(&live_ranges, j));
			printf("%d and %d interfere\n", i, j);
		}
	}

	//Print out all adjacency lists here
	print_adjacency_lists(&live_ranges);

	//Now let's construct the full graph from this
	graph = construct_interference_graph_from_adjacency_lists(&live_ranges);

	//Print the full one
	print_interference_graph(graph);

	//Now run through and do the counts
	for(u_int16_t i = 0; i < 20; i++){
		live_range_t* range = dynamic_array_get_at(&live_ranges, i);

		printf("LR%d has %d neighbors\n", range->live_range_id, get_live_range_degree(range));
	}

	//Once we're done, deallocate it
	interference_graph_dealloc(graph);

	//Success
	return 0;
}

