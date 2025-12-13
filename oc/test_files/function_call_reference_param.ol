/**
* Author: Jack Robbins
* Test using a function call with a reference parameter
*/

fn add_vars(x:i32&, y:i32&) -> i32 {
	ret x + y;
}


pub fn main() -> i32 {
	let x:mut i32 = 3;
	let y:mut i32 = 3;

	let x_ref:mut i32& = x;
	let y_ref:mut i32& = y;

	//The compiler should recognize that we do *not*
	//want to perform the implicit deref here
	ret @add_vars(x_ref, y_ref);
}
