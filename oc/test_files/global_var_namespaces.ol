/**
 * Author: Jack Robbins
 * Test our ability to use global variables that are public from other
 * namespaces
 */

namespace sample {
	let pub sample_global:i32 = 5;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 5]
	ret sample::sample_global;
}
