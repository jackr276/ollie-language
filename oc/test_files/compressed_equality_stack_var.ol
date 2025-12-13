/**
* Author: Jack Robbins
* Testing of the compressed equality operators with stack variables
*/

pub fn test_compressed_equality() -> i32* {
	let x:mut i32 = 3;

	//Some compressed equality
	x += 3;
	x >>= 3;
	x *= 33;

	//Force to be a stack var
	ret &x;
}

//Dummy
pub fn main() -> i32 {
	ret	0;
}
