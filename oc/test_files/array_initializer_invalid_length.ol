/**
* Author: Jack Robbins
* Testing array initializers in ollie
*/

pub fn main() -> i32 {
	//Should fail - length is invalid
	let mut arr:i32[2] := [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

	ret arr[3];
}
