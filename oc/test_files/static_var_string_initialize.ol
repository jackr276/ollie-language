/**
* Author: Jack Robbins
* Test our ability to initialize a static variable. Static variables are initialized in the same way
* that global variables
*/


/**
* Initialize a static string to a compile-time constant
*/
pub fn static_var_init(x:i32) -> i32 {
	let static static_string:char* = "Hello World";

	ret static_string[x];
}


pub fn main() -> i32 {
	ret 0;
}
