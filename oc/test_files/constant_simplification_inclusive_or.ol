/**
* Author: Jack Robbins
* Test the parser's ability to do in-flight constant calculations
* with inclusive or
*/

define struct my_struct{
	x:mut i32*;
	y:i16;
} as custom_struct;

/**
* Custom struct is 16 bits - 10000
* 0x2 is 00010
* 10000 | 00010 = 10010 = 18
*/
pub fn or_type_size() -> i64{
	ret 0x2 | typesize(custom_struct);
}

/**
* 3 - 0011 
* 5 - 0101
* 0011 | 0101 = 0111 =  7
*/
pub fn expanding_constant_size() -> i64{
	ret 3 | 5;
}

/**
* 0  - 00000
* 16 - 10000
* 00000 | 10000 = 10000 =  16
*/
pub fn or_type_size2() -> i32 {
	//Should be a negative
	ret 0 | typesize(custom_struct);
}

/**
* 1  - 00001
* 16 - 10000
* 00001 | 10000 = 10001 =  17
*/
pub fn main() -> i32 {
	declare x:mut custom_struct;

	//More constant simplification
	ret sizeof(x) | 0x001;
}
