/**
 * Author: Jack Robbins
 * This header file defines all that is needed for the Ollie basic block. The basic
 * block is the fundamental building block of all Ollie functions
 *
 * This file contains the basic block struct itself as well as some related enumerations
 */

#ifndef BASIC_BLOCK_H
#define BASIC_BLOCK_H

#include "../dynamic_array/dynamic_array.h"
#include "../jump_table/jump_table.h"
#include "../ollie_instruction.h"
#include <string.h>
#include <sys/types.h>

//Basic blocks in our CFG
typedef struct basic_block_t basic_block_t;
//An edge struct for successor/predecessor relationships
typedef struct cfg_edge_t cfg_edge_t;
//A set of edges. This is used for predecessor/successor sets
typedef struct edge_set_t edge_set_t;

/**
 * Initializing an empty edge
 */
#define EMPTY_CFG_EDGE (cfg_edge_t){EDGE_TYPE_REAL, NULL}

/**
 * Initializing an empty edge set
 */
#define EMPTY_EDGE_SET (edge_set_t){NULL, 0, 0}

/**
 * What is the general type of the block. Again most
 * blocks are normal, but there are exceptions
 */
typedef enum{
	BLOCK_TYPE_NORMAL, //Normal block
	BLOCK_TYPE_SWITCH, //The whole block is a switch statement
	BLOCK_TYPE_CASE, //Case statement -- it also encapsulates default(just a special kind of case)
	BLOCK_TYPE_FUNC_ENTRY, //Block is a function entry
	BLOCK_TYPE_FUNC_EXIT, //Block is a function exit
	BLOCK_TYPE_IF_ENTRY, //If statement entry
	BLOCK_TYPE_IF_EXIT, //End of an if-statement
	BLOCK_TYPE_LOOP_ENTRY, //Loop entry block
	BLOCK_TYPE_LOOP_EXIT, //Loop exit block
	BLOCK_TYPE_LABEL, //This block comes from a user-defined label
} block_type_t;


/**
 * What kind of edge is this? Currently we have real
 * edges(default and most common) and analysis only
 * edges
 */
typedef enum {
	EDGE_TYPE_REAL,
	EDGE_TYPE_VIRTUAL
} edge_type_t;


/**
 * A CFG edge contains of a block and an edge type. The Ollie compiler
 * allows us to have real edges and so-called virtual or analysis only
 * edges
 */
struct cfg_edge_t {
	edge_type_t edge_type;
	basic_block_t* block;
};


/**
 * An edge set is, in reality, a dynamic array that contains
 * cfg_edge_t structs as its members
 */
struct edge_set_t {
	cfg_edge_t* edges;
	int32_t current_index;
	int32_t max_index;
};


/**
 * Define: a basic block is a sequence of consecutive 
 * intermediate language statements in which flow of 
 * control can only enter at the beginning and leave at the end 
 *
 * A basic block has ONE entrance and ONE exit. These points are referenced 
 * by the "leader" and "exit" references for quick access
*/
struct basic_block_t{
	instruction_t* leader_statement;
	instruction_t* exit_statement;
	/**
	 * Predecessors and successors are represented by the cfg_edge_t struct. This
	 * is necessary because we can have both real and virtual(analysis only) succesors
	 * and predecessors, so we need to not only store the block but also the edge type
	 */
	edge_set_t predecessors;
	edge_set_t successors;
	//The "LIVE_IN" variables for this node
	dynamic_array_t live_in;
	//The "LIVE_OUT" variables for this node
	dynamic_array_t live_out;
	//The set of "used_before_definition" defines all variables that were used
	//before they were assigned in the block
	dynamic_array_t used_before_definition;
	//The array of all assigned variables
	dynamic_array_t assigned_variables;
	//For convenience here. This is the successor that we use to
	//"drill" to the bottom
	basic_block_t* direct_successor;
	//The blocks dominance frontier
	dynamic_array_t dominance_frontier;
	//The reverse dominance frontier(for analysis)
	dynamic_array_t reverse_dominance_frontier;
	//The dominator children of this block
	dynamic_array_t dominator_children;
	/**
	 * Dominator information that each and every block will own. This 
	 * information is needed whenever we compute the immediate dominator
	 * and/or postdominator
	 */
	struct {
		//Block's immediate dominator
		basic_block_t* immediate_dominator;
		//Block's immediate postdominator
		basic_block_t* immediate_postdominator;
		/**
		 * The DFS number of this block after it's been DFS(or reverse DFS)
		 * numbered
		 */
		int32_t dfs_number;
		/**
		 * The DFS number of this block's semidominator
		 */
		int32_t semidominator_number;
		/**
		 * The parent of this block(NOT the ancestor)
		 */
		basic_block_t* parent;
		/**
		 * The union-find ancestor of this
		 * block(NOT the parent)
		 */
		basic_block_t* ancestor;
		/**
		 * The node with the smallest semidominator
		 * number along the currently known path - we 
		 * cache this to avoid recomputation
		 */
		basic_block_t* optimal_candidate;
		/**
		 * The worklist is a deferred work queue that we use. It will
		 * store the list of all nodes that are semidominated by this
		 * given node
		 */
		dynamic_array_t worklist;
	} dominator_info;

	/**
	 * When we inline functions(among other things), we may need to maintain
	 * a 1-to-1 relationship where a block maps to another block. This is wrapped
	 * in "mapping info" for clarity
	 */
	struct {
		basic_block_t* maps_to;
	} mapping_info;

	//The reference to a jump table. This is often not used at all
	jump_table_t* jump_table;
	//The case statement value -- usually blank
	int64_t case_stmt_val;
	//The function that we're defined in
	symtab_function_record_t* function_defined_in;
	//An integer ID
	int32_t block_id;
	//The number of instructions that the given block has
	u_int32_t number_of_instructions;
	//The estimated execution frequency. This will change if a block is in a loop, etc.
	u_int32_t estimated_execution_frequency;
	//What is the general classification of this block
	block_type_t block_type;
	//Does this block contain a marked record?
	u_int8_t contains_mark;
	//Was this block visited by traverser?
	u_int8_t visited;
	//Does this block already have a phi function for the given variable?
	u_int8_t already_has_phi_func;
};

//======================================================== CFG Edge Set Utility ==============================================

/**
 * Allocate a CFG edge set using the default size
 */
edge_set_t edge_set_alloc();


/**
 * Dynamically resize an edge set to accomodate the given index if need be
 */
void edge_set_dynamic_resize_if_needed(edge_set_t* edge_set, int32_t index);


/**
 * Get the edge at a given index. If the index is invalid then NULL is returned
 */
static inline cfg_edge_t* get_edge_at_index(edge_set_t* edge_set, int32_t index){
	/**
	 * This makes no sense so we return NULL
	 */
	if(index >= edge_set->current_index){
		return NULL;
	}

	/**
	 * Otherwise return a poiner to the requested edge
	 */
	return &(edge_set->edges[index]);
}


/**
 * Add a block to an edge set. There can be *no* duplicates inside of the edge set, even if they
 * were to have edge types that are different. This means that if you try to add a virtual edge
 * to block .L4, but a real edge to .L4 already exists, we will silently add nothing
 */
static inline void add_edge(edge_set_t* edge_set, basic_block_t* block, edge_type_t edge_type){
	/**
	 * We should never be trying to do this so panic if we are
	 */
	if(block == NULL){
		fprintf(stderr, "Fatal internal compiler error. Attempt to add a NULL edge\n");
		exit(1);
	}

	/**
	 * Scan the entirety of the current edge set. If this block already exists in the
	 * edge set then we will just leave and not add it
	 */
	for(int32_t i = 0; i < edge_set->current_index; i++){
		if(edge_set->edges[i].block == block){
			return;
		}
	}

	/**
	 * Dynamically resize this if we need to
	 */
	edge_set_dynamic_resize_if_needed(edge_set, edge_set->current_index);

	/**
	 * Grab a pointer to this edge and add the block and edge type requested
	 */
	cfg_edge_t* edge_to_add = &(edge_set->edges[edge_set->current_index]);
	edge_to_add->block = block;
	edge_to_add->edge_type = edge_type;

	//Bump this up for the next go around
	(edge_set->current_index)++;
}


/**
 * Get the index of the given block in an edge set. If the block is not found, -1 is returned
 */
static inline int32_t edge_set_index_of(edge_set_t* edge_set, basic_block_t* basic_block){
	/**
	 * Linear scan through the whole thing to find the requested index
	 */
	for(int32_t i = 0; i < edge_set->current_index; i++){
		if(edge_set->edges[i].block == basic_block){
			return i;
		}
	}

	return NOT_FOUND;
}


/**
 * Delete the edge value at the given index inside of the edge set
 */
void edge_set_delete_at_index(edge_set_t* edge_set, int32_t index);


/**
 * Remove the given block from the edge set. This is in reality a wrapper
 * for edge_set_index_of and edge_set_delete_at_index, but combined here
 * for convenience. If the block is not in the edge set then nothing will
 * be deleted
 */
static inline void edge_set_delete(edge_set_t* edge_set, basic_block_t* block){
	/**
	 * Get the deletion index. If it's NOT_FOUND then just get out
	 */
	int32_t deletion_index = edge_set_index_of(edge_set, block);
	if(deletion_index == NOT_FOUND){
		return;
	}

	/**
	 * Invoke the real deletion function to actually do this
	 */
	edge_set_delete_at_index(edge_set, deletion_index);
}


/**
 * Is the given edge set entirely empty?
 */
static inline u_int8_t edge_set_empty(edge_set_t* edge_set){
	return edge_set->current_index == 0 ? TRUE : FALSE;
}


/**
 * Completely erase the values in the edge set while still keeping it allocated.
 * This allows us to reuse edge sets without having to reallocate memory
 */
static inline void clear_edge_set(edge_set_t* edge_set){
	memset(edge_set->edges, 0, sizeof(cfg_edge_t) * edge_set->max_index);
	edge_set->current_index = 0;
}


/**
 * Deallocate a CFG edge set
 */
void edge_set_dealloc(edge_set_t* edge_set);

//======================================================== CFG Edge Set Utility ==============================================
//======================================================== Basic Block Utilities =============================================

/**
 * Exclusively add a successor to the target block. The predecessor set will not be touched
 */
static inline void add_successor_only(basic_block_t* target, basic_block_t* successor, edge_type_t type){
	/**
	 * Allocate the edge set if need be
	 */
	if(target->successors.edges == NULL){
		target->successors = edge_set_alloc();
	}

	/**
	 * Now add the edge into the successor set. The add_edge function
	 * handles all duplicate checking already
	 */
	add_edge(&(target->successors), successor, type);
}


/**
 * Exclusively add a predecessor to the target block. The successor set will not be touched
 */
static inline void add_predecessor_only(basic_block_t* target, basic_block_t* predecessor, edge_type_t type){
	/**
	 * Allocate the edge set if need be
	 */
	if(target->predecessors.edges == NULL){
		target->predecessors = edge_set_alloc();
	}

	/**
	 * Now add the edge into the successor set. The add_edge function
	 * handles all duplicate checking already
	 */
	add_edge(&(target->predecessors), predecessor, type);
}


/**
 * Add a successor to the target block. This method is entirely comprehensive.
 * Since "successor" comes after "target", "target" will be added as a predecessor
 * of "successor". If you wish to ONLY add a successor or predecessor(very rare),
 * then use the "only" methods
 */
static inline void add_successor(basic_block_t* target, basic_block_t* successor, edge_type_t type){
	/**
	 * First we'll add successor as a successor of target
	 */
	add_successor_only(target, successor, type);

	/**
	 * Then add the target as a predecessor of successor
	 */
	add_predecessor_only(successor, target, type);
}


/**
 * Delete a successor from a block
 *
 * This is mainly just a wrapper for a dynamic_array_delete, for readability
 *
 * Target: the block that has the successor
 * Successor: the successor itself
 *
 * TODO DELETE IS
 */
void delete_successor_only(basic_block_t* target, basic_block_t* successor){
	dynamic_array_delete(&(target->successors), successor);
}


/**
 * Delete a predecessor from a block
 *
 * This is mainly just a wrapper for a dynamic_array_delete, for readability
 *
 * Target: the block that has the predecessor
 * Predecessor: the predecessor itself
 *
 * TODO DELETE THIS
 */
void delete_predecessor_only(basic_block_t* target, basic_block_t* predecessor){
	dynamic_array_delete(&(target->predecessors), predecessor);
}


/**
 * Delete a successor from the target block's list. This function is
 * entirely comprehensive. Since target used to be a predecessor
 * of the deleted successor, that will also be deleted
 *
 * TODO DELETE THIS
 */
void delete_successor(basic_block_t* target, basic_block_t* deleted_successor){
	//Bail out if this happens
	if(deleted_successor == NULL){
		return;
	}

	//The target is no longer a predecessor of said successor
	delete_predecessor_only(deleted_successor, target);

	//The said successor is no longer a successor of the target
	delete_successor_only(target, deleted_successor);
}



/**
 * Utility for deallocating a basic block
 */
void basic_block_dealloc(basic_block_t* block);
//======================================================== Basic Block Utilities =============================================

#endif /* BASIC_BLOCK_H */
