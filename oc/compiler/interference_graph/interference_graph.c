/**
 * Author: Jack Robbins
 *
 * This file contains the implementations of the APIs
 * defined in the interference graph header file
*/

#include "interference_graph.h"
#include <sys/types.h>

/**
 * Allocate an interference graph. The graph itself should be stack allocated,
 * this will only serve to allocate the internal nodes
*/
void interference_graph_alloc(interference_graph_t* graph, u_int16_t live_range_count){
	//Mark this first
	graph->live_range_count = live_range_count;

	//Since this is a 2-way graph, we'll need to allocate enough space for a 2-way table
	graph->nodes = calloc(live_range_count * live_range_count, sizeof(u_int8_t));

	//and we're all set
}


/**
 * Destroy the interference graph
*/
void interference_graph_dealloc(interference_graph_t* graph){
	//All that we need to do here is deallocate the internal list
	free(graph->nodes);

	//And set these to null/0 as a warning
	graph->live_range_count = 0;
	graph->nodes = NULL;
}

