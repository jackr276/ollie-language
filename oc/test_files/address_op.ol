/**
* Author: Jack Robbins
* Testing the address operator
*/

pub fn main() -> i32 {
	let x:mut u32 = 3;
	let y:mut u32* = &x;
	let z:mut u32** = &y;

	x = 127;

	ret **z;
}
