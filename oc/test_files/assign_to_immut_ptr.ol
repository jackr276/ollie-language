/**
* Author: Jack Robbins
* Invalid attempt to dereference and assign to an immutable pointer
*/

fn tester(x:i32*) -> void {
	//SHOULD FAIL
	*x = 3;
}


pub fn main() -> i32 {
	let x:mut i32 = 3;
	@tester(&x);

	ret 0;
}
