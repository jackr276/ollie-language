/**
 * Author: Jack Robbins
 * Test a really complex case of an initializer that calls out to functions for its
 * values
 */

define struct my_struct {
	x:i32[5];
	y:i64;
	z:f64;
};


pub fn ret_int(x:i32) -> i32 {
	ret x;
}


pub fn ret_double(x:f64) -> f64 {
	ret x;
}


pub fn return_struct() -> struct my_struct {
	ret {[@ret_int(5), @ret_int(4), 3, 2, 1], @ret_int(16), @ret_double(5.55)};
}


pub fn main() -> i32 {
	let result:struct my_struct = @return_struct();

	OUNIT: [exit_status = 25]
	ret result:x[0] + result:x[1] + <i32>result:y;
}
