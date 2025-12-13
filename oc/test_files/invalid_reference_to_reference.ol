/**
* Author: Jack Robbins
* Test an attempt to assign a reference to another reference - this is illegal
* and should fail
*/

pub fn main() -> i32 {
	let y:mut i32 = 3;
	let z:mut i32 = 3;

	//First assignment - all good
	let a:mut i32& = y;
	let b:mut i32& = z;

	//BAD - you cannot pass references around
	let c:mut i32& = a;

	ret 0;
}
