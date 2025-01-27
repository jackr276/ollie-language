/**
 * This module represents the call-graph IR for the ollie language
 *
 * The call graph has a node for each procedure and an edge for each call site
*/

#ifndef CALL_GRAPH_H
#define CALL_GRAPH_H

#include <sys/types.h>
#include "../symtab/symtab.h"

//Maximum number of function calls per function is set
#define MAX_FUNCTION_CALLS 50

//A call graph struct. Simply represents a function and edges to the functions that it calls
typedef struct call_graph_node_t call_graph_node_t;

/**
 * A call graph stores a node for each procedure(function) and an edge going to 
 * each procedure that that function calls
*/
struct call_graph_node_t{
	//The node's function
	symtab_function_record_t* function_record;

	//We'll also store a list of records in an array
	call_graph_node_t* calls[MAX_FUNCTION_CALLS];

	//Store the current number here
	u_int8_t num_callees;
};


/**
 * Dynamically allocates and creates a call graph node
*/
call_graph_node_t* create_call_graph_node(symtab_function_record_t* function_record);


/**
 * Records a call to a function
*/
void call_function(call_graph_node_t* caller, call_graph_node_t* callee);

#endif
