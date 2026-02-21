/**
* Author: Jack Robbins
* Test the invalid case where a user tries to coerce a 2D array into an
* array of pointers
*/

pub fn take_pointer_array(ptr:mut i32*[3]) -> i32 {
	ret (*ptr)[2];
}

pub fn main() -> i32 {
	//Flat data structure. One contiguous block of stack data
	let x:mut i32[][] = [[1, 2, 3], [2, 3, 4], [4, 5, 6]];

	//This should fail. We cannot assign a flat data structure(x)
	//to a non-contiguous region(the function param)
	ret @take_pointer_array(x);
}
