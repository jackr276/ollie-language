/**
* Author: Jack Robbins
* Testing OC's handling of a duplicated enum
*/

define enum my_enum {
	A = 3,
	B = 4,
	C = 22,
	D = 4
} as custom_enum;

fn tester(param:custom_enum) -> i32{
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

		case 11 -> {
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
	@tester(A);
}
