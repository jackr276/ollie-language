/**
* Author: Jack Robbins
* Test the ability to handle array arithmetic
*/

pub fn main() -> i32 {	
	declare arr:mut i32[4];

	//Should work
	let x:mut i32* = arr + 1;

	ret x[3];
}
