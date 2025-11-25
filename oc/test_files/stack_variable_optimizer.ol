/**
* Author: Jack Robbins
* Tests the optimizing away of a useless stack variable
*/

pub fn main() -> i32 {
	let x:mut i32 = 3;
	let y:mut i32* = &x;

	x = 33;

	ret 0;
}

