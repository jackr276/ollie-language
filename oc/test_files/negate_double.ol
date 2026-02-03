/**
* Author: Jack Robbins
* Test the negation of a double using our special system
*/


pub fn negate_double(x:f64) -> f64 {
	ret -x;
}


pub fn main() -> i32 {
	@negate_double(3.333D);

	ret 0;
}
