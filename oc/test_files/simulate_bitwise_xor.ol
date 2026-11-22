/**
 * Author: Jack Robbins
 * Test the simulator where we're dealing with bitwise XOR
 */


pub fn simulate_xor_bin_op() -> i32 {
	let x:i32 = 5;
	let y:i32 = 5;

	if(x ^ y){
		ret 5;
	} else {
		ret 7;
	}
}


pub fn simulate_xor_bin_op_with_const() -> i32 {
	let x:i32 = 5;

	if(x ^ 4){
		ret 11;
	} else {
		ret 71;
	}
}


pub fn main() -> i32 {
	//Should return 7 + 11 = 18
	OUNIT: [exit_status = 18]
	ret @simulate_xor_bin_op() + @simulate_xor_bin_op_with_const();
}
