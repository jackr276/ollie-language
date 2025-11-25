/**
* Author: Jack Robbins
* Testing nested switch statement functionality
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

		default -> {
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
	}

	ret x;
}
