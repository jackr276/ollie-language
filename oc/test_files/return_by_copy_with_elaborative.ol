/**
* Author: Jack Robbins
* Test returning by copy when we have an elaborative param type as well
*/


define struct my_struct {
	x:i32;
	y:i32;
	z:i32;
	a:i32;
	b:i32;
	c:i32;
} as custom_struct;


pub fn return_by_copy_with_elaborative(array:params i32) -> custom_struct {
	let return_value:custom_struct = {array[0], array[1], array[2], array[3], array[4], array[5]};

	ret return_value;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 6]
	ret @return_by_copy_with_elaborative(1, 2, 3, 4, 5, 6):c;
}
