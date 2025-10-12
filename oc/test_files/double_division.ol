/**
* Author: Jack Robbins
* Testing double division
*/


pub fn main() -> i32 {
	let mut x:i32 = 2;
	let mut y:i16 = 8;
	let mut a:i16 = 8;

	let mut z:i32 = x / (y % a);

	ret z;
}
