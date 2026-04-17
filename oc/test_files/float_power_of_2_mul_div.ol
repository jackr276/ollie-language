/**
* Author: Jack Robbins
* Verify that we do not mistakenly come here when doing float multiply/divide by a power of 2
*/


pub fn float_div_pow_2(f:f32) -> f32 {
	ret f / 4;
}


pub fn float_mul_pow_2(f:f32) -> f32 {
	ret f * 4;
}

pub fn main() -> i32 {
	ret 0;
}
