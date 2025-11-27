/**
* Author: Jack Robbins
* Test an invalid attempt to assign an array in a let statement
*/

pub fn main() -> i32 {
	let arr:mut i32[] = [2, 3, 4, 5, 6];
	
	//This is invalid, you can't use anything besides
	//an array initializer to do this
	let arr2:mut i32[] = arr;

	ret 0;
}
