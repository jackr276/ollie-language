/**
* Author: Jack Robbins
* Test how postoperations on constants work
*/

pub fn main() -> i32 {
	let x:i32 = --8;
	let y:i32 = ++2;

	ret x + y;
}
