/**
* Author: Jack Robbins
* Test an attempt to mix enums together
*/

define enum my_enum {
	A = 88,
	B, // This is invalid. If one is user defined, then they all are
	C = 'a',
	D = 99,
	E = 'b'
} as custom_enum;

fn tester(mut param:custom_enum) -> i32{
	let mut x:i32 = 32;

	switch(param){
		case A -> {
			x = 32;
		}
		case B -> {
			x = -3;
		}
		case C -> {}
		case D -> {
			x = 211;
		}

		case E -> {
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
	@tester(D);
}
