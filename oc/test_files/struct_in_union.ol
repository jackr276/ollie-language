/**
* Author: Jack Robbins
* This file will test a struct within a union type in ollie
*/

//Define a struct type
define struct custom {
		x:i32;
		a:i64;
		y:char;
} as my_struct;

//Declare the union type
define union my_union {
	mut x:my_struct; // internal struct type
	mut y:i16;
	mut ch:char;
} as custom_union;


pub fn main() -> i32{
	declare mut my_union:custom_union;
	
	//Store x
	my_union.x:a = 32;
	my_union.x:y = 'a';
	
	//Read as char
	ret my_union.ch + my_union.x:x;
}
