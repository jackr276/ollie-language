/**
* Author: Jack Robbins
* Test the case where we have an elaborative param in an inlined function
*/

inline fn elaborative_inlined(x:i32, y:params i32) -> i32 {
	ret x + y[2] + y[1];
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 10]
	ret @elaborative_inlined(5, 1, 2, 3);
}
