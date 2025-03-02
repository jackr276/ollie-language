/**
 * Author: Jack Robbins
 * This is the implementation file for the dependency analyzer, linked
 * to the header file of the same name
*/

#include "dependency_analyzer.h"
#include <string.h>
#include <stdlib.h>

//The current head of the tree, used for memory management
static dependency_tree_node_t* current_head = NULL;

/**
 * Create a node for a given filename
 */
dependency_tree_node_t* dependency_tree_node_alloc(char* filename){
	//First we allocate the node
	dependency_tree_node_t* node = calloc(1, sizeof(dependency_tree_node_t));

	//We'll then copy over the filename
	strncpy(node->filename, filename, FILENAME_LENGTH);
	
	//Attach this to the head
	node->next_created = current_head;
	current_head = node;

	//And now we're done, we'll bail out
	return node;
}


/**
 * Add a directed connection between two nodes
 */
void add_dependency_node(dependency_tree_node_t* parent, dependency_tree_node_t* child){
	//Very easy case here, if there is no first child, then this is the first child
	if(parent->first_child == NULL){
		parent->first_child = child;
	}

	//Otherwise, we'll need to tack this on to the end
	 dependency_tree_node_t* cursor = parent->first_child;

	//So long as we aren't at the end here, we keep iterating
	while(cursor->next_sibling != NULL){
		cursor = cursor->next_sibling;
	}

	//Now once we get here, we can add it in
	cursor->next_sibling = child;
}


/**
 * Destroy the dependency tree by doing a depth first search
 */
void destroy_dependency_tree(){
	//We will run through the linked list of created nodes, freeing them
	//one-by-one. This memory management style allows us to free memory
	//in a low-computing cost way
	dependency_tree_node_t* temp;

	//Run through the creation list
	while(current_head != NULL){
		//Grab a temporary pointer
		temp = current_head;
		//Advance this up
		current_head = current_head->next_created;
		//Free the node
		free(temp);
	}
}

