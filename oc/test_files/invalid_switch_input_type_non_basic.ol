/**
 * Author: Jack Robbins
 * Test a case where we are attempting to switch on a non-switch eligible non basic type(i32*)
 */

pub fn main() -> i32 {
	let x:i32 = 5;

	//Should fail - we can't switch on pointers
	switch(&x) {
		case 5 -> {
			ret 5;
		}

		default -> {
			ret 0;
		}
	}

	OUNIT: [fail_to_compile]
}
