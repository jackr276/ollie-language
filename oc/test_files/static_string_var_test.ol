/**
* Author: Jack Robbins
* Test our ability to handle static string variables
*/


pub fn static_string_var(x:i32) -> i32 {
	let static str_var:char* = "Hello World";

	ret str_var[x];
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
