/**
* Author: Jack Robbins
* This program tests the functionality of two ternaries in an expression
*/

pub fn main() -> i32 {
	let x:mut i32 = 3;
	let y:mut i32 = 4;
	let z:mut i32 = 5;
	let a:mut i32 = 6;

	let ret_val:i32 = (x == 2 ? x else y) + (y == 3 ? z else a);

	ret ret_val;
}
