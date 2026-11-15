/**
 * Author: Jack Robbins
 * Test a float conditional where we're using != to see if this works
 * properly with NaN handling
 */

pub fn float_not_equals_no_inverse_branch(f1:f32, f2:f32, y:i32, z:i32) -> i32 {
	let result:mut i32 = 0;
	
	jump if_condition when(f1 != f2);
	result = z + 17;
	jump return_block;
	
	#if_condition:
		result = y + 11;

	#return_block:
		ret result;
}


pub fn float_if_assignment(f1:f32, f2:f32, y:i32, z:i32) -> i32 {
	//NaN can never equal anything, so we should jump to the else area
	if(f1 == f2) {
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

	//Should get 9 + 19 = 28
	OUNIT: [exit_status = 28]
	ret @float_if_assignment(NaN, not_nan, 8, 9)
		+ @float_not_equals_no_inverse_branch(NaN, not_nan, 8, 9);
}
