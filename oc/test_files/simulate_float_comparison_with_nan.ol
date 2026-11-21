/**
 * Author: Jack Robbins
 * Test the simulator where we're dealing with float comparions that have
 * NaN as one of the operands. This test covers all of the relational operators
 */

pub fn simulate_equals_bin_op() -> i32 {
	let x:f32 = 0.0 / 0.0;
	let y:f32 = 7.25;

	if(x == y) {
		ret -1;
	} else {
		ret 11;
	}
}


pub fn simulate_equals_bin_op_with_const() -> i32 {
	let x:f32 = 0.0 / 0.0;

	if(x == -1.012) {
		ret 3;
	} else {
		ret 5;
	}
}


pub fn simulate_not_equals_bin_op() -> i32 {
	let x:f32 = 0.0 / 0.0;
	let y:f32 = 0.01;

	if(x != y) {
		ret 10;
	} else {
		ret -1555;
	}
}


pub fn simulate_not_equals_bin_op_with_const() -> i32 {
	let x:f32 = 0.0 / 0.0;

	if(x != 17.1) {
		ret 3;
	} else {
		ret 5;
	}
}


pub fn simulate_g_than_bin_op() -> i32 {
	let x:f32 = 0.0 / 0.0;
	let y:f32 = 9.99;

	if(x > y) {
		ret -3333333;
	} else {
		ret 5;
	}
}


pub fn simulate_g_than_bin_op_with_const() -> i32 {
	let x:f32 = 0.0 / 0.0;

	if(x > -11.1232) {
		ret 3;
	} else {
		ret 5;
	}
}


pub fn simulate_l_than_bin_op() -> i32 {
	let x:f32 = 0.0 / 0.0;
	let y:f32 = 9.19191;

	if(x < y) {
		ret -3333333;
	} else {
		ret 17;
	}
}


pub fn simulate_l_than_bin_op_with_const() -> i32 {
	let x:f32 = 0.0 / 0.0;

	if(x < -11.3333) {
		ret 3;
	} else {
		ret 5;
	}
}


pub fn main() -> i32 {
	//Should return 11 + 5 + 10 + 3 + 5 + 5 + 17 + 5
	OUNIT: [exit_status = 61]
	ret @simulate_equals_bin_op()
		+ @simulate_equals_bin_op_with_const()
		+ @simulate_not_equals_bin_op()
		+ @simulate_not_equals_bin_op_with_const()
		+ @simulate_g_than_bin_op()
		+ @simulate_g_than_bin_op_with_const()
		+ @simulate_l_than_bin_op()
		+ @simulate_l_than_bin_op_with_const();
}
