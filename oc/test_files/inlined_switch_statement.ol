/**
 * Author: Jack Robbins
 * Test the case where we have to inline a switch statement and all of the baggage
 * that comes with it
 */


pub inline fn switch_to_be_inlined(y:i32, z:i32) -> i32 {
	let result:mut i32 = 0;

	switch(y) {
		case 5:
			result += z;
			ret result;

		case 7:
			result -= z;
			ret result;

		case 15:
			result = z;
			//Fall through

		case 11:
			result *= 2;
			ret result;

		default:
			result = 5;
			ret result;
	}
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 28]
	ret @switch_to_be_inlined(15, 14);
}
