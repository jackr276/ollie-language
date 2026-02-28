/**
 * Author: Jack Robbins
 *
 * This is the implementation file for the dependency graph header file of the same name
*/

#include "data_dependency_graph.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

/**
 * Create a data dependency graph. Parent struct
 * is stack allocated
 */
data_dependency_graph_t dependency_graph_alloc(u_int32_t num_nodes){
	//Stack allocate
	data_dependency_graph_t graph;

	//Allocate the nodes
	graph.nodes = calloc(num_nodes, sizeof(data_dependency_graph_node_t*));

	//Initially we have the default amount
	graph.node_count = num_nodes;

	//Initialize the adjacency matrix too
	graph.adjacency_matrix = calloc(num_nodes * num_nodes, sizeof(u_int8_t));

	//This ends up being copied from the adjacency matrix
	graph.transitive_closure = calloc(num_nodes * num_nodes, sizeof(u_int8_t));

	//And the index
	graph.current_index = 0;

	//And give it back
	return graph;
}


/**
 * Reset the visited status for the graph
 */
static inline void reset_dependency_graph_visited_status(data_dependency_graph_t* graph){
	//Run through them all
	for(u_int32_t i = 0; i < graph->node_count; i++){
		//Clear it out
		graph->nodes[i]->visited = FALSE;
	}
}


/**
 * Helper method that visits a node in the topological sort
 *
 * Algorithm:
 * 	if node is visited then
 * 		return
 *
 * 	for each dependency of node "m" do:
 * 		visit(m)
 *
 * 	mark node as visited
 * 	add node to *head* of list
 */
static void topological_sort_visit_node(data_dependency_graph_node_t* node, data_dependency_graph_node_t** sorted, u_int32_t* sorted_index){
	//Base case, we're visited already
	if(node->visited == TRUE){
		return;
	}

	//Run through all of the dependencies
	for(u_int16_t i = 0; i < node->neighbors.current_index; i++){
		//Recursive visit call
		topological_sort_visit_node(dynamic_array_get_at(&(node->neighbors), i), sorted, sorted_index);
	}

	//Now that we're here, the node is truly visited
	node->visited = TRUE;

	//Insert at the head
	sorted[*sorted_index] = node;
	
	//Push it up
	(*sorted_index)++;
}


/**
 * Perform an inplace topological sort on the graph. This is a necessary
 * step before we attempt to find any priorities. This sort happens *inplace*,
 * meaning that it will modify the internal array of the graph
 *
 * Basic algorithm:
 * 	for each node in the node list n
 * 		if node was visited:
 * 			continue
 * 		else:
 * 			visit(n)
 */
void inplace_topological_sort(data_dependency_graph_t* graph){
	//Let's first create a new memory area that we can use to store
	//the topologically sorted version
	//NOTE: for efficiency sake, this list will actually be in reverse order(head is at the last index). We
	//will reverse it at the end
	data_dependency_graph_node_t** sorted_in_reverse = calloc(graph->node_count, sizeof(data_dependency_graph_node_t*));
	//We need to track this too
	u_int32_t sorted_current_index = 0;

	//Let's first wipe the visited status just in case something else ran before us
	reset_dependency_graph_visited_status(graph);

	//Run through all of the nodes
	for(u_int16_t i = 0; i < graph->node_count; i++){
		//It's already visited, so skip it
		if(graph->nodes[i]->visited == TRUE){
			continue;
		}

		//Otherwise, we visit the node
		topological_sort_visit_node(graph->nodes[i], sorted_in_reverse, &sorted_current_index);
	}

	//Once we've done that, we'll actually reverse the list in sorted by going from back-to-front
	//and replacing the graph's list
	for(int16_t i = sorted_current_index - 1; i >= 0; i--){
		//Grab the node out
		data_dependency_graph_node_t* node = sorted_in_reverse[i];

		//Grab the in-order index
		u_int16_t index = graph->node_count - i - 1;

		//Store the index for later on
		node->index = index;

		//Loading in backwards here, so the graph nodes
		//will be at their node count minus i
		graph->nodes[index] = node;
	}

	//Once we're here, we can scrap the reverse sorted list
	free(sorted_in_reverse);
}


/**
 * Create and add a node for a given instruction
 */
void add_data_dependency_node_for_instruction(data_dependency_graph_t* graph, instruction_t* instruction){
	//Allocate it
	data_dependency_graph_node_t* node = calloc(1, sizeof(data_dependency_graph_node_t));

	//Store the instruction pointer
	node->instruction = instruction;

	//Allocate the dynamic array
	node->neighbors = dynamic_array_alloc();

	//Populate the cycle count
	node->cycles_to_complete = get_estimated_cycle_count(instruction);

	//Add it into the list
	graph->nodes[graph->current_index] = node;

	//And now all we need to do is push the index value up
	graph->current_index++;
}


/**
 * Get the leaves of the data DAG. The leaves are simply instructions that have no dependencies
 */
dynamic_array_t get_data_dependency_graph_leaf_nodes(data_dependency_graph_t* graph){
	//Create the dynamic array first
	dynamic_array_t leaves = dynamic_array_alloc();

	//Done via a simple linear scan
	for(u_int32_t i = 0; i < graph->current_index; i++){
		//We want nothing that we rely on here
		if(graph->nodes[i]->relies_on_count == 0){
			dynamic_array_add(&leaves, graph->nodes[i]);
		}
	}

	return leaves;
}


/**
 * Get the roots of the data DAG. The roots are simply instructions that have nothing
 * else depends on. There will often be more than one root
 */
dynamic_array_t get_data_dependency_graph_root_nodes(data_dependency_graph_t* graph){
	//Create the dynamic array first
	dynamic_array_t roots = dynamic_array_alloc();

	//Done via a simple linear scan
	for(u_int32_t i = 0; i < graph->current_index; i++){
		//For a root, we want to be relied on by nothing
		if(graph->nodes[i]->relied_on_by_count == 0){
			dynamic_array_add(&roots, graph->nodes[i]);
		}
	}

	return roots;
}


/**
 * Find the dependency graph node for a given instruction
 *
 * To do this, we can perform a simple linear lookup and compare the memory addresses
 *
 * Returns NULL if the instruction is not in there. However, if we're using this rule, the instruction
 * should always be in there
 */
data_dependency_graph_node_t* get_dependency_node_for_given_instruction(data_dependency_graph_t* graph, instruction_t* instruction){
	//Run through all of the nodes
	for(u_int32_t i = 0; i < graph->node_count; i++){
		if(graph->nodes[i]->instruction == instruction){
			return graph->nodes[i];
		}
	}

	//Otherwise if we get to here, we didn't find it so we return NULL
	return NULL;
}


/**
 * Use the transitive closure to determine how many descendants a given node has. This includes both transitive
 * descendants and direct ones. This can be done by counting every element in this node's given row
 */
static u_int32_t get_number_of_node_descendants(u_int8_t* transitive_closure, data_dependency_graph_node_t* given, u_int32_t num_nodes){
	//Extract a pointer to the row start
	u_int8_t* row = &(transitive_closure[given->index * num_nodes]);

	//Initialize the count
	u_int32_t count = 0;

	//Loop through this item's row
	for(u_int32_t i = 0; i < num_nodes; i++){
		//It's either 0 or 1, so just add whatever it is to avoid the branch
		count += row[i];
	}

	//Give back the count
	return count;
}


/**
 * Given two nodes "a" and "b" that are tied, use several other heuristics to break the tie
 *
 * The tie breaking goes in this order. If this order is found to be unsatisfactory, then we can rearrange
 * it. Recall that the original method:
 *
 * 1.) Look at the total number of descendants for the nodes(done via transitive closure). Nodes with more total
 * descendants(even if they aren't immediate) come first
 * 2.) Look at the rank of both nodes. The rank is the number of immediate successors(nodes that depend on it)
 * that a node has
 * 3.) Look directly at the delay. The higher the delay, the higher the priority
 */
data_dependency_graph_node_t* tie_break(data_dependency_graph_t* graph, data_dependency_graph_node_t* a, data_dependency_graph_node_t* b){
	//Two scores that we'll be comparing
	u_int32_t a_score;
	u_int32_t b_score;

	/**
	 * 1st try: we're looking at the number of total decendants that a node has. Luckily, we've already
	 * computed the transitive closure so this is a simple lookup
	 */
	a_score = get_number_of_node_descendants(graph->transitive_closure, a, graph->node_count);
	b_score = get_number_of_node_descendants(graph->transitive_closure, b, graph->node_count);

	//A is more so give it back
	if(a_score > b_score){
		return a;
	}

	//Vice versa for b
	if(b_score > a_score){
		return b;
	}

	/**
	 * 2nd try: If we hit here - then they're equal, so we need to go to our next tie breaker. Since the total
	 * number of dependents was a tie, we're now going to look at the number of immediate dependents that
	 * the node has
	 */
	a_score = a->relied_on_by_count;
	b_score = b->relied_on_by_count;

	//A is more so give it back
	if(a_score > b_score){
		return a;
	}

	//Vice versa for b
	if(b_score > a_score){
		return b;
	}

	/**
	 * 3rd and final try: if both of the above did not break the tie, we will just use the cycle count. If for some
	 * reason they also have the exact same cycle count, we will just return a
	 */
	a_score = a->cycles_to_complete;
	b_score = b->cycles_to_complete;

	//Just return a if there is a 3rd tie
	if(a_score >= b_score){
		return a;
	} else {
		return b;
	}
}


/**
 * Construct the adjacency matrix for a given graph
 *
 * NOTE: This should be done *after* the topological sort has been done
 * to preserve the ordering
 *
 * We can assume that the matrix has already been made by the allocator
 *
 * The nodes in the rows are the dependencies, while the nodes in the columns
 * are there dependents(depended on by)
 */
void construct_adjacency_matrix(data_dependency_graph_t* graph){
	//Extract the node count
	u_int32_t node_count = graph->node_count;

	//Run through all the nodes
	for(u_int32_t i = 0; i < node_count; i++){
		//Grab the target
		data_dependency_graph_node_t* depends_on_node = graph->nodes[i];

		//Now we grab all of its dependencies
		for(u_int16_t i = 0; i < depends_on_node->neighbors.current_index; i++){
			//Get his dependent
			data_dependency_graph_node_t* dependent = dynamic_array_get_at(&(depends_on_node->neighbors), i);

			//Add it into the adjacency matrix. Recall, the pattern is row = from, column = to 
			graph->adjacency_matrix[depends_on_node->index * node_count + dependent->index] = 1;
		}
	}
}


/**
 * Compute the transitive closure of a DAG that is already topologically sorted. The DAG luckily for
 * us is acyclic, so we don't need to worry about any cycles.
 *
 * In our transitive closure, the *rows are from*, the *columns are to*. So for example, if
 * closure[1][2] = 1, then there is a path from 1 to 2
 *
 * Pseudocode:
 * 	Given a DAG D in topologically sorted order
 *
 * 	transitive_closure <- adjacency_matrix
 *
 * 	For each node U in D iterated *backwards*:
 * 		For each edge V -> U:
 * 			if transitive_closure[u][v] == 1 #U can reach V
 * 				For each edge W -> V: 
 * 					if transitive_closure[v][w] == 1 #V can reach W
 * 						transitive_closure[u][w] = 1 #U can reach W
 *		
 */
static void compute_transitive_closure_of_graph(data_dependency_graph_t* graph){
	//Extract the transitive closure, it will have already been allocated
	u_int8_t* transitive_closure = graph->transitive_closure;

	//Extract this to avoid the repeat memory access
	u_int8_t* adjacency_matrix = graph->adjacency_matrix;

	//Extract so we have it on hand
	u_int32_t node_count = graph->node_count;

	//Duplicate the memory
	memcpy(transitive_closure, adjacency_matrix, node_count * node_count * sizeof(u_int8_t));

	//Assign it over
	graph->transitive_closure = transitive_closure;

	//Run through every node backwards in reverse topological order
	for(int32_t i = node_count - 1; i >= 0; i--){
		//Just so we have it on hand
		data_dependency_graph_node_t* node = graph->nodes[i];

		//Let's iterate over his entire row
		u_int32_t U = node->index;

		//For each vertex adjacent to U -> this would be vertices that
		//are in U's row with the 1 flagged as true. If this is the case, then
		//those vertices are *dependencies* of U
		for(u_int32_t j  = 0; j < node_count; j++){
			//There's no direct edge if this is the case
			//U cannot reach V
			if(adjacency_matrix[U * node_count + j] == 0){
				continue;
			}

			//Extract this one's row number. Remember that this is a dependency
			u_int32_t V = j;

			//For each vertex adjacent to V.(Nodes reachable from V)
			for(u_int32_t k = 0; k < node_count; k++){
				//Does V depend on K? If not, we need to bail out
				if(transitive_closure[V * node_count + k] == 0){
					continue;
				}

				//Otherwise, let's mark that k -> u exists. 
				//In other words, K now has a transitive dependency in U
				transitive_closure[U * node_count + k] = 1;
			}
		}
	}
}


/**
 * Run through the topologically sorted graph and get a list of all nodes that are independent of the given node. Conceptually,
 * this means that we want all nodes that are "independent" from the given node. Conceptually, this means that we want all
 * nodes that are *not* transitive successors or transitive predecessors of the given node. We can do this by finding the transitive
 * closure over the whole graph
 *
 * To do this, we will need to have the transitive closure of the graph already created and stored in the graph struct itself
 *
 * Pseudocode:
 * independent = []
 *
 * For each node N in the graph that is not the given:
 * 	if TC[node][N] == 0 and TC[N][Node] == 0:
 * 		independent = independent U {N}
 *
 * Essentially, we are checking to see if both nodes never cross paths. We should also check here to see if
 * we ever find a load instruction. If we find no load instructions, then the rest of this entire procedure is meaningless.
 * We return the count of load operations that we found which are independent from our given instruction
 */
static u_int32_t get_nodes_independent_of_given(data_dependency_graph_t* graph, data_dependency_graph_node_t* node, dynamic_array_t* independent){
	//Wipe the dynamic array - we are avoiding reallocation for efficiency
	clear_dynamic_array(independent);

	//Assume that we have found no load instructions
	u_int32_t independent_load_count = 0;

	//In the transitive closure, the nodes that are independent of the given node are the ones that are not in
	//it's row(not dependended on by it) and not in its column(not a dependency of it)
	
	//Extract the nodes row
	u_int8_t* node_row = &(graph->transitive_closure[node->index]);

	//Run through everyting in here
	for(u_int32_t i = 0; i < graph->node_count; i++){
		//No point checking here
		if(i == node->index){
			continue;
		}

		//It's dependent, so move on
		if(node_row[i] == 1){
			continue;
		}

		//We now know that i *is not* a transitive successor
		//of the given node. The next thing we need to check is
		//if i is a transitive predecessor of the node. We can
		//do this by looking to see if the node is a transitive
		//predecessor of i
		if(graph->transitive_closure[i * graph->node_count + node->index] == 1){
			continue;
		}

		//We found one more load
		if(is_load_instruction(graph->nodes[i]->instruction) == TRUE){
			independent_load_count++;
		}

		//If we survive to all the way down here, then we know for sure that i is neither
		//a transitive predecessor or a transitive successor of the node
		dynamic_array_add(independent, graph->nodes[i]);
	}

	//Return how many load instructions are independent
	return independent_load_count;
}


/**
 * Recursively go through a graph, adding vertices to the connected component list
 *
 * Pseudocode:
 * connected_component_dfs(vertex, component)
 *
 * mark vertex as visited
 * append vertex to component
 *
 * for each neighbor u of v
 * 	if u is unvisited:
 * 		DFS(vertex, component)
 */
static void connected_component_rec_DFS(data_dependency_graph_node_t* vertex, dynamic_array_t* connected_component){
	//Mark our vertex as visited
	vertex->visited = TRUE;

	//Add this vertex to the component array
	dynamic_array_add(connected_component, vertex);

	//Run through every neighbor of the vertex
	for(u_int16_t i = 0; i < vertex->neighbors.current_index; i++){
		//Extract it
		data_dependency_graph_node_t* neighbor = dynamic_array_get_at(&(vertex->neighbors), i);

		//If this hasn't been visited, add it in
		if(neighbor->visited == FALSE){
			connected_component_rec_DFS(neighbor, connected_component);
		}
	}
}


/**
 * Get all of the connected components of a node in a given subgraph
 * 
 * We are using a subset of our graph that is independent of a given node, 
 * so we will only be creating connected components over this graph
 *
 * for each node in the subgraph:
 * 	component = get_connected_component_dfs(node)
 * 	add component to components
 *
 */
static dynamic_array_t* get_all_connected_components(dynamic_array_t* subgraph, dynamic_array_t* connected_components){
	//Mark everything in the subgraph as unvisited
	for(u_int16_t i = 0; i < subgraph->current_index; i++){
		//Extract it
		data_dependency_graph_node_t* node = dynamic_array_get_at(subgraph, i);

		//Make it unvisited
		node->visited = FALSE;
	}

	//Wipe out the connected components graph from the prior run
	clear_dynamic_array(connected_components);

	for(u_int16_t i = 0; i < subgraph->current_index; i++){
		//Extract it
		data_dependency_graph_node_t* node = dynamic_array_get_at(subgraph, i);

		//If it has been visited, move on
		if(node->visited == TRUE){
			continue;
		}

		//Allocate this on the *heap* specifically. We do this becauase
		//we will be passing this up to a parent, so we can't have it
		//being on the stack
		dynamic_array_t* connected_component = dynamic_array_heap_alloc();

		//Otherwise it hasn't been visited, so find it's connected components
		connected_component_rec_DFS(node, connected_component);

		//Once we're here, we can add this connected component to our overall array of them
		dynamic_array_add(connected_components, connected_component);
	}

	//Give back the dynamic array of dynamic arrays of connected components
	return connected_components;
}


/**
 * Compute the maximum number of loads through any given path in this graph
 *
 * The convenient part is, the graph will come to us in topological order. Using this,
 * it should be pretty easy to compute all possible paths through our graph. We can
 * start up at the very front and run through every path through there. This only works
 * because our graph is acyclic
 *
 * For efficiency's sake here, we do have a reusable array that we keep using for efficiency
 *
 * Pseudocode:
 * for each node in the subgraph:
 * 	load_count[i] = 1 if node is load else 0
 *
 * for each node "V" in the subgraph(which is in topological order):
 * 	for each node "U" in its neighbor set:
 * 		if U is a load:
 * 			add = 1 # one more load on V's path
 * 		else:
 * 			add = 0 # no more loads on V's path
 *	 	load_count[U index] = max(load_count[U_index], load_count[V_index] + add)
 */
static u_int32_t get_maximum_loads_through_any_path_in_subgraph(dynamic_array_t* graph, u_int32_t* load_counts){
	for(u_int16_t i = 0; i < graph->current_index; i++){
	 	//Extract it
	 	data_dependency_graph_node_t* node = dynamic_array_get_at(graph, i);

		//If this is a load instruction, we already have one along the path. If not, then
		//we have 0
		if(is_load_instruction(node->instruction) == TRUE){
			load_counts[i] = 1;
		} else {
			load_counts[i] = 0;
		}
	}

	//Now run through every instruction in the subgraph
	for(u_int16_t i = 0; i < graph->current_index; i++){
		//Extract it
		data_dependency_graph_node_t* node = dynamic_array_get_at(graph, i);

		//For every node in the neighbor set, run through those
		for(u_int16_t j = 0; j  < node->neighbors.current_index; j++){
			//Extract the neighbor "U"
			data_dependency_graph_node_t* U = dynamic_array_get_at(&(node->neighbors), j);

			//What are we adding
			u_int32_t add = 0;

			//If this is a load instruction, we are adding one more
			if(is_load_instruction(U->instruction) == TRUE){
				add = 1;
			}

			//The load count that we have could potentially be the old load count on the U path
			//plus one more. This path increment is representative of the parent node's load
			//count plus either one more load or none at all
			u_int32_t path_increment = load_counts[i] + add;

			//Now we'll get U's final count
			u_int32_t final_count = load_counts[j] > path_increment ? load_counts[j] : path_increment;

			//Whatever one out there, we wadd it here
			load_counts[j] = final_count;
		}
	}

	//Maximum number of loads
	u_int32_t max_loads = 0;

	//One last quick loop to find the highest
	for(u_int16_t i = 0; i < graph->current_index; i++){
		//If this one is higher, it is the new max
		if(max_loads < load_counts[i]){
			max_loads = load_counts[i];
		}
	}

	//Give back whatever the max is
	return max_loads;
}


/**
 * Compute the cycle counts for load operations using a special algorithm. This will
 * help us in getting more accurate delay counts that somewhat account for the
 * possibility of cache misses. This should only be run on graphs that definitely
 * have load instructions in them, otherwise we are wasting our time on this
 *
 * Pseudocode:
 * 	for each operation i in D:
 * 		let Di be the nodes in D *independent* of i
 * 		for each connected component C of Di:
 * 			find the maximal number of loads N on any path through C
 * 			for each load operation l in D
 * 				cycles(l) = cycles(l) + cycles(i) / N
 *
 * This algorithm works because all of the independent operations will share
 * in the slack time of delayed loads. Every load is added a fractional part of the 
 * maximum number of loads in the last step
 */
void compute_cycle_counts_for_load_operations(data_dependency_graph_t* graph){
	//Let's create a reusable "independent" array to lighten memory pressure
	dynamic_array_t independent = dynamic_array_alloc();

	//The set of connected components is also reusable
	dynamic_array_t connected_components = dynamic_array_alloc();

	//A reusable static array for load counts in the maximum load computation
	//step. We can just initialize it to the size of the original graph
	u_int32_t* load_counts = calloc(graph->node_count, sizeof(u_int32_t));

	//Run through all of the nodes
	for(u_int32_t i = 0; i < graph->node_count; i++){
		//Graph each node
		data_dependency_graph_node_t* node = graph->nodes[i];

		//Get a list of all nodes independent of this one(will be loaded
		//into "independent"). This essentially gives us a sub-graph
		//that has had everything related to the above node removed
		u_int32_t independent_load_count = get_nodes_independent_of_given(graph, node, &independent);

		//If there is no load on this path, then N will be 0
		//and we will have done all of this work for no reason. As such,
		//we will skip out here if this is the case
		if(independent_load_count == 0){
			continue;
		}

		//Create the connected component array for this subgraph(the nodes in independent)
		//so that we can search through it
		get_all_connected_components(&independent, &connected_components);

		//Run through every connected component in here
		for(u_int16_t j = 0; j < connected_components.current_index; j++){
			//Extract it
			dynamic_array_t* connected_component = dynamic_array_get_at(&connected_components, j);

			//Get the maximum number of loads through any given path in this connected component
			u_int32_t maximum_loads = get_maximum_loads_through_any_path_in_subgraph(connected_component, load_counts);

			//Once we're done using it, we can release this entire thing. Remember that it was
			//on the heap, so we need to use the heap deallocator for it
			dynamic_array_heap_dealloc(&connected_component);

			//If there are no loads along this path, just move along
			if(maximum_loads == 0){
				continue;
			}

			//One final loop here, we need to run through all of the instructions and update
			//the load instructions
			for(u_int16_t k =  0; k < graph->node_count; k++){
				//Extract it
				data_dependency_graph_node_t* updated_node = graph->nodes[k];

				//Now we update it here. The formula is:
				//	cycles(k) = cycles(k) + cycles(i) / maximum_loads 
				u_int32_t cycle_count = updated_node->cycles_to_complete + (node->cycles_to_complete / maximum_loads);

				//Now we update the node's cycle count
				updated_node->cycles_to_complete = cycle_count;
			}
		}
	}

	//Release the load counts
	free(load_counts);
	//Let go of these now that we're done
	dynamic_array_dealloc(&independent);
	//Let go of this now that we're done
	dynamic_array_dealloc(&connected_components);
}


/**
 * Compute the longest path on a topologically sorted graph between the node and the root
 *
 * Pseudocde(Source S, Root R, DAG D(in topological order))
 *
 * if root has no depencies:
 * 	return 0
 *
 * for each vertex V in D:
 * 	dist[V] = -INF
 *
 * dist[S] = 0
 *
 * for each node U in D
 * 	if dist[U] == -INF:
 * 		continue //unreachable
 * 	for each edge U -> V with weight w:
 * 		if dist[U] + W > dist[V]:
 * 			dist[V] = dist[U] + W
 *
 * 	return dist[R] //dist[R] holds the longest path to R
 *
 */
static int32_t compute_longest_path_to_root_node(data_dependency_graph_t* graph, data_dependency_graph_node_t* start, data_dependency_graph_node_t* root, int32_t* distances){
	//If this root has no dependencies, then we can just get out
	//instead of going through the trouble. This can potentially happen
	//for jmps that end a block
	if(root->relies_on_count == 0){
		return 0;
	}
	
	//Make them all INT_MIN except for our source
	for(u_int32_t i = 0; i < graph->current_index; i++){
		distances[i] = INT_MIN;
	}

	//With the exception of our given vertex. This one we know the
	//distance is 0
	distances[start->index] = 0;

	//For each node U in D
	for(u_int32_t i = 0; i < graph->node_count; i++){
		//Grab out our node
		data_dependency_graph_node_t* U = graph->nodes[i];

		//If the distance is infinite, it's unreachable from
		//our source so we don't care
		if(distances[U->index] == INT_MIN){
			continue;
		}

		//Otherwise, it is reachable from our source, so we need
		//to check the weights for each edge
		for(u_int16_t j = 0; j < U->neighbors.current_index; j++){
			//Extract the neighbor
			data_dependency_graph_node_t* V = dynamic_array_get_at(&(U->neighbors), j);

			//The weight is the number of cycles that *U* takes to run
			int32_t weight = U->cycles_to_complete;

			//If the distance with this weight exceeds our current distance,
			//then this is our new distance
			if(distances[U->index] + weight > distances[V->index]){
				distances[V->index] = distances[U->index] + weight;
			}
		}
	}

	//The longest path is always the one at the root's area
	int32_t longest_path = distances[root->index];

	//Whatever our longest one is
	return longest_path;
}


/**
 * Find the priority for a given node in the dependency graph D
 *
 * The priority is found by finding the longest weighted path from the node to any root in D.
 *
 * Pseudocode:
 * 	longest_path = 0
 *
 * 	for every root in graph D
 * 		candidate = compute_longest_path(node, root_node)
 *
 * 		if candidate > longest_path:
 * 			longest_path = candidate
 * 		
 * By the end, we will have found the longest path
 */
static int32_t compute_longest_weighted_path_heuristic_for_node(data_dependency_graph_t* graph, data_dependency_graph_node_t* node, dynamic_array_t* roots, int32_t* distances){
	//If this node is already a root, we can just return 0, there is no path to speak of
	if(node->relied_on_by_count == 0){
		return 0;
	}

	//Initialize this to have the longest path be at 0
	int32_t longest_path = 0;
	//Potential longest path
	int32_t candidate;

	//Run through every single node
	for(u_int16_t i = 0; i < roots->current_index; i++){
		//Extract the root node
		data_dependency_graph_node_t* root_node = dynamic_array_get_at(roots, i);

		//Let the helper compute it
		candidate = compute_longest_path_to_root_node(graph, node, root_node, distances);

		//Did we beat the current longest path? If so then
		//we are the longest path
		if(candidate > longest_path){
			longest_path = candidate;
		}
	}

	//Give back whatever our longest path was
	return longest_path;
}


/**
 * Find the priority for all nodes in the dependency graph D. This is done
 * internally using the longest path between a given node and a root
 *
 * NOTE: the graph *must* be topologically sorted before we invoke this function
 * for it to work properly
 */
void compute_priorities_for_all_nodes(data_dependency_graph_t* graph){
	//Extract the graph's roots
	dynamic_array_t roots = get_data_dependency_graph_root_nodes(graph);

	//A reusable distances[] array - this lets us avoid allocating memory every time.
	//The distances array itself is wiped out every time the helper runs, so this
	//won't be an issue
	int32_t* distances = calloc(graph->node_count, sizeof(int32_t));

	//Run through every single node in the graph
	for(u_int16_t i = 0; i < graph->current_index; i++){
		//Compute the priority for the given node
		graph->nodes[i]->priority = compute_longest_weighted_path_heuristic_for_node(graph, graph->nodes[i], &roots, distances);
	}

	//We're done with distances[] now
	free(distances);

	//We're done with the roots so scrap them
	dynamic_array_dealloc(&roots);
}


/**
 * Finalize the data dependency graph by:
 * 	1.) topologically sorting it
 * 	2.) constructing the adjacency matrix
 * 	3.) constructing the transitive closure
 *
 * This needs to be done before we start thinking about anything else
 */
void finalize_data_dependency_graph(data_dependency_graph_t* graph){
	//Step 1 - topologically sort it
	inplace_topological_sort(graph);

	//Step 2 - construct the adjacency matrix. We can't do this on the fly
	//because we know that we'll be sorting it. We do so now to keep the indices
	//matching
	construct_adjacency_matrix(graph);

	//Once we've done all of that, we'll construct the transitive closure
	compute_transitive_closure_of_graph(graph);
}


/**
 * Add a dependence between the dependent and the dependency
 *
 * NOTE: This assumes that the graph *already* has nodes for the target and depends_on instructions
 * created and at the ready
*/
void add_dependence(data_dependency_graph_t* graph, instruction_t* target, instruction_t* depends_on){
	//Extract both of their nodes from the graph's lists
	data_dependency_graph_node_t* target_node = get_dependency_node_for_given_instruction(graph, target);
	data_dependency_graph_node_t* depends_on_node = get_dependency_node_for_given_instruction(graph, depends_on);

	//This is a hard failure if we ever get here
	if(target_node == NULL || depends_on_node == NULL){
		printf("Fatal internal compiler error: attempt to add dependence for nonexistent instruction DAG nodes\n");
		exit(1);
	}

	//This is possible, if we have something like testl t6, t6, t6 may try to get in there
	//twice
	if(dynamic_array_contains(&(depends_on_node->neighbors), target_node) != NOT_FOUND){
		return;
	}

	//The target now depends on one more thing
	target_node->relies_on_count++;

	//The dependency node now has one more thing relying on it
	depends_on_node->relied_on_by_count++;

	//Now we link them together in the list. This will be added to the "depends_on" node's list because the list
	//is a "from->to" type list. We have a dependency connection from the depends_on node to the target
	dynamic_array_add(&(depends_on_node->neighbors), target_node);
}


/**
 * A utility that will print an N x N adjacency matrix out to the console for debug reasons
 */
void print_adjacency_matrix(FILE* output, u_int8_t* matrix, u_int32_t num_nodes){
	//Run through each row
	for(u_int32_t i = 0; i < num_nodes; i++){
		//Print out the row number
		fprintf(output, "[%2d]: ", i);

		//Now print out the columns
		for(u_int32_t j = 0; j < num_nodes; j++){
			//Will be 1(connected) or 0
			fprintf(output, "%d ", matrix[i * num_nodes + j]);
		}

		//Final newline
		fprintf(output, "\n");
	}
}


/**
 * Print out the entirety of the data dependence graph
 */
void print_data_dependence_graph(FILE* output, data_dependency_graph_t* graph){
	//Run through all of the nodes
	for(u_int32_t i = 0; i < graph->current_index; i++){
		//Extract the address
		data_dependency_graph_node_t* node = graph->nodes[i];

		fprintf(output, "================================================\n");

		//Print the instruction
		fprintf(output, "ID %d, Instruction: ", node->index);
		print_instruction(stdout, node->instruction, PRINTING_VAR_IN_INSTRUCTION);
		//Now show what it depends on
		fprintf(output, "Depended on by: [\n");

		//Run through all of what we depend on
		for(u_int16_t j = 0; j < node->neighbors.current_index; j++){
			//Print out the successor 
			data_dependency_graph_node_t* successor = dynamic_array_get_at(&(node->neighbors), j);
			print_instruction(stdout, successor->instruction, PRINTING_VAR_IN_INSTRUCTION);
		}

		printf("]\n");
		//Display the priority too
		printf("Priority is %d\n", node->priority);

		fprintf(output, "================================================\n");
	}

	fprintf(output, "================== Adjacency Matrix ===================\n");
	print_adjacency_matrix(output, graph->adjacency_matrix, graph->node_count);
	fprintf(output, "================== Adjacency Matrix ===================\n");

	//Print out the transitive closure if one exists
	if(graph->transitive_closure != NULL){
		fprintf(output, "================== Transitive Closure ===================\n");
		print_adjacency_matrix(output, graph->transitive_closure, graph->node_count);
		fprintf(output, "================== Transitive Closure ===================\n");
	}
}


/**
 * Free a data dependency graph. This also includes freeing all 
 * of the nodes and resultant lists
 */
void dependency_graph_dealloc(data_dependency_graph_t* graph){
	//Run through all of the nodes
	for(u_int16_t i = 0; i < graph->node_count; i++){
		//Grab a pointer to this
		data_dependency_graph_node_t* node = graph->nodes[i];

		//Free the dynamic array
		dynamic_array_dealloc(&(node->neighbors));

		//Now deallocate the node itself
		free(node);
	}

	//Now free the overall array of nodes
	free(graph->nodes);

	//Free the adjacency matrix
	free(graph->adjacency_matrix);

	//Free the transitive closure too
	free(graph->transitive_closure);
}
