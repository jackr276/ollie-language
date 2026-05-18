/**
* Author: Jack Robbins
* Test returning a struct by copy when we have 6 parameters. Due to the way that 
* we pass in the return memory address, having 6 parameters for a return by copy
* will force this function to have stack parameters
*/


define struct my_struct {
	x:i32;
	y:i32;
	z:i32;
	a:i32;
	b:i32;
	c:i32;
} as custom_struct;


pub fn return_by_copy_with_6(x:i32, y:i32, z:i32, a:i32, b:i32, c:i32) -> custom_struct {
	let return_value:custom_struct = {x, y, z, a, b, c};

	ret return_value;
}


pub fn main() -> i32 {
	//Grab a function pointer to the one that we want
	let func_ptr:fn(i32, i32, i32, i32, i32, i32) -> custom_struct = return_by_copy_with_6;

	OUNIT: [console = 6]
	ret @func_ptr(1, 2, 3, 4, 5, 6):c;
}
