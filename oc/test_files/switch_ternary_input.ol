/**
* Author: Jack Robbins
* This program is made for the purposes of testing switch
* with a ternary input
*/

fn main(argc:i32, argv:char**) -> i32{
	let mut x:i32 := 32;

	switch(arg < 2 ? x else arg){
		case 2 -> {
			x := 32;
		}
		case 1 -> {
			x := -3;
		}
		case 4 -> {}
		case 3 -> {
			x := 211;
		}

		case 6 -> {
			x := 22;
		}

		default -> {
			x := x - 22;
		}
	}

	//So it isn't optimized away
	ret x;
}
