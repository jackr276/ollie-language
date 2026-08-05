/**
 * Author: Jack Robbins
 * Do a basic bitwise not test
 */


pub fn main() -> i32 {
	let x:i32 = -2;

	OUNIT: [exit_status = 1]
	ret ~x;
}
