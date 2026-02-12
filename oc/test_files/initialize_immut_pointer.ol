/**
* Author: Jack Robbins
* Test the case where we want to initialize a pointer
*/

pub fn main() -> i32{
	//Pointer
	declare ptr:mut i32*;

	//Value
	let x:mut i32 = 33;

	//Assign it - this should work
	ptr = &x;

	ret *ptr;
}
