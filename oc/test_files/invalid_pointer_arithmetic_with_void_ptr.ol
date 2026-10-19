/**
 * Author: Jack Robbins
 * Test a case where we try doing invalid pointer math with a void pointer
 */


pub fn main() -> i32{
	let x:i32 = 5;
	let y:i32* = &x;

	let z:void* = y;

	OUNIT: [fail_to_compile]
	//BAD - can't do this with a void pointer
	ret *(z + 3);
	
}
