/**
* Author: Jack Robbins
* Test the case where we have a very basic static variable
*/

pub fn reentrant(adder:i32) -> i32 {
	let static x:mut i32 = 5;

	x += adder;

	ret x;
}


pub fn main() -> i32 {
	@reentrant(1);
	@reentrant(2);
	@reentrant(3);

	ret @reentrant(4);
}
