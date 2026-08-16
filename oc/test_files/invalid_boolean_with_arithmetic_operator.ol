/**
 * Author: Jack Robbins
 * Test an invalid attempt to use a boolean with an arithmetic operator
 */

pub fn bool_relational(x:bool) -> bool {
	//INVALID
	ret x + 5;
}

pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret 0;
}
