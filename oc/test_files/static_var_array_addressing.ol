/**
* Author: Jack Robbins
* Test initializing and indexing into an array that is static
*/


pub fn index_into_static(x:i32, y:i32) -> i32 {
	let static arr:mut i32[] = [1, 2, 3, 4, 5];

	//Store with variable offset
	arr[y] = 5;

	//Store with constant offset
	arr[2] = 3;

	//Load with variable offset
	let result1:i32 = arr[x];

	//Load with constant offset
	let result2:i32 = arr[1];

	ret result1 + result2;
}


pub fn main() -> i32 {
	ret 0;
}
