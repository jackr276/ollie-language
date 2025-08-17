/**
* Author: Jack Robbins
* Testing a valid void declaration.
*/


pub fn main(void) -> i32{
	let mut x:i32 := 32;

	switch(x){
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

	switch(x){
		default -> {
			let i:i32 := 2;
		}
		case 2 -> {
			let i:i32 := 3;
		}
	}
	
	

	//So it isn't optimized away
	ret x;
}
