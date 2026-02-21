/**
* Author: Jack Robbins
* Normally arrays decay to pointers like usual. However, if we have an array to pointer
* assignment with a more than one dimensional array, we will not allow it. The reason why
* is because when we assign flat memory regions(2d array) to non-contiguous regions(like **), 
* we get undefined behavior(extra dereferences) at use
*/


pub fn invalid_assignment() -> i32 {
	declare x:mut i32[5][6];
	
	//Should not work
	let y:i32** = x;

	ret 0;
}


pub fn main() -> i32 {
	ret 0;
}
