/**
* Author: Jack Robbins
* Testing the constant folding for shifting 
*/

pub fn main() -> i32{
	let mut x:i32 := 3;
	let mut y:i32 := x - 1;

	//Should just become an assignment
	let mut z:i32 := y >> 0;
	
	//Should also be an assignment
	x := z << 0;

	ret z + x;
}
