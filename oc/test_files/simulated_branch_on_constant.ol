/**
 * Author: Jack Robbins
 * Simulate a branch based on a constant
 */


pub fn simulate_int() -> i32 {
	let x:i32 = 5;

	if(5) {
		ret 3;
	} else {
		ret 4;
	}
}


pub fn simulate_float() -> i32 {
	if(3.333) {
		ret 5;
	} else {
		ret 6;
	}
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 8]
	ret @simulate_int() + @simulate_float();
}
