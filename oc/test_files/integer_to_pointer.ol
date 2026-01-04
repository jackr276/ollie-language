/**
* Author: Jack Robbins
* Test the ability to assign an integer to a pointer
*/

fn integer_param_to_pointer(x:i32) -> i32 {
	let x_ptr:i32* = x;

	ret *x_ptr;
}


pub fn main() -> i32 {
	//Should work
	let null_ptr:i32* = 0;

	ret *null_ptr;
}
