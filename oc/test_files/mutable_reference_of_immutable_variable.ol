/**
* Author: Jack Robbins
* Test a fail case where we attempt to make a mutable reference of an immutable variable
*/

pub fn main() -> i32 {
	//Immutable
	let x:i32 = 3;

	//Fail case - can't do this
	let y:mut i32& = x;

	ret y;
}
