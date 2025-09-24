/**
* Author: Jack Robbins
* This file will test a the parser's handling of string initializers
* with invalid lengths
*/


fn string_init() -> char* {
	//Invoke the string initializer with an extremely common error -
	// forgetting that the string's true length is actually one more than
	// what's shown due to the \0
	let my_str:char[11] = "Hello world";
	let ptr:char* = my_str;

	ret ptr;
}


pub fn main(argc:i32, argv:char**) -> i32 {
	@string_init();

	ret 0;
}
