/**
* Author: Jack Robbins
* Testing nested switch statement functionality
* with c-style switches
*/


pub fn main(argc:i32, argv:char**) -> i32 {
	let mut x:i32 := 3;
	let mut y:i32 := 5;

	switch (argc) {
		case 1 -> {
			//Fall through
			x += y;
		}

		case 3 -> {
			//Nested ollie style switch switch
			switch(y) {
				case 11:
					x *= y;
					break;
				
				case 17:
					x /= y;
					break when(x == 5);

				default:
					x <<= y;
					//Test with no break
			}
		}

		default -> {
			x -= y;
		}
	}

	ret x;
}
