/**
* Author: Jack Robbins
* Test an invalid union access attempt
*/

//Declare the union type
define union my_union {
	x:i32;
	y:i16;
	ch:char;
} as custom_union;

pub fn mut_union(x:mut custom_union*) -> i32 {
	//We'll use the special union pointer accessor here
	x->x = 4;
	x->y = 2;

	//Try accessing an invalid type
	ret x->a;
}


pub fn main() -> i32{
	declare my_union:mut custom_union;
	
	//Store x
	my_union.x = 32;

	@mut_union(&my_union);
	
	//Read as char
	ret my_union.ch;
}
