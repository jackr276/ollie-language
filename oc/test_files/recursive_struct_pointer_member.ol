/**
* Author: Jack Robbins
* Test a linked list functionality for a recursive struct pointer member
*/

define struct linked_list_node {
	//Next node
	next:mut struct linked_list_node*;
	data:mut void*;
} as list_node_t;

/**
* Linked list functionality
*/
fn get_next(current:mut list_node_t*) ->mut list_node_t* {
	ret current=>next;
}



pub fn main() -> i32 {
	ret 0;
}
