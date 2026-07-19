/**
 * Author: Jack Robbins
 * Test how the Ollie compiler handles an unsequenced operation. We should be guaranteeing execution
 * from left to right
 */



pub fn main() -> i32 {
	let x:mut i32 = 1;

	/**
	 * This should do x = 1 + 2 in the end
	 */
	x += (x = 2);

	OUNIT: [exit_status = 3]
	ret x ;
}
