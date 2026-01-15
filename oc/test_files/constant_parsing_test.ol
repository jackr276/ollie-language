/**
* Author: Jack Robbins
* Tiny test to see how we deal with constant parsing
*/


pub fn main() -> i32 {
	let y:i32 = 33;
	//Should parse fine
	let x:i32 = y - -3;

	ret x + y;
}
