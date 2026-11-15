/**
 * Author: Jack Robbins
 * Test a float conditional where we're using logical not to see if this works
 * properly with NaN handling
 */

pub fn float_if_assignment(x:f32, y:i32, z:i32) -> i32 {
	//!(nan) is 0, so we should have 9 as our answer
	if(!x) {
		ret y;
	} else {
		ret z;
	}
}


pub fn main() -> i32 {
	//Make this all 1's to turn it into NaN
	let x:mut i32 = 0;
	x = ~x;

	//Get this value to actually be NaN
	let NaN:f32 = *(<f32*>(&x));

	OUNIT: [exit_status = 9]
	ret @float_if_assignment(NaN, 8, 9);
}
