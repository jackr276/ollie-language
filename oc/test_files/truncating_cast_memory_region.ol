/**
 * Author: Jack Robbins
 * Test a case where we have a truncating cast from a memory region
 */

pub fn truncate_from_memory(x:f64*) -> i8 {
	ret <i8>*x;
}


pub fn main() -> i32 {
	let x:f64 = 4.44d;

	OUNIT: [exit_status = 4]
	ret @truncate_from_memory(&x);
}
