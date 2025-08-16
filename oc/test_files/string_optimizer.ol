/**
* Author: Jack Robbins
* This program deals with the handling of string constants
* that are optimized away by the compiler
* 
* We only deal with string constant assignment in this file
* i.e. char* = "string"
*/

//test that string are subscriptable
fn handling_string(a:char*, b:char*) -> char{
	ret a[1] + b[2];
}



fn main(argc:i32, argv:char**) -> i32 {
	let my_string:char* := "Hello";
	//This should be useless and optimized away
	let string_arr:char* := " world";

	ret @handling_string(my_string, "world")
		+ @handling_string("direct", "strings");
}
