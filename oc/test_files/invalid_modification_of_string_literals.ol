/**
* Author: Jack Robbins
* Test an attempt by the user to grab a mutable reference to a string literal. In ollie,
* string literals go into the read-only data section of the program, and are thus not mutable.
* Their type is char*, not mut char*. As such, any attempt to modify one should be smacked down.
*/


pub fn main() -> i32 {
	//Should fail - string literals are constant
	let x:mut char* = "I am a string literal";

	//Shouldn't even get here
	x[3] = 'a';
	
	ret 0;
}
