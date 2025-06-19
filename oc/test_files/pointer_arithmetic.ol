/**
* Author: Jack Robbins
* This file tests the pointer arithmetic
*/

fn main(argc:i32, argv:char**) -> i32 {
	let mut x:i32 := 32;
	let mut y:i32* := &x;

	*y := x - 11;
	*y := 32;

	ret *y + x;
}
