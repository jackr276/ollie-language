/**
* Author: Jack Robbins
* Test the parser's ability to do in-flight constant calculations
* with addition and subtraction
*/

define struct my_struct{
	x:mut i32*;
	y:i16;
} as custom_struct;

pub fn shift_type_size() -> i64{
	ret typesize(custom_struct) << 2;
}

pub fn expanding_constant_size() -> i64{
	ret 3 << 3;
}

pub fn shift_type_size2() -> i32 {
	ret typesize(custom_struct) >> 2;
}

pub fn main() -> i32 {
	declare x:mut custom_struct;

	ret sizeof(x) << 3;
}
