/**
* Author: Jack Robbins
* This file will test a string initializer in Ollie
*/

alias char as character;


fn string_init() -> char {
	//Invoke the string initializer
	let my_str:character[] = "The quick brown fox";

	//And return it
	ret my_str[2];
}


pub fn main(argc:i32, argv:char**) -> i32 {
	@string_init();

	ret 0;
}
