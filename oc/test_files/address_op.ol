/**
* Author: Jack Robbins
* Testing the address operator
*/

pub fn main() -> i32 {
	let mut x:u32 = 3;
	let mut y:u32* = &x;
	let mut z:u32** = &y;

	x = 127;

	ret **z;
}
