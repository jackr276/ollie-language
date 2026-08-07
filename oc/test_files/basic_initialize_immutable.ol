/**
 * Author: Jack Robbins
 * Test a basic case where we initialize an immutable variable
 */

pub fn main() -> i32 {
	//Uninitialized at first
	declare x:i32;

	//This should be allowed
	x = 5;

	OUNIT: [exit_status = 5]
	ret x;
}
