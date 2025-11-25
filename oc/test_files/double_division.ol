/**
* Author: Jack Robbins
* Testing double division
*/


pub fn main() -> i32 {
	let x:mut i32 = 2;
	let y:mut i16 = 8;
	let a:mut i16 = 8;

	let z:mut i32 = x / (y % a);

	ret z;
}
