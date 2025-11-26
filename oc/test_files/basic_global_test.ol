/**
* Author: Jack Robbins
* Basic global variable test
*/

//Global variable declaration
declare x:mut i32;

pub fn add_vars(y:i32) -> i64 {
	//We must assign first
	x = 3;

	//Then add
	ret x + y;
}

pub fn reassign_global(z:i32) -> void {
	x = z;
}


/**
* Basic case initializing the global variable
*/
pub fn main() -> i32 {
	x = 32;

	ret x;
}
