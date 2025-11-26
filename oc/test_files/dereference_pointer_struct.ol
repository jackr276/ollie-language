/**
* Author: Jack Robbins
* Test dereferencing a pointer array in ollie
*/

define struct my_struct{
	ch:mut char;
	y:mut i32*;
	lch:mut char;
} as custom_struct;


pub fn main() -> i32 {
	declare strct:mut custom_struct;

	*(strct:y) = 3;

	ret 0;
}
