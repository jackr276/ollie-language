/**
* Author: Jack Robbins
* Test case where we have a super small(sub 4 byte) struct that we're copying
*/

define struct custom {
	x:char;
	y:char;
	z:char;
} as super_small_struct;


pub fn main() -> i32 {
	let original:super_small_struct = {'a', 'b', 'c'};

	//Small struct copy
	let copy:super_small_struct = original;

	//Should give us "a" - 97 as an int
	ret copy:y;
}
