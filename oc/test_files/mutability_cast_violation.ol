/**
* Author: Jack Robbins
* Test an invalid cast attempt for mutable/immutable references
*/

fn tester(x:mut i32*) -> i32 {
	//Mutates it
	x[3] = 5;

	ret x[3];
}

pub fn main() -> i32 {
	//Immutable array
	let x:i32[] = [3, 4, 5, 2, 1, 2];

	//Should fail - attempt to do a mutable cast
	@tester(<mut i32*>x);
	
	ret 0;
}
