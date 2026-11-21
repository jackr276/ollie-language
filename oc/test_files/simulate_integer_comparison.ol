/**
 * Author: Jack Robbins
 * Test the simulator where we're dealing with integer comparions. This test
 * covers all of the relational operators
 */

pub fn simulate_equals_bin_op() -> i32 {
	let x:i32 = 7;
	let y:i32 = 7;

	if(x == y) {
		ret -1;
	} else {
		ret 11;
	}
}


pub fn simulate_equals_bin_op_with_const() -> i32 {
	let x:i32 = 7;

	if(x == -1) {
		ret 3;
	} else {
		ret 5;
	}
}


pub fn simulate_not_equals_bin_op() -> i32 {
	let x:i32 = 7;
	let y:i32 = 7;

	if(x != y) {
		ret -1555;
	} else {
		ret 10;
	}
}


pub fn simulate_not_equals_bin_op_with_const() -> i32 {
	let x:i32 = -5;

	if(x != 17) {
		ret 3;
	} else {
		ret 5;
	}
}


pub fn simulate_g_than_bin_op() -> i32 {
	let x:i32 = 7;
	let y:i32 = 9;

	if(x > y) {
		ret -3333333;
	} else {
		ret 5;
	}
}


pub fn simulate_g_than_bin_op_with_const() -> i32 {
	let x:i32 = -5;

	if(x > -11) {
		ret 3;
	} else {
		ret 5;
	}
}


pub fn simulate_l_than_bin_op() -> i32 {
	let x:i32 = 7;
	let y:i32 = 9;

	if(x < y) {
		ret 17;
	} else {
		ret -3333333;
	}
}


pub fn simulate_l_than_bin_op_with_const() -> i32 {
	let x:i32 = -5;

	if(x < -11) {
		ret 3;
	} else {
		ret 5;
	}
}


pub fn main() -> i32 {
	//Should return -1 + 5 + 10 + 3 + 5 + 3 + 17 + 5
	OUNIT: [exit_status = 47]
	ret @simulate_equals_bin_op()
		+ @simulate_equals_bin_op_with_const()
		+ @simulate_not_equals_bin_op()
		+ @simulate_not_equals_bin_op_with_const()
		+ @simulate_g_than_bin_op()
		+ @simulate_g_than_bin_op_with_const()
		+ @simulate_l_than_bin_op()
		+ @simulate_l_than_bin_op_with_const();
}
