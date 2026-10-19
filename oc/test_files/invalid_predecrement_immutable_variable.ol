/**
 * Author: Jack Robbins
 * Test a fail case where we predecrement an immutable variable
 */


pub fn main() -> i32 {
	let x:i32 = 5;

	//BAD
	--x;

	OUNIT: [fail_to_compile]
	ret 0;
}
