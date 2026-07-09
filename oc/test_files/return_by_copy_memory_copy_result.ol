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
	//Grab the address of the return value
	let copied_ret_val:custom_struct = @return_struct(1, 2);

	//Should return 1 + 3 = 4
	OUNIT:[exit_status = 4]
	ret copied_ret_val:x + copied_ret_val:y[3];
}
