/**
* Author: Jack Robbins
* Test returning a string literal
*/


/**
* Should return the address of the string literal,
* it will be a rip-relative address
*/
pub fn return_string_literal() -> char* {
	ret "String Literal";
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
