/**
* Author: Jack Robbins
* Test a basic negation instruction
*/

pub fn negate_int(x:i32) -> i32 {
	ret -x;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 1]
	ret @negate_int(-1);
}
