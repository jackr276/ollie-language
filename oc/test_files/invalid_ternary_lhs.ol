/**
* Author: Jack Robbins
* This file will test an invalid attempt to assign to a ternary
*/

fn main() -> i32 {
	let mut x:i32 := 3;
	let mut y:i32 := 5
	let mut z:i32 := 6;

	(z == 6 ? x else y) := 32;

	ret x + y;
}
