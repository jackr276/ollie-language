/**
* Author: Jack Robbins
* Testing the address operator
*/

fn main() -> i32 {
	let mut x:u32 := 3;
	let mut y:u32* := &x;
	let mut z:u32** := &y;

	x := 3222;

	ret **z;
}
