/**
 * Author: Jack Robbins
 * Test a case where we inline a return by copy function
 */

define struct my_struct {
	x:mut i32;
	y:i64;
	z:f64;
	c:mut i32[5];
} ;


inline fn tester(x:i32, y:i32) -> struct my_struct {
	let ret_by_copy:struct my_struct = {x, y, 5.555, [x, y, x, y, x]};

	ret ret_by_copy;
}


pub fn main() -> i32 {
	let x:i32 = 5;
	let y:i32 = 6;

	let result:struct my_struct = @tester(x, y);

	//Should return 5 + 6 + 5 = 16
	OUNIT: [exit_status = 16]
	ret result:x + result:c[1] + result:c[2];
}
