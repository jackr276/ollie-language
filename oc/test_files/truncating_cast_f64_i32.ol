/**
 * Author: Jack Robbins
 * Test a truncating cast where we have an f64 going down to an i32
 */

pub fn f64_to_i32(x:f64) -> i32 {
	ret <i32>x;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 123]
	ret @f64_to_i32(123.456d);
}
