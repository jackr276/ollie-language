/**
* Author: Jack Robbins
* Testing type coercion rules
*/

pub fn main(argc:i32, argv:char**) -> i32 {
	let mut x:u8 := 'a';
	let mut y:i32 := 32;

	//Should be casted to an int
	ret y + x;
}
