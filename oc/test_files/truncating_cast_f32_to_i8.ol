/**
 * Author: Jack Robbins
 * Test a case where we are truncating down from an f32 to an i8
 */

pub fn f32_to_i8(x:f32) -> i8 {
	ret <i8>x;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 175]
	ret @f32_to_i8(175.555555);
}
