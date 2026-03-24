/**
* Author: Jack Robbins
* Test an attempt to use sizeof on an elaborative param type. This does not
* work as elaborative params are not known at compile-time and vary with literally
* every call to the function
*/

pub fn invalid_elaborative_type(x:i32, y:i32, elaborative_type:params i32) -> i32 {
	//Not going to work - we can't take this one's size
	let size:i32 = sizeof(elaborative_type);

	ret x + y + size;
}

pub fn main() -> i32 {
	ret 0;
}
