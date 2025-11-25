/**
* Author: Jack Robbins
* Test handling of an empty body for loop
*/


pub fn main() -> i32 {
	let x:mut i32 = 0;

	for(; x < 333; x++){}

	ret x;
}
