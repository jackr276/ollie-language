/**
* Author: Jack Robbins
* Test an invalid array to pointer conversion
*/

pub fn main() -> i32 {
	//Array of 5 pointers
	declare x:mut i32[5];

	//This should not work
	let y:mut i32** = &x;
	

	ret 0;
}
