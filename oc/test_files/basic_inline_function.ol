/**
* Author: Jack Robbins
* Test the parser's ability to validate and handle a basic inline function
*/

inline fn example_inline(x:i32, y:i32) -> i32 {
	ret x + y;
}


pub fn main() -> i32 {
	let x:i32 = @example_inline(3, 5);

	OUNIT: [exit_status = 8]
	ret x;
}
