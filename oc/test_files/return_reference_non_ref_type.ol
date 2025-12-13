/**
* Author: Jack Robbins
* Test the case where we return a non-reference and need to auto-deref
*/


//This sh
fn get_max(a:i32&, b:i32&) -> i32 {
	ret (a > b) ? a else b;
}

pub fn main() -> i32 {
	let x:mut i32 = 10;
	let y:mut i32 = 15;

	//The compiler should implicitly get the addresses of x and y here
	ret @get_max(x, y);
}
