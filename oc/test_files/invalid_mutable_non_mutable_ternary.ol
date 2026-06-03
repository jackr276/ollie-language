/**
* Author: Jack Robbins
* Test a case where we have a path that could potentially assign a non-mutable pointer
* to a mutable variable. This should fail
*/

pub fn tester(param:i32) -> mut i32* {
	let x:mut i32 = 5;
	let y:mut i32 = 5;
	
	let x_ptr: i32* = &x;
	let y_ptr:mut i32* = &x;

	/**
	* This should fail because x is immutable
	*/
	ret param > 5 ? y_ptr else x_ptr;
}


pub fn main() -> i32 {
	ret *@tester(5);
}
