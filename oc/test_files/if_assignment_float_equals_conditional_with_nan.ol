/**
 * Author: Jack Robbins
 * Test float conditionals in if assignments where we have variable assignments to test 
 * our float-specific logic
 *
 * Specifically we are looking for the parity move here to be triggered by the NaN
 */

pub fn float_if_assignment(x:f32) -> i32 {
	declare result:mut i32;

	//This should trigger a floating point equals where we will have a final NaN(parity) check
	result = x == 5.5 ? 88 else 99;

	ret result;
}


pub fn main() -> i32 {
	//Make this all 1's to turn it into NaN
	let x:mut i32 = 0;
	x = ~x;

	//Get this value to actually be NaN
	let NaN:f32 = *(<f32*>(&x));

	OUNIT: [console = 99]
	ret @float_if_assignment(NaN);
}
