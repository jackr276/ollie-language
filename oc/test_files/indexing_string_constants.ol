/**
* Author: Jack Robbins
* Testing the indexing of string constants in Ollie
*/

alias char* as string;

pub fn main() -> i32 {
	let my_str:string = "Hello world";

	ret my_str[2] + my_str[3];
}
