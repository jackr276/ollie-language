/**
* Author: Jack Robbins
* Testing optimization strategies relying on the address operator
*/

pub fn main(argc:i32, argv:char**) -> i32 {
	let mut x:u32 = 3;

	//Should be irrelevant
	x <<= 3;
	x = 23;

	//But everything after here should be relevant
	let mut y:u32* = &x;

	x = 32;
	x /= argc;

	ret *y; //This in theory is a use of x because we're dereferencing it in memory
}
