/**
* Author: Jack Robbins
* A basic functionality test for floating point negation
*/

pub fn negate_float(x:f32) -> f32 {
	ret -x;
}


pub fn main() -> i32 {
	ret @negate_float(3.33);
}
