/**
* Author: Jack Robbins
* Attempt to return a reference type in an invalid way
*/

pub fn invalid_return_reference() -> i32& {
	let x:i32 = 3;

	//Should not work - we don't allow implicit reference
	//taking in this context
	ret x;
}


pub fn main() -> i32 {
	ret 0;
}
