/**
* Author: Jack Robbins
* This file will test an invalid attempt to assign to a ternary
*/

pub fn main() -> i32 {
	let x:mut i32 = 3;
	let y:mut i32 = 5
	let z:mut i32 = 6;

	(z == 6 ? x else y) = 32;

	ret x + y;
}
