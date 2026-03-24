/**
* Author: Jack Robbins
* Test the fail case where we have an elaborative param inside of an inlined
* function. This is not allowed ever
*/

//Can't have this
inline fn invalid_inlined(x:i32, y:params i32) -> i32 {
	ret x + y[2] + y[1];
}


pub fn main() -> i32 {
	ret 0;
}
