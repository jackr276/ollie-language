/**
* Author: Jack Robbins
* Test an attempt to assign an immutable pointer to a mutable one
*/

//Will mutate the value in x
pub fn tester(x:mut i32*) -> void {
	*x = 3;
}

pub fn main() -> i32 {
	let x:mut i32 = 33;

	//Immutable reference to x
	let y:i32* = &x;

	@tester(y);
	
	ret 0;
}
