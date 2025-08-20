/**
* Author: Jack Robbins
* This file will test a string initializer in Ollie
*/

alias char as character;


fn string_init() -> char* {
	//Invoke the string initializer
	let my_str:character[20] := "The quick brown fox";

	//Get a pointer to this value
	let ptr:char* := my_str;

	//And return it
	ret ptr;
}


pub fn main(argc:i32, argv:char**) -> i32 {
	@string_init();

	ret 0;
}
