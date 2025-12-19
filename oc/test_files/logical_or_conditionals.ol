/**
* Author: Jack Robbins
* Test the compiler's ability to logical and conditionals
* after they're evaluated
*/

pub fn main() -> i32 {
	let x:i32 = 3;
	let y:i32 = 4;

	let z:i32 = x > y || y < 0;

	ret z > 0 || x < y;
}
