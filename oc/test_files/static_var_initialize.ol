/**
* Author: Jack Robbins
* Test our ability to initialize a static variable. Static variables are initialized in the same way
* that global variables
*/


/**
* Initialize a static int to a compile-time constant
*/
pub fn static_var_init(x:i32) -> i32 {
	let static static_int:mut i32 = 5;

	static_int += x;
	
	ret static_int;
}


pub fn main() -> i32 {
	ret 0;
}
