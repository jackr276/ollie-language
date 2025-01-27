/**
 * This is the implementation file for the call graph defined in call_graph.h
*/

#include "call_graph.h"
#include <stdio.h>


/**
 * Dynamically allocates a function's call graph node, and initializes every value 
 * except for the function record to be 0
*/
call_graph_node_t* create_call_graph_node(symtab_function_record_t* function_record){
	//Allocate it
	call_graph_node_t* call_graph_node = calloc(1, sizeof(call_graph_node_t));

	//We can assign the function record as is
	call_graph_node->function_record = function_record;

	return call_graph_node;
}


/**
 * Records a call to a function. Caller -> callee
*/
void call_function(call_graph_node_t* caller, call_graph_node_t* callee){
	//For developer use only -- if we've exceeded the number of internal nodes
	if(caller->num_callees == MAX_FUNCTION_CALLS){
		fprintf(stderr, "Fatal internal compiler error. Conisder increasing the number of allowed function calls");
		exit(1);
	}

	//All we need to do here is keep a record
	caller->calls[caller->num_callees] = callee;
	//Increment how many of these that we have
	(caller->num_callees)++;
}



