/**
* Author: Jack Robbins
* Test switching on a char
*/

fn tester(param:char) -> i32 {
	let x:mut i32 = 3;

	switch(param){
		case 'a' -> {
			x = 32;
		}
		case 'b' -> {
			x = -3;
		}
		case 'c' -> {}
		case 'd' -> {
			x = 211;
		}

		case 'e' -> {
			x = 22;
		}

		default -> {
			x = x - 22;
		}
	}

	//So it isn't optimized away
	ret x;
}


pub fn main() -> i32{
	ret @tester('a');
}
