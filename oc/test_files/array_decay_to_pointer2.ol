/**
* Author: Jack Robbins
* Test our ability to handle an array decaying to a pointer
*/

pub fn decay_to_pointer(x:i32[5]) -> i32 {
	ret *x;
}

pub fn main() -> i32 {
	let x:i32[] = [1, 2, 3, 4, 5];

	ret @decay_to_pointer(x);
}
