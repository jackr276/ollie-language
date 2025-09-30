/**
* Author: Jack Robbins
* A simple testing file to test logical and, or and not
*/


pub fn main(arc:i32, argv:char**) -> i32 {
	let mut x:i32 = 73;
	let mut y:i32 = 88;

	ret (x || y) + (34 && y);
}
