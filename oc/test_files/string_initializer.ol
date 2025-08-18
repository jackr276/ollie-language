/**
* Author: Jack Robbins
* This file will test a string initializer in Ollie
*/


fn string_init() -> char* {
	//Invoke the string initializer
	let my_str:char[] := "Hello world";
	let ptr:char* := my_str;

	ret ptr;
}


fn main(argc:i32, argv:char**) -> i32 {
	@string_init();

	ret 0;
}
