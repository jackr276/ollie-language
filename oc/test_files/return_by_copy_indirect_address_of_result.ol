/**
* Author: Jack Robbins
* Test returning via copy and then memory copying the actual result of said return
* Such cases like these are very important in overall functionality verification for
* us. This is an OUNIT compatible test
*/

define struct my_struct {
	x:i32;
	y:i32[4];
	z:char;
} as custom_struct;


pub fn return_struct(x:i32, y:i32) -> custom_struct {
	let return_value:custom_struct = {x, [y, 1, 2, 3], 'a'};

	ret return_value;
}


pub fn main() -> i32 {
	//Grab a function pointer to the thing we want to test
	let func_ptr:fn(i32, i32) -> custom_struct = return_struct;

	//Grab the address of the return value
	let ret_val_ptr:custom_struct* = &(@func_ptr(1, 2));

	//Should return 1 + 3 = 4
	OUNIT:[exit_status = 4]
	ret ret_val_ptr=>x + ret_val_ptr=>y[3];
}
