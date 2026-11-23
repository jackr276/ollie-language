/**
 * Author: Jack Robbins
 * Test our ability to simulate a binary operation accurately with casting
 */

pub fn simulate_with_cast() -> i32 {
	let x:f32 = 5.55;
	let y:i32 = 5;

	if(<i32>x - y) {
		ret 7;
	} else {
		ret 17;
	}
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 17]
	ret @simulate_with_cast();
}
