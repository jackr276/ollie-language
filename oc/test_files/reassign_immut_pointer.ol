/**
* Author: Jack Robbins
* Test the case where we want to initialize an immutable pointer
*/

pub fn main() -> i32{
	//Value
	let x:mut i32 = 33;
	let y:mut i32 = 44;

	//Assign it - this should work
	let ptr:i32* = &x;

	//This should fail - we've already initialized at this point
	ptr = &y;

	ret *ptr;
}
