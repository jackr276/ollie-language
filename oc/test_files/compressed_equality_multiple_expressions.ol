/**
 * Author: Jack Robbins
 * Test a compressed equality operation that has more than one expression on the RHS
 */

pub fn main() -> i32 {
	let x:mut i32 = 5;
	let y:i32 = 7;
	let z:i32 = 4;

	//Should become x = 5 + 7 * 4 = 5 + 28 = 33
	x += y * z;

	OUNIT: [exit_status = 33]
	ret x;
}
