/**
* Author: Jack Robbins
* Test a linked list functionality for a recursive union pointer member
*/

define union my_union {
	//Next node
	next:union my_union*;
	data:void*;
} as union_t;


pub fn ret_union(x:union_t) -> union_t* {
	ret x.next;
}



pub fn main() -> i32 {
	ret 0;
}
