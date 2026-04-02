/**
* Author: Jack Robbins
* Test case where we have a super small(sub 4 byte) union that we're copying
*/

define union custom {
	x:char;
	y:i8;
	z:u8;
} as super_small_union;


pub fn main() -> i32 {
	declare original:mut super_small_union;

	original.x = 'a';

	//Small struct copy
	let copy:super_small_union = original;

	//Should give us "a" - 97 as an int
	ret copy.y;
}
