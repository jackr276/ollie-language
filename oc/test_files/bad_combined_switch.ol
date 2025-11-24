/**
* Author: Jack Robbins
* Testing an invalid attempt to mix C-style and ollie-style switch statements
*/


pub fn main(argc:i32, argv:char**) -> i32 {
	let x:mut i32 = 3;
	let y:mut i32 = 5;

	switch (argc) {
		case 1 -> {
			x += y;
		}

		case 3 -> {
			x -= y;
		}

		//Invalid, we can't mix the two
		case 7:
			x = y;
			break;
	

		default -> {
			x -= y;
		}
	}

	ret x;
}
