/**
* Author: Jack Robbins
* References with type coercion
*/

pub fn main() -> i32 {
	let x:mut i16 = 3;

	//Type widening here
	let y:mut i32& = x;

	ret y + x;
}
