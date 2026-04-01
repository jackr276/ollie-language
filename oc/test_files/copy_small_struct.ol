/**
* Author: Jack Robbins
* Test case where we have a small(sub 16 byte) struct that we're copying
*/

define struct custom {
	x:i32;
	y:char;
} as small_struct;


pub fn main() -> i32 {
	let original:small_struct = {1, 'a'};

	//Small struct copy
	let copy:small_struct = original;

	//Should give us "a" - 97 as an int
	ret copy:y;
}
