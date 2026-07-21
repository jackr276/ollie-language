/**
 * Author: Jack Robbins
 * Test a valid use case for an Ollie truncating cast
 */


pub fn truncating_cast(x:i64) -> i16 {
	let y:i64 = 5;


	ret <i16>(x + y);
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 20]
	ret @truncating_cast(15);
}
