/**
* Author: Jack Robbins
* Attempt to use an invalid type as an elaborative param. As a reminder, only
* primitives and pointers may be elaborated on
*/

//Is going to fail - can't have an array like that
pub fn invalid_elaborative_type(x:i32, y:i32, bad_type:params i32[5]) -> i32 {
	ret x + y;
}

pub fn main() -> i32 {
	ret 0;
}
