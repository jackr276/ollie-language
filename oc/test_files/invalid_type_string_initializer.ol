/**
* Author: Jack Robbins
* This file will test a the parser's handling of string initializers
* with invalid types. For instance, trying to do something like:
*    let a:i16[] = "hi";
*/


fn string_init() -> char* {
	//Attempt to use a string initiailizer for a non-char[]. This will fail.
	//In Ollie, string initializers only work for char[]
	let my_str:i16[11] = "Hello world";
	let ptr:char* = my_str;

	ret ptr;
}


pub fn main(argc:i32, argv:char**) -> i32 {
	@string_init();

	ret 0;
}
