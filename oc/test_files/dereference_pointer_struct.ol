/**
* Author: Jack Robbins
* Test dereferencing a pointer array in ollie
*/

define struct my_struct{
	mut ch:char;
	mut y:i32*;
	mut lch:char;
} as custom_struct;


pub fn main() -> i32 {
	declare mut strct:custom_struct;

	*(strct:y) = 3;

	ret 0;
}
