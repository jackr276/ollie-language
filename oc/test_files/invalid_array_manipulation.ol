/**
* Author: Jack Robbins
* Test an attempt by the user to manipulate an array variable. Remember in ollie,
* array variables are not assignable themselves
*/

pub fn main() -> i32 {
	let arr:mut i32[] = [2, 3, 4, 5, 6];

	//INVALID - cannot assign to this array
	arr = arr + 1;

	ret 0;
}
