/**
* Author: Jack Robbins
* This file tests the pointer arithmetic
*/

pub fn main(argc:i32, argv:char**) -> i32 {
	let x:mut i32 = 32;
	let y:mut i32* = &x;

	*y = x - 11;
	*y = 32;

	ret *y + x;
}
