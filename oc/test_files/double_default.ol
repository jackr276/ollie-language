/**
* Author: Jack Robbins
* Incorrect dual default statement
*/


pub fn main(argc:i32, argv:char**) -> i32 {
	let mut x:i32 := 3;
	let mut y:i32 := 5;

	switch (argc) {
		default -> {
			x := y * 2;
		}

		case 1 -> {
			x += y;
		}

		case 3 -> {
			x -= y;
		}

		default -> {
			x -= y;
		}
	}

	ret x;
}
