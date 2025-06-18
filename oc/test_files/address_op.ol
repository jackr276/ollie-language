/**
* Author: Jack Robbins
* Testing the address operator
*/

fn main() -> i32 {
	let mut x:u32 := 3;
	let mut y:u32* := &x;

	ret *y;
}
