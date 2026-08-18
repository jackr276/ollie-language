/**
 * Author: Jack Robbins
 * Test a case where we inline the same function multiple times to
 * see how we handle the double copying
 */


inline fn add(x:i32, y:i32) -> i32 {
	ret x + y;
}


pub fn main() -> i32 {
	let x:i32 = 5;
	let y:i32 = 6;
	let z:i32 = 7;

	//Should return 5 + 6 + 6 + 7
	OUNIT: [exit_status = 24]
	ret @add(x, y) + @add(y, z);
}
