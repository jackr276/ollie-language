/**
 * Author: Jack Robbins
 * This C file contains the implementations for APIs defined inside of the header file
 * of the same name
 */

#include "graph_analyzer.h"
#include <sys/types.h>


/**
 * Run through an entire array of function blocks and reset the status for
 * every single one. We assume that the caller knows what they are doing, and
 * that the blocks inside of the array are really the correct blocks
 */
static inline void reset_visit_status_for_function(dynamic_array_t* function_blocks){
	//Run through all of the blocks
	for(u_int32_t i = 0; i < function_blocks->current_index; i++){
		//Extract the current block
		basic_block_t* current = dynamic_array_get_at(function_blocks, i);

		//Flag it as false
		current->visited = FALSE;
	}
}


/**
 * We'll go through in the regular traversal, pushing each node onto the stack in
 * postorder. 
 */
static void reverse_post_order_traversal_reverse_cfg_rec(heap_stack_t* stack, basic_block_t* entry){
	//If we've already seen this then we're done
	if(entry->visited == TRUE){
		return;
	}

	//Mark it as visited
	entry->visited = TRUE;

	//For every child(predecessor-it's reverse), we visit it as well
	for(u_int16_t _ = 0; _ < entry->predecessors.current_index; _++){
		//Visit each of the blocks
		reverse_post_order_traversal_reverse_cfg_rec(stack, dynamic_array_get_at(&(entry->predecessors), _));
	}

	//Now we can push entry onto the stack
	push(stack, entry);
}


/**
 * Get and return a reverse post order traversal of a function-level CFG where
 * we are going in reverse order. This is used mainly for data flow(liveness)
 */
static dynamic_array_t compute_reverse_post_order_traversal_reverse_cfg(basic_block_t* entry){
	//For our postorder traversal
	heap_stack_t stack = heap_stack_alloc();
	//We'll need this eventually for postorder
	dynamic_array_t reverse_post_order_traversal = dynamic_array_alloc();

	//Go all the way to the bottom
	while(entry->block_type != BLOCK_TYPE_FUNC_EXIT){
		entry = entry->direct_successor;
	}

	//Invoke the recursive helper
	reverse_post_order_traversal_reverse_cfg_rec(&stack, entry);

	/**
	 * Now we'll pop everything off of the stack, and put it onto the RPO 
	 * array in backwards order
	 */
	while(heap_stack_is_empty(&stack) == FALSE){
		dynamic_array_add(&reverse_post_order_traversal, pop(&stack));
	}

	//And when we're done, get rid of the stack
	heap_stack_dealloc(&stack);

	//Give back the reverse post order traversal
	return reverse_post_order_traversal;
}


/**
 * We'll go through in the regular traversal, pushing each node onto the stack in
 * postorder. 
 */
static void reverse_post_order_traversal_rec(heap_stack_t* stack, basic_block_t* entry){
	//If we've already seen this then we're done
	if(entry->visited == TRUE){
		return;
	}

	//Mark it as visited
	entry->visited = TRUE;

	//For every child(successor), we visit it as well
	for(u_int32_t i = 0; i < entry->successors.current_index; i++){
		//Visit each of the blocks
		reverse_post_order_traversal_rec(stack, dynamic_array_get_at(&(entry->successors), i));
	}

	//Now we can push entry onto the stack
	push(stack, entry);
}


/**
 * Get and return a reverse-post order traversal for a function level CFG
 */
dynamic_array_t compute_reverse_post_order_traversal(basic_block_t* entry){
	//For our postorder traversal
	heap_stack_t stack = heap_stack_alloc();
	//We'll need this eventually for postorder
	dynamic_array_t reverse_post_order_traversal = dynamic_array_alloc();

	//Invoke the recursive helper
	reverse_post_order_traversal_rec(&stack, entry);

	/**
	 * Now we'll pop everything off of the stack, and put it onto the RPO 
	 * array in backwards order
	 */
	while(heap_stack_is_empty(&stack) == FALSE){
		dynamic_array_add(&reverse_post_order_traversal, pop(&stack));
	}

	//And when we're done, get rid of the stack
	heap_stack_dealloc(&stack);

	//Give back the reverse post order traversal
	return reverse_post_order_traversal;
}


/**
 * A recursive post order simplifies the code, so it's what we'll use here
 */
static void post_order_traversal_rec(dynamic_array_t* post_order_traversal, basic_block_t* entry){
	//If we've visited this one before, skip
	if(entry->visited == TRUE){
		return;
	}

	//Otherwise mark that we've visited
	entry->visited = TRUE;

	//Run through every successor
	for(u_int32_t i = 0; i < entry->successors.current_index; i++){
		//Recursive call to every child first
		post_order_traversal_rec(post_order_traversal, dynamic_array_get_at(&(entry->successors), i));
	}
	
	//Now we'll finally visit the node
	dynamic_array_add(post_order_traversal, entry);
}


/**
 * Get and return the regular postorder traversal for a function-level CFG
 *
 * NOTE: This assumes that the caller has already wiped the function's visited
 * status clean
 */
static dynamic_array_t compute_post_order_traversal(basic_block_t* entry){
	//Create our dynamic array
	dynamic_array_t post_order_traversal = dynamic_array_alloc();

	//Make the recursive call
	post_order_traversal_rec(&post_order_traversal, entry);

	//Give the traversal back
	return post_order_traversal;
}


/**
 * Calculate all reverse traversals for a given function. A reverse traversal is simply a traversal on the graph
 * where every successor is a predecessor, and every predecessor is a successor. This is needed for the postdominance
 * computation
 */
static inline void calculate_all_reverse_traversals(basic_block_t* function_entry_block, dynamic_array_t* function_blocks){
	//Set the RPO to be null
	if(function_entry_block->reverse_post_order.internal_array != NULL){
		dynamic_array_dealloc(&(function_entry_block->reverse_post_order));
	}

	//Set the RPO reverse CFG to be null
	if(function_entry_block->reverse_post_order_reverse_cfg.internal_array != NULL){
		dynamic_array_dealloc(&(function_entry_block->reverse_post_order_reverse_cfg));
	}

	//Reset the function visited status
	reset_visit_status_for_function(function_blocks);

	//Compute the reverse post order traversal
	function_entry_block->reverse_post_order = compute_post_order_traversal(function_entry_block);

	//Reset the function visited status
	reset_visit_status_for_function(function_blocks);

	//Now use the reverse CFG(successors are predecessors, and vice versa)
	function_entry_block->reverse_post_order_reverse_cfg = compute_reverse_post_order_traversal_reverse_cfg(function_entry_block);
}


/**
 * Calculate the dominator sets for each and every node
 *
 * For each node in the nodeset:
 * 	dom(N) <- All nodes
 *	
 * Worklist = {StartNode}
 * while worklist is not empty
 * 	Remove any node Y from Worklist
 * 	New = {Y} U {X | X elem Pred(Y)}
 *
 * 	if new != dom 
 * 		Dom(Y) = New
 * 		For each successor X of Y
 * 			add X to the worklist
 *
 * This algorithm repeats indefinitely UNTIL a stable solution
 * is found(this is when new == DOM for every node, hence there's nowhere
 * left to go)
 *
 * NOTE: We repeat this for each and every function in the CFG. If blocks aren't in
 * the same function, then their dominance is completely unrelated
 */
static void calculate_dominator_sets(basic_block_t* function_entry_block, dynamic_array_t* function_blocks){
	/**
	 * Every node in the CFG has a dominator set that is set
	 * to be identical to the list of all nodes
	 */
	for(u_int32_t i = 0; i < function_blocks->current_index; i++){
		basic_block_t* block = dynamic_array_get_at(function_blocks, i);

		/**
		 * We will initialize the block's dominator set to be the entire set of nodes
		 * in the given function
		 */
		block->dominator_set = clone_dynamic_array(function_blocks);
	}

	//Initialize a "worklist" dynamic array for this particular function
	dynamic_array_t worklist = dynamic_array_alloc();

	//Add this into the worklist as a seed
	dynamic_array_add(&worklist, function_entry_block);
	
	//The new dominance frontier that we have each time
	dynamic_array_t new;

	//So long as the worklist is not empty
	while(dynamic_array_is_empty(&worklist) == FALSE){
		//Remove a node Y from the worklist(remove from back - most efficient{O(1)})
		basic_block_t* Y = dynamic_array_delete_from_back(&worklist);
		
		//Create the new dynamic array that will be used for the next dominator set
		new = dynamic_array_alloc();

		//We will add Y into it's own dominator set
		dynamic_array_add(&new, Y);

		//If Y has predecessors, we will find the intersection of their dominator sets
		if(Y->predecessors.internal_array != NULL){
			//Grab the very first predecessor's dominator set
			dynamic_array_t pred_dom_set = ((basic_block_t*)(Y->predecessors.internal_array[0]))->dominator_set;

			//Are we in the intersection of the dominator sets?
			u_int8_t in_intersection;

			//We will now search every item in this dominator set
			for(u_int32_t i = 0; i < pred_dom_set.current_index; i++){
				//Grab the dominator out
				basic_block_t* dominator = dynamic_array_get_at(&pred_dom_set, i);

				/**
				 * By default we assume that this given dominator is in the set. If it
				 * isn't we'll set it appropriately
				 */
				in_intersection = TRUE;

				/**
				 * An item is in the intersection if and only if it is contained 
				 * in all of the dominator sets of the predecessors of Y
				 *
				 * We'll start at 1 here - we've already accounted for 0
				*/
				for(u_int32_t j = 1; j < Y->predecessors.current_index; j++){
					//Grab our other predecessor
					basic_block_t* other_predecessor = Y->predecessors.internal_array[j];

					/**
					 * Now we will go over this predecessor's dominator set, and see if "dominator"
					 * is also contained within it
					 */

					//Let's check for it in here. If we can't find it, we set the flag to false and bail out
					if(dynamic_array_contains(&(other_predecessor->dominator_set), dominator) == NOT_FOUND){
						in_intersection = FALSE;
						break;
					}
				
					/**
					 * Otherwise we did find it, so we'll look at the next predecessor, and see if it is also
					 * in there. If we get to the end and "in_intersection" is true, then we know that we've
					 * found this one dominator in every single set
					 */
				}

				if(in_intersection == TRUE){
					//Add the dominator in
					dynamic_array_add(&new, dominator);
				}
			}
		}

		//Now we'll check - are these two dominator sets the same? If not, we'll need to update them
		if(dynamic_arrays_equal(&new, &(Y->dominator_set)) == FALSE){
			//Destroy the old one
			dynamic_array_dealloc(&(Y->dominator_set));

			//And replace it with the new
			Y->dominator_set = new;

			//Now for every successor of Y, add it into the worklist
			for(u_int32_t i = 0; i < Y->successors.current_index; i++){
				dynamic_array_add(&worklist, Y->successors.internal_array[i]);
			}

		//Otherwise they are the same, so destroy the one that we just made
		} else {
			dynamic_array_dealloc(&new);
		}
	}

	//Destroy the worklist now that we're done with it
	dynamic_array_dealloc(&worklist);
}


/**
 * We will calculate:
 *  1.) Reverse post order traversals
 *  2.) Dominator Sets
 *  3.) Dominator Trees
 *  4.) Dominance Frontiers
 *  5.) Postdominator sets
 *  6.) Reverse Dominance frontiers
 *
 * For every block in the given function. This externally facing API hides all of
 * the complexity behind it
 */
void calculate_all_control_flow_relations_for_function(basic_block_t* function_entry_block, dynamic_array_t* function_blocks){
	//Before any calculation can be done, we need to compute every single reverse traversal
	calculate_all_reverse_traversals(function_entry_block, function_blocks);
	
	//Now calculate the dominator set for every function block
	calculate_dominator_sets(function_entry_block, function_blocks);

	//Now we'll build the dominator tree up
	//TODO DOC
	build_dominator_trees(function_blocks);

	//Now calculate the dominance frontier for every single block
	//TODO DOC
	calculate_dominance_frontiers(function_blocks);

	//Calculate the postdominator sets for later analysis in the optimizer
	//TODO DOC
	calculate_postdominator_sets(function_entry_block, function_blocks);

	//We'll also now calculate the reverse dominance frontier that will be used
	//in later analysis by the optimizer
	//TODO DOC
	calculate_reverse_dominance_frontiers(function_blocks);
}


/**
 * Destroy all old control relations in anticipation of new ones coming in. This 
 * operates on a per-function level
 */
void cleanup_all_control_relations(dynamic_array_t* function_blocks){
	//For each block in the CFG
	for(u_int32_t i = 0; i < function_blocks->current_index; i++){
		//Grab the block out
		basic_block_t* block = dynamic_array_get_at(function_blocks, i);

		if(block->postdominator_set.internal_array != NULL){
			dynamic_array_dealloc(&(block->postdominator_set));
		}

		if(block->dominator_set.internal_array != NULL){
			dynamic_array_dealloc(&(block->dominator_set));
		}

		if(block->dominator_children.internal_array != NULL){
			dynamic_array_dealloc(&(block->dominator_children));
		}

		if(block->dominance_frontier.internal_array != NULL){
			dynamic_array_dealloc(&(block->dominance_frontier));
		}

		if(block->reverse_dominance_frontier.internal_array != NULL){
			dynamic_array_dealloc(&(block->reverse_dominance_frontier));
		}

		if(block->reverse_post_order_reverse_cfg.internal_array != NULL){
			dynamic_array_dealloc(&(block->reverse_post_order_reverse_cfg));
		}

		if(block->reverse_post_order.internal_array != NULL){
			dynamic_array_dealloc(&(block->reverse_post_order));
		}
	}
}
