/**
* Author: Jack Robbins
* Test the parser's ability to validate and handle a basic inline function
* TODO - the actual inlining is not yet complete - just the parser-side validations
*/

inline fn example_inline(x:i32, y:i32) -> i32 {
	ret x + y;
}


pub fn main() -> i32 {
	let x:i32 = @example_inline(3, 5);

	ret x;
}
