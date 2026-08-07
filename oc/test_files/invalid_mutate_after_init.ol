/**
 * Author: Jack Robbins
 * Test a case where we mutate after we initialize a variable
 */

pub fn main() -> i32 {
	declare x:i32;

	x = 5;
	//Should fail here it's already been initialized
	x = 7;

	OUNIT: [fail_to_compile]
	ret	x;
}
