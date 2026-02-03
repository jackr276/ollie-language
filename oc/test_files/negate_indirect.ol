/**
* Author: Jack Robbins
* Test indirect negation where temporary variables are involved
*/

pub fn indirect_gp_negate(x:i64, y:i64) -> i64 {
	ret -(x + y);
}


pub fn indirect_sse_negate(x:f64, y:f64) -> f64 {
	ret -(x + y);
}


//Dummy
pub fn main() -> i32 {
	ret 0;
}
