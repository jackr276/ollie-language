/**
* Author: Jack Robbins
* Testing of the compressed equality operators with reference variables
*/

pub fn test_compressed_equality() -> i32 {
	let y:mut i32 = 3;
	let x:mut i32& = y;

	//Some compressed equality
	x += 3;
	x >>= 3;
	x *= 33;

	ret x;
}

//Dummy
pub fn main() -> i32 {
	ret	0;
}
