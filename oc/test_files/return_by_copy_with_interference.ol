/**
 * Author: Jack Robbins
 * Test a case where we return by copy where there is interference
 */


define struct my_struct {
	x:mut i32;
	y:mut i32[4];
	z:mut char;
} as custom_struct;


pub fn ret_with_interference2(x:i32, y:i32) -> struct my_struct {
	let ret_val:struct my_struct = {x, [x, y, x, y], 'a'};

	ret ret_val;
}


pub fn ret_with_interference(x:i32, y:i32) -> struct my_struct {
	let ret_val:struct my_struct = @ret_with_interference2(x, y);

	ret ret_val;
}



pub fn main() -> i32 {
	OUNIT: [exit_status = 5]
	ret @ret_with_interference(5, 4):y[2];
}
