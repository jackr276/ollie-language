/**
* Author: Jack Robbins
* Testing an attempt to reassign a reference type. Reference types, mutable or not, may
* only be assigned at intialization
*/

pub fn main() -> i32 {
	let y:mut i32 = 3;
	let z:mut i32 = 3;

	//First assignment - all good
	let x:mut i32& = y;

	//Bad - can't reassign a reference
	x = z;

	ret 0;
}
