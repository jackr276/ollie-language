/**
* Author: Jack Robbins
* Test indexing into a string variable
*/


pub fn index_into_string() -> i32 {
	let string_literal:char* = "Hello World";

	ret string_literal[3] + string_literal[1];
}


pub fn main() -> i32 {	
	ret @index_into_string();
}
