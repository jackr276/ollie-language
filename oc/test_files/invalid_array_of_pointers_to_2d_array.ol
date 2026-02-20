/**
* Author: Jack Robbins
* Test the invalid case where a user tries to coerce a 2D array into an
* array of pointers
*/

pub fn main() -> i32 {
	//Flat data structure. One contiguous block of stack data
	let x:mut i32[][] = [[1, 2, 3], [2, 3, 4], [4, 5, 6]];

	declare ptr:mut i32*[3];

	//This should fail. "Ptr" is a non-contiguous memory region, whereas "x" is contiguous
	ptr = x;

	ret ptr[2][1];
}
