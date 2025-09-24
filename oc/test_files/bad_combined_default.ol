/**
* Author: Jack Robbins
* Testing an invalid attempt to mix C-style and ollie-style switch statements
*/


pub fn main(argc:i32, argv:char**) -> i32 {
	let mut x:i32 = 3;
	let mut y:i32 = 5;

	switch (argc) {
		case 1 -> {
			x += y;
		}

		case 3 -> {
			x -= y;
		}

		case 7 -> {
			x = y;
		}

		//Invalid, we can't mix the two
		default:
			x -= y;
			break;
	}

	ret x;
}
