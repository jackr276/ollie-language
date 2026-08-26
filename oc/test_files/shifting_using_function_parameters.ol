/**
 * Author: Jack Robbins
 * Test a case that used to trip the Ollie compiler up where we are shifting using
 * function parameters. This used to cause a precoloring clash, but now it should
 * work fine
 */


pub fn shifting_by_param(x:i32, y:i32, z:i32, aa:i32) -> i32 {
	ret (z << y) + (aa >> x);
}


pub fn main() -> i32 {
	//Should be 3 << 5 + 12 >> 1 = 96 + 6 = 102
	OUNIT: [exit_status = 102]
	ret @shifting_by_param(1, 5, 3, 12);
}
