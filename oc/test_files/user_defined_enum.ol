/**
* Author: Jack Robbins
* Testing OC's handling of a user-defined enum
*/

define enum my_enum {
	A = 88,
	B = 89,
	C = 'a',
	D = 99,
	E = 'b'
} as custom_enum;

fn tester(param:mut custom_enum) -> i32{
	let x:mut i32 = 32;

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
