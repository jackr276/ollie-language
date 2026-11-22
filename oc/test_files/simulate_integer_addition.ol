/**
 * Author: Jack Robbins
 * Test the simulator where we're dealing with integer addition
 */


pub fn simulate_add_bin_op() -> i32 {
	let x:i32 = 5;
	let y:i32 = 5;

	if(x + y){
		ret 5;
	} else {
		ret 7;
	}
}


pub fn simulate_add_bin_op_with_const() -> i32 {
	let x:i32 = 5;

	if(x + -5){
		ret 11;
	} else {
		ret 71;
	}
}


pub fn main() -> i32 {
	//Should return 5 + 71 = 76
	OUNIT: [exit_status = 76]
	ret @simulate_add_bin_op() + @simulate_add_bin_op_with_const();
}
