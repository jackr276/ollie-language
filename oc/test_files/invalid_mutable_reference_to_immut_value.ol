/**
* Author: Jack Robbins
* Test an attempt to grab a mutable reference to an immutable value
* This should fail
*/

pub fn main() -> i32 {
	let y:i32 = 3;

	//Invalid assignment
	let x:mut i32& = y;

	ret 0;
}
