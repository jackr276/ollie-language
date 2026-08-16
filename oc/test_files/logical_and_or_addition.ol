/**
* Author: Jack Robbins
* A simple testing file to test logical and, or and not
*/


pub fn main(arc:i32, argv:char**) -> i32 {
	let x:mut i32 = 73;
	let y:mut i32 = 88;

	OUNIT: [exit_status = 2]
	ret <i8>(x || y) + <i8>(34 && y);
}
