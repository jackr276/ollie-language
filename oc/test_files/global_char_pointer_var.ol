/**
* Author: Jack Robbins
* Test the compiler's ability to handle global char* variables
*/

let global_str:char* = "I am a global string";

pub fn get_string() -> i32 {
	ret global_str[0];
}

pub fn main() -> i32 {
	ret 0;
}

