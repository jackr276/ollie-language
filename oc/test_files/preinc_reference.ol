/**
* Author: Jack Robbins
* Test the case where we want to pre-increment a reference
*/

pub fn main() -> i32 {
	let x:mut i32 = 3;
	let y:mut i32 = 5;

	//Grab immutable references to them
	let x_ref:mut i32& = x;
	let y_ref:mut i32& = y;

	//Test preinc's
	++x_ref;
	--y_ref;

	ret x_ref + y_ref;
}
