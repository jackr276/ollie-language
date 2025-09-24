/**
* Author: Jack Robbins
* Testing nested switch statement functionality when said
* nested switch is optimized away
*/


pub fn main(argc:i32, argv:char**) -> i32 {
	let mut x:i32 = 3;
	let mut y:i32 = 5;
	//This will be totally useless
	let mut z:i32 = 0;

	switch (argc) {
		case 1 -> {
			x += y;
		}

		case 3 -> {
			//Nested switch
			switch(y) {
				case 11 -> {
					z *= y;
				}
				
				case 17 -> {
					z /= y;
				}

				default -> {
					z <<= y;
				}
			}
		}

		default -> {
			x -= y;
		}
	}

	//Not using z at all, so in theory, the entire nested switch
	//is completely usless
	ret x;
}
