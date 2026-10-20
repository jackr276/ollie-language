/**
 * Author: Jack Robbins
 * Test a case where we're using dereferecing inside of compressed equality
 */


pub fn main() -> i32 {
	let x:i32 = 5;
	let y:mut i32 = 6;
	let z:mut i32* = &y;

	*z += x;

	OUNIT: [exit_status = 11]
	ret *z;
}
