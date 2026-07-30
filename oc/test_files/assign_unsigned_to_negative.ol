/**
 * Author: Jack Robbins
 * Test the assignment of a negative number to an unsigned integer - should work just fine
 */


pub fn main() -> i32 {
	//-5 in u8 form is really 251 unsigned
	let ret_val:u8 = -5;

	OUNIT: [exit_status = 251]
	ret <u32>ret_val;
}
