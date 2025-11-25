/**
* Author: Jack Robbins
* This file tests the case where an invalid return in defer is attempted
*/

pub fn main() -> i32 {
	let x:mut i32 = 3;
	let y:mut i32 = 3;

	//valid
	defer {
		y++;
	}

	y = y - 11;

	defer {
		ret y;
	}


	ret x << y;
}
