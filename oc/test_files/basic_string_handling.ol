/**
* Author: Jack Robbins
* This program deals with basic string handling in Ollie
*/

//test that string are subscriptable
fn handling_string(a:char*, b:char*) -> char{
	ret a[1] + b[2];
}



fn main(argc:i32, argv:char**) -> i32 {
	let my_string:char* := "Hello";
	let string_arr:char[5] := " world";

	ret @handling_string(my_string, string_arr);
}
