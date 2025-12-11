/**
* Author: Jack Robbins
* Test a unique case where we want to initialize
* a global variable with the typesize() or sizeof()
* commands. This *should* work just fine
*/

define struct my_struct{
	x:i32[22];
	y:i64;
	c:u8;
} as custom_struct;

//Global variable x
declare x:mut i32*;

//This should work
let x_size:mut i32 = sizeof(x);

//This should also work
let struct_size:mut i32 = typesize(custom_struct);

//Dummy
pub fn main() -> i32 {
	ret x_size + struct_size;
}


