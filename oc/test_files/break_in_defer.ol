/**
* Author: Jack Robbins
* This file tests the case where an invalid break is attempted
*/

pub fn main() -> i32 {
	let mut x:i32 = 3;
	let mut y:i32 = 3;

	//valid
	defer {
		y++;
	}

	y = y - 11;

	defer {
		x -= 32;
		break; //invalid
	}


	ret x << y;
}
