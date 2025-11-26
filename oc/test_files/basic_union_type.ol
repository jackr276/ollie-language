/**
* Author: Jack Robbins
* This file will test a basic union type in ollie language
*/

//Declare the union type
define union my_union {
	x:i32;
	y:i16;
	ch:char;
} as custom_union;

pub fn main() -> i32{
	declare my_union:mut custom_union;
	
	//Store x
	my_union.x = 32;
	
	//Read as char
	ret my_union.ch;
}
