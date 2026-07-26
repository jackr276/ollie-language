/**
 * Author: Jack Robbins
 * Test an instance where we are truncating from an f64 down to an f32. This is a basic
 * case for float trunaction
 */


pub fn f64_to_f32(x:f64, y:f64) -> i32 {
	ret <f32>(x + y);
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 8]
	ret @f64_to_f32(3.3d, 4.8d);
}
