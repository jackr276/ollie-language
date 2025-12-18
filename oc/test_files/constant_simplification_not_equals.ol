/**
* Author: Jack Robbins
* Test the parser's ability to do in-flight constant calculations
* with exclusive or
*/

define struct my_struct{
	x:mut i32*;
	y:i16;
} as custom_struct;

pub fn ne_type_size() -> i64{
	ret 0x2 != typesize(custom_struct);
}

pub fn expanding_constant_size() -> i64{
	ret 3 != 5;
}

pub fn equals_type_size2() -> i32 {
	ret 14 != typesize(custom_struct);
}

pub fn main() -> i32 {
	declare x:mut custom_struct;

	//More constant simplification
	ret sizeof(x) != 0x10;
}
