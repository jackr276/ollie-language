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

	//Test how it handles operations taking place
	//inside of the call
	ret @add_vars(x_ref + 1, y_ref - 1);
}
