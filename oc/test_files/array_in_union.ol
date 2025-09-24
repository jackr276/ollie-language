/**
* Author: Jack Robbins
* This file will test an array within a union type in ollie
*/

//Declare the union type
define union my_union {
	mut x:i32[5];
	mut y:i16;
	mut ch:char;
} as custom_union;


pub fn main() -> i32{
	declare my_union:custom_union;
	
	//Store x
	my_union.x[2] = 32;
	
	//Read as char
	ret my_union.ch + my_union.x[3];
}
