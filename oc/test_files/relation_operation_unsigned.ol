/**
 * Author: Jack Robbins
 * Verify that a relational operation does indeed have unsigned comparison forced on it
 */


pub fn compare_signed_unsigned(x:i32, y:u32) -> bool{
	ret x > y;
}


pub fn main() -> i32 {
	let x:i32 = -5;
	let y:u32 = 17;

	/**
	 * Remember that -5 signed is really 42694967291 as an unsigned
	 * value, so this should be true
	 */
	OUNIT: [exit_status = true]
	ret @compare_signed_unsigned(x, y);
}
