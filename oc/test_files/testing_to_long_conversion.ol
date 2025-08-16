/**
* Author: Jack Robbins
* This file will test the very niche case where we do *not* need a movzx instruction
* to cast up to a u64
*/

fn test_subtraction() -> u64 {
	let mut y:u64 := 32;
	let mut x:i16 := 3;

	y := y - x;
	y := y * x;
	y := y % x;
	ret y;
}


fn test() -> u64 {
	let mut y:u64 := 32;
	let mut x:i32 := 3;

	y := y + x;
	ret y;
}


pub fn main() -> i32 {
	@test();
	ret 0;
}
