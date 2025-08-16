/**
* Author: Jack Robbins
* Testing array initializers in ollie
*/

pub fn main() -> i32 {
	//The compiler should detect and count the number
	//in the array initializer list.
	let mut arr:i32[] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

	ret arr[3];
}
