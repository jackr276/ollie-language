/**
 * Author: Jack Robbins
 * Test a case where we truncate from an i64 down to an f32
 */


pub fn i64_to_f32(x:i64) -> f32 {
	ret <f32>x;
}


pub fn main() -> i32 {
	let x:i64 = 222;

	OUNIT: [exit_status = 222]
	ret @i64_to_f32(x);
}
