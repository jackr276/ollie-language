/**
* Author: Jack Robbins
* This file tests the pointer arithmetic
*/

fn main(argc:i32, argv:char**) -> i32 {
	let mut x:i32 := 32;
	let mut y:i32* := &x;

	x := x - 3;

	y := --y;

	ret *y;
}
