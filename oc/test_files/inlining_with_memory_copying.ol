/**
 * Author: Jack Robbins
 * Test a basic case where we do memory copying in an inlined function
 */

define struct my_struct {
	x:mut i32;
	y:i64;
	z:f64;
	c:char;
};


inline fn copy_over(x:i32, y:i64, z:f64, c:char) -> i32{
	declare dest:mut struct my_struct;
	declare dest2:mut struct my_struct;

	//First create the struct
	let source:struct my_struct = {x, y, z, c};

	//Perform the actual copy
	dest = source;

	//Reassign
	source:x = 5;

	//Now go for destination2
	dest2 = source;

	ret dest:x + dest2:x + <i32>dest2:y;
}


pub fn main() -> i32 {
	//Should return 11 + 5 + 88 = 104
	OUNIT: [exit_status = 104]
	ret @copy_over(11, 88, 5.55, 'a');
}

