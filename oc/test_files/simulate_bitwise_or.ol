/**
 * Author: Jack Robbins
 * Test the simulator where we're dealing with bitwise OR
 */


pub fn simulate_or_bin_op() -> i32 {
	let x:i32 = 5;
	let y:i32 = 7;

	if(x | y){
		ret 5;
	} else {
		ret 7;
	}
}


pub fn simulate_or_bin_op_with_const() -> i32 {
	let x:i32 = 0;

	if(x | 5){
		ret 11;
	} else {
		ret 71;
	}
}


pub fn main() -> i32 {
	//Should return 5 + 11 = 16
	OUNIT: [exit_status = 16]
	ret @simulate_or_bin_op() + @simulate_or_bin_op_with_const();
}
