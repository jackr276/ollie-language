/**
 * Author: Jack Robbins
 * Test our ability to do a memory copy into an array member
 */

define struct my_struct {
	x:i32[5];
	y:i64;
	z:char;
};


pub fn copy_into_array(x:mut struct my_struct[3], y:mut struct my_struct*) -> void {
	x[2] = *y;
}


pub fn main() -> i32 {	
	declare x:mut struct my_struct[3];
	let y:mut struct my_struct = {[1, 2, 3, 4, 5], 7, 8};

	@copy_into_array(x, &y);

	OUNIT: [exit_status = 5]
	ret x[2]:x[4];
}
