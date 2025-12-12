/**
* Author: Jack Robbins
* Test the case where we are returning a reference to a variable
* from a function. We are using the technically invalid case 
* of returning a reference to a local variable, but this only 
* exists to test the underlying compiler machinery so it's fine
*/


fn get_max(a:i32&, b:i32&) -> i32& {
	ret (a > b) ? a else b;
}

pub fn main() -> i32 {
	let x:mut i32 = 10;
	let y:mut i32 = 15;

	let x_ref:i32& = x;
	let y_ref:i32& = y;

	let ref:i32& = @get_max(x_ref, y_ref);

	ret ref;
}
