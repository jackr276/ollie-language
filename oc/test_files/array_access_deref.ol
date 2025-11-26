/**
* Author: Jack Robbins
* Testing array initializers in ollie
*/

pub fn access_array(a:mut i32**) -> i32{
	(*a)[1] = 3;
	(*a)[2] = 3;

	ret (*a)[2];
}


pub fn main() -> i32 {
	//The compiler should detect and count the number
	//in the array initializer list.
	let arr:mut i32[] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

	let x:mut i32** = &arr;

	//TODO THIS IS NOT WORKING
	@access_array(x);

	ret arr[3];
}
