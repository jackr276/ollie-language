/**
* Author: Jack Robbins
* Test a linked list functionality for a recursive struct pointer member
*/

define struct linked_list_node {
	//Next node
	mut next:struct linked_list_node*;
	mut data:void*;
} as list_node_t;

/**
* Linked list functionality
*/
fn get_next(mut current:list_node_t*) -> list_node_t* {
	ret current=>next;
}



pub fn main() -> i32 {
	ret 0;
}
