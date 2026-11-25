/**
 * Author: Jack Robbins
 * Test a simulation with a truncating conversion
 */

pub fn simulate_branch() -> i32 {
	let x:f64 = 4.33d;

	if(<i16>x < 5) {
		ret 4;
	} else {
		ret 5;
	}
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 4]
	ret @simulate_branch();
}
