/**
* Author: Jack Robbins
* Test the case where we want to initialize an immutable pointer
*/

pub fn main() -> i32{
	//Immutable pointer
	declare ptr:i32*;

	//Value
	let x:mut i32 = 33;
	let y:mut i32 = 44;

	//Assign it - this should work
	ptr = &x;

	//This should fail - we've already initialized at this point
	ptr = &y;

	ret *ptr;
}
