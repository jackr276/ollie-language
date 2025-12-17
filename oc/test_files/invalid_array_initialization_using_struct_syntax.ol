/**
* Author: Jack Robbins
* Test a case where a user tries to initialize an array using the struct{} syntax
*/

pub fn main() -> i32 {
	//This is invalid syntax. We can only use the "[]" for arrays
	let x:mut i32[] = {1, 2, 3, 4, 6};

	ret *x;
}
