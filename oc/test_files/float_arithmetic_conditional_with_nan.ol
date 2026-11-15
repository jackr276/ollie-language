/**
 * Author: Jack Robbins
 * Test a float conditional where we're using arithmetic to see if this works
 * properly with NaN handling
 */

pub fn float_if_assignment(f1:f32, f2:f32, y:i32, z:i32) -> i32 {
	//Nan is not zero, so we should return y
	if(f1 + f2) {
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
	let not_nan:f32 = 5.55;

	OUNIT: [exit_status = 8]
	ret @float_if_assignment(NaN, 5.55, 8, 9);
}
