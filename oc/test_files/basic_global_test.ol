/**
* Author: Jack Robbins
* Basic global variable test
*/

//Global variable declaration
declare mut x:i32;

pub fn add_vars(y:i32) -> i64 {
	ret x + y;
}


/**
* Basic case initializing the global variable
*/
pub fn main() -> i32 {
	x = 32;

	ret x;
}
