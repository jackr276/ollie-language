/**
* Author: Jack Robbins
* Test the fail case where we're using the "raises" keyword with no bang
*/

define error custom_error;

pub fn sample() -> i32 {
	ret 0;
}

pub fn main() -> i32 {
	let x:fn() -> i32 raises(custom_error) = sample;

	ret @x();
}
