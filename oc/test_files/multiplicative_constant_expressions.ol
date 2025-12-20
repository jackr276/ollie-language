/**
* Author: Jack Robbins
* Test the parser's ability to do in-flight constant calculations
* with mulitplication, division and modulo
*/

define struct my_struct{
	x:mut i32*;
	y:i16;
} as custom_struct;

//Multiplying with typesize, should be just one number
pub fn mult_type_size() -> i64{
	ret 2 * typesize(custom_struct);
}

//Dividing with typesize, should be just one number
pub fn div_type_size() -> i32 {
	ret typesize(custom_struct) / 2;
}

//Modulo with typesize, should be just one number
pub fn mod_type_size() -> i32 {
	ret typesize(custom_struct) % 3;
}

pub fn main() -> i32 {
	declare x:mut custom_struct;

	//More constant simplification
	ret sizeof(x) * 8 / 4;
}
