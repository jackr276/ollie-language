/**
* Author: Jack Robbins
* Test the different ways of getting to structs
*/

define struct my_struct {
	c:char;
	x:i32[6];
	y:i64;
} as custom_struct;


pub fn get_to_struct1(structs:custom_struct*) -> i32 {
	ret structs[1]:c;
}

pub fn get_to_struct2(structs:custom_struct**) -> i32 {
	ret structs[1]=>c;
}

pub fn main() -> i32 {	
	ret 0;
}
