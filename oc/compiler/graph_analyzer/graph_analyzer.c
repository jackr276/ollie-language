/**
 * Author: Jack Robbins
 * This C file contains the implementations for APIs defined inside of the header file
 * of the same name
 */

#include "graph_analyzer.h"
#include "../utils/queue/heap_queue.h"
#include <sys/types.h>

/**
 * Our "invalid" Lengauer-Tarjan DFS num is -1(sentinel value)
 */
#define LT_UNVISITED (-1)

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
 * Grab the immediate dominator of the block
 * A IDOM B if A SDOM B and there does not exist a node C 
 * such that C ≠ A, C ≠ B, A dom C, and C dom B
 *
 *
 *
 * TODO WE WILL REWORK THIS
 */
static basic_block_t* immediate_dominator(basic_block_t* B){
	//If we've already found the immediate dominator, why find it again?
	//We'll just give that back
	if(B->immediate_dominator != NULL){
		return B->immediate_dominator;
	}

	//Regular variable declarations
	basic_block_t* A; 
	basic_block_t* C;
	u_int8_t A_is_IDOM;
	
	//For each node in B's Dominance Frontier set(we call this node A)
	//These nodes are our candidates for immediate dominator
	//If B even has a dominator ser
	for(u_int16_t i = 0; i < B->dominator_set.current_index; i++){
		//By default we assume A is an IDOM
		A_is_IDOM = TRUE;

		//A is our "candidate" for possibly being an immediate dominator
		A = dynamic_array_get_at(&(B->dominator_set), i);

		//If A == B, that means that A does NOT strictly dominate(SDOM)
		//B, so it's disqualified
		if(A == B){
			continue;
		}

		//If we get here, we know that A SDOM B
		//Now we must check, is there any "C" in the way.
		//We can tell if this is the case by checking every other
		//node in the dominance frontier of B, and seeing if that
		//node is also dominated by A
		
		//For everything in B's dominator set that IS NOT A, we need
		//to check if this is an intermediary. As in, does C get in-between
		//A and B in the dominance chain
		for(u_int16_t j = 0; j < B->dominator_set.current_index; j++){
			//Skip this case
			if(i == j){
				continue;
			}

			//If it's aleady B or A, we're skipping
			C = dynamic_array_get_at(&(B->dominator_set), j);

			//If this is the case, disqualified
			if(C == B || C == A){
				continue;
			}

			//We can now see that C dominates B. The true test now is
			//if C is dominated by A. If that's the case, then we do NOT
			//have an immediate dominator in A.
			//
			//This would look like A -Doms> C -Doms> B, so A is not an immediate dominator
			if(dynamic_array_contains(&(C->dominator_set), A) != NOT_FOUND){
				//A is disqualified, it's not an IDOM
				A_is_IDOM = FALSE;
				break;
			}
		}

		//If we survived, then we're done here
		if(A_is_IDOM == TRUE){
			//Mark this for any future runs...we won't waste any time doing this
			//calculation over again
			B->immediate_dominator = A;
			return A;
		}
	}

	//Otherwise we didn't find it, so there is no immediate dominator
	return NULL;
}


/**
 * Initialize a block for idom computation. Remember that we may compute the immediate
 * dominators many times as the graph changes, so we *need* to ensure that we wipe
 * the slate clean every time
 */
static inline void initialize_block_for_idom_computation(basic_block_t* block){
	block->dominator_info.ancestor = NULL;
	block->dominator_info.idom = NULL;
	block->dominator_info.label = NULL;
	block->dominator_info.semidominator_number = LT_UNVISITED;
	block->dominator_info.dominator_parent = NULL;
	block->dominator_info.dfs_number = LT_UNVISITED;
}


/**
 * Perform the immediate dominator DFS traversal for a given
 * block. This traversal will assign the block it's given DFS
 * number, and will perform bookkeeping for the semidominator,
 * union-find ancestor, and label.
 *
 * The entire point of the DFS traversal is to assign every value
 * a DFS number. This DFS number will tell us how many hops away from the 
 * root node we are
 *
 * The main important mapping that we care about here is DFS number to basic block
 * because the semidominator set stores DFS numbers
 *
 * This function is recursive. The caller should only invoke this on the parent of the
 * entire graph(i.e. the function entry) and let it do the rest
 *
 * Procedure IDOM_DFS(block b):
 * 	b->dfs_number = current dfs number
 * 	number_to_vertex[current dfs number] = block
 * 	b->semidominator = current dfs number
 * 	b->label = b
 * 	b->ancestor = NULL
 *
 * 	current_dfs_number++
 *
 * 	foreach successor s of b:
 * 		if b->dfs_number == -1:
 * 			s->parent = b
 * 			IDOM_DFS(s)
 */
static void dfs_number_block(basic_block_t* block, basic_block_t** dfs_number_to_vertex_mapping, int32_t* current_dfs_number){
	//Assign this to be the current DFS number
	block->dominator_info.dfs_number = *current_dfs_number;

	//We'll also have a reverse mapping from number to block
	dfs_number_to_vertex_mapping[*current_dfs_number] = block;

	//Initialize the semidominator number to be this
	block->dominator_info.semidominator_number = *current_dfs_number;

	//The label here is our block(for now)
	block->dominator_info.label = block;

	//As of right now we don't have an ancestor so just make it NULL
	block->dominator_info.ancestor = NULL;


	//Bump the current number up
	(*current_dfs_number)++;

	/**
	 * Now for every successor of this block, we need to perform
	 * a DFS and do the parent bookkeeping
	 */
	for(int32_t i = 0; i < block->successors.current_index; i++){
		basic_block_t* successor = dynamic_array_get_at(&(block->successors), i);

		/**
		 * If our successor does not yet have a DFS number, then we'll need to 
		 * give it one now
		 */
		if(successor->dominator_info.dfs_number == LT_UNVISITED){
			//This successor's parent is the current block
			successor->dominator_info.dominator_parent = block;

			//Recursively call out to have this block populated
			dfs_number_block(successor, dfs_number_to_vertex_mapping, current_dfs_number);
		}
	}
}


/**
 * Simple helper to link the ancestor to it's descendant
 */
static inline void link_ancestor(basic_block_t* ancestor, basic_block_t* descendant){
	descendant->dominator_info.ancestor = ancestor;
}


/**
 * NOTE: This function operates on an entire function-level CFG, with the entry block
 * passed in. It will compute the immediate dominator for every single node in the
 * CFG in one run
 *
 * The immediate dominator of a vertex v, denoted IDOM(v), is the unique strict dominator
 * of v that is closest to v in the dominator tree
 *
 * For example:
 * 			A
 * 		  /   \
 * 		 B	   C
 *		  \	  /
 *		    D
 *		    |
 *		    E
 *
 * Even though E is dominated by A, D and E, it's immediate dominator is E because
 * it is the closes strict dominator
 *
 * The Ollie Compiler uses a version of the Lengauer-Tarjan algorithm to compute
 * immediate dominators, given just one node
 *
 * General idea:
 *  
 *  1. Perform a DFS to assign a DFS number to each block
 *  2. Perform a reverse DFS to compute the semidominator sets
 *  3. Perform a reverse DFS to compute the tentative immediate dominator
 *  4. Perform one final pass to repair non-trivial cases
 *
 *
 * procedure LT_IDOM(entry, block_set):
 * 	foreach block:
 * 		initalize block->dfs_number to -1(unvisited)
 *
 * 	dfs_number_block(entry)
 * 
 *
 * 	 
 *
 *
 * Property: Every node *except* the entry node has exactly one immediate dominator
 */
static void compute_immediate_dominators(basic_block_t* function_entry_block, dynamic_array_t* function_blocks){
	//The number of blocks is static
	u_int32_t number_of_blocks = function_blocks->current_index;

	/**
	 * Step 0: wipe every block's existing dominator info completely
	 * clean
	 */
	for(u_int32_t i = 0; i < number_of_blocks; i++){
		basic_block_t* block = dynamic_array_get_at(function_blocks, i);
		initialize_block_for_idom_computation(block);
	}

	/**
	 * Step 1: Run the DFS numbering algorithm to populate
	 * the DFS numbers for every block. This also
	 * initializes the "label", semidominator, and ancestor. This
	 * helper recursively does the whole thing so all that we
	 * need to do is call it and pass in the entry
	 */
	int32_t current_dfs_number = 0;
	basic_block_t** dfs_number_to_vertex_mapping = calloc(number_of_blocks, sizeof(basic_block_t*));
	dfs_number_block(function_entry_block, dfs_number_to_vertex_mapping, &current_dfs_number);






	//We're done with this now so release it
	free(dfs_number_to_vertex_mapping);
}


/**
 * The immediate postdominator is the first breadth-first 
 * successor that post dominates a node
 *
 *
 * TODO THIS ENTIRE THING IS GOING TO BE REWORKEDk
 */
static basic_block_t* immediate_postdominator(basic_block_t* B){
	//If we've already found the immediate dominator, why find it again?
	if(B->immediate_postdominator != NULL){
		return B->immediate_postdominator;
	}

	heap_queue_t traversal_queue = heap_queue_alloc();

	//The visited array
	dynamic_array_t visited = dynamic_array_alloc();

	//Save this for when we find it
	basic_block_t* ipdom = NULL;

	//Extract the postdominator set
	dynamic_array_t postdominator_set = B->postdominator_set;

	//Seed the search with B
	enqueue(&traversal_queue, B);

	//So long as the queue isn't empty
	while(queue_is_empty(&traversal_queue) == FALSE){
		//Pop off of the queue
		basic_block_t* current = dequeue(&traversal_queue);

		/**
		 * If we have found the first breadth-first successor that postdominates B,
		 * we are done
		 */
		if(current != B && dynamic_array_contains(&postdominator_set, current) != NOT_FOUND){
			ipdom = current;
			break;
		}

		//Add to the visited set
		dynamic_array_add(&visited, current);

		//Run through all successors
		for(u_int16_t j = 0; j < current->successors.current_index; j++){
			//Add the successor into the queue, if it has not yet been visited
			basic_block_t* successor = current->successors.internal_array[j];

			if(dynamic_array_contains(&visited, successor) == NOT_FOUND){
				enqueue(&traversal_queue, successor);
			}
		}
	}

	//Destroy visited
	dynamic_array_dealloc(&visited);

	//GOING TO BE FIXED
	heap_queue_dealloc(&traversal_queue);

	//Give it back
	return ipdom;
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
	for(u_int32_t i = 0; i < entry->predecessors.current_index; i++){
		reverse_post_order_traversal_reverse_cfg_rec(stack, dynamic_array_get_at(&(entry->predecessors), i));
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
 * Add a dominated block to the dominator block that we have
 */
static inline void add_dominated_block(basic_block_t* dominator, basic_block_t* dominated){
	//If this is NULL, then we'll allocate it right now
	if(dominator->dominator_children.internal_array == NULL){
		dominator->dominator_children = dynamic_array_alloc();
	}

	//If we do not already have this in the dominator children, then we will add it
	if(dynamic_array_contains(&(dominator->dominator_children), dominated) == NOT_FOUND){
		dynamic_array_add(&(dominator->dominator_children), dominated);
	}
}


/**
 * Build the dominator tree for a given function's blocks
 *
 * For each node in the function, we will use that node's immediate
 * dominator to build a dominator tree
 */
static inline void build_dominator_trees(dynamic_array_t* function_blocks){
	//Hold the current block
	basic_block_t* current;

	//For each block in the CFG
	for(int32_t _ = function_blocks->current_index - 1; _ >= 0; _--){
		//Grab out whatever block we're on
		current = dynamic_array_get_at(function_blocks, _);

		/**
		 * We will find this block's "immediate dominator". Once we have that,
		 * we will add this block to the "dominator children" set of said immediate
		 * dominator
		 */
		//TODO FIX - WE SHOULD JUST BE ABLE TO GRAB
		basic_block_t* immediate_dom = immediate_dominator(current);

		/**
		 * Now we'll go to the immediate dominator's list and add the dominated block in. Of course,
		 * we'll account for the case where there is no immediate dominator. This is possible in
		 * the case of function entry blocks
		 */
		if(immediate_dom != NULL){
			add_dominated_block(immediate_dom, current);
		}
	}
}


/**
 * Add a block to the dominance frontier of the first block
 */
static inline void add_block_to_dominance_frontier(basic_block_t* block, basic_block_t* df_block){
	//If the dominance frontier hasn't been allocated yet, we'll do that here
	if(block->dominance_frontier.internal_array == NULL){
		block->dominance_frontier = dynamic_array_alloc();
	}

	//Let's just check - is this already in there. If it is, we will not add it
	for(u_int32_t i = 0; i < block->dominance_frontier.current_index; i++){
		//This is not a problem at all, we just won't add it
		if(block->dominance_frontier.internal_array[i] == df_block){
			return;
		}
	}

	//Add this into the dominance frontier
	dynamic_array_add(&(block->dominance_frontier), df_block);
}


/**
 * Calculate the dominance frontiers of every block in the CFG
 *
 * The dominance frontier is every block, in relation to the current block, that:
 * 	Is a successor of a block that IS dominated by the current block
 * 		BUT
 * 	It itself is not dominated by the current block
 *
 * To think of it, it's essentially every block that is "just barely not dominated" by the current block
 *
 * Standard dominance frontier algorithm:
 * 	for all nodes b in the CFG
 * 		if b has less than 2 predecessors
 * 			continue
 * 		else
 * 			for all predecessors p of b
 * 				cursor = p
 * 				while cursor is not IDOM(b)
 * 					add b to cursor DF set
 * 					cursor = IDOM(cursor)
 * 	
 */
static inline void calculate_dominance_frontiers(dynamic_array_t* function_blocks){
	//Run through every block
	for(u_int32_t i = 0; i < function_blocks->current_index; i++){
		//Grab this from the array
		basic_block_t* block = dynamic_array_get_at(function_blocks, i);

		//If we have less than 2 successors,the rest of the search here is useless
		if(block->predecessors.internal_array == NULL || block->predecessors.current_index < 2){
			continue;
		}

		//A cursor for traversing our predecessors
		basic_block_t* cursor;

		//Now we run through every predecessor of the block
		for(u_int32_t i = 0; i < block->predecessors.current_index; i++){
			//Grab it out
			cursor = block->predecessors.internal_array[i];

			//While cursor is not the immediate dominator of block
			//TODO FIX - WE SHOULD JUST BE ABLE TO GRAB THE IDOM
			while(cursor != immediate_dominator(block)){
				//Add block to cursor's dominance frontier set
				add_block_to_dominance_frontier(cursor, block);
				
				/**
				 * Cursor now becomes it's own immediate dominator, and
				 * we crawl our way up the CFG
				 */
				//TODO FIX - WE SHOULD JUST BE ABLE TO GRAB THE IDOM
				cursor = immediate_dominator(cursor);
			}
		}
	}
}


/**
 * Calculate the postdominator sets for each and every node
 *
 * Routing postdominators
 * 	For each basic block
 * 		if block is exit then Pdom <- exit else pdom = all nodes
 *
 * while change = true
 * 	changed = false
 * 	for each Basic Block w/o exit block
 * 		temp = BB + {x | x elem of intersect of pdom(s) | s is a successor to B}
 *
 * 		if temp != pdom
 * 			pdom = temp
 * 			change = true
 *
 *
 * We'll be using a change watcher algorithm for this one. This algorithm will repeat until a stable solution
 * is found
 */
static void calculate_postdominator_sets(basic_block_t* function_entry_block, dynamic_array_t* function_blocks){
	basic_block_t* current;

	//Reset the function visited status
	reset_visit_status_for_function(function_blocks);

	//We'll first initialize everything here
	for(u_int32_t i = 0; i < function_blocks->current_index; i++){
		//Grab the block out
		current = dynamic_array_get_at(function_blocks, i);

		//If it's an exit block, then it's postdominator set just has itself
		if(current->block_type == BLOCK_TYPE_FUNC_EXIT){
			//If it's an exit block, then this set just contains itself
			current->postdominator_set = dynamic_array_alloc();
			//Add the block to it's own set
			dynamic_array_add(&(current->postdominator_set), current);

		} else {
			//If it's not an exit block, then we set this to be the entire body of blocks
			current->postdominator_set = clone_dynamic_array(function_blocks);
		}
	}

	//Copy over for our current function block
	basic_block_t* current_function_block = function_entry_block;

	//Have we seen a change
	u_int8_t changed;
	
	//Now we will go through everything in this blocks reverse post order set
	do {
		//By default, we'll assume there was no change
		changed = FALSE;

		//Now for each basic block in the reverse post order set
		for(u_int32_t _ = 0; _ < current_function_block->reverse_post_order.current_index; _++){
			//Grab the block out
			basic_block_t* current = dynamic_array_get_at(&(current_function_block->reverse_post_order), _);

			/**
			 * If it's the exit block, we don't need to bother with it. The exit block is always postdominated
			 * by itself
			 */
			if(current->block_type == BLOCK_TYPE_FUNC_EXIT){
				continue;
			}

			//The temporary array that we will use as a holder for this iteration's postdominator set 
			dynamic_array_t temp = dynamic_array_alloc();

			//The temp will always have this block in it
			dynamic_array_add(&temp, current);

			/**
			 * The temporary array also has the intersection of all of the successor's of BB's postdominator 
			 * sets in it. As such, we'll now compute those
			 */

			//If this block has any successors
			if(current->successors.internal_array != NULL){
				//Let's just grab out the very first successor
				basic_block_t* first_successor = dynamic_array_get_at(&(current->successors), 0);

				/**
				 * Now, if a node IS in the set of all successor's postdominator sets, it must be in here. As such, any node that
				 * is not in the set of all successors will NOT be in here, and every node that IS in the set
				 * of all successors WILL be. So, we can just run through this entire array and see if each node is 
				 * everywhere else
				 */
				if(first_successor->postdominator_set.internal_array != NULL){
					//For each node in the first one's postdominator set
					for(u_int32_t k = 0; k < first_successor->postdominator_set.current_index; k++){
						//Are we in the intersection of the sets? By default we think so
						u_int8_t in_intersection = TRUE;

						//Grab out the postdominator
						basic_block_t* postdominator = dynamic_array_get_at(&(first_successor->postdominator_set), k);

						//Now let's see if this postdominator is in every other postdominator set for the remaining successors
						for(u_int32_t l = 1; l < current->successors.current_index; l++){
							//Grab the successor out
							basic_block_t* other_successor = dynamic_array_get_at(&(current->successors), l);

							/**
							 * Now we'll check to see - is our given postdominator in this one's dominator set?
							 * If it isn't, we'll set the flag and break out. If it is we'll move on to the next one
							 */
							if(dynamic_array_contains(&(other_successor->postdominator_set), postdominator) == NOT_FOUND){
								//We didn't find it, set the flag and get out
								in_intersection = FALSE;
								break;
							}

							//Otherwise we did find it, so we'll keep going
						}

						/**
						 * By the time we make it here, we'll either have our flag set to true or false. If the postdominator
						 * made it to true, it's in the intersection, and will add it to the new set
						 */
						if(in_intersection == TRUE){
							dynamic_array_add(&temp, postdominator);
						}
					}
				}
			}

			//Let's compare the two dynamic arrays - if they aren't the same, we've found a difference
			if(dynamic_arrays_equal(&temp, &(current->postdominator_set)) == FALSE){
				//Set the flag
				changed = TRUE;

				//And we can get rid of the old one
				dynamic_array_dealloc(&(current->postdominator_set));
				
				//Set temp to be the new postdominator set
				current->postdominator_set = temp;

			//Otherwise they weren't changed, so the new one that we made has to go
			} else {
				dynamic_array_dealloc(&temp);
			}
		}

	} while(changed == TRUE);
}


/**
 * Add a block to the reverse dominance frontier of the first block
 */
static inline void add_block_to_reverse_dominance_frontier(basic_block_t* block, basic_block_t* rdf_block){
	//If the dominance frontier hasn't been allocated yet, we'll do that here
	if(block->reverse_dominance_frontier.internal_array == NULL){
		block->reverse_dominance_frontier = dynamic_array_alloc();
	}

	//Let's just check - is this already in there. If it is, we will not add it
	for(u_int32_t i = 0; i < block->reverse_dominance_frontier.current_index; i++){
		if(block->reverse_dominance_frontier.internal_array[i] == rdf_block){
			return;
		}
	}

	//Add this into the dominance frontier
	dynamic_array_add(&(block->reverse_dominance_frontier), rdf_block);
}


/**
 * Calculate the reverse dominance frontiers of every block in the CFG
 *
 * The reverse dominance frontier is every block, in relation to the current block, that:
 * 	Is a predecessor of a block that IS postdominated by the current block
 * 		BUT
 * 	It itself is not postdominated by the current block
 *
 * To think of it, it's essentially every block that is "just barely not postdominated" by the current block
 *
 * Standard reverse dominance frontier algorithm:
 * 	for all nodes b in the CFG
 * 		if b has less than 2 successors 
 * 			continue
 * 		else
 * 			for all successors p of b
 * 				cursor = p
 * 				while cursor is not IPDOM(b)
 * 					add b to cursor RDF set
 * 					cursor = IPDOM(cursor)
 * 	
 */
static inline void calculate_reverse_dominance_frontiers(dynamic_array_t* function_blocks){
	//Run through every block
	for(u_int32_t i = 0; i < function_blocks->current_index; i++){
		//Grab this from the array
		basic_block_t* block = dynamic_array_get_at(function_blocks, i);

		//If we have less than 2 successors,the rest of the search here is useless
		if(block->successors.internal_array == NULL || block->successors.current_index < 2){
			continue;
		}

		//A cursor for traversing our successors 
		basic_block_t* cursor;

		//Now we run through every successor of the block
		for(u_int32_t i = 0; i < block->successors.current_index; i++){
			cursor = block->successors.internal_array[i];

			//While cursor is not the immediate postdominator of block
			//TODO FIX - WE SHOULD JUST BE ABLE TO GRAB THE IPDOM
			while(cursor != immediate_postdominator(block)){
				//Add block to cursor's reverse dominance frontier set
				add_block_to_reverse_dominance_frontier(cursor, block);
				
				/**
				 * Cursor now becomes it's own immediate postdominator, and
				 * we crawl our way down the CFG
				 */
				//TODO FIX - WE SHOULD JUST BE ABLE TO GRAB THE IPDOM
				cursor = immediate_postdominator(cursor);
			}
		}
	}
}

/**
 * Special exposes post order traversal API. The postorder traversal is needed
 * specifically in branch reduction in the optimizer/postprocessor. In this case,
 * we'll need a pre-allocated dynamic array to be passed in
 */
void get_post_order_traversal(basic_block_t* function_entry_block, dynamic_array_t* post_order_traversal){
	post_order_traversal_rec(post_order_traversal, function_entry_block);
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

	//
	//
	//
	//TODO IDOM GOES HERE
	//
	//JUST FOR TESTING CURRENTLY HAS NO EFFECT
	//
	//
	//
	compute_immediate_dominators(function_entry_block, function_blocks);
	

	//We'll now use the immediate dominator to construct our dominator trees
	build_dominator_trees(function_blocks);

	//Once we have the dominator tree, we can compute the dominance frontier
	calculate_dominance_frontiers(function_blocks);

	//Calculate the postdominator sets for analysis
	calculate_postdominator_sets(function_entry_block, function_blocks);

	//And the reverse dominance frontier(needed for branch ops)
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

		//Reset both of these as they will need to be recomputed
		block->immediate_dominator = NULL;
		block->immediate_postdominator = NULL;

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
