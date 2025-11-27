/**
* Author: Jack Robbins
* Test an attempt by the user to reassign an array variable. Array variables are *never*
* assignable
*/

pub fn main() -> i32 {
	//Identical types
	let arr:mut i32[] = [2, 3, 4, 5, 6, 7];
	let arr2:mut i32[] = [2, 3, 4, 5, 6, 7];

	//INVALID - you can never assign an array
	arr = arr2;

	ret 0;
}
