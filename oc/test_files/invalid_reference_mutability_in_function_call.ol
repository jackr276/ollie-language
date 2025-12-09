/**
* Author: Jack Robbins
* Test using a function call with a reference parameter
*/

fn add_vars(x:mut i32&, y:mut i32&) -> i32 {
	ret x + y;
}


pub fn main() -> i32 {
	let x:mut i32 = 3;
	let y:mut i32 = 3;

	//Grab immutable references to them
	let x_ref:i32& = x;
	let y_ref:i32& = y;

	//Should fail - you can't pass immutable
	//references to places that expect mutable ones
	ret @add_vars(x_ref, y_ref);
}
