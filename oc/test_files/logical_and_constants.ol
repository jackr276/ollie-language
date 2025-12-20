/**
* Author: Jack Robbins
* Test the parser's ability to automatically recognize and simplify
* logical and'ing of constants on the spot
*/

define struct my_struct{
	x:mut i32*;
	y:i16;
} as custom_struct;

pub fn and_type_size() -> i64{
	ret 2 && typesize(custom_struct);
}

pub fn expanding_constant_size() -> i64{
	ret 2 && 1;
}

pub fn and_type_size2() -> i32 {
	ret typesize(custom_struct) && 2;
}

pub fn main() -> i32 {
	ret 2 && 0;
}
