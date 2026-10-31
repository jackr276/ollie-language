/**
* Author: Jack Robbins
* Verify that we're able to properly inline the same return by copy function twice
* in the same function without causing any issues with the variable mappings
*/


define struct my_struct {
	x:i32;
	y:i32;
	z:i32;
	a:i32;
	b:i32;
	c:i32;
} as custom_struct;


pub inline fn return_by_copy_with_6(x:i32, y:i32, z:i32, a:i32, b:i32, c:i32) -> custom_struct {
	let return_value:custom_struct = {x, y, z, a, b, c};

	ret return_value;
}


pub fn main() -> i32 {
	let x:struct my_struct = @return_by_copy_with_6(1, 2, 3, 4, 5, 6);
	let y:struct my_struct = @return_by_copy_with_6(7, 8, 9, 10, 11, 12);

	OUNIT: [exit_status = 18]
	ret x:c + y:c;
}
