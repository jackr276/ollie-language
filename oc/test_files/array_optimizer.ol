/**
* Author: Jack Robbins
* Testing array initializers in ollie
*/

pub fn array_call(arr:i32*) -> i32{
	ret arr[2];
}


pub fn main() -> i32 {
	//The compiler should detect and count the number
	//in the array initializer list.
	let arr:mut i32[] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

	//Make the call
	ret @array_call(arr);
}
