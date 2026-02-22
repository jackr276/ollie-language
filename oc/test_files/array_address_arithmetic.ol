/**
* Author: Jack Robbins
* Test the ability to handle array arithmetic when
* we're using the address of an array
*/

pub fn main() -> i32 {	
	declare arr:mut i32[4];

	//This should trigger an addition by
	//16 because the size of the array itself
	//is 16
	let x:mut i32[4]* = &arr + 1;

	ret (*x)[3];
}
