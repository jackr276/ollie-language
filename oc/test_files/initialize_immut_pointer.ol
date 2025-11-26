/**
* Author: Jack Robbins
* Test the case where we want to initialize an immutable pointer
*/

pub fn main() -> i32{
	//Immutable pointer
	declare ptr:i32*;

	//Value
	let x:mut i32 = 33;

	//Assign it - this should work
	ptr = &x;

	ret *ptr;
}
