/**
 * Author: Jack Robbins
 * Test our ability to do casting with bitwise not
 */


pub fn main() -> i32 {
	let x:f32 = -1.11111;

	/**
	 * Should have a 0 in the sign bit
	 */
	let y:i32 = ~(<i32>x);
	let sign_bit_mask:i32 = 1 << 31;

	//Sign bit should be 0 post flip
	OUNIT: [exit_status = 1]
	ret (y & sign_bit_mask) == 0;
}

