/**
 * Author: Jack Robbins
 * Test a case that used to trip the Ollie compiler up where we are shifting using
 * function parameters. This used to cause a precoloring clash, but now it should
 * work fine
 */


pub fn shifting_by_param(x:i32, y:i32, z:i32, aa:mut i32) -> i32 {
	//Now test doing it like this
	aa <<= y;
	aa >>= x;

	ret z + aa;
}


pub fn main() -> i32 {
	//Should be ((2 << 5) >> 1) + 4 = 36
	OUNIT: [exit_status = 36]
	ret @shifting_by_param(1, 5, 4, 2);
}
