/**
* Author: Jack Robbins
* Testing nested switch statement functionality
*/


pub fn main(argc:i32, argv:char**) -> i32 {
	let mut x:i32 := 3;
	let mut y:i32 := 5;

	switch (argc) {
		case 1 -> {
			x += y;
		}

		case 3 -> {
			//Nested switch
			switch(y) {
				case 11 -> {
					x *= y;
				}
				
				case 17 -> {
					x /= y;
				}

				default -> {
					x <<= y;
				}
			}
		}

		default -> {
			x -= y;
		}
	}

	ret x;
}
