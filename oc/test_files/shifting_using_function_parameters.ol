/**
 * Author: Jack Robbins
 * Test a case that used to trip the Ollie compiler up where we are shifting using
 * function parameters. This used to cause a precoloring clash, but now it should
 * work fine
 */


pub fn shifting_by_param(x:i32, y:i32, z:i32) -> i32 {
	ret (z << y) + (z >> x);
}


pub fn main() -> i32 {
	//Should be 3 << 5 + 3 >> 1 = 96 + 1 = 81
	OUNIT: [exit_status = 97]
	ret @shifting_by_param(5, 1, 3);
}
