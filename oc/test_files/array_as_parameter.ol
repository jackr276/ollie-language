/**
* Author: Jack Robbins
* Test an array as a parameter
*/

pub fn array_as_param(x:i32[5]) -> i32 {
	ret x[3];
}

pub fn main() -> i32 {
	let x:i32[] = [1, 2, 3, 4, 5];

	ret @array_as_param(x);
}
