/**
 * Author: Jack Robbins
 * Test a case where we have a struct initializer that also has a memory copy internally
 */

define struct nested_struct {
	x:i32[5];
	y:i64;
};


define struct my_struct {
	array:i32[5];
	nester:mut struct nested_struct;
	y:i64;
};


pub fn populate_struct() -> struct my_struct {
	let nester:struct nested_struct = {[1, 2, 3, 4, 5], 8};

	ret {[6, 7, 8, 9, 10], nester, 11};
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 5]
	ret @populate_struct():nester:x[4];
}
