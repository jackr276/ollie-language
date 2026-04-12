/**
* Author: Jack Robbins
* Test the most basic binary operation that we can make
*/


pub fn main() -> i32 {
	let x:i32 = 5;
	let y:i32 = 4;

	let z:i32 = x + y;
	
	//Verify that this does turn into a bin_op_with_const
	let aa:i32 = z + 5;

	ret z + aa;
}
