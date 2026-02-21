/**
* Author: Jack Robbins
* Test how we initialize a floating point 0 const. This should be done using pxor, not a constant
*/


pub fn float_0_const() -> f32 {
	ret 0.0;
}


pub fn double_0_const() -> f64 {
	ret 0.0;
}


pub fn main() -> i32 {
	ret 0;
}
