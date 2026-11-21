/**
 * Author: Jack Robbins
 * Test our ability to do constant coercion in multiplication
 */


fn tester(i:i16) -> u32 {
	ret 3s * i;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 15]
	ret @tester(5);
}
