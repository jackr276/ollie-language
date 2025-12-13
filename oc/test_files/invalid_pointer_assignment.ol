/**
* Author: Jack Robbins
* Invalid type coercion in pointers
*/

pub fn main() -> i32 {
	let x:mut i16 = 3;
	
	//This is illegal - can't have an i32* to an i16*
	let y:mut i32* = &x;

	ret *y + x;
}
