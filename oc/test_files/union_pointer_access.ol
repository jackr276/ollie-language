/**
* Author: Jack Robbins
* This file will test a basic union type in ollie language
*/

//Declare the union type
define union my_union {
	mut x:i32;
	mut y:i16;
	mut ch:char;
} as custom_union;

pub fn mut_union(mut x:custom_union*) -> i32 {
	//We'll use the special union pointer accessor here
	x->x := 4;
	x->y := 2;

	ret x->x;
}


pub fn main() -> i32{
	declare my_union:custom_union;
	
	//Store x
	my_union.x := 32;

	@mut_union(&my_union);
	
	//Read as char
	ret my_union.ch;
}
