/**
 * Author: Jack Robbins
 * Test the simulator where we're dealing with bitwise XOR
 */



pub fn main() -> i32 {
	let x:i32 = 5;
	let y:i32 = 0;

	OUNIT: [exit_status = 5]
	if(y | x) {
		ret 5;
	} else {
		ret 7;
	}
}
