/**
* Author: Jack Robbins
* Tests the optimizing away of a useless stack variable
*/

pub fn main() -> i32 {
	let mut x:i32 = 3;
	let mut y:i32* = &x;

	x = 33;

	ret 0;
}

