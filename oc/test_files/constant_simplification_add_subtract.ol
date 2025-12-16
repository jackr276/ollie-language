/**
* Author: Jack Robbins
* Test the parser's ability to do in-flight constant calculations
* with addition and subtraction
*/

define struct my_struct{
	x:mut i32*;
	y:i16;
} as custom_struct;

//Adding with typesize, should be just one number
pub fn add_type_size() -> i64{
	ret 2 + typesize(custom_struct);
}

pub fn expanding_constant_size() -> i64{
	ret 3 + 3;
}

//Subtracting with typesize, should be just one number
pub fn sub_type_size() -> i32 {
	//Should be a negative
	ret 2 - typesize(custom_struct);
}

pub fn main() -> i32 {
	declare x:mut custom_struct;

	//More constant simplification
	ret sizeof(x) + 33;
}
