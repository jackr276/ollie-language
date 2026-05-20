/**
* Author: Jack Robbins
* Verify that the integer -0 works as expected. The expectation for this
* is that -0 is the same as 0 because Ollie uses 2's complement
*/


pub fn main() -> i32 {
	OUNIT: [console = 0]
	ret -0;
}
