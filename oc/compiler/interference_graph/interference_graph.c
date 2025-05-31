/**
 * Author: Jack Robbins
 *
 * This file contains the implementations of the APIs
 * defined in the interference graph header file
*/

#include "interference_graph.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

//For standardization across all modules
#define TRUE 1
#define FALSE 0


/**
 * Allocate an interference graph. The graph itself should be stack allocated,
 * this will only serve to allocate the internal nodes
*/
static interference_graph_t* interference_graph_alloc(u_int16_t live_range_count){
	//Allocate it
	interference_graph_t* graph = calloc(1, sizeof(interference_graph_t));

	//Mark this first
	graph->live_range_count = live_range_count;

	//Since this is a 2-way graph, we'll need to allocate enough space for a 2-way table
	graph->nodes = calloc(live_range_count * live_range_count, sizeof(u_int8_t));

	//and we're all set
	return graph;
}


/**
 * Mark that live ranges a and b interfere
 */
void add_interference(interference_graph_t* graph, live_range_t* a, live_range_t* b){
	//If these are the exact same live range, they can't interfere with eachother 
	//so we'll skip this
	if(a == b){
		return;
	}

	//These are now eachother's neighbors
	dynamic_array_add(a->neighbors, b);
	dynamic_array_add(b->neighbors, a);

	//If the graph isn't null, we'll assume that the caller wants us to add this in
	if(graph != NULL){
		//Calculate the offsets
		u_int16_t offset_a_b = a->interference_graph_index * graph->live_range_count + b->interference_graph_index;
		u_int16_t offset_b_a = b->interference_graph_index * graph->live_range_count + a->interference_graph_index;

		//Now we'll go to the adjacency matrix and add this in
		graph->nodes[offset_a_b] = TRUE;
		graph->nodes[offset_b_a] = TRUE;
	}

	//Increment their degress
	(a->degree)++;
	(b->degree)++;
}


/**
 * Mark that live ranges a and b do not interfere. This can be used if an interference
 * relation is removed
 */
void remove_interference(interference_graph_t* graph, live_range_t* a, live_range_t* b){
	//To add the interference we'll first need to calculate the offsets for both
	//b's and a's version
	u_int16_t offset_a_b = a->interference_graph_index * graph->live_range_count + b->interference_graph_index;
	u_int16_t offset_b_a = b->interference_graph_index * graph->live_range_count + a->interference_graph_index;

	//Now we'll go to the adjacency matrix and add this in
	graph->nodes[offset_a_b] = FALSE;
	graph->nodes[offset_b_a] = FALSE;

	//And we'll remove them from eachother's adjacency lists
	dynamic_array_delete(a->neighbors, b);
	dynamic_array_delete(b->neighbors, a);

	(a->degree)--;
	(b->degree)--;
}


/**
 * Build the interference graph from the adjacency lists
 */
interference_graph_t* construct_interference_graph_from_adjacency_lists(dynamic_array_t* live_ranges){
	//Run through and give everything an index
	for(u_int16_t i = 0; i < live_ranges->current_index; i++){
		live_range_t* range = dynamic_array_get_at(live_ranges, i);
		range->interference_graph_index = i;
	}
	
	//Now we'll create the actual graph itself
	interference_graph_t* graph = interference_graph_alloc(live_ranges->current_index);

	//Once we have that, we're ready to add all of our interferences
	for(u_int16_t i = 0; i < live_ranges->current_index; i++){
		//Grab it out
		live_range_t* range = dynamic_array_get_at(live_ranges, i);

		//Now we iterate through it's adjacency list
		for(u_int16_t j = 0; j < range->neighbors->current_index; j++){
			//Grab it out
			live_range_t* neighbor = dynamic_array_get_at(range->neighbors, j);

			//Calculate the offsets
			u_int16_t offset_a_b = range->interference_graph_index * graph->live_range_count + neighbor->interference_graph_index;
			u_int16_t offset_b_a = neighbor->interference_graph_index * graph->live_range_count + range->interference_graph_index;

			//Now we'll go to the adjacency matrix and add this in
			graph->nodes[offset_a_b] = TRUE;
			graph->nodes[offset_b_a] = TRUE;
		}
	}

	//Give the graph back
	return graph;
}


/**
 * Check whether or not two live ranges interfere
 *
 * Returns true if yes, false if no
 */
u_int8_t do_live_ranges_interfere(interference_graph_t* graph, live_range_t* a, live_range_t* b){
	//To determine this, we'll first need the offset
	u_int16_t offset_a_b = a->live_range_id * graph->live_range_count + b->live_range_id;

	//Now we'll need to return the graph at said value
	return graph->nodes[offset_a_b];
}


/**
 * Print out a visual representation of the interference graph
 */
void print_interference_graph(interference_graph_t* graph){
	char name[50];
	//Print out every column first
	printf("%4s ", "#");
	
	//Column headers
	for(u_int16_t i = 0; i < graph->live_range_count; i++){
		sprintf(name, "LR%d", i);
		printf(" %4s", name);
	}

	printf("\n");

	//Now we'll print every single row
	for(u_int16_t i = 0; i < graph->live_range_count; i++){
		//Print the name first
		sprintf(name, "LR%d", i);
		printf(" %4s ", name);

		//Then for each column, we'll print out X for true or _ for false
		for(u_int16_t j = 0; j < graph->live_range_count; j++){
			if(graph->nodes[i * graph->live_range_count + j] == TRUE){
				printf(" %3s ", "X");
			} else {
				printf(" %3s ", "_");
			}
		}

	//Newline to end it out
	printf("\n");
	}
}


/**
 * Get the "degree" for a certain live range. The degree is the number of
 * nodes that interfere with it. We also call these neighbors
 */
u_int16_t get_live_range_degree(interference_graph_t* graph, live_range_t* a){
	//Overall count
	u_int16_t count = 0;

	//Grab the ID number out
	u_int16_t id = a->live_range_id;

	//Grab that "row" in the graph
	u_int16_t row_index = graph->live_range_count * id * sizeof(u_int8_t);

	//Grab a pointer to this
	u_int8_t* row = graph->nodes + row_index;

	//Run through until we hit the end
	for(u_int16_t i = 0; i < graph->live_range_count; i++){
		if(row[i] == TRUE){
			count++;
		}
	}	

	//And give it back
	return count;
}


/**
 * Destroy the interference graph
*/
void interference_graph_dealloc(interference_graph_t* graph){
	//All that we need to do here is deallocate the internal list
	free(graph->nodes);

	//And then deallocate this
	free(graph);
}

