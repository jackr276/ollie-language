/**
 * Author: Jack Robbins
 * Test an invalid case where we declare an incomplete array
 */

pub fn main() -> i32 {
	//BAD!
	declare x:i32[];

	OUNIT: [fail_to_compile]
	ret 0;
}
