/**
 * Author: Jack Robbins
 * Test a contrived edge case where we have more than 1 unsequenced operation in a calculation
 */

pub fn main() -> i32 {
	let x:mut i32 = 1;

	let y:i32 = x + (x = 2) + (x = 3) + (x = 4);

	//Should become x = 1 + 2 + 3 + 4 = 10
	OUNIT: [exit_status = 10]
	ret y;
}
