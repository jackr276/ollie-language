/**
 * Author: Jack Robbins
 * Simulate a branch based on the logical not'ing of a value
 */


pub fn simulate_logical_not_int() -> i32 {
	let x:i32 = 5;

	if(!x) {
		ret 3;
	} else {
		ret 4;
	}
}


pub fn simulate_logical_not_float() -> i32 {
	let x:f32 = 3.333;

	if(!x) {
		ret 5;
	} else {
		ret 6;
	}
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 10]
	ret @simulate_logical_not_int() + @simulate_logical_not_float();
}
