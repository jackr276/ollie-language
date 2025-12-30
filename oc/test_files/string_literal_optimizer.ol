/**
* Author: Jack Robbins
* Test optimizing a string literal
*/


pub fn index_into_string() -> i32 {
	let string_literal:char* = "Hello World";

	//Totally useless, should all be optimized away
	let x:i32 = string_literal[3] + string_literal[1];

	ret 0;
}


pub fn main() -> i32 {	
	ret @index_into_string();
}
