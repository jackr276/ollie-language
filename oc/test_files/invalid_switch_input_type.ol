/**
 * Author: Jack Robbins
 * Test a case where we are attempting to switch on a non-switch eligible basic type(f32)
 */

pub fn main() -> i32 {
	let x:f32 = 5.55;

	//Should fail - we can't switch on floats
	switch(x) {
		case 5.55 -> {
			ret 5;
		}

		default -> {
			ret 0;
		}
	}
}
