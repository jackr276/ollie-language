/**
* Author: Jack Robbins
* This file is meant to test all compressed equality operators
*/

fn tester() -> i32 {
	ret 0;
}


fn main() -> i32 {
	let mut x:i32 := 3;
	let mut y:i32 := 23;

	x <<= y * 2;
	
	ret x;
}

