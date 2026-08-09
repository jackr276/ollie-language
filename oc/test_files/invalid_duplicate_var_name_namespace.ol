/**
 * Author: Jack Robbins
 * Test a case where we have a duplicate variable name in a namespace
 */

namespace my_space{
	let pub var:i32 = 5;
	//DUPLICATE!
	let pub var:i32 = 6;
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret 0;
}
