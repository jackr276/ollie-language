/**
* Author: Jack Robbins
* Test the ability for the logical not system to expand to fit a different sized LR if need be
*/


pub fn expanding_float_lnot(x:f32) -> u64 {
	//Should expand
	ret !x;
}


pub fn main() -> i32 {
	@expanding_float_lnot(3.333);
	ret 0;
}
