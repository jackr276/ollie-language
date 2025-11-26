/**
* Author: Jack Robbins
* This file will test the functionality of the ternary operator
*/

pub fn main() -> i32 {
	let x:mut i32 = 2;
	let a:mut i32 = 3;
	let b:mut i32 = 3;
	
	let test:mut i32 = x == 0 ? a else b;

	ret test;
}
