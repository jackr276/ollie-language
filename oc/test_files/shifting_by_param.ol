/**
* Author: Jack Robbins
* Use case where we are shifting by a function parameter
*/

/**
* A shift tester function
*/
fn tester(x:i32, j:i32) -> i32 {
	ret x >> j;
}

/**
* A shift tester function with the
* parameters reversed
*/
fn tester2(x:i32, j:i32) -> i32 {
	ret j << x;
}

pub fn main() -> i32 {
	//Just invoke the shifter
	ret @tester(3, 5) + @tester2(5, 3);
}
