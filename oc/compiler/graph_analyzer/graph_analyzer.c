/**
 * Author: Jack Robbins
 * This C file contains the implementations for APIs defined inside of the header file
 * of the same name
 */

#include "graph_analyzer.h"
#include <stdint.h>
#include <sys/types.h>

/**
 * Our "invalid" Lengauer-Tarjan DFS num is -1(sentinel value)
 */
#define LT_UNNUMBERED (-1)

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
 * Initialize a block for idom computation. Remember that we may compute the immediate
 * dominators many times as the graph changes, so we *need* to ensure that we wipe
 * the slate clean every time
 */
static inline void initialize_block_for_idom_computation(basic_block_t* block){
	block->dominator_info.ancestor = NULL;
	block->dominator_info.immediate_dominator = NULL;
	block->dominator_info.optimal_candidate = NULL;
	block->dominator_info.semidominator_number = LT_UNNUMBERED;
	block->dominator_info.parent = NULL;
	block->dominator_info.dfs_number = LT_UNNUMBERED;

	/**
	 * If we already have a dynamic array we'll just wipe it, otherwise
	 * we've got to allocate
	 */
	if(block->dominator_info.worklist.internal_array != NULL){
		clear_dynamic_array(&(block->dominator_info.worklist));
	} else {
		block->dominator_info.worklist = dynamic_array_alloc();
	}
}


/**
 * Initialize a block for immediate postdominator computation. Remember that we may compute the immediate
 * dominators many times as the graph changes, so we *need* to ensure that we wipe
 * the slate clean every time
 *
 * NOTE: this *must* be used for ipdom calculation because it does not clear the IDOM
 */
static inline void initialize_block_for_ipdom_computation(basic_block_t* block){
	block->dominator_info.ancestor = NULL;
	block->dominator_info.immediate_postdominator = NULL;
	block->dominator_info.optimal_candidate = NULL;
	block->dominator_info.semidominator_number = LT_UNNUMBERED;
	block->dominator_info.parent = NULL;
	block->dominator_info.dfs_number = LT_UNNUMBERED;

	/**
	 * If we already have a dynamic array we'll just wipe it, otherwise
	 * we've got to allocate
	 */
	if(block->dominator_info.worklist.internal_array != NULL){
		clear_dynamic_array(&(block->dominator_info.worklist));
	} else {
		block->dominator_info.worklist = dynamic_array_alloc();
	}
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

	//By default our optimal candidate so far is just this block
	block->dominator_info.optimal_candidate = block;

	//As of right now we don't have a union-find ancestor so just make it NULL
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
		if(successor->dominator_info.dfs_number == LT_UNNUMBERED){
			//Simple parent is just this block
			successor->dominator_info.parent = block;

			//Recursively call out to have this block populated
			dfs_number_block(successor, dfs_number_to_vertex_mapping, current_dfs_number);
		}
	}
}


/**
 * Perform the immediate postdominator DFS traversal for a given
 * block. This traversal will assign the block it's given reverse DFS
 * number, and will perform bookkeeping for the semipostdominator,
 * union-find postdominator ancestor, and label.
 *
 * The entire point of the reverse DFS traversal is to assign every value
 * a reverse DFS number. This reverse DFS number will tell us how many hops away from the 
 * exit node we are
 *
 * The main important mapping that we care about here is reverse DFS number to basic block
 * because the semidominator set stores DFS numbers
 *
 * This function is recursive. The caller should only invoke this on the parent of the
 * entire graph(i.e. the function entry) and let it do the rest
 *
 * Procedure IDOM_REVERSE_DFS(block b):
 * 	b->dfs_number = current dfs number
 * 	number_to_vertex[current dfs number] = block
 * 	b->semidominator = current dfs number
 * 	b->label = b
 * 	b->ancestor = NULL
 *
 * 	current_dfs_number++
 *
 * 	foreach predecessor p of b:
 * 		if b->dfs_number == -1:
 * 			p->parent = b
 * 			IDOM_REVERSE_DFS(p)
 */
static void reverse_dfs_number_block(basic_block_t* block, basic_block_t** reverse_dfs_number_to_vertex_mapping, int32_t* current_reverse_dfs_number){
	//Assign this to be the current DFS number
	block->dominator_info.dfs_number = *current_reverse_dfs_number;

	//We'll also have a reverse mapping from number to block
	reverse_dfs_number_to_vertex_mapping[*current_reverse_dfs_number] = block;

	//Initialize the semidominator number to be this
	block->dominator_info.semidominator_number = *current_reverse_dfs_number;

	//By default our optimal candidate so far is just this block
	block->dominator_info.optimal_candidate = block;

	//As of right now we don't have a union-find ancestor so just make it NULL
	block->dominator_info.ancestor = NULL;

	//Bump the current number up
	(*current_reverse_dfs_number)++;

	/**
	 * Now for every predecessor of this block, we need to perform
	 * a reverse DFS and do the parent bookkeeping
	 */
	for(int32_t i = 0; i < block->predecessors.current_index; i++){
		basic_block_t* predecessor = dynamic_array_get_at(&(block->predecessors), i);

		/**
		 * If our predecessor does not yet have a DFS number, then we'll need to 
		 * give it one now
		 */
		if(predecessor->dominator_info.dfs_number == LT_UNNUMBERED){
			//Simple parent is just this block
			predecessor->dominator_info.parent = block;

			//Recursively call out to have this block populated
			reverse_dfs_number_block(predecessor, reverse_dfs_number_to_vertex_mapping, current_reverse_dfs_number);
		}
	}
}


/**
 * Idea of the path_compression:
 * """
 * 	walk from the block towards the root of the tree recursively
 *
 * 	while walking:
 * 		remember the node on our path with the smallest semidominator
 *
 * 	rewire the ancestor pointers so that our next walk is shorter
 * """
 *  
 * It helps to imagine this as us taking a block, recursively compressing
 * everything above it, and then doing two things: updating the block's "label"
 * which cache's the best semidominator candidate along the path from the block
 * to the root *and* compressing this block's path to the root by replacing it's
 * ancestor pointer with the ancestor of its ancestor
 *
 *
 * Algorithm path_compression(block)
 * 	if block has no ancestor:
 * 		return
 *
 * 	if block's ancestor has no ancestor:
 * 		return
 *
 * 	path_compression(ancestor)
 *
 * 	if(ancestor's cahced semidom number < block's cached semidom number){
 * 		replace block's cached semidom with the ancestors
 * 	}
 *
 * 	set block's ancestor to be its ancestor's ancestor(path compression)
 *
 * This is reusable for immediate dominator and immediate postdominator
 * calculation, it's just context-dependant as to what the ancestor is
 *
 * NOTE: This is a recursive function
 */
static void path_compression(basic_block_t* block){
	//Extract the current ancestor for our block
	basic_block_t* ancestor = block->dominator_info.ancestor;

	/**
	 * Base case 1: we have no ancestor so we bail out
	 */
	if(ancestor == NULL){
		return;
	}

	/**
	 * Base case 2: the ancestor itself has no ancestor, so we
	 * also bail out
	 */
	if(ancestor->dominator_info.ancestor == NULL){
		return;
	}

	/**
	 * Recursively compress the ancestor. We can think
	 * of this as collapsing everything higher than us
	 * recursively until we get down to this block
	 */
	path_compression(ancestor);

	/**
	 * Extract two values:
	 * 	The ancestor holds onto what we consider to be the "best" semidominator on our path as of right now
	 * 	The block contains what it believes the "best" semidominator is
	 *
	 * 	If the ancestor has a "better"(smaller DFS number) semidominator, then we will replace
	 * 	what the block thinks is the the best candidate with what we know the best semidominator
	 * 	candidate to be 
	 *
	 * This is how we "remember" what the best semidominator candidate is along our path for use down 
	 * the road
	 */
	int32_t ancestor_semidominator = ancestor->dominator_info.optimal_candidate->dominator_info.semidominator_number;
	int32_t block_semidominator = block->dominator_info.optimal_candidate->dominator_info.semidominator_number;

	if(ancestor_semidominator < block_semidominator){
		block->dominator_info.optimal_candidate = ancestor->dominator_info.optimal_candidate;
	}

	/**
	 * Path compression: Update the block's current ancestor with
	 * what comes before this ancestor. This makes all future
	 * walks between the block and it's ancestor much shorter
	 */
	block->dominator_info.ancestor = ancestor->dominator_info.ancestor;
}


/**
 * Evaluate does the following: given a block b, return the node on b's
 * current ancestor path that has the smallest semidominator
 *
 * Our path compression helper stores the "best_semi" pointer for exactly
 * this reason
 *
 * This is reusable for immediate dominator and immediate postdominator
 * calculation, it's just context-dependant as to what the ancestor is
 */
static inline basic_block_t* evaluate(basic_block_t* block){
	/**
	 * If we have no ancestor then path compression is useless
	 * anyways, we'll just return our current best guess
	 */
	if(block->dominator_info.ancestor == NULL){
		return block->dominator_info.optimal_candidate;
	}

	/**
	 * Run the path compressor to walk the graph towards the root
	 * and update our best semidominator
	 */
	path_compression(block);

	//Give back the optimal candidate that we've found during compression
	return block->dominator_info.optimal_candidate;
}


/**
 * Simple helper to link the union-find ancestor to it's descendant
 *
 * This is reusable for immediate dominator and immediate postdominator
 * calculation, it's just context-dependant as to what the ancestor is
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
 *  2. Iterate over DFS in reverse to:
 *  		 a. compute the semidominator sets
 *  		 b. compute the tentative immediate dominator
 *
 *  4. Perform one final pass to repair non-trivial cases(where IDOM != semidominator)
 *
 * The DFS numbers are very important because we need to map semidominators to DFS numbers for lookup speed
 *
 *
 * procedure LT_IDOM(entry, block_set):
 * 	foreach block:
 * 		clear the block's dominator info
 *
 * 	dfs_number_block(entry)
 *
 *  foreach block w in reverse DFS order:
 * 		foreach predecessor p of the block:
 * 			if predecessor has an unreachable DFS number:
 * 				continue
 *
 * 			declare candidate
 *
 * 			if p's DFS num < w's dfs num:
 * 				candidate = p
 * 			else:
 * 				candidate = evaluate(p)
 *
 * 			if candidate's semi # < w's semi #:
 * 				replace w's semi # with the candidate's
 *
 * 		add block into it's semidominator's bucket
 *
 * 		get the parent of block
 *
 *		link_ancestor(parent, block)
 *
 *		foreach block b in the parent's bucket:
 *			candidate = evaluate(b)
 *
 *			if candidate.semi # < b.semi #:
 *				set b's IDOM to be candidate
 *			else:
 *				set b's IDOM to be parent
 *
 *	foreach block b:
 *		if b's IDOM == b's semidominator:
 *			set b's IDOM to be IDOM(IDOM(b))
 *
 * Property: Every node *except* the entry node has exactly one immediate dominator
 */
static void compute_immediate_dominators(basic_block_t* function_entry_block, dynamic_array_t* function_blocks){
	//The number of blocks is static
	const u_int32_t number_of_blocks = function_blocks->current_index;

	/**
	 * Wipe every block's existing dominator info completely clean
	 */
	for(u_int32_t i = 0; i < number_of_blocks; i++){
		basic_block_t* block = dynamic_array_get_at(function_blocks, i);
		initialize_block_for_idom_computation(block);
	}

	/**
	 * Run the DFS numbering algorithm to populate
	 * the DFS numbers for every block. This also
	 * initializes the "label", semidominator, and ancestor. This
	 * helper recursively does the whole thing so all that we
	 * need to do is call it and pass in the entry
	 *
	 * NOTE: While we could use C's VLA allocation to make this a stack array, we don't
	 * know how big a function can get and we don't want to risk it. Using calloc()
	 * here is much safer with more blocks and it's not much more expensive
	 */
	int32_t current_dfs_number = 0;
	basic_block_t** dfs_number_to_vertex_mapping = calloc(number_of_blocks, sizeof(basic_block_t*));
	dfs_number_block(function_entry_block, dfs_number_to_vertex_mapping, &current_dfs_number);

	/**
	 * Now that we have the DFS numbers in place, we will run through
	 * the blocks in reverse DFS order so that we are dealing with
	 * the deepest blocks first
	 */
	for(int32_t i = current_dfs_number - 1; i > 0; i--){
		//Extract the block that we're working on
		basic_block_t* working_block = dfs_number_to_vertex_mapping[i];

		/**
		 * Compute the semidominators for each given block. Remember that semidominators
		 * are the earliest DFS ancestor that can "almost" dominate a given block
		 */
		for(u_int32_t j = 0; j < working_block->predecessors.current_index; j++){
			//Our semidominator candidate
			basic_block_t* candidate = NULL;
			basic_block_t* predecessor = dynamic_array_get_at(&(working_block->predecessors), j);

			/**
			 * Just something to account for - if this is somehow unreachable
			 * we skip ahead. This in theory should never happen but we don't
			 * want to go down this road if it does so best to be safe
			 */
			if(predecessor->dominator_info.dfs_number == LT_UNNUMBERED){
				continue;
			}

			/**
			 * If the predecessor's DFS number is higher up(smaller number) than the working block's,
			 * then the predecessor is going to be our candidate for this. Otherwise, we'll
			 * run evaluate on the predecessor which will walk the graph, compress the path,
			 * and return what it thinks the best semidominator is
			 */
			if(predecessor->dominator_info.dfs_number < working_block->dominator_info.dfs_number){
				candidate = predecessor;
			} else {
				candidate = evaluate(predecessor);
			}
			
			/**
			 * If this candidate has a superior(lower DFS numbered) semidominator, then we will
			 * replace the one that we currently have in the working block with it's number
			 */
			if(candidate->dominator_info.semidominator_number < working_block->dominator_info.semidominator_number){
				working_block->dominator_info.semidominator_number = candidate->dominator_info.semidominator_number;
			}
		}

		/**
		 * Now that we have what we think is the best semidominator number stored, we will
		 * add the working block into our processing bucket for this given semidominator block.
		 * First we'll need to use the DFS number to block mapping to get the actual semidominator
		 */
		basic_block_t* semidominator = dfs_number_to_vertex_mapping[working_block->dominator_info.semidominator_number];

		/**
		 * Add the working block into the semidominator's worklist
		 *
		 * Remember: the "worklist" of the semidominator "x" contains all blocks
		 * whose semidominator is "x"
		 */
		dynamic_array_add(&(semidominator->dominator_info.worklist), working_block);

		//We'll need the parent p of this block going forward
		basic_block_t* dominator_parent = working_block->dominator_info.parent;

		/**
		 * The parent of the working block is it's union-find ancestor
		 */
		link_ancestor(dominator_parent, working_block);

		/**
		 * By the time we've reached here, we have enough information
		 * to determine the *potential* IDOMs for all node's whose semidominator
		 * is our working block's parent.
		 *
		 * The worklist is essentially a deferred work queue that allows our algorithm
		 * to postpone any/all IDOM processing until the ancestor structure
		 * has enough info to make the decision
		 *
		 * Our core theory when processing a bucket is: The immediate
		 * dominator of a block is either it's semidominator *or* some
		 * dominator above said semidominator. *At this moment, we do
		 * not know which one it is*. This is why a final correction pass is
		 * needed after this. This only computes a provisional(or potential/best guess)
		 * IDOM
		 */
		dynamic_array_t* parent_worklist = &(dominator_parent->dominator_info.worklist);
		for(u_int32_t k = 0; k < parent_worklist->current_index; k++){
			/**
			 * Extract our bucket block and use evaluate to perform path compression
			 * and get compute the smallest semidominator number along this path
			 */
			basic_block_t* semidominated_block = dynamic_array_get_at(parent_worklist, k);
			basic_block_t* candidate = evaluate(semidominated_block);

			/**
			 * If the candidate post evaluation has a smaller semidominator number than
			 * the bucket block, we will set the candidate as this block's IDOM. Otherwise,
			 * we will set this block's IDOM to be the parent block
			 */
			if(candidate->dominator_info.semidominator_number < semidominated_block->dominator_info.semidominator_number){
				semidominated_block->dominator_info.immediate_dominator = candidate;
			} else {
				semidominated_block->dominator_info.immediate_dominator = dominator_parent;
			}
		}

		//Clear out the bucket now that we've processed
		clear_dynamic_array(parent_worklist);
	}

	/**
	 * Even though we've processed every block, we still need a correction
	 * pass here.
	 *
	 * Lengauer-Tarjan proves:
	 * 	if idom(v) != semi(v):
	 * 		idom(v) = idom(idom(v))
	 *
	 * In other words: "If the candidate is not the semidominator, then the true
	 * immediate dominator is the immediate dominator of the candidate"
	 */
	for(int32_t i = 1; i < current_dfs_number; i++){
		//Get the block to work on
		basic_block_t* working_block = dfs_number_to_vertex_mapping[i];

		//Extract this block's semidominator
		basic_block_t* semidominator = dfs_number_to_vertex_mapping[working_block->dominator_info.semidominator_number];

		/**
		 * If the IDOM is not the semidominator, then the real IDOM is
		 * the immediate dominator of said candidate
		 */
		if(working_block->dominator_info.immediate_dominator != semidominator){
			working_block->dominator_info.immediate_dominator = working_block->dominator_info.immediate_dominator->dominator_info.immediate_dominator;
		}
	}

	//The function entry block itself never has an immediate dominator
	function_entry_block->dominator_info.immediate_dominator = NULL;

	//We're done with this now so release it
	free(dfs_number_to_vertex_mapping);
}



/**
 * NOTE: This function operates on an entire function-level CFG, with the entry block
 * passed in. It will compute the immediate dominator for every single node in the
 * CFG in one run
 *
 * The immediate postdominator of a vertex v, denoted IPDOM(v), is the unique strict postdominator
 * of v that is closest to v in the postdominator tree
 *
 * For example:
 * 			A
 * 		  /   \
 * 		 B	   C
 *		  \	  /
 *		    D
 *		    |
 *		    F
 *		    |
 *		    E
 *
 * The immediate postdominator of A is D, because D is the closest Node that A
 * must pass through to reach the exit
 *
 * The Ollie Compiler uses a version of the Lengauer-Tarjan algorithm to compute
 * immediate postdominators, given just one node
 *
 * General idea:
 *  
 *  1. Perform a reverse DFS(pred = succ) to assign a reverse DFS number to each block
 *  2. Iterate over reverse DFS in reverse to:
 *  		 a. compute the semipostdominator sets
 *  		 b. compute the tentative immediate postdominator
 *
 *  4. Perform one final pass to repair non-trivial cases(where IPDOM != semipostdominator)
 *
 * The reverse DFS numbers are very important because we map semipostdominators to reverse DFS numbers for lookup speed
 *
 * procedure LT_IPDOM(entry, block_set):
 * 	foreach block:
 * 		clear the block's dominator info
 *
 * 	reverse_dfs_number_block(entry)
 *
 *  foreach block w in reverse of reverse DFS order:
 * 		foreach successor s of the block:
 * 			if s has an unreachable DFS number:
 * 				continue
 *
 * 			declare candidate
 *
 * 			if s's DFS num < w's dfs num:
 * 				candidate = s
 * 			else:
 * 				candidate = evaluate(s)
 *
 * 			if candidate's semi # < w's semi #:
 * 				replace w's semi # with the candidate's
 *
 * 		add block into it's semidominator's bucket
 *
 * 		get the parent of block
 *
 *		link_ancestor(parent, block)
 *
 *		foreach block b in the parent's bucket:
 *			candidate = evaluate(b)
 *
 *			if candidate.semi # < b.semi #:
 *				set b's IPDOM to be candidate
 *			else:
 *				set b's IPDOM to be parent
 *
 *	foreach block b:
 *		if b's IPDOM == b's semipostdominator:
 *			set b's IPDOM to be IPDOM(IPDOM(b))
 *
 * Property: Every node *except* the entry node has exactly one immediate dominator
 */
static void compute_immediate_postdominators(basic_block_t* function_exit_block, dynamic_array_t* function_blocks){
	//Extract the number of blocks that we have 
	const u_int32_t number_of_blocks = function_blocks->current_index;

	/**
	 * The first thing that we need to do is initialize every block for IPDOM calculation
	 */
	for(u_int32_t i = 0; i < number_of_blocks; i++){
		basic_block_t* block = dynamic_array_get_at(function_blocks, i);
		initialize_block_for_ipdom_computation(block);
	}

	/**
	 * Let the reverse DFS numbering algorithm recursively give
	 * every block a reverse DFS number. We'll need this number 
	 * for later parts of the algorithm because the higher the number,
	 * the farther away from the exit node of the graph that we are
	 *
	 * The reverse DFS numbering algorithm also initializes the parent,
	 * ancestor and "best candidate" fields, though these will be expanded
	 * more upon later
	 */
	int32_t current_reverse_dfs_number = 0;
	basic_block_t** reverse_dfs_number_to_vertex_mapping = calloc(number_of_blocks, sizeof(basic_block_t*));
	reverse_dfs_number_block(function_exit_block, reverse_dfs_number_to_vertex_mapping, &current_reverse_dfs_number);

	/**
	 * Work our way through the graph from top to bottom(reverse traversal
	 * of the reverse DFS numbers will do this for us)
	 */
	for(int32_t i = current_reverse_dfs_number - 1; i > 0; i--){
		//Extract the block from our mapping
		basic_block_t* working_block = reverse_dfs_number_to_vertex_mapping[i];

		/**
		 * Run through all of this block's successors to compute the semipostdominators.
		 * Remember that a semipostdominator is a block that just barely does not postdominate
		 * this one
		 */
		for(u_int32_t j = 0; j < working_block->successors.current_index; j++){
			//Our semipostdominator candidate
			basic_block_t* candidate;
			basic_block_t* successor = dynamic_array_get_at(&(working_block->successors), j);

			/**
			 * Rare case but something we will account for just to be safe - we will
			 * skip a block if it has an unreachable DFS number
			 */
			if(successor->dominator_info.dfs_number == LT_UNNUMBERED){
				continue;
			}

			/**
			 * If the successor has a smaller reverse DFS number than the working block, than
			 * the successor is our candidate. Otherwise, we will find our candidate by running
			 * the union-find + path compression algorihtm on the successor
			 */
			if(successor->dominator_info.dfs_number < working_block->dominator_info.dfs_number){
				candidate = successor;
			} else {
				candidate = evaluate(successor);
			}

			/**
			 * If the candidate has a smaller(more optimal) semipostdominator number, then we will
			 * replace the current semipostdominator number of our current working block with this
			 * candidate
			 */
			if(candidate->dominator_info.semidominator_number < working_block->dominator_info.semidominator_number){
				working_block->dominator_info.semidominator_number = candidate->dominator_info.semidominator_number;
			}
		}

		/**
		 * Now that we've found our best semipostdominator candidate, we are going to add this block
		 * into said semipostdominators "worklist" that we'll use for deferred processing when
		 * the time comes
		 */
		basic_block_t* semipostdominator = reverse_dfs_number_to_vertex_mapping[working_block->dominator_info.semidominator_number];
		dynamic_array_add(&(semipostdominator->dominator_info.worklist), working_block);

		//We'll need the postdominator parent p of this block going forward
		basic_block_t* postdominator_parent = working_block->dominator_info.parent;

		/**
		 * The parent of the working block is it's union-find ancestor
		 */
		link_ancestor(postdominator_parent, working_block);

		/**
		 * By the time we've reached here, we have enough information
		 * to determine the *potential* IPDOMs for all node's whose semipostdominator
		 * is our working block's parent.
		 *
		 * The worklist is essentially a deferred work queue that allows our algorithm
		 * to postpone any/all IPDOM processing until the ancestor structure
		 * has enough info to make the decision
		 *
		 * Our core theory when processing a bucket is: The immediate
		 * dominator of a block is either it's semipostdominator *or* some
		 * dominator above said semipostdominator. *At this moment, we do
		 * not know which one it is*. This is why a final correction pass is
		 * needed after this. This only computes a provisional(or potential/best guess)
		 * IPDOM
		 */
		dynamic_array_t* parent_worklist = &(postdominator_parent->dominator_info.worklist);
		for(u_int32_t k = 0; k < parent_worklist->current_index; k++){
			/**
			 * Extract our semipostdominated block and use evaluate to perform path compression
			 * and get compute the smallest semidominator number along this path
			 */
			basic_block_t* semipostdominated_block = dynamic_array_get_at(parent_worklist, k);
			basic_block_t* candidate = evaluate(semipostdominated_block);

			/**
			 * If the candidate post evaluation has a smaller semidpostominator number than
			 * the bucket block, we will set the candidate as this block's IPDOM. Otherwise,
			 * we will set this block's IPDOM to be the parent block
			 */
			if(candidate->dominator_info.semidominator_number < semipostdominated_block->dominator_info.semidominator_number){
				semipostdominated_block->dominator_info.immediate_postdominator = candidate;
			} else {
				semipostdominated_block->dominator_info.immediate_postdominator = postdominator_parent;
			}
		}

		//Clear out the bucket now that we've processed
		clear_dynamic_array(parent_worklist);
	}

	/**
	 * Even though we've processed every block, we still need a correction
	 * pass here.
	 *
	 * Lengauer-Tarjan proves:
	 * 	if ipdom(v) != semipostdominator(v):
	 * 		ipdom(v) = ipdom(ipdom(v))
	 *
	 * In other words: "If the candidate is not the semipostdominator, then the true
	 * immediate postdominator is the immediate postdominator of the candidate"
	 */
	for(int32_t i = 1; i < current_reverse_dfs_number; i++){
		//Get the block to work on
		basic_block_t* working_block = reverse_dfs_number_to_vertex_mapping[i];

		//Extract this block's semipostdominator
		basic_block_t* semipostdominator = reverse_dfs_number_to_vertex_mapping[working_block->dominator_info.semidominator_number];

		/**
		 * If the IPDOM is not the semidominator, then the real IPDOM is
		 * the immediate postdominator dominator of said candidate
		 */
		if(working_block->dominator_info.immediate_postdominator != semipostdominator){
			working_block->dominator_info.immediate_postdominator = working_block->dominator_info.immediate_postdominator->dominator_info.immediate_postdominator;
		}
	}

	/**
	 * By definition, the exit block may have no immediate postdominator
	 */
	function_exit_block->dominator_info.immediate_postdominator = NULL;

	//Release the memory now that we're done
	free(reverse_dfs_number_to_vertex_mapping);
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
 * Add a dominated block to the dominator block that we have
 */
static inline void add_dominator_child(basic_block_t* dominator, basic_block_t* dominated){
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
 *
 * Dominator trees are used for SSA form conversion. It is absolutely
 * essential that these be computed upon every run
 */
static inline void build_dominator_trees(dynamic_array_t* function_blocks){
	//Hold the current block
	basic_block_t* current;

	//For each block in the CFG
	for(int32_t i = function_blocks->current_index - 1; i >= 0; i--){
		//Grab out whatever block we're on
		current = dynamic_array_get_at(function_blocks, i);

		/**
		 * We will find this block's "immediate dominator". Once we have that,
		 * we will add this block to the "dominator children" set of said immediate
		 * dominator
		 */
		basic_block_t* immediate_dominator = current->dominator_info.immediate_dominator;

		/**
		 * Now we'll go to the immediate dominator's list and add the dominated block in. Of course,
		 * we'll account for the case where there is no immediate dominator. This is possible in
		 * the case of function entry blocks
		 */
		if(immediate_dominator != NULL){
			add_dominator_child(immediate_dominator, current);
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

		//Now we run through every predecessor of the block
		for(u_int32_t j = 0; j < block->predecessors.current_index; j++){
			basic_block_t* cursor = dynamic_array_get_at(&(block->predecessors), j);

			//While cursor is not the immediate dominator of block
			while(cursor != block->dominator_info.immediate_dominator){
				//Add block to cursor's dominance frontier set
				add_block_to_dominance_frontier(cursor, block);
				
				/**
				 * Cursor now becomes it's own immediate dominator, and
				 * we crawl our way up the CFG
				 */
				cursor = cursor->dominator_info.immediate_dominator;
			}
		}
	}
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

		//Now we run through every successor of the block
		for(u_int32_t j  = 0; j  < block->successors.current_index; j++){
			//Extract the successor
			basic_block_t* cursor = dynamic_array_get_at(&(block->successors), j);

			//While cursor is not the immediate postdominator of block
			while(cursor != block->dominator_info.immediate_postdominator){
				//Add block to cursor's reverse dominance frontier set
				add_block_to_reverse_dominance_frontier(cursor, block);
				
				/**
				 * Cursor now becomes it's own immediate postdominator, and
				 * we crawl our way down the CFG
				 */
				cursor = cursor->dominator_info.immediate_postdominator;
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
 * Get the nearest marked postdominator of a given block
 *
 * The algorithm here is simple: Since we already have all immediate
 * postdominators calculated, we inherently have a postdominator tree
 * that we can leverage and walk. 
 *
 * 			D
 * 			|	
 * 			C(mark)
 *			|
 * 			B
 * 			|
 * 			A
 *
 * If we are searching for A's nearest marked postdominator, we'll maintain
 * a cursor, walk up the tree by going from cursor to IPDOM(cursor), and 
 * bailing out whenever we have a marked value
 *
 * Algorithm Nearest Marked Postdominator(Block b):
 * 		cursor = IPDOM(b)
 * 		
 * 		while cursor != NULL:
 * 			if cursor is marked:
 * 				return cursor
 *
 * 			cursor = IPDOM(cursor)
 *
 *
 * NOTE: in order for this to be accurate, we must have already computed
 * the immediate postdominators of all blocks within the function that this
 * block comes from
 */
basic_block_t* get_nearest_marked_postdominator(basic_block_t* block){
	//We seed the search with the first(closest) dominator that we have
	basic_block_t* cursor = block->dominator_info.immediate_postdominator;

	while(cursor != NULL){
		//This block contains a mark, so we give it back
		if(cursor->contains_mark == TRUE){
			return cursor;
		}

		//Otherwise, climb the tree by going up to this one's IPDOM
		cursor = cursor->dominator_info.immediate_postdominator;
	}

	/**
	 * If we get here, then the cursor went to NULL. In this case, there
	 * is no nearest *marked* postdominator
	 */
	return NULL;
}


/**
 * We will calculate:
 *  1.) Reverse post order traversals
 *  2.) Immediate dominators
 *  3.) Dominator Trees
 *  4.) Dominance Frontiers
 *  5.) Immediate Postdominators
 *  6.) Postdominator sets
 *  7.) Reverse Dominance frontiers
 *
 * For every block in the given function. This externally facing API hides all of
 * the complexity behind it
 */
void calculate_all_control_flow_relations_for_function(basic_block_t* function_entry_block, basic_block_t* function_exit_block, dynamic_array_t* function_blocks){
	//Before any calculation can be done, we need to compute every single reverse traversal
	//
	//TODO EVALUATE THIS ONE'S USE
	calculate_all_reverse_traversals(function_entry_block, function_blocks);
	/**
	 * Before going forward, we must know the immediate dominator for every
	 * single block. We use the efficient Lengauer-Tarjan algorithm to 
	 * do this for all function blocks in one go
	 *
	 * Immediate dominators are a prerequisite for two other essential
	 * graph relations: dominator trees and dominance frontiers. These
	 * *must* be computed upon every run
	 */
	compute_immediate_dominators(function_entry_block, function_blocks);

	/**
	 * Now that we have the immediate dominators needed, we can create
	 * our dominator tree for this function.
	 *
	 * Dominator trees are used for SSA form conversion. It is absolutely
	 * essential that these be computed upon every run
	 *
	 * TODO I DOUBT WE NEED THIS
	 */
	build_dominator_trees(function_blocks);

	/**
	 * Dominance frontiers are essential for inserting phi functions when performing
	 * an SSA conversion. It is for this reason that we must calculate them upon every 
	 * run of the control flow calculator
	 */
	calculate_dominance_frontiers(function_blocks);

	/**
	 * Immediate postdominators are also computed with the Lengauer-Tarjan linear
	 * time algorithm, just on the reversed CFG. These are needed to find the reverse dominance
	 * frontier, and they're also used when we perform branch reduction in the 
	 * optimizer(see nearest_marked_postdominator)
	 */
	compute_immediate_postdominators(function_exit_block, function_blocks);

	/**
	 * Reverse dominance frontiers are the same as regular dominance frontiers(just
	 * barely not *post*dominated by a block X) and are used when we are doint
	 * mark and sweep. The reverse dominance frontier is thus calculated every
	 * time we do this. The algorithm relies on immediate postdominators, which is
	 * why this comes last
	 */
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

		//Wipe the immediate dominator slate clean
		initialize_block_for_idom_computation(block);

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
