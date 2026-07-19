/**
 * Author: Jack Robbins
 * Test a compressed equality operation that has more than one expression on the RHS
 */

pub fn main() -> i32 {
	let x:mut i32 = 5;
	let y:mut i32 = 7;
	let z:mut i32 = 4;

	//Should become x = 5 + 8 * 5 = 5 + 40 = 45
	x += (y = 8) * (z = 5);

	OUNIT: [exit_status = 45]
	ret x;
}
