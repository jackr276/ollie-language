/**
* Author: Jack Robbins
* Test an attempt to assign an immutable pointer to a mutable one
*/

//Will mutate the value in x
pub fn tester(x:mut i32*) -> void {
	*x = 3;
}

pub fn main() -> i32 {
	let x:i32 = 33;

	//Invalid - x is immutable
	@tester(&x);
	
	ret 0;
}
