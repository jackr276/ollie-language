/**
* Author: Jack Robbins
* This program tests the functionality of returning a ternary operator
*/

pub fn main() -> i32 {
	let x:mut i32 = 2;
	let a:mut i32 = 3;
	let b:mut i32 = 3;
	
	//Return the ternary
	ret x == 0 ? a - 1 else b + 2;
}
